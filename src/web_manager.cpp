#include "web_manager.h"

#include <WebServer.h>
#include <WiFi.h>
#include <esp_system.h>
#include <math.h>

#include "board_config.h"
#include "config.h"
#include "davis_radio.h"
#include "lightning_manager.h"
#include "meteobridge_client.h"
#include "mqtt_publisher.h"
#include "network_manager.h"
#include "pressure_manager.h"
#include "remote_access.h"
#include "runtime_config.h"
#include "sd_logger.h"
#include "web_ui_generated.h"

namespace {
WebServer server(80);
StationState *statePtr=nullptr;
bool started=false;

String escJson(const String &s){String o;o.reserve(s.length()+8);for(size_t i=0;i<s.length();++i){char c=s[i];if(c=='\\'||c=='\"'){o+='\\';o+=c;}else if(c=='\n')o+="\\n";else if(c!='\r')o+=c;}return o;}
String jf(float v,unsigned int d=1U){return isfinite(v)?String(v,d):String("null");}
bool validIp(const String &x){IPAddress p;return p.fromString(x);}
void noCache(){server.sendHeader("Cache-Control","no-store, no-cache, must-revalidate, max-age=0");server.sendHeader("Pragma","no-cache");}

const char *resetReasonName(){switch(esp_reset_reason()){case ESP_RST_POWERON:return "POWERON";case ESP_RST_EXT:return "EXTERNAL";case ESP_RST_SW:return "SOFTWARE";case ESP_RST_PANIC:return "PANIC";case ESP_RST_INT_WDT:return "INT_WDT";case ESP_RST_TASK_WDT:return "TASK_WDT";case ESP_RST_WDT:return "WDT";case ESP_RST_DEEPSLEEP:return "DEEPSLEEP";case ESP_RST_BROWNOUT:return "BROWNOUT";case ESP_RST_SDIO:return "SDIO";default:return "UNKNOWN";}}

String systemJson(){String j;j.reserve(320);j="{\"uptime_ms\":"+String(millis())+",\"free_heap\":"+String(ESP.getFreeHeap())+",\"min_free_heap\":"+String(ESP.getMinFreeHeap())+",\"cpu_mhz\":"+String(ESP.getCpuFreqMHz())+",\"wifi_rssi_dbm\":"+(networkConnected()?String(WiFi.RSSI()):String("null"))+",\"reset_reason\":\""+String(resetReasonName())+"\"}";return j;}

String rfJsonWithAge(bool history=false){String j=davisRadioDiagnosticsJson(*statePtr,runtimeConfig.issId,history);if(j.endsWith("}")){j.remove(j.length()-1);j+=",\"last_packet_age_ms\":"+(statePtr->lastPacketMs?String((uint32_t)(millis()-statePtr->lastPacketMs)):String("null"))+"}";}return j;}

String sdStateJson(){String j=sdLoggerStatusJson();const SdLoggerConfig c=getSdLoggerConfig();if(j.startsWith("{")){j.remove(0,1);j="{\"enabled\":"+String(c.enabled?"true":"false")+","+j;}return j;}

String stateJson(){
  const StationState&s=*statePtr;const UploadStatus&up=getUploadStatus();const MqttRuntimeConfig mc=getMqttConfig();const MqttRuntimeStatus&ms=getMqttStatus();
  String j;j.reserve(9800);j="{\"firmware\":\""+String(FIRMWARE_VERSION)+"\",\"board\":\""+String(BOARD_NAME)+"\"";
  j+=",\"system\":"+systemJson();
  j+=",\"network\":{\"connected\":";j+=networkConnected()?"true":"false";j+=",\"ip\":\""+escJson(networkIp())+"\",\"ssid\":\""+escJson(networkSsid())+"\",\"mode\":\""+escJson(networkModeName())+"\"}";
  j+=",\"weather\":{\"temperature_c\":"+jf(s.outTempC)+",\"humidity_pct\":"+jf(s.outHumidity,0)+",\"dewpoint_c\":"+jf(calcDewPointC(s.outTempC,s.outHumidity))+",\"wind_kmh\":"+jf(s.windKmh)+",\"gust_kmh\":"+jf(s.windGustKmh)+",\"direction_deg\":"+jf(s.windDirDeg,0)+",\"wind_chill_c\":"+jf(calcWindChillC(s.outTempC,s.windKmh))+",\"rain_rate_mmh\":"+jf(s.rainRateMmH)+",\"rain_day_mm\":"+jf(s.rainDayMm)+",\"rain_month_mm\":"+jf(s.rainMonthMm)+",\"rain_year_mm\":"+jf(s.rainYearMm)+",\"uv\":"+jf(s.uv)+",\"solar_wm2\":"+jf(s.solarWm2,0)+"}";
  j+=",\"rf\":"+rfJsonWithAge(false);j+=",\"bme\":"+pressureStatusJson(s);j+=",\"lightning\":"+lightningStateJson();
  j+=",\"mqtt\":{\"enabled\":";j+=mc.enabled?"true":"false";j+=",\"connected\":";j+=ms.connected?"true":"false";j+=",\"broker\":\""+escJson(mc.broker)+"\",\"tls\":\""+String(mqttTlsModeName(mc.tlsMode))+"\",\"connect_attempts\":"+String(ms.connectAttempts)+",\"publishes\":"+String(ms.publishes)+",\"last_error\":\""+escJson(ms.lastError)+"\"}";
  j+=",\"sd\":"+sdStateJson();j+=",\"remote\":"+remoteAccessStatusJson();
  j+=",\"upload\":{\"attempts\":"+String(up.attempts)+",\"successes\":"+String(up.successes)+",\"last_http_code\":"+String(up.lastHttpCode)+",\"last_attempt_age_ms\":"+(up.lastAttemptMs?String((uint32_t)(millis()-up.lastAttemptMs)):String("0"))+",\"last_success_age_ms\":"+(up.lastSuccessMs?String((uint32_t)(millis()-up.lastSuccessMs)):String("0"))+",\"last_message\":\""+escJson(up.lastMessage)+"\"}}";
  return j;
}

String configJson(){String j;j.reserve(850);j="{\"hostname\":\""+escJson(runtimeConfig.hostname)+"\",\"dhcp\":";j+=runtimeConfig.useDhcp?"true":"false";j+=",\"static_ip\":\""+escJson(runtimeConfig.staticIp)+"\",\"gateway\":\""+escJson(runtimeConfig.gateway)+"\",\"netmask\":\""+escJson(runtimeConfig.netmask)+"\",\"dns\":\""+escJson(runtimeConfig.dns)+"\",\"iss_id\":"+String(runtimeConfig.issId)+",\"rain_tip_mm\":"+String(runtimeConfig.rainMmPerTip,3)+",\"bme_altitude_m\":"+String(runtimeConfig.bmeAltitudeM,1)+",\"timezone\":\""+escJson(runtimeConfig.tzInfo)+"\",\"mb_url\":\""+escJson(runtimeConfig.mbUrl)+"\",\"upload_interval_ms\":"+String(runtimeConfig.uploadIntervalMs)+",\"tls_insecure\":";j+=runtimeConfig.tlsInsecure?"true":"false";j+="}";return j;}

String diagnosticReport(){String r;r.reserve(16000);r+=davisRadioDiagnosticReport(*statePtr,runtimeConfig.issId);r+="\nSystem:\nReset: "+String(resetReasonName())+"\nUptime ms: "+String(millis())+"\nFree heap: "+String(ESP.getFreeHeap())+"\nMin free heap: "+String(ESP.getMinFreeHeap())+"\nCPU MHz: "+String(ESP.getCpuFreqMHz())+"\nWiFi: "+String(networkConnected()?"connected":"offline")+"\nIP: "+networkIp()+"\nWiFi RSSI: "+(networkConnected()?String(WiFi.RSSI())+" dBm":String("--"))+"\n\nBME280:\n"+pressureStatusJson(*statePtr)+"\n\nAS3935:\n"+lightningStateJson()+"\n\nMQTT:\n"+mqttStatusJson()+"\n\nmicroSD:\n"+sdLoggerStatusJson()+"\n\nRemote-ready:\n"+remoteAccessStatusJson()+"\n\nHTTP upload:\n";const UploadStatus&u=getUploadStatus();r+="Attempts="+String(u.attempts)+" Successes="+String(u.successes)+" LastHTTP="+String(u.lastHttpCode)+" Message="+u.lastMessage+"\n";return r;}

void sendDashboard(){noCache();server.sendHeader("Content-Encoding","gzip");server.send_P(200,"text/html; charset=utf-8",(PGM_P)WEB_UI_GZ,WEB_UI_GZ_LEN);}
} // namespace

void initWeb(StationState &station){
  if(started)return;statePtr=&station;
  server.on("/",HTTP_GET,sendDashboard);
  server.on("/config",HTTP_GET,[]{server.sendHeader("Location","/#config",true);server.send(302,"text/plain","");});
  server.on("/api/state",HTTP_GET,[]{noCache();server.send(200,"application/json",stateJson());});
  server.on("/api/status",HTTP_GET,[]{noCache();server.send(200,"application/json",stateJson());});
  server.on("/api/system",HTTP_GET,[]{server.send(200,"application/json",systemJson());});
  server.on("/api/rf",HTTP_GET,[]{server.send(200,"application/json",rfJsonWithAge(false));});
  server.on("/api/rf/diagnostics",HTTP_GET,[]{server.send(200,"application/json",rfJsonWithAge(true));});
  server.on("/api/rf/reset",HTTP_POST,[]{resetDavisDiagnosticWindow();server.send(200,"text/plain","Finestra diagnostica RF azzerata");});
  server.on("/api/diag/report",HTTP_GET,[]{server.sendHeader("Content-Disposition","attachment; filename=davis-diagnostic.txt");server.send(200,"text/plain; charset=utf-8",diagnosticReport());});
  server.on("/api/bme",HTTP_GET,[]{server.send(200,"application/json",pressureStatusJson(*statePtr));});
  server.on("/api/i2c/scan",HTTP_GET,[]{server.send(200,"application/json",i2cScanJson());});
  server.on("/api/as3935/state",HTTP_GET,[]{server.send(200,"application/json",lightningStateJson());});
  server.on("/api/as3935/config",HTTP_GET,[]{server.send(200,"application/json",lightningConfigJson());});
  server.on("/api/mqtt/config",HTTP_GET,[]{server.send(200,"application/json",mqttConfigJson());});
  server.on("/api/mqtt/status",HTTP_GET,[]{server.send(200,"application/json",mqttStatusJson());});
  server.on("/api/config",HTTP_GET,[]{server.send(200,"application/json",configJson());});
  server.on("/api/meteobridge",HTTP_GET,[]{server.send(200,"text/plain; charset=utf-8",buildMeteobridgeCompatibleRecord(*statePtr));});
  server.on("/api/sd",HTTP_GET,[]{server.send(200,"application/json","{\"config\":"+sdLoggerConfigJson()+",\"status\":"+sdLoggerStatusJson()+"}");});
  server.on("/api/remote/config",HTTP_GET,[]{server.send(200,"application/json",remoteAccessConfigJson());});
  server.on("/api/remote/status",HTTP_GET,[]{server.send(200,"application/json",remoteAccessStatusJson());});

  server.on("/api/config",HTTP_POST,[]{
    runtimeConfig.hostname=server.arg("host");if(runtimeConfig.hostname.isEmpty())runtimeConfig.hostname=DEVICE_HOSTNAME_DEFAULT;
    runtimeConfig.useDhcp=server.hasArg("dhcp");runtimeConfig.staticIp=server.arg("ip");runtimeConfig.gateway=server.arg("gw");runtimeConfig.netmask=server.arg("mask");runtimeConfig.dns=server.arg("dns");
    if(!runtimeConfig.useDhcp&&(!validIp(runtimeConfig.staticIp)||!validIp(runtimeConfig.gateway)||!validIp(runtimeConfig.netmask)||!validIp(runtimeConfig.dns))){server.send(400,"text/plain","Profilo IP statico non valido");return;}
    runtimeConfig.issId=(uint8_t)constrain(server.arg("issid").toInt(),0,8);const float tip=server.arg("raintip").toFloat();if(tip>=0.05f&&tip<=1.0f)runtimeConfig.rainMmPerTip=tip;
    const float alt=server.arg("altm").toFloat();if(alt>=-500&&alt<=9000)runtimeConfig.bmeAltitudeM=alt;runtimeConfig.tzInfo=server.arg("tz");if(runtimeConfig.tzInfo.isEmpty())runtimeConfig.tzInfo=TZ_INFO_DEFAULT;
    runtimeConfig.mbUrl=server.arg("mburl");uint32_t iv=(uint32_t)server.arg("mbint").toInt();runtimeConfig.uploadIntervalMs=constrain(iv,5000UL,300000UL);runtimeConfig.tlsInsecure=server.hasArg("tlsinsec");saveRuntimeConfig();
    server.send(200,"text/plain","Configurazione gateway salvata. Riavviare se sono cambiati i parametri di rete.");
  });

  server.on("/api/mqtt/config",HTTP_POST,[]{MqttRuntimeConfig c=getMqttConfig();c.enabled=server.hasArg("enabled");c.broker=server.arg("broker");c.port=(uint16_t)server.arg("port").toInt();c.user=server.arg("user");c.clientId=server.arg("client");c.baseTopic=server.arg("topic");c.tlsMode=(MqttTlsMode)constrain(server.arg("tls").toInt(),0,2);c.publishIntervalMs=(uint32_t)server.arg("period").toInt();const bool rp=server.hasArg("replace_password"),rc=server.hasArg("replace_ca");if(rp)c.password=server.arg("password");if(rc)c.caCertificate=server.arg("ca");if(!saveMqttConfig(c,rp,rc)){server.send(400,"text/plain","Configurazione MQTT non valida");return;}server.send(200,"text/plain","Configurazione MQTT salvata");});
  server.on("/api/mqtt/reset",HTTP_POST,[]{const bool ok=resetMqttConfig();server.send(ok?200:500,"text/plain",ok?"MQTT reset":"MQTT reset fallito");});

  server.on("/api/as3935/config",HTTP_POST,[]{LightningConfig c=getLightningConfig();c.enabled=server.hasArg("enabled");c.indoor=server.hasArg("indoor");c.i2cAddress=(uint8_t)server.arg("addr").toInt();c.irqPin=(int8_t)server.arg("irq").toInt();c.noiseFloor=(uint8_t)server.arg("noise").toInt();c.watchdogThreshold=(uint8_t)server.arg("watch").toInt();c.spikeRejection=(uint8_t)server.arg("spike").toInt();c.minStrikes=(uint8_t)server.arg("min").toInt();c.maskDisturbers=server.hasArg("mask");c.tuningCap=(uint8_t)server.arg("cap").toInt();c.autoTune=server.hasArg("auto");if(!saveLightningConfig(c)){server.send(400,"text/plain","Configurazione AS3935 non valida o sensore non disponibile");return;}server.send(200,"text/plain","Configurazione AS3935 salvata");});
  server.on("/api/as3935/reinit",HTTP_POST,[]{const bool ok=reinitializeLightning();server.send(ok?200:503,"text/plain",ok?"AS3935 reinizializzato":"AS3935 non disponibile");});
  server.on("/api/as3935/reset",HTTP_POST,[]{const bool ok=resetLightningConfig();server.send(ok?200:500,"text/plain",ok?"AS3935 reset":"AS3935 reset fallito");});

  server.on("/api/sd",HTTP_POST,[]{SdLoggerConfig c=getSdLoggerConfig();c.enabled=server.hasArg("enabled");c.logRfFrames=server.hasArg("rf");c.logWeatherSnapshots=server.hasArg("weather");c.logBme280=server.hasArg("bme");c.logAs3935=server.hasArg("as3935");c.snapshotIntervalSec=(uint16_t)server.arg("period").toInt();bool changed=false;if(!saveSdLoggerConfig(c,changed)){server.send(400,"text/plain","Configurazione microSD non valida");return;}server.send(200,"text/plain",changed?"Configurazione microSD salvata":"Configurazione microSD invariata");});
  server.on("/api/sd/reset",HTTP_POST,[]{bool changed=false;const bool ok=resetSdLoggerConfig(changed);server.send(ok?200:500,"text/plain",ok?"Configurazione microSD ripristinata":"Reset microSD fallito");});
  server.on("/api/sd/remount",HTTP_POST,[]{const bool ok=remountSdLogger();server.send(ok?200:503,"text/plain",ok?"microSD montata":"Mount microSD fallito");});
  server.on("/api/sd/format",HTTP_POST,[]{if(server.arg("confirm")!="FORMAT"){server.send(400,"text/plain","Conferma FORMAT mancante");return;}const bool ok=formatSdLogger();server.send(ok?200:500,"text/plain",ok?"microSD formattata e montata":"Formattazione microSD fallita");});

  server.on("/api/remote/config",HTTP_POST,[]{RemoteAccessConfig c=getRemoteAccessConfig();c.enabled=server.hasArg("enabled");c.relayUrl=server.arg("url");c.deviceId=server.arg("device");c.heartbeatSec=(uint16_t)server.arg("heartbeat").toInt();c.allowRemoteAdmin=server.hasArg("admin");const bool rt=server.hasArg("replace_token"),rc=server.hasArg("replace_ca");if(rt)c.token=server.arg("token");if(rc)c.caCertificate=server.arg("ca");if(!saveRemoteAccessConfig(c,rt,rc)){server.send(400,"text/plain","Profilo remoto non valido: per abilitarlo servono relay HTTPS, Device ID, token e CA");return;}server.send(200,"text/plain","Profilo accesso remoto salvato");});
  server.on("/api/remote/reset",HTTP_POST,[]{const bool ok=resetRemoteAccessConfig();server.send(ok?200:500,"text/plain",ok?"Profilo remoto cancellato":"Reset remoto fallito");});

  server.on("/api/test-upload",HTTP_POST,[]{const bool ok=sendWeatherRecordNow(*statePtr);server.send(ok?200:502,"text/plain",ok?"Upload riuscito":getUploadStatus().lastMessage);});
  server.on("/api/network/reset",HTTP_POST,[]{shutdownSdLogger();clearRuntimeWifiConfig();server.send(200,"text/plain","Rete cancellata. Riavvio...");delay(500);ESP.restart();});
  server.on("/api/restart",HTTP_POST,[]{shutdownSdLogger();server.send(200,"text/plain","Riavvio...");delay(400);ESP.restart();});
  server.onNotFound([](){server.send(404,"text/plain","Not found");});server.begin();started=true;
}

void serviceWeb(){if(started)server.handleClient();}
bool webStarted(){return started;}
