#include "firmware_update.h"

#include <Update.h>
#include <esp_ota_ops.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <mbedtls/sha256.h>

#include "config.h"
#include "sd_logger.h"

namespace {
enum class UpdateSource : uint8_t { NONE=0, LOCAL=1, REMOTE=2 };

SemaphoreHandle_t otaMutex = nullptr;
UpdateSource source = UpdateSource::NONE;
bool inProgress = false;
bool failed = false;
bool firstChunk = true;
bool shaStarted = false;
bool restartPending = false;
uint32_t restartAtMs = 0;
uint32_t expectedSequence = 0;
size_t expectedSize = 0;
size_t receivedSize = 0;
String expectedSha256;
String lastSha256;
String lastError;
String lastResult = "Nessun aggiornamento eseguito";
mbedtls_sha256_context shaCtx;

bool lock(uint32_t timeoutMs=1500) {
  if (!otaMutex) otaMutex = xSemaphoreCreateMutex();
  return otaMutex && xSemaphoreTake(otaMutex, pdMS_TO_TICKS(timeoutMs)) == pdTRUE;
}
void unlock() { if (otaMutex) xSemaphoreGive(otaMutex); }

const char *sourceName(UpdateSource s) {
  switch (s) {
    case UpdateSource::LOCAL: return "local";
    case UpdateSource::REMOTE: return "remote";
    default: return "none";
  }
}

bool validSha256(const String &s) {
  if (s.length() != 64) return false;
  for (char c : s) if (!isxdigit((unsigned char)c)) return false;
  return true;
}

String shaHex(const unsigned char digest[32]) {
  char out[65];
  for (size_t i=0;i<32;i++) snprintf(out+i*2,3,"%02x",digest[i]);
  out[64]=0;
  return String(out);
}

void stopSha() {
  if (shaStarted) {
    mbedtls_sha256_free(&shaCtx);
    shaStarted=false;
  }
}

void setFailure(const String &msg) {
  failed=true;
  lastError=msg;
  lastResult="Aggiornamento fallito";
}

bool beginUpdate(UpdateSource requestedSource, size_t imageSize, const String &sha, String &error) {
  error="";
  if (!lock()) { error="Updater occupato"; return false; }
  if (inProgress) { error="Aggiornamento gia in corso"; unlock(); return false; }

  const esp_partition_t *next = esp_ota_get_next_update_partition(nullptr);
  if (!next) { error="Partizione OTA alternativa non disponibile"; unlock(); return false; }
  if (imageSize > 0 && imageSize > next->size) {
    error="Firmware troppo grande per lo slot OTA";
    unlock();
    return false;
  }
  if (requestedSource == UpdateSource::REMOTE && (!validSha256(sha) || imageSize == 0)) {
    error="Aggiornamento remoto richiede size e SHA-256 validi";
    unlock();
    return false;
  }

  Update.abort();
  const size_t beginSize = imageSize ? imageSize : UPDATE_SIZE_UNKNOWN;
  if (!Update.begin(beginSize, U_FLASH)) {
    error="Update.begin fallita, codice "+String((unsigned)Update.getError());
    unlock();
    return false;
  }

  mbedtls_sha256_init(&shaCtx);
  if (mbedtls_sha256_starts_ret(&shaCtx,0) != 0) {
    Update.abort();
    mbedtls_sha256_free(&shaCtx);
    error="Inizializzazione SHA-256 fallita";
    unlock();
    return false;
  }
  shaStarted=true;
  source=requestedSource;
  inProgress=true;
  failed=false;
  firstChunk=true;
  expectedSequence=0;
  expectedSize=imageSize;
  receivedSize=0;
  expectedSha256=sha;
  expectedSha256.toLowerCase();
  lastSha256="";
  lastError="";
  lastResult="Aggiornamento in corso";
  restartPending=false;
  unlock();
  return true;
}

bool writeUpdate(UpdateSource requestedSource, uint32_t sequence, uint8_t *data, size_t len, String &error) {
  error="";
  if (!data || len == 0) { error="Blocco firmware vuoto"; return false; }
  if (!lock()) { error="Updater occupato"; return false; }
  if (!inProgress || source != requestedSource || failed) { error="Sessione firmware non valida"; unlock(); return false; }
  if (requestedSource == UpdateSource::REMOTE && sequence != expectedSequence) {
    error="Sequenza blocco non valida: atteso "+String(expectedSequence);
    setFailure(error);
    Update.abort();
    stopSha();
    inProgress=false;
    source=UpdateSource::NONE;
    unlock();
    return false;
  }
  if (expectedSize && receivedSize + len > expectedSize) {
    error="Dimensione firmware oltre il valore dichiarato";
    setFailure(error);
    Update.abort();
    stopSha();
    inProgress=false;
    source=UpdateSource::NONE;
    unlock();
    return false;
  }
  if (firstChunk) {
    firstChunk=false;
    if (data[0] != 0xE9) {
      error="File non riconosciuto come immagine firmware ESP32";
      setFailure(error);
      Update.abort();
      stopSha();
      inProgress=false;
      source=UpdateSource::NONE;
      unlock();
      return false;
    }
  }
  const size_t written=Update.write(data,len);
  if (written != len) {
    error="Scrittura OTA incompleta, codice "+String((unsigned)Update.getError());
    setFailure(error);
    Update.abort();
    stopSha();
    inProgress=false;
    source=UpdateSource::NONE;
    unlock();
    return false;
  }
  if (mbedtls_sha256_update_ret(&shaCtx,data,len) != 0) {
    error="Calcolo SHA-256 fallito";
    setFailure(error);
    Update.abort();
    stopSha();
    inProgress=false;
    source=UpdateSource::NONE;
    unlock();
    return false;
  }
  receivedSize += len;
  if (requestedSource == UpdateSource::REMOTE) expectedSequence++;
  unlock();
  return true;
}

bool finishUpdate(UpdateSource requestedSource, String &error) {
  error="";
  if (!lock()) { error="Updater occupato"; return false; }
  if (!inProgress || source != requestedSource || failed) { error="Sessione firmware non valida"; unlock(); return false; }
  if (receivedSize == 0 || (expectedSize && receivedSize != expectedSize)) {
    error="Dimensione firmware incompleta";
    setFailure(error);
    Update.abort();
    stopSha();
    inProgress=false;
    source=UpdateSource::NONE;
    unlock();
    return false;
  }

  unsigned char digest[32]={0};
  if (!shaStarted || mbedtls_sha256_finish_ret(&shaCtx,digest) != 0) {
    error="Chiusura SHA-256 fallita";
    setFailure(error);
    Update.abort();
    stopSha();
    inProgress=false;
    source=UpdateSource::NONE;
    unlock();
    return false;
  }
  lastSha256=shaHex(digest);
  stopSha();

  if (requestedSource == UpdateSource::REMOTE && lastSha256 != expectedSha256) {
    error="SHA-256 firmware non corrispondente";
    setFailure(error);
    Update.abort();
    inProgress=false;
    source=UpdateSource::NONE;
    unlock();
    return false;
  }

  if (!Update.end(false) || !Update.isFinished()) {
    error="Finalizzazione OTA fallita, codice "+String((unsigned)Update.getError());
    setFailure(error);
    Update.abort();
    inProgress=false;
    source=UpdateSource::NONE;
    unlock();
    return false;
  }

  inProgress=false;
  source=UpdateSource::NONE;
  failed=false;
  lastError="";
  lastResult="Firmware scritto correttamente; riavvio programmato";
  restartPending=true;
  restartAtMs=millis()+1800UL;
  unlock();
  return true;
}

void abortUpdate(UpdateSource requestedSource, const String &reason) {
  if (!lock()) return;
  if (inProgress && source == requestedSource) {
    Update.abort();
    stopSha();
    inProgress=false;
    source=UpdateSource::NONE;
    failed=true;
    lastError=reason.isEmpty()?"Aggiornamento interrotto":reason;
    lastResult="Aggiornamento annullato";
  }
  unlock();
}

String jsonEscape(const String &s) {
  String o;o.reserve(s.length()+8);
  for(char c:s){if(c=='\\'||c=='\"'){o+='\\';o+=c;}else if(c=='\n')o+="\\n";else if(c!='\r')o+=c;}
  return o;
}

bool localUploadStarted=false;
bool localUploadOk=false;
String localUploadError;

} // namespace

String firmwareUpdateStatusJson() {
  const esp_partition_t *running=esp_ota_get_running_partition();
  const esp_partition_t *next=esp_ota_get_next_update_partition(nullptr);
  String j;j.reserve(720);
  if (lock(250)) {
    const float pct=(expectedSize>0)?(100.0f*(float)receivedSize/(float)expectedSize):0.0f;
    j="{\"firmware_version\":\""+String(FIRMWARE_VERSION)+"\",\"ota_supported\":"+String(next?"true":"false")+
      ",\"running_slot\":\""+String(running?running->label:"--")+"\",\"running_size\":"+String(running?running->size:0)+
      ",\"next_slot\":\""+String(next?next->label:"--")+"\",\"next_size\":"+String(next?next->size:0)+
      ",\"in_progress\":"+String(inProgress?"true":"false")+",\"source\":\""+String(sourceName(source))+"\",\"received\":"+String(receivedSize)+
      ",\"expected\":"+String(expectedSize)+",\"percent\":"+String(pct,1)+",\"restart_pending\":"+String(restartPending?"true":"false")+
      ",\"last_sha256\":\""+jsonEscape(lastSha256)+"\",\"last_result\":\""+jsonEscape(lastResult)+"\",\"last_error\":\""+jsonEscape(lastError)+"\"}";
    unlock();
  } else j="{\"firmware_version\":\""+String(FIRMWARE_VERSION)+"\",\"in_progress\":true,\"status\":\"busy\"}";
  return j;
}

bool firmwareUpdateInProgress() {
  bool v=true;
  if (lock(100)) { v=inProgress; unlock(); }
  return v;
}

bool firmwareRemoteBegin(size_t imageSize, const String &sha256Hex, String &error) {
  return beginUpdate(UpdateSource::REMOTE,imageSize,sha256Hex,error);
}

bool firmwareRemoteWrite(uint32_t sequence, uint8_t *data, size_t len, String &error) {
  if (len > 8192U) { error="Blocco remoto oltre 8192 byte"; return false; }
  return writeUpdate(UpdateSource::REMOTE,sequence,data,len,error);
}

bool firmwareRemoteEnd(String &error) { return finishUpdate(UpdateSource::REMOTE,error); }
void firmwareRemoteAbort(const String &reason) { abortUpdate(UpdateSource::REMOTE,reason); }

void registerFirmwareUpdateRoutes(WebServer &server) {
  server.on("/api/firmware/status",HTTP_GET,[&server](){
    server.sendHeader("Cache-Control","no-store");
    server.send(200,"application/json",firmwareUpdateStatusJson());
  });

  server.on("/api/firmware/upload",HTTP_POST,[&server](){
    if (!localUploadStarted) {
      server.send(400,"text/plain; charset=utf-8","Nessun firmware ricevuto");
      return;
    }
    if (!localUploadOk) {
      server.send(500,"text/plain; charset=utf-8",localUploadError.isEmpty()?"Aggiornamento firmware fallito":localUploadError);
      return;
    }
    server.send(200,"text/plain; charset=utf-8","Firmware caricato correttamente. Riavvio del gateway in corso...");
  },[&server](){
    HTTPUpload &up=server.upload();
    if (up.status == UPLOAD_FILE_START) {
      localUploadStarted=true;
      localUploadOk=false;
      localUploadError="";
      if (!up.filename.endsWith(".bin")) {
        localUploadError="Selezionare un file firmware .bin";
        return;
      }
      String err;
      if (!beginUpdate(UpdateSource::LOCAL,0,String(),err)) localUploadError=err;
    } else if (up.status == UPLOAD_FILE_WRITE) {
      if (!localUploadError.isEmpty()) return;
      String err;
      if (!writeUpdate(UpdateSource::LOCAL,0,up.buf,up.currentSize,err)) localUploadError=err;
    } else if (up.status == UPLOAD_FILE_END) {
      if (!localUploadError.isEmpty()) { abortUpdate(UpdateSource::LOCAL,localUploadError); return; }
      String err;
      localUploadOk=finishUpdate(UpdateSource::LOCAL,err);
      if (!localUploadOk) localUploadError=err;
    } else if (up.status == UPLOAD_FILE_ABORTED) {
      localUploadError="Upload interrotto dal client";
      abortUpdate(UpdateSource::LOCAL,localUploadError);
    }
  });
}

void serviceFirmwareUpdate() {
  bool doRestart=false;
  if (lock(50)) {
    if (restartPending && (int32_t)(millis()-restartAtMs) >= 0) {
      restartPending=false;
      doRestart=true;
    }
    unlock();
  }
  if (doRestart) {
    shutdownSdLogger();
    delay(80);
    ESP.restart();
  }
}
