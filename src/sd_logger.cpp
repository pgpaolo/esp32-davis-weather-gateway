#include "sd_logger.h"

#include <Arduino.h>
#include <Preferences.h>
#include <SPI.h>
#include <SdFat.h>
#include <math.h>
#include <time.h>
#include <string.h>

#include "board_config.h"
#include "davis_radio.h"
#include "lightning_manager.h"

namespace {
constexpr const char *NVS_NS = "sdlog";
constexpr uint8_t QUEUE_SIZE = 16;
constexpr size_t LINE_SIZE = 520;
constexpr uint32_t WRITE_PERIOD_MS = 750UL;
constexpr uint8_t WRITE_BATCH_MAX = 6;
constexpr uint32_t CAPACITY_REFRESH_MS = 30000UL;
constexpr time_t VALID_EPOCH_MIN = 1700000000;
constexpr uint32_t RETRY_DELAYS_MS[] = {5000UL, 15000UL, 60000UL, 300000UL};

const char *CSV_HEADER =
    "timestamp_utc,uptime_ms,record_type,station_id,packet_type,channel,frequency_mhz,rssi_dbm,crc_ok,crc_received,crc_calculated,"
    "temperature_c,humidity_pct,wind_kmh,gust_kmh,wind_dir_deg,rain_day_mm,rain_rate_mmh,rain_month_mm,rain_year_mm,uv,solar_wm2,"
    "pressure_abs_hpa,pressure_sl_hpa,local_temp_c,local_humidity_pct,lightning_distance_km,lightning_energy,lightning_count,battery_low,raw,normalized\n";

struct PendingLine { char text[LINE_SIZE] = {0}; };

SPIClass sdSpi(HSPI);
SdFat32 sd;
SdLoggerConfig cfg;
SdLoggerStatus status;
PendingLine queueBuf[QUEUE_SIZE];
uint8_t queueHead = 0;
uint8_t queueTail = 0;
bool spiStarted = false;
uint32_t lastWriteServiceMs = 0;
uint32_t lastCapacityRefreshMs = 0;
uint32_t lastSnapshotMs = 0;
uint32_t lastFrameSequence = 0;
uint32_t lastLightningTotal = 0;
uint8_t retryStep = 0;
uint32_t retryDueMs = 0;

uint8_t queueDepth() {
  return queueHead >= queueTail ? static_cast<uint8_t>(queueHead - queueTail)
                                : static_cast<uint8_t>(QUEUE_SIZE - queueTail + queueHead);
}

bool timeValid(struct tm *utcOut = nullptr) {
  const time_t now = time(nullptr);
  if (now < VALID_EPOCH_MIN) return false;
  if (utcOut) gmtime_r(&now, utcOut);
  return true;
}

void timestampUtc(char *out, size_t len) {
  if (!out || !len) return;
  struct tm utc{};
  if (!timeValid(&utc)) { out[0] = '\0'; return; }
  strftime(out, len, "%Y-%m-%dT%H:%M:%SZ", &utc);
}

void formatFloat(char *out, size_t len, float value, uint8_t decimals = 1) {
  if (!out || !len) return;
  if (!isfinite(value)) { out[0] = '\0'; return; }
  if (decimals == 0) snprintf(out, len, "%.0f", value);
  else if (decimals == 2) snprintf(out, len, "%.2f", value);
  else if (decimals == 3) snprintf(out, len, "%.3f", value);
  else snprintf(out, len, "%.1f", value);
}

void hex10(const uint8_t data[10], char *out, size_t len) {
  if (!out || len < 21U) return;
  size_t pos = 0;
  for (uint8_t i = 0; i < 10 && pos + 3U < len; ++i)
    pos += snprintf(out + pos, len - pos, "%02X", data[i]);
}

bool queueLine(const char *line) {
  if (!cfg.enabled || !status.mounted || !line || !line[0]) return false;
  const uint8_t next = static_cast<uint8_t>((queueHead + 1U) % QUEUE_SIZE);
  if (next == queueTail) { status.recordsDropped++; return false; }
  strncpy(queueBuf[queueHead].text, line, LINE_SIZE - 1U);
  queueBuf[queueHead].text[LINE_SIZE - 1U] = '\0';
  queueHead = next;
  status.recordsQueued++;
  status.queueDepth = queueDepth();
  return true;
}

void ensureDir(const char *path) {
  if (!path || !*path || sd.exists(path)) return;
  sd.mkdir(path);
}

bool logPath(char *out, size_t len) {
  if (!out || len < 32U) return false;
  ensureDir("/weather");
  struct tm utc{};
  if (!timeValid(&utc)) {
    snprintf(out, len, "/weather/unsynced.csv");
    return true;
  }
  char y[20], m[28];
  snprintf(y, sizeof(y), "/weather/%04d", utc.tm_year + 1900);
  snprintf(m, sizeof(m), "%s/%02d", y, utc.tm_mon + 1);
  ensureDir(y); ensureDir(m);
  snprintf(out, len, "%s/%04d-%02d-%02d.csv", m, utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday);
  return true;
}

void refreshCapacity() {
  if (!status.mounted || !sd.card()) return;
  status.cardSizeBytes = static_cast<uint64_t>(sd.card()->sectorCount()) * 512ULL;
  const uint64_t bytesPerCluster = sd.bytesPerCluster();
  status.totalBytes = static_cast<uint64_t>(sd.clusterCount()) * bytesPerCluster;
  const uint64_t freeBytes = static_cast<uint64_t>(sd.freeClusterCount()) * bytesPerCluster;
  status.usedBytes = status.totalBytes >= freeBytes ? status.totalBytes - freeBytes : 0;
  lastCapacityRefreshMs = millis();
}

bool appendBatch() {
  if (!status.mounted || queueTail == queueHead) return false;
  char path[80];
  if (!logPath(path, sizeof(path))) return false;
  const bool fresh = !sd.exists(path);
  File32 f = sd.open(path, O_WRONLY | O_CREAT | O_APPEND);
  if (!f) { status.writeErrors++; return false; }
  bool ok = true;
  if (fresh && f.print(CSV_HEADER) == 0) ok = false;
  uint8_t count = 0;
  while (ok && queueTail != queueHead && count < WRITE_BATCH_MAX) {
    if (f.println(queueBuf[queueTail].text) == 0) { ok = false; break; }
    queueTail = static_cast<uint8_t>((queueTail + 1U) % QUEUE_SIZE);
    status.recordsWritten++; count++;
  }
  f.flush(); f.close();
  if (!ok) status.writeErrors++;
  else {
    status.lastWriteMs = millis();
    strncpy(status.currentFile, path, sizeof(status.currentFile) - 1U);
    status.currentFile[sizeof(status.currentFile) - 1U] = '\0';
  }
  status.queueDepth = queueDepth();
  return ok;
}

void closeStorage() {
  sd.end();
  if (spiStarted) { sdSpi.end(); spiStarted = false; }
  status.mounted = false;
  status.cardSizeBytes = status.totalBytes = status.usedBytes = 0;
  status.spiFrequencyHz = 0;
  status.currentFile[0] = '\0';
  queueHead = queueTail = 0;
  status.queueDepth = 0;
}

void clearRetry() {
  retryStep = 0; retryDueMs = 0; status.retryPending = false; status.retryInMs = 0;
}

void scheduleRetry() {
  if (!cfg.enabled) { clearRetry(); return; }
  const uint8_t idx = retryStep < 4U ? retryStep : 3U;
  retryDueMs = millis() + RETRY_DELAYS_MS[idx];
  if (retryStep < 3U) retryStep++;
  status.retryPending = true;
}

bool mountCard(bool formatRequested) {
#if !SDCARD_SUPPORTED
  status.supported = false;
  return false;
#else
  closeStorage();
  status.supported = true;
  status.mountAttempts++;
  status.sdErrorCode = status.sdErrorData = 0;
  constexpr uint32_t speeds[] = {SD_SCK_MHZ(4), 400000UL};
  for (uint8_t i = 0; i < 2U; ++i) {
    pinMode(SDCARD_CS_PIN, OUTPUT);
    digitalWrite(SDCARD_CS_PIN, HIGH);
    delay(8);
    sdSpi.begin(SDCARD_SCLK_PIN, SDCARD_MISO_PIN, SDCARD_MOSI_PIN);
    spiStarted = true;
    const SdSpiConfig bus(SDCARD_CS_PIN, SHARED_SPI, speeds[i], &sdSpi);
    bool mounted = sd.begin(bus);
    status.sdErrorCode = sd.sdErrorCode();
    status.sdErrorData = sd.sdErrorData();
    const bool cardTransportReady = mounted || status.sdErrorCode == 0;

    if (formatRequested && cardTransportReady) {
      Serial.println(F("[SD] formattazione FAT richiesta esplicitamente"));
      if (!sd.format(&Serial)) {
        status.sdErrorCode = sd.sdErrorCode();
        status.sdErrorData = sd.sdErrorData();
        closeStorage();
        return false;
      }
      sd.end(); sdSpi.end(); spiStarted = false; delay(20);
      pinMode(SDCARD_CS_PIN, OUTPUT); digitalWrite(SDCARD_CS_PIN, HIGH);
      sdSpi.begin(SDCARD_SCLK_PIN, SDCARD_MISO_PIN, SDCARD_MOSI_PIN); spiStarted = true;
      mounted = sd.begin(bus);
      status.sdErrorCode = sd.sdErrorCode(); status.sdErrorData = sd.sdErrorData();
    }

    if (mounted) {
      status.mounted = true; status.spiFrequencyHz = speeds[i];
      status.sdErrorCode = status.sdErrorData = 0;
      refreshCapacity(); clearRetry();
      Serial.print(F("[SD] montata a ")); Serial.print(speeds[i] / 1000UL); Serial.print(F(" kHz, "));
      Serial.print(static_cast<unsigned long>(status.cardSizeBytes / (1024ULL * 1024ULL))); Serial.println(F(" MB"));
      return true;
    }

    sd.end(); sdSpi.end(); spiStarted = false;
    if (cardTransportReady) break;
    delay(20);
  }
  closeStorage(); scheduleRetry();
  Serial.print(F("[SD] mount fallito; error=0x")); Serial.print(status.sdErrorCode, HEX);
  Serial.print(F(" data=0x")); Serial.println(status.sdErrorData, HEX);
  return false;
#endif
}

bool sameConfig(const SdLoggerConfig &a, const SdLoggerConfig &b) {
  return a.enabled == b.enabled && a.logRfFrames == b.logRfFrames &&
         a.logWeatherSnapshots == b.logWeatherSnapshots && a.logBme280 == b.logBme280 &&
         a.logAs3935 == b.logAs3935 && a.snapshotIntervalSec == b.snapshotIntervalSec;
}

bool validConfig(const SdLoggerConfig &c) { return c.snapshotIntervalSec >= 30U && c.snapshotIntervalSec <= 3600U; }

void loadConfig() {
  cfg = SdLoggerConfig{};
  Preferences p;
  if (!p.begin(NVS_NS, true)) return;
  cfg.enabled = p.getBool("enabled", cfg.enabled);
  cfg.logRfFrames = p.getBool("rf", cfg.logRfFrames);
  cfg.logWeatherSnapshots = p.getBool("weather", cfg.logWeatherSnapshots);
  cfg.logBme280 = p.getBool("bme", cfg.logBme280);
  cfg.logAs3935 = p.getBool("as3935", cfg.logAs3935);
  cfg.snapshotIntervalSec = p.getUShort("snap_s", cfg.snapshotIntervalSec);
  p.end();
  if (!validConfig(cfg)) cfg = SdLoggerConfig{};
}

void queueRfFrame(const DavisFrameDiagnostic &f) {
  if (!cfg.logRfFrames || !f.valid) return;
  char ts[24], raw[24] = {0}, norm[24] = {0}; timestampUtc(ts, sizeof(ts));
  hex10(f.raw, raw, sizeof(raw)); hex10(f.normalized, norm, sizeof(norm));
  const uint8_t stationId = f.crcOk ? static_cast<uint8_t>((f.normalized[0] & 0x07U) + 1U) : 0U;
  const uint8_t type = f.crcOk ? static_cast<uint8_t>(f.normalized[0] >> 4U) : 0U;
  char line[LINE_SIZE];
  snprintf(line, sizeof(line),
           "%s,%lu,davis_rf,%u,%s,%u,%.6f,%.1f,%u,%04X,%04X,,,,,,,,,,,,,,,,,,,,%s,%s",
           ts, static_cast<unsigned long>(f.timestampMs), static_cast<unsigned>(stationId),
           f.crcOk ? davisPacketTypeName(type) : "", static_cast<unsigned>(f.channel + 1U),
           f.frequencyMhz, isfinite(f.rssi) ? f.rssi : 0.0f, f.crcOk ? 1U : 0U,
           static_cast<unsigned>(f.crcReceived), static_cast<unsigned>(f.crcCalculated), raw, norm);
  queueLine(line);
}

void queueWeather(const StationState &s) {
  if (!cfg.logWeatherSnapshots) return;
  char ts[24]; timestampUtc(ts, sizeof(ts));
  char temp[16], hum[16], wind[16], gust[16], dir[16], rain[16], rate[16], month[16], year[16], uv[16], solar[16];
  char pAbs[16], pSl[16], tin[16], hin[16];
  formatFloat(temp,sizeof(temp),s.outTempC); formatFloat(hum,sizeof(hum),s.outHumidity);
  formatFloat(wind,sizeof(wind),s.windKmh); formatFloat(gust,sizeof(gust),s.windGustKmh); formatFloat(dir,sizeof(dir),s.windDirDeg,0);
  formatFloat(rain,sizeof(rain),s.rainDayMm,2); formatFloat(rate,sizeof(rate),s.rainRateMmH,2);
  formatFloat(month,sizeof(month),s.rainMonthMm,2); formatFloat(year,sizeof(year),s.rainYearMm,2);
  formatFloat(uv,sizeof(uv),s.uv); formatFloat(solar,sizeof(solar),s.solarWm2,0);
  formatFloat(pAbs,sizeof(pAbs),cfg.logBme280?s.pressureAbsoluteHpa:NAN); formatFloat(pSl,sizeof(pSl),cfg.logBme280?s.pressureHpa:NAN);
  formatFloat(tin,sizeof(tin),cfg.logBme280?s.indoorTempC:NAN); formatFloat(hin,sizeof(hin),cfg.logBme280?s.indoorHumidity:NAN);
  const LightningState ls = getLightningState();
  char line[LINE_SIZE];
  snprintf(line, sizeof(line),
           "%s,%lu,weather,%u,%s,,,,,,,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%u,%lu,%lu,%u,,",
           ts, static_cast<unsigned long>(millis()), s.locked ? static_cast<unsigned>(s.stationId + 1U) : 0U,
           davisPacketTypeName(s.lastPacketType), temp, hum, wind, gust, dir, rain, rate, month, year, uv, solar,
           pAbs, pSl, tin, hin,
           (cfg.logAs3935 && ls.detected) ? static_cast<unsigned>(ls.lastDistanceKm) : 0U,
           (cfg.logAs3935 && ls.detected) ? static_cast<unsigned long>(ls.lastEnergy) : 0UL,
           (cfg.logAs3935 && ls.detected) ? static_cast<unsigned long>(ls.lightningTotal) : 0UL,
           s.batteryLow ? 1U : 0U);
  queueLine(line);
}

void queueLightningEventIfNeeded() {
  if (!cfg.logAs3935) return;
  const LightningState ls = getLightningState();
  if (ls.lightningTotal == lastLightningTotal) return;
  lastLightningTotal = ls.lightningTotal;
  char ts[24]; timestampUtc(ts, sizeof(ts));
  char line[LINE_SIZE];
  snprintf(line, sizeof(line),
           "%s,%lu,lightning,,,,,,,,,,,,,,,,,,,,,,,,%u,%lu,%lu,,,",
           ts, static_cast<unsigned long>(millis()), static_cast<unsigned>(ls.lastDistanceKm),
           static_cast<unsigned long>(ls.lastEnergy), static_cast<unsigned long>(ls.lightningTotal));
  queueLine(line);
}

void queueNewRfFrames() {
  if (!cfg.logRfFrames || !status.mounted) return;
  const uint8_t count = getDavisFrameHistoryCount();
  if (!count) return;
  DavisFrameDiagnostic newest{};
  if (getDavisFrameHistory(0, newest) && newest.valid && newest.sequence < lastFrameSequence) lastFrameSequence = 0;
  for (int i = static_cast<int>(count) - 1; i >= 0; --i) {
    DavisFrameDiagnostic f{};
    if (!getDavisFrameHistory(static_cast<uint8_t>(i), f) || !f.valid || f.sequence <= lastFrameSequence) continue;
    queueRfFrame(f); lastFrameSequence = f.sequence;
  }
}
} // namespace

void initSdLogger() {
#if SDCARD_SUPPORTED
  status.supported = true;
#else
  status.supported = false;
#endif
  loadConfig();
  lastLightningTotal = getLightningState().lightningTotal;
  if (cfg.enabled && status.supported) mountCard(false);
  else Serial.println(F("[SD] datalogger disabilitato"));
}

void serviceSdLogger(const StationState &station) {
  status.timeSynced = timeValid();
  if (!cfg.enabled || !status.supported) return;
  const uint32_t now = millis();
  if (!status.mounted) {
    if (status.retryPending) {
      const int32_t delta = static_cast<int32_t>(retryDueMs - now);
      status.retryInMs = delta > 0 ? static_cast<uint32_t>(delta) : 0U;
      if (delta <= 0) mountCard(false);
    }
    return;
  }
  queueNewRfFrames();
  queueLightningEventIfNeeded();
  const uint32_t snapshotMs = static_cast<uint32_t>(cfg.snapshotIntervalSec) * 1000UL;
  if (snapshotMs && static_cast<uint32_t>(now - lastSnapshotMs) >= snapshotMs) {
    lastSnapshotMs = now; queueWeather(station);
  }
  if (static_cast<uint32_t>(now - lastWriteServiceMs) >= WRITE_PERIOD_MS) {
    lastWriteServiceMs = now; appendBatch();
  }
  if (static_cast<uint32_t>(now - lastCapacityRefreshMs) >= CAPACITY_REFRESH_MS) refreshCapacity();
}

void shutdownSdLogger() {
  if (status.mounted) for (uint8_t i = 0; i < 4U && queueTail != queueHead; ++i) appendBatch();
  closeStorage();
}

SdLoggerConfig getSdLoggerConfig() { return cfg; }
SdLoggerStatus getSdLoggerStatus() {
  SdLoggerStatus s = status; s.queueDepth = queueDepth(); s.timeSynced = timeValid();
  if (s.retryPending) {
    const int32_t d = static_cast<int32_t>(retryDueMs - millis());
    s.retryInMs = d > 0 ? static_cast<uint32_t>(d) : 0U;
  }
  return s;
}

bool saveSdLoggerConfig(const SdLoggerConfig &next, bool &changed) {
  if (!validConfig(next)) return false;
  changed = !sameConfig(next, cfg); if (!changed) return true;
  Preferences p; if (!p.begin(NVS_NS, false)) return false;
  p.putBool("enabled",next.enabled); p.putBool("rf",next.logRfFrames); p.putBool("weather",next.logWeatherSnapshots);
  p.putBool("bme",next.logBme280); p.putBool("as3935",next.logAs3935); p.putUShort("snap_s",next.snapshotIntervalSec); p.end();
  const bool wasEnabled = cfg.enabled; cfg = next;
  if (wasEnabled && !cfg.enabled) { shutdownSdLogger(); clearRetry(); }
  else if (!wasEnabled && cfg.enabled) mountCard(false);
  return true;
}

bool resetSdLoggerConfig(bool &changed) { return saveSdLoggerConfig(SdLoggerConfig{}, changed); }
bool remountSdLogger() { const bool ok = mountCard(false); if (!ok) scheduleRetry(); return ok; }
bool formatSdLogger() { const bool ok = mountCard(true); if (!ok) scheduleRetry(); return ok; }

bool sdLoggerCurrentFilePreview(String &content, uint32_t &fileSize, bool &truncated) {
  content = "";
  fileSize = 0;
  truncated = false;
#if !SDCARD_SUPPORTED
  return false;
#else
  if (!status.mounted || !status.currentFile[0]) return false;
  File32 f = sd.open(status.currentFile, O_RDONLY);
  if (!f || f.isDirectory()) { if (f) f.close(); return false; }

  constexpr size_t MAX_PREVIEW = 12U * 1024U;
  constexpr size_t READ_CHUNK = 256U;
  char buf[READ_CHUNK];
  fileSize = static_cast<uint32_t>(f.fileSize());

  if (fileSize <= MAX_PREVIEW) {
    content.reserve(fileSize + 1U);
    while (content.length() < MAX_PREVIEW) {
      const size_t room = MAX_PREVIEW - content.length();
      const int n = f.read(buf, room < READ_CHUNK ? room : READ_CHUNK);
      if (n <= 0) break;
      content.concat(buf, static_cast<unsigned int>(n));
    }
    f.close();
    return true;
  }

  truncated = true;
  String header;
  header.reserve(640U);
  f.seekSet(0);
  char ch = 0;
  while (header.length() < 640U && f.read(&ch, 1U) == 1) {
    if (ch == '\n') break;
    if (ch != '\r') header += ch;
  }

  const String marker = "\n# ... anteprima troncata: ultime righe del file ...\n";
  const size_t fixed = header.length() + marker.length();
  const size_t tailBudget = MAX_PREVIEW > fixed ? MAX_PREVIEW - fixed : MAX_PREVIEW / 2U;
  const uint32_t seekPos = fileSize > tailBudget ? fileSize - static_cast<uint32_t>(tailBudget) : 0U;
  f.seekSet(seekPos);
  if (seekPos > 0U) {
    while (f.read(&ch, 1U) == 1) if (ch == '\n') break;
  }

  content.reserve(MAX_PREVIEW + 1U);
  content = header;
  content += marker;
  while (content.length() < MAX_PREVIEW) {
    const size_t room = MAX_PREVIEW - content.length();
    const int n = f.read(buf, room < READ_CHUNK ? room : READ_CHUNK);
    if (n <= 0) break;
    content.concat(buf, static_cast<unsigned int>(n));
  }
  f.close();
  return true;
#endif
}

String sdLoggerConfigJson() {
  String j; j.reserve(220); j="{\"enabled\":"; j+=cfg.enabled?"true":"false";
  j+=",\"log_rf\":"; j+=cfg.logRfFrames?"true":"false";
  j+=",\"log_weather\":"; j+=cfg.logWeatherSnapshots?"true":"false";
  j+=",\"log_bme280\":"; j+=cfg.logBme280?"true":"false";
  j+=",\"log_as3935\":"; j+=cfg.logAs3935?"true":"false";
  j+=",\"snapshot_interval_s\":"+String(cfg.snapshotIntervalSec)+"}"; return j;
}

String sdLoggerStatusJson() {
  const SdLoggerStatus s=getSdLoggerStatus(); String j; j.reserve(600); j="{\"supported\":";j+=s.supported?"true":"false";
  j+=",\"mounted\":";j+=s.mounted?"true":"false";j+=",\"time_synced\":";j+=s.timeSynced?"true":"false";
  j+=",\"retry_pending\":";j+=s.retryPending?"true":"false";j+=",\"retry_in_ms\":"+String(s.retryInMs);
  j+=",\"card_size\":"+String(static_cast<unsigned long long>(s.cardSizeBytes));j+=",\"total_bytes\":"+String(static_cast<unsigned long long>(s.totalBytes));j+=",\"used_bytes\":"+String(static_cast<unsigned long long>(s.usedBytes));
  j+=",\"mount_attempts\":"+String(s.mountAttempts)+",\"queued\":"+String(s.recordsQueued)+",\"written\":"+String(s.recordsWritten)+",\"dropped\":"+String(s.recordsDropped)+",\"write_errors\":"+String(s.writeErrors)+",\"queue_depth\":"+String(s.queueDepth)+",\"last_write_ms\":"+String(s.lastWriteMs);
  j+=",\"spi_hz\":"+String(s.spiFrequencyHz)+",\"sd_error\":"+String(s.sdErrorCode)+",\"sd_error_data\":"+String(s.sdErrorData)+",\"file\":\""+String(s.currentFile)+"\"}"; return j;
}
