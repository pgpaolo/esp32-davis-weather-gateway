#include "network_web.h"

#include <WiFi.h>

#include "network_manager.h"
#include "runtime_config.h"

namespace {
String escJson(const String &s) {
  String o;o.reserve(s.length()+8);
  for(char c:s){
    if(c=='\\'||c=='\"'){o+='\\';o+=c;}
    else if(c=='\n')o+="\\n";
    else if(c!='\r')o+=c;
  }
  return o;
}

String wifiStateJson() {
  String j;j.reserve(420);
  j="{\"saved_ssid\":\""+escJson(runtimeConfig.wifiSsid)+"\",\"password_set\":";
  j+=runtimeConfig.wifiPassword.isEmpty()?"false":"true";
  j+=",\"connected\":";j+=networkConnected()?"true":"false";
  j+=",\"current_ssid\":\""+escJson(networkSsid())+"\",\"ip\":\""+escJson(networkIp())+"\",\"rssi_dbm\":";
  j+=networkConnected()?String(WiFi.RSSI()):String("null");
  j+=",\"mode\":\""+escJson(networkModeName())+"\"}";
  return j;
}

bool alreadySeen(const String &ssid, const String *seen, size_t count) {
  for(size_t i=0;i<count;i++) if(seen[i]==ssid) return true;
  return false;
}

String scanResultJson(int count) {
  String j;j.reserve(1200+(count>0?(size_t)count*120U:0U));
  j="{\"status\":\"complete\",\"networks\":[";
  String seen[24];size_t seenCount=0;bool first=true;
  for(int i=0;i<count&&seenCount<24;i++){
    String ssid=WiFi.SSID(i);
    if(ssid.isEmpty()||alreadySeen(ssid,seen,seenCount))continue;
    seen[seenCount++]=ssid;
    if(!first)j+=',';first=false;
    const bool open=WiFi.encryptionType(i)==WIFI_AUTH_OPEN;
    j+="{\"ssid\":\""+escJson(ssid)+"\",\"rssi_dbm\":"+String(WiFi.RSSI(i))+",\"channel\":"+String(WiFi.channel(i))+",\"open\":";
    j+=open?"true":"false";
    j+=",\"current\":";j+=(networkConnected()&&ssid==WiFi.SSID())?"true":"false";
    j+="}";
  }
  j+="]}";
  return j;
}
} // namespace

void registerNetworkWebRoutes(WebServer &server) {
  server.on("/api/network/wifi",HTTP_GET,[&server](){
    server.sendHeader("Cache-Control","no-store");
    server.send(200,"application/json",wifiStateJson());
  });

  server.on("/api/network/scan/start",HTTP_POST,[&server](){
    const int state=WiFi.scanComplete();
    if(state==WIFI_SCAN_RUNNING){
      server.send(202,"application/json","{\"status\":\"running\"}");
      return;
    }
    WiFi.scanDelete();
    const int rc=WiFi.scanNetworks(true,true);
    if(rc==WIFI_SCAN_FAILED){
      server.send(500,"application/json","{\"status\":\"error\",\"message\":\"Impossibile avviare scansione Wi-Fi\"}");
      return;
    }
    server.send(202,"application/json","{\"status\":\"running\"}");
  });

  server.on("/api/network/scan",HTTP_GET,[&server](){
    const int n=WiFi.scanComplete();
    server.sendHeader("Cache-Control","no-store");
    if(n==WIFI_SCAN_RUNNING){server.send(200,"application/json","{\"status\":\"running\",\"networks\":[]}");return;}
    if(n<0){server.send(200,"application/json","{\"status\":\"idle\",\"networks\":[]}");return;}
    const String json=scanResultJson(n);
    WiFi.scanDelete();
    server.send(200,"application/json",json);
  });

  server.on("/api/network/wifi",HTTP_POST,[&server](){
    String ssid=server.arg("ssid");ssid.trim();
    if(ssid.isEmpty()||ssid.length()>32){server.send(400,"text/plain; charset=utf-8","SSID Wi-Fi non valido");return;}
    String pass=server.arg("password");
    if(pass.length()>63){server.send(400,"text/plain; charset=utf-8","Password Wi-Fi troppo lunga");return;}
    const bool clearPass=server.hasArg("clearpass")||server.arg("clearpass")=="1";
    const String oldSsid=runtimeConfig.wifiSsid;
    const String oldPass=runtimeConfig.wifiPassword;
    runtimeConfig.wifiSsid=ssid;
    if(clearPass)runtimeConfig.wifiPassword="";
    else if(!pass.isEmpty())runtimeConfig.wifiPassword=pass;
    else if(ssid!=oldSsid)runtimeConfig.wifiPassword="";
    const bool changed=(runtimeConfig.wifiSsid!=oldSsid)||(runtimeConfig.wifiPassword!=oldPass);
    if(changed)saveRuntimeConfig();
    server.send(200,"text/plain; charset=utf-8",changed?"Configurazione Wi-Fi salvata. Riavviare per applicarla.":"Configurazione Wi-Fi invariata.");
  });
}
