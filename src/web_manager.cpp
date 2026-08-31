#include "web_manager.h"

#include <WebServer.h>
#include <WiFi.h>
#include <math.h>

#include "config.h"
#include "davis_radio.h"
#include "meteobridge_client.h"
#include "network_manager.h"
#include "pressure_manager.h"
#include "runtime_config.h"

namespace {
WebServer server(80);
StationState *statePtr=nullptr;
bool started=false;

String esc(const String &s){
  String o; o.reserve(s.length()+8);
  for(size_t i=0;i<s.length();i++){ char c=s[i]; if(c=='&')o+=F("&amp;"); else if(c=='<')o+=F("&lt;"); else if(c=='>')o+=F("&gt;"); else if(c=='\"')o+=F("&quot;"); else o+=c; }
  return o;
}
String jf(float v,uint8_t d=1){ return isfinite(v)?String(v,d):String("null"); }
String hv(float v,uint8_t d=1,const char *unit=""){ if(!isfinite(v))return "--"; return String(v,d)+unit; }

String page(){
  const StationState &s=*statePtr;
  const DavisRadioStatus &rf=getDavisRadioStatus();
  const UploadStatus &up=getUploadStatus();
  String h; h.reserve(9000);
  h+=F("<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'><meta http-equiv='refresh' content='15'><title>Davis Gateway</title><style>body{font-family:Arial,sans-serif;max-width:980px;margin:22px auto;padding:0 14px;color:#222}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(180px,1fr));gap:10px}.c{border:1px solid #ccc;border-radius:8px;padding:12px}.v{font-size:1.5em;font-weight:700}a,button{display:inline-block;margin:5px 5px 5px 0;padding:9px 12px}table{border-collapse:collapse;width:100%}td{padding:6px;border-bottom:1px solid #eee}.ok{color:#087b32}.ko{color:#a22}</style></head><body>");
  h+=F("<h1>ESP32 Davis Weather Gateway</h1><p>"); h+=BOARD_NAME; h+=F(" - firmware "); h+=FIRMWARE_VERSION; h+=F("</p><div class='grid'>");
  auto card=[&](const char *name,const String &value){ h+=F("<div class='c'><b>");h+=name;h+=F("</b><div class='v'>");h+=value;h+=F("</div></div>");};
  card("Temperatura",hv(s.outTempC,1," C")); card("Umidita",hv(s.outHumidity,0," %")); card("Vento",hv(s.windKmh,1," km/h")); card("Raffica 10m",hv(s.windGustKmh,1," km/h"));
  card("Direzione",hv(s.windDirDeg,0," deg")); card("Pioggia oggi",hv(s.rainDayMm,1," mm")); card("Rain rate",hv(s.rainRateMmH,1," mm/h")); card("Pressione",hv(s.pressureHpa,1," hPa"));
  card("UV",hv(s.uv,1,"")); card("Solare",hv(s.solarWm2,0," W/m2")); card("RSSI",hv(s.rssi,1," dBm")); card("ID ISS",s.locked?String(s.stationId+1U):String("--"));
  h+=F("</div><h2>Stato</h2><table>");
  h+=F("<tr><td>Rete</td><td>");h+=esc(networkModeName());h+=F(" / ");h+=esc(networkIp());h+=F("</td></tr>");
  h+=F("<tr><td>RF</td><td>");h+=rf.synchronized?F("sincronizzato"):F("ricerca");h+=F(" - ch ");h+=String(rf.channel+1);h+=F(" - ");h+=String(rf.frequencyMhz,6);h+=F(" MHz</td></tr>");
  h+=F("<tr><td>Pacchetti</td><td>OK ");h+=String(s.packetsOk);h+=F(" / CRC ");h+=String(s.crcErrors);h+=F(" / missed ");h+=String(s.packetsMissed);h+=F("</td></tr>");
  h+=F("<tr><td>BME280</td><td>");h+=pressureSensorAvailable()?F("presente"):F("non rilevato");h+=F("</td></tr>");
  h+=F("<tr><td>Upload</td><td>");h+=String(up.successes);h+=F("/");h+=String(up.attempts);h+=F(" - ");h+=esc(up.lastMessage);h+=F("</td></tr></table>");
  h+=F("<p><a href='/config'>CONFIGURAZIONE</a><a href='/api/status'>JSON STATUS</a><a href='/api/meteobridge'>ANTEPRIMA RECORD</a></p>");
  h+=F("</body></html>"); return h;
}

String configPage(const String &msg=String()){
  String h; h.reserve(7500);
  h+=F("<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'><title>Configurazione</title><style>body{font-family:Arial,sans-serif;max-width:760px;margin:24px auto;padding:0 14px}fieldset{margin:14px 0;padding:14px}label{display:block;margin-top:10px;font-weight:600}input{box-sizing:border-box;width:100%;padding:8px}input[type=checkbox]{width:auto}button{padding:10px 15px}.msg{background:#eef6ee;padding:8px}</style></head><body><h1>Configurazione</h1>");
  if(!msg.isEmpty()){h+=F("<p class='msg'>");h+=esc(msg);h+=F("</p>");}
  h+=F("<form method='POST' action='/save'><fieldset><legend>Rete</legend><label>SSID</label><input name='ssid' value='");h+=esc(runtimeConfig.wifiSsid);h+=F("'><label>Nuova password Wi-Fi</label><input type='password' name='pass' placeholder='vuoto = mantiene quella esistente'><label>Hostname</label><input name='host' value='");h+=esc(runtimeConfig.hostname);h+=F("'><label><input type='checkbox' name='dhcp' value='1' ");if(runtimeConfig.useDhcp)h+=F("checked");h+=F("> DHCP</label><label>IP statico</label><input name='ip' value='");h+=esc(runtimeConfig.staticIp);h+=F("'><label>Gateway</label><input name='gw' value='");h+=esc(runtimeConfig.gateway);h+=F("'><label>Netmask</label><input name='mask' value='");h+=esc(runtimeConfig.netmask);h+=F("'><label>DNS</label><input name='dns' value='");h+=esc(runtimeConfig.dns);h+=F("'></fieldset>");
  h+=F("<fieldset><legend>Davis / sensori</legend><label>ID ISS (0=auto)</label><input type='number' min='0' max='8' name='issid' value='");h+=String(runtimeConfig.issId);h+=F("'><label>Rain tip mm</label><input type='number' step='0.001' min='0.05' max='1' name='raintip' value='");h+=String(runtimeConfig.rainMmPerTip,3);h+=F("'><label>Quota BME280 m</label><input type='number' step='0.1' min='-500' max='9000' name='altm' value='");h+=String(runtimeConfig.bmeAltitudeM,1);h+=F("'><label>Timezone POSIX</label><input name='tz' value='");h+=esc(runtimeConfig.tzInfo);h+=F("'></fieldset>");
  h+=F("<fieldset><legend>Upload HTTP</legend><label>Endpoint receiver</label><input name='mburl' placeholder='https://server.example/path/mb.php' value='");h+=esc(runtimeConfig.mbUrl);h+=F("'><label>Intervallo ms</label><input type='number' min='5000' max='300000' name='mbint' value='");h+=String(runtimeConfig.uploadIntervalMs);h+=F("'><label><input type='checkbox' name='tlsinsec' value='1' ");if(runtimeConfig.tlsInsecure)h+=F("checked");h+=F("> HTTPS senza verifica certificato</label></fieldset><button>SALVA E RIAVVIA</button></form>");
  h+=F("<form method='POST' action='/test-upload'><button>TEST UPLOAD</button></form><form method='POST' action='/reset-network' onsubmit=\"return confirm('Cancellare la configurazione Wi-Fi?')\"><button>RESET RETE / PORTALE SETUP</button></form><p><a href='/'>Torna alla dashboard</a></p></body></html>"); return h;
}

bool validIp(const String &x){ IPAddress p; return p.fromString(x); }
}

void initWeb(StationState &station){
  if(started) return;
  statePtr=&station;
  server.on("/",HTTP_GET,[]{server.send(200,"text/html; charset=utf-8",page());});
  server.on("/config",HTTP_GET,[]{server.send(200,"text/html; charset=utf-8",configPage());});
  server.on("/save",HTTP_POST,[]{
    String ssid=server.arg("ssid"); if(ssid.isEmpty()){server.send(400,"text/html; charset=utf-8",configPage("SSID obbligatorio"));return;}
    runtimeConfig.wifiSsid=ssid;
    if(!server.arg("pass").isEmpty()) runtimeConfig.wifiPassword=server.arg("pass");
    runtimeConfig.hostname=server.arg("host"); if(runtimeConfig.hostname.isEmpty())runtimeConfig.hostname=DEVICE_HOSTNAME_DEFAULT;
    runtimeConfig.useDhcp=server.hasArg("dhcp");
    runtimeConfig.staticIp=server.arg("ip"); runtimeConfig.gateway=server.arg("gw"); runtimeConfig.netmask=server.arg("mask"); runtimeConfig.dns=server.arg("dns");
    if(!runtimeConfig.useDhcp && (!validIp(runtimeConfig.staticIp)||!validIp(runtimeConfig.gateway)||!validIp(runtimeConfig.netmask)||!validIp(runtimeConfig.dns))){server.send(400,"text/html; charset=utf-8",configPage("Profilo IP statico non valido"));return;}
    runtimeConfig.issId=(uint8_t)constrain(server.arg("issid").toInt(),0,8);
    float tip=server.arg("raintip").toFloat();if(tip>=0.05f&&tip<=1.0f)runtimeConfig.rainMmPerTip=tip;
    float alt=server.arg("altm").toFloat();if(alt>=-500&&alt<=9000)runtimeConfig.bmeAltitudeM=alt;
    runtimeConfig.tzInfo=server.arg("tz");if(runtimeConfig.tzInfo.isEmpty())runtimeConfig.tzInfo=TZ_INFO_DEFAULT;
    runtimeConfig.mbUrl=server.arg("mburl");
    uint32_t iv=(uint32_t)server.arg("mbint").toInt();runtimeConfig.uploadIntervalMs=constrain(iv,5000UL,300000UL);
    runtimeConfig.tlsInsecure=server.hasArg("tlsinsec");saveRuntimeConfig();
    server.send(200,"text/html; charset=utf-8","<html><body><h2>Salvato. Riavvio...</h2></body></html>");delay(700);ESP.restart();
  });
  server.on("/test-upload",HTTP_POST,[]{bool ok=sendWeatherRecordNow(*statePtr);String m;if(ok)m="Upload riuscito";else{m="Upload fallito: ";m+=getUploadStatus().lastMessage;}server.send(ok?200:502,"text/html; charset=utf-8",configPage(m));});
  server.on("/reset-network",HTTP_POST,[]{clearRuntimeWifiConfig();server.send(200,"text/html; charset=utf-8","<html><body><h2>Rete cancellata. Riavvio nel portale setup...</h2></body></html>");delay(700);ESP.restart();});
  server.on("/api/meteobridge",HTTP_GET,[]{server.send(200,"text/plain; charset=utf-8",buildMeteobridgeCompatibleRecord(*statePtr));});
  server.on("/api/status",HTTP_GET,[]{
    const StationState&s=*statePtr;const DavisRadioStatus&rf=getDavisRadioStatus();String j="{";
    j+="\"firmware\":\""+String(FIRMWARE_VERSION)+"\",\"network\":\""+networkModeName()+"\",\"ip\":\""+networkIp()+"\",";
    j+="\"rf_sync\":"+String(rf.synchronized?"true":"false")+",\"channel\":"+String(rf.channel+1)+",\"frequency_mhz\":"+String(rf.frequencyMhz,6)+",";
    j+="\"temp_c\":"+jf(s.outTempC)+",\"humidity\":"+jf(s.outHumidity,0)+",\"wind_kmh\":"+jf(s.windKmh)+",\"gust_kmh\":"+jf(s.windGustKmh)+",";
    j+="\"wind_dir\":"+jf(s.windDirDeg,0)+",\"rain_day_mm\":"+jf(s.rainDayMm)+",\"rain_rate_mmh\":"+jf(s.rainRateMmH)+",\"pressure_hpa\":"+jf(s.pressureHpa)+",";
    j+="\"uv\":"+jf(s.uv)+",\"solar_wm2\":"+jf(s.solarWm2,0)+",\"rssi\":"+jf(s.rssi)+",\"packets_ok\":"+String(s.packetsOk)+",\"crc_errors\":"+String(s.crcErrors)+"}";
    server.send(200,"application/json",j);
  });
  server.onNotFound([](){server.send(404,"text/plain","Not found");});
  server.begin();started=true;
}

void serviceWeb(){if(started)server.handleClient();}
bool webStarted(){return started;}
