#include "remote_access.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WebSocketsClient.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <esp_mac.h>
#include <esp_random.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <mbedtls/base64.h>
#include <time.h>
#include <vector>

#include "config.h"
#include "firmware_update.h"
#include "network_manager.h"
#include "remote_trust.h"
#include "runtime_config.h"
#include "web_manager.h"

namespace {
constexpr const char *NVS_NS="remote", *NVS_URL="url", *NVS_TOKEN="token";
constexpr time_t TLS_EPOCH=1700000000;
constexpr uint32_t RETRY_PENDING=5000UL;
constexpr uint32_t RETRY_ERROR=5000UL;
constexpr uint32_t RETRY_APPROVED=15000UL;
constexpr uint32_t APP_HEARTBEAT_MS=15000UL;
constexpr size_t MAX_REQ=12288U, MAX_RESP=24576U, MAX_WS=38000U, FW_CHUNK_MAX=8192U;

struct UrlParts { String host,path; uint16_t port=443; };
struct LocalResp {
  int code=502;
  String type="text/plain; charset=utf-8",encoding,location,cache,disposition;
  std::vector<uint8_t> body;
};

RemoteAccessConfig cfg;
RemoteAccessStatus st;
String token;
SemaphoreHandle_t mux=nullptr;
TaskHandle_t taskHandle=nullptr;
WebSocketsClient ws;
String wsAuth,activeWsUrl;
bool wsStarted=false;
bool intentionalDisconnect=false;
volatile uint32_t generation=1;
volatile bool forceRetry=false;
uint32_t lastAppHeartbeatMs=0;

bool take(){return mux&&xSemaphoreTake(mux,pdMS_TO_TICKS(250))==pdTRUE;}
void give(){if(mux)xSemaphoreGive(mux);}

String esc(const String&s){
  String o;o.reserve(s.length()+8);
  for(char c:s){
    if(c=='\\'||c=='"'){o+='\\';o+=c;}
    else if(c=='\n')o+="\\n";
    else if(c!='\r')o+=c;
  }
  return o;
}

String makeId(){
  uint8_t m[6]={0};
  if(esp_read_mac(m,ESP_MAC_WIFI_STA)!=ESP_OK){
    uint64_t e=ESP.getEfuseMac();
    for(uint8_t i=0;i<6;i++)m[5-i]=(e>>(8*i))&0xff;
  }
  char b[32];
  snprintf(b,sizeof(b),"esp32-%02x%02x%02x%02x%02x%02x",m[0],m[1],m[2],m[3],m[4],m[5]);
  return String(b);
}

bool validToken(const String&s){
  if(s.length()!=64)return false;
  for(char c:s)if(!isxdigit((unsigned char)c))return false;
  return true;
}

String newToken(){
  uint8_t r[32];esp_fill_random(r,sizeof(r));
  char b[65];
  for(size_t i=0;i<32;i++)snprintf(b+i*2,3,"%02x",r[i]);
  b[64]=0;
  return String(b);
}

bool parseUrl(const String&u,const char*scheme,UrlParts&o){
  String p=String(scheme)+"://";
  if(!u.startsWith(p))return false;
  String r=u.substring(p.length());
  if(r.isEmpty()||r.indexOf('@')>=0||r.indexOf('\r')>=0||r.indexOf('\n')>=0)return false;
  int slash=r.indexOf('/');
  String a=slash>=0?r.substring(0,slash):r;
  o.path=slash>=0?r.substring(slash):"/";
  int colon=a.lastIndexOf(':');
  if(colon>0){
    long port=a.substring(colon+1).toInt();
    if(port<1||port>65535)return false;
    o.port=(uint16_t)port;o.host=a.substring(0,colon);
  }else{o.port=443;o.host=a;}
  return !o.host.isEmpty()&&o.path.startsWith("/");
}

bool normalizePortal(String&u){
  u.trim();while(u.endsWith("/"))u.remove(u.length()-1);
  if(u.isEmpty())return true;
  if(u.length()>220||u.indexOf('?')>=0||u.indexOf('#')>=0)return false;
  UrlParts p;return parseUrl(u,"https",p);
}

void setState(const String&name,const String&err=""){
  if(!take())return;
  st.state=name;st.lastError=err;st.configured=!cfg.portalUrl.isEmpty();st.deviceId=remoteDefaultDeviceId();
  give();
}

void load(){
  Preferences p;
  if(!p.begin(NVS_NS,false)){cfg={};token=newToken();return;}
  cfg.portalUrl=p.getString(NVS_URL,"");normalizePortal(cfg.portalUrl);
  token=p.getString(NVS_TOKEN,"");
  if(!validToken(token)){token=newToken();p.putString(NVS_TOKEN,token);}
  p.remove("device");p.remove("ca");p.remove("heartbeat");p.remove("admin");p.remove("enabled");
  p.end();
}

String b64enc(const uint8_t*d,size_t n){
  if(!d||!n)return String();
  size_t cap=4*((n+2)/3)+1,w=0;
  std::vector<unsigned char>b(cap);
  if(mbedtls_base64_encode(b.data(),b.size(),&w,d,n)!=0)return String();
  String s;s.reserve(w+1);s.concat((const char*)b.data(),w);return s;
}

bool b64dec(const String&s,std::vector<uint8_t>&out){
  out.clear();if(s.isEmpty())return true;
  size_t cap=s.length()*3/4+4,w=0;
  if(cap>MAX_REQ+4)return false;
  out.resize(cap);
  if(mbedtls_base64_decode(out.data(),out.size(),&w,(const unsigned char*)s.c_str(),s.length())!=0||w>MAX_REQ){out.clear();return false;}
  out.resize(w);return true;
}

String hdr(const JsonObjectConst&h,const char*wanted){
  if(h.isNull())return String();String target=wanted;target.toLowerCase();
  for(JsonPairConst kv:h){
    String k=kv.key().c_str();k.toLowerCase();
    if(k==target){String v=kv.value().as<String>();v.replace("\r","");v.replace("\n","");if(v.length()>256)v.remove(256);return v;}
  }
  return String();
}

bool safePath(const String&p){return !p.isEmpty()&&p.length()<768&&p[0]=='/'&&p.indexOf("\r")<0&&p.indexOf("\n")<0&&p.indexOf("://")<0;}

bool localHttp(String method,const String&path,const JsonObjectConst&headers,const std::vector<uint8_t>&req,LocalResp&r,String&err){
  if(!webStarted()){err="Web UI locale non pronta";return false;}
  method.toUpperCase();
  if(!safePath(path)){err="Path non valido";return false;}
  if(method!="GET"&&method!="POST"&&method!="HEAD"){
    r.code=405;const char*m="Method not allowed";r.body.assign(m,m+strlen(m));return true;
  }
  WiFiClient c;c.setTimeout(6000);IPAddress ip=WiFi.localIP();
  if(!c.connect(ip,80)){
    c.stop();if(!c.connect(IPAddress(127,0,0,1),80)){err="Connessione Web UI locale fallita";return false;}
  }
  c.print(method);c.print(' ');c.print(path);c.print(F(" HTTP/1.0\r\nHost: 127.0.0.1\r\nConnection: close\r\n"));
  String ct=hdr(headers,"content-type"),ac=hdr(headers,"accept");
  if(!ct.isEmpty()){c.print(F("Content-Type: "));c.print(ct);c.print(F("\r\n"));}
  if(!ac.isEmpty()){c.print(F("Accept: "));c.print(ac);c.print(F("\r\n"));}
  if(!req.empty()){c.print(F("Content-Length: "));c.print(req.size());c.print(F("\r\n"));}
  c.print(F("\r\n"));if(!req.empty())c.write(req.data(),req.size());
  uint32_t start=millis();
  while(!c.available()&&c.connected()&&millis()-start<6000)vTaskDelay(pdMS_TO_TICKS(2));
  if(!c.available()){c.stop();err="Timeout Web UI locale";return false;}
  String sl=c.readStringUntil('\n');sl.trim();int a=sl.indexOf(' '),b=sl.indexOf(' ',a+1);
  if(a<0){c.stop();err="Risposta locale non valida";return false;}
  r.code=sl.substring(a+1,b>a?b:sl.length()).toInt();size_t len=0;bool haveLen=false;
  while(c.connected()||c.available()){
    String l=c.readStringUntil('\n');if(l=="\r"||l.isEmpty())break;l.trim();int x=l.indexOf(':');if(x<1)continue;
    String k=l.substring(0,x),v=l.substring(x+1);k.toLowerCase();v.trim();
    if(k=="content-type")r.type=v;else if(k=="content-encoding")r.encoding=v;else if(k=="location")r.location=v;else if(k=="cache-control")r.cache=v;else if(k=="content-disposition")r.disposition=v;else if(k=="content-length"){len=v.toInt();haveLen=true;}
  }
  if(haveLen&&len>MAX_RESP){c.stop();err="Risposta locale troppo grande";return false;}
  r.body.clear();r.body.reserve(haveLen?len:2048);start=millis();
  while((c.connected()||c.available())&&millis()-start<6000){
    while(c.available()){
      int ch=c.read();if(ch<0)break;
      if(r.body.size()>=MAX_RESP){c.stop();err="Risposta locale oltre limite";return false;}
      r.body.push_back((uint8_t)ch);if(haveLen&&r.body.size()>=len)break;
    }
    if(haveLen&&r.body.size()>=len)break;
    vTaskDelay(pdMS_TO_TICKS(1));
  }
  c.stop();if(haveLen&&r.body.size()<len){err="Risposta locale incompleta";return false;}return true;
}

void sendResp(const String&id,const LocalResp&r){
  String b=b64enc(r.body.data(),r.body.size());String o;o.reserve(b.length()+650);
  o="{\"type\":\"http_response\",\"id\":\""+esc(id)+"\",\"status\":"+String(r.code)+",\"headers\":{\"content-type\":\""+esc(r.type)+"\"";
  if(!r.encoding.isEmpty())o+=",\"content-encoding\":\""+esc(r.encoding)+"\"";
  if(!r.location.isEmpty())o+=",\"location\":\""+esc(r.location)+"\"";
  if(!r.cache.isEmpty())o+=",\"cache-control\":\""+esc(r.cache)+"\"";
  if(!r.disposition.isEmpty())o+=",\"content-disposition\":\""+esc(r.disposition)+"\"";
  o+="},\"body_b64\":\""+b+"\"}";
  if(o.length()<=MAX_WS&&ws.sendTXT(o)){if(take()){st.responses++;st.lastActivityMs=millis();give();}}
}

void sendErr(const String&id,int code,const String&msg){LocalResp r;r.code=code;r.body.assign(msg.c_str(),msg.c_str()+msg.length());sendResp(id,r);}
void sendFw(const String&type,const String&id,const String&extra=""){String o="{\"type\":\""+type+"\",\"id\":\""+esc(id)+"\"";if(!extra.isEmpty())o+=","+extra;o+="}";ws.sendTXT(o);}
void sendFwError(const String&id,const String&msg){sendFw("firmware_error",id,"\"message\":\""+esc(msg)+"\"");}

void wsText(uint8_t*p,size_t n){
  if(!p||!n||n>MAX_WS)return;
  JsonDocument d;if(deserializeJson(d,p,n))return;
  String type=d["type"]|"",id=d["id"]|"";
  if(take()){st.lastActivityMs=millis();give();}
  if(type=="ping"){
    String x="{\"type\":\"pong\"";if(!id.isEmpty())x+=",\"id\":\""+esc(id)+"\"";x+="}";ws.sendTXT(x);return;
  }
  if(type=="pong"){
    if(take()){st.lastWsEvent="PONG";give();}return;
  }
  if(type=="firmware_begin"&&!id.isEmpty()){
    const size_t size=(size_t)(d["size"]|0U);String sha=d["sha256"]|"",e;
    if(!firmwareRemoteBegin(size,sha,e)){sendFwError(id,e);return;}
    sendFw("firmware_ready",id,"\"max_chunk\":"+String(FW_CHUNK_MAX)+",\"status\":\"ready\"");return;
  }
  if(type=="firmware_chunk"&&!id.isEmpty()){
    const uint32_t seq=d["seq"]|0U;String bb=d["body_b64"]|"";std::vector<uint8_t>body;
    if(!b64dec(bb,body)||body.empty()||body.size()>FW_CHUNK_MAX){sendFwError(id,"Blocco firmware Base64 non valido");firmwareRemoteAbort("Blocco remoto non valido");return;}
    String e;if(!firmwareRemoteWrite(seq,body.data(),body.size(),e)){sendFwError(id,e);return;}
    sendFw("firmware_ack",id,"\"seq\":"+String(seq));return;
  }
  if(type=="firmware_end"&&!id.isEmpty()){
    String e;if(!firmwareRemoteEnd(e)){sendFwError(id,e);return;}
    sendFw("firmware_complete",id,"\"status\":\"ok\",\"restart\":true");return;
  }
  if(type=="firmware_abort"&&!id.isEmpty()){
    firmwareRemoteAbort("Aggiornamento annullato dal portale");sendFw("firmware_aborted",id,"\"status\":\"ok\"");return;
  }
  if(type!="http_request"||id.isEmpty())return;
  if(firmwareUpdateInProgress()){sendErr(id,503,"Aggiornamento firmware in corso");return;}
  if(take()){st.requests++;give();}
  String method=d["method"]|"GET",path=d["path"]|"/",bb=d["body_b64"]|"";std::vector<uint8_t>body;
  if(!b64dec(bb,body)){sendErr(id,413,"Request body non valido");return;}
  LocalResp r;String e;if(!localHttp(method,path,d["headers"].as<JsonObjectConst>(),body,r,e)){sendErr(id,502,e);return;}sendResp(id,r);
}

void wsEvent(WStype_t t,uint8_t*p,size_t n){
  if(t==WStype_CONNECTED){
    if(take()){
      st.transportActive=true;st.approved=true;st.state="ONLINE";st.wsConnects++;st.lastActivityMs=millis();st.lastWsEvent="CONNECTED";st.lastError="";
      give();
    }
    lastAppHeartbeatMs=0;
    Serial.println(F("[REMOTE] AdminSensor ONLINE"));
  }else if(t==WStype_DISCONNECTED){
    firmwareRemoteAbort("WebSocket AdminSensor disconnesso");
    if(intentionalDisconnect){intentionalDisconnect=false;return;}
    if(take()){
      bool was=st.transportActive;st.transportActive=false;st.lastWsEvent="DISCONNECTED";
      if(st.configured&&st.approved)st.state="RECONNECT";
      if(was)st.wsDisconnects++;else st.wsHandshakeFailures++;
      st.lastError=was?"WebSocket disconnesso; riconnessione automatica":"Handshake WebSocket non completato; verificare endpoint WSS/proxy e Authorization Bearer";
      give();
    }
  }else if(t==WStype_ERROR){
    String msg="Errore WebSocket";
    if(p&&n){String detail;detail.reserve(min((size_t)160,n));for(size_t i=0;i<n&&i<160;i++){char c=(char)p[i];if(c>=' '&&c<='~')detail+=c;}if(!detail.isEmpty())msg+=" : "+detail;}
    if(take()){st.transportActive=false;st.state="WS_ERROR";st.lastWsEvent="ERROR";st.lastError=msg;give();}
  }else if(t==WStype_TEXT)wsText(p,n);
  else if(t==WStype_PING||t==WStype_PONG){if(take()){st.lastActivityMs=millis();st.lastWsEvent=t==WStype_PING?"WS_PING":"WS_PONG";give();}}
}

bool enroll(const String&portal,String&wsUrl,bool&pending){
  pending=false;wsUrl="";
  WiFiClientSecure c;c.setCACert(REMOTE_TRUST_CA);c.setTimeout(8000);
  HTTPClient h;h.setConnectTimeout(7000);h.setTimeout(8000);
  if(!h.begin(c,portal+"/api/device/enroll")){setState("ERROR","HTTPS enroll init fallita");return false;}
  h.addHeader("Content-Type","application/json");
  JsonDocument q;
  q["device_id"]=remoteDefaultDeviceId();q["device_token"]=token;
  q["name"]=runtimeConfig.hostname.isEmpty()?"Stazione meteo":runtimeConfig.hostname;
  q["model"]="ESP32 Davis Weather Gateway";q["firmware_version"]=FIRMWARE_VERSION;
  q["capabilities"]["remote_http"]=true;q["capabilities"]["firmware_update"]=true;q["capabilities"]["firmware_chunk_bytes"]=(uint32_t)FW_CHUNK_MAX;
  String body;serializeJson(q,body);
  if(take()){st.enrollAttempts++;st.lastWsEvent="ENROLL";give();}
  int code=h.POST(body);String resp=code>0?h.getString():String();h.end();
  if(take()){st.lastEnrollHttpCode=code;st.lastActivityMs=millis();give();}
  if(code<200||code>=300){setState("ERROR",code>0?String("Enroll HTTP ")+code:"Errore TLS/HTTP enroll");return false;}
  JsonDocument d;if(deserializeJson(d,resp)){setState("ERROR","Risposta enroll JSON non valida");return false;}
  String s=d["status"]|"";
  if(s=="pending"){
    pending=true;if(take()){st.approved=false;st.transportActive=false;st.lastWsEvent="PENDING";give();}setState("PENDING");return true;
  }
  if(s!="approved"){
    if(take()){st.approved=false;st.transportActive=false;st.lastWsEvent="DENIED";give();}
    setState("DENIED",s.isEmpty()?"Stato enroll mancante":String("Stato portale: ")+s);return false;
  }
  wsUrl=d["websocket_url"]|"";UrlParts u;
  if(!parseUrl(wsUrl,"wss",u)){setState("ERROR","websocket_url WSS non valido");return false;}
  if(take()){st.approved=true;st.lastError="";st.wsHost=u.host;st.wsPath=u.path;st.lastWsEvent="APPROVED";give();}
  setState("APPROVED");return true;
}

bool startWs(const String&url){
  UrlParts u;if(!parseUrl(url,"wss",u))return false;
  if(wsStarted){intentionalDisconnect=true;ws.disconnect();}
  WiFi.setSleep(false);
  wsAuth="Authorization: Bearer "+token;
  ws.setExtraHeaders(wsAuth.c_str());
  ws.beginSslWithCA(u.host.c_str(),u.port,u.path.c_str(),REMOTE_TRUST_CA,"");
  ws.setExtraHeaders(wsAuth.c_str());
  ws.setReconnectInterval(3000);
  ws.enableHeartbeat(15000,4000,3);
  activeWsUrl=url;wsStarted=true;
  if(take()){st.wsAttempts++;st.lastWsAttemptMs=millis();st.wsHost=u.host;st.wsPath=u.path;st.lastWsEvent="CONNECTING";give();}
  setState("CONNECTING");return true;
}

void sendAppHeartbeat(){
  String x="{\"type\":\"ping\",\"device_id\":\""+esc(remoteDefaultDeviceId())+"\",\"uptime_ms\":"+String(millis())+"}";
  if(ws.sendTXT(x)&&take()){st.heartbeatsSent++;st.lastActivityMs=millis();st.lastWsEvent="PING_SENT";give();}
}

void task(void*){
  uint32_t seen=0,next=0;
  for(;;){
    RemoteAccessConfig c;if(take()){c=cfg;give();}
    uint32_t g=generation;bool fr=forceRetry;if(fr)forceRetry=false;
    if(g!=seen||fr){
      seen=g;firmwareRemoteAbort("Configurazione remota cambiata");
      if(wsStarted){intentionalDisconnect=true;ws.disconnect();}
      wsStarted=false;activeWsUrl="";next=0;lastAppHeartbeatMs=0;
      if(take()){st.transportActive=false;st.approved=false;st.lastWsEvent=fr?"MANUAL_RETRY":"CONFIG_CHANGED";give();}
    }
    if(c.portalUrl.isEmpty()){setState("OFF");vTaskDelay(pdMS_TO_TICKS(300));continue;}
    if(!networkConnected()||networkProvisioningActive()){
      if(wsStarted){intentionalDisconnect=true;ws.disconnect();wsStarted=false;}
      setState("WAIT_NETWORK");vTaskDelay(pdMS_TO_TICKS(500));continue;
    }
    if(time(nullptr)<TLS_EPOCH){setState("WAIT_TIME");vTaskDelay(pdMS_TO_TICKS(500));continue;}

    uint32_t now=millis();bool online=false;
    if(take()){online=st.transportActive;give();}
    if(next==0||(!online&&(int32_t)(now-next)>=0)){
      bool pending=false;String u;setState("ENROLLING");bool ok=enroll(c.portalUrl,u,pending);
      if(ok&&pending){if(wsStarted){intentionalDisconnect=true;ws.disconnect();wsStarted=false;}next=millis()+RETRY_PENDING;}
      else if(ok&&!u.isEmpty()){if(!wsStarted||u!=activeWsUrl)startWs(u);next=millis()+RETRY_APPROVED;}
      else next=millis()+RETRY_ERROR;
    }
    if(wsStarted)ws.loop();
    if(online&&(lastAppHeartbeatMs==0||(uint32_t)(now-lastAppHeartbeatMs)>=APP_HEARTBEAT_MS)){
      lastAppHeartbeatMs=now;sendAppHeartbeat();
    }
    vTaskDelay(pdMS_TO_TICKS(8));
  }
}
} // namespace

String remoteDefaultDeviceId(){static String id=makeId();return id;}

void initRemoteAccess(){
  if(!mux)mux=xSemaphoreCreateMutex();load();
  if(take()){
    st=RemoteAccessStatus{};st.initialized=true;st.configured=!cfg.portalUrl.isEmpty();st.deviceId=remoteDefaultDeviceId();st.state=cfg.portalUrl.isEmpty()?"OFF":"WAIT_NETWORK";st.lastWsEvent="INIT";give();
  }
  ws.onEvent(wsEvent);
  if(!taskHandle&&xTaskCreate(task,"adminsensor",12288,nullptr,1,&taskHandle)!=pdPASS){taskHandle=nullptr;setState("ERROR","Task remoto non avviato");}
  Serial.print(F("[REMOTE] Device ID: "));Serial.println(remoteDefaultDeviceId());
  Serial.println(F("[REMOTE] Token in NVS (non mostrato)"));
}

RemoteAccessConfig getRemoteAccessConfig(){RemoteAccessConfig o;if(take()){o=cfg;give();}return o;}
RemoteAccessStatus getRemoteAccessStatus(){RemoteAccessStatus o;if(take()){o=st;give();}return o;}

bool saveRemoteAccessPortalUrl(const String&in){
  String u=in;if(!normalizePortal(u))return false;
  Preferences p;if(!p.begin(NVS_NS,false))return false;p.putString(NVS_URL,u);p.end();
  if(take()){cfg.portalUrl=u;st.configured=!u.isEmpty();st.approved=false;st.transportActive=false;st.state=u.isEmpty()?"OFF":"WAIT_NETWORK";st.lastError="";st.lastWsEvent="CONFIG_SAVED";give();}
  generation++;return true;
}

bool resetRemoteAccessConfig(){return saveRemoteAccessPortalUrl("");}
void retryRemoteAccessNow(){forceRetry=true;}

String remoteAccessConfigJson(){
  RemoteAccessConfig c=getRemoteAccessConfig();
  String j="{\"portal_url\":\""+esc(c.portalUrl)+"\",\"device_id\":\""+esc(remoteDefaultDeviceId())+"\",\"has_token\":";
  j+=validToken(token)?"true":"false";
  j+=",\"identity_managed_by_firmware\":true,\"firmware_update\":true,\"firmware_chunk_bytes\":"+String(FW_CHUNK_MAX)+"}";
  return j;
}

String remoteAccessStatusJson(){
  RemoteAccessStatus s=getRemoteAccessStatus();
  String j="{\"initialized\":";j+=s.initialized?"true":"false";
  j+=",\"configured\":";j+=s.configured?"true":"false";
  j+=",\"approved\":";j+=s.approved?"true":"false";
  j+=",\"transport_active\":";j+=s.transportActive?"true":"false";
  j+=",\"state\":\""+esc(s.state)+"\",\"device_id\":\""+esc(s.deviceId)+"\"";
  j+=",\"enroll_attempts\":"+String(s.enrollAttempts)+",\"last_enroll_http_code\":"+String(s.lastEnrollHttpCode);
  j+=",\"ws_attempts\":"+String(s.wsAttempts)+",\"ws_connects\":"+String(s.wsConnects)+",\"ws_disconnects\":"+String(s.wsDisconnects)+",\"ws_handshake_failures\":"+String(s.wsHandshakeFailures);
  j+=",\"ws_host\":\""+esc(s.wsHost)+"\",\"ws_path\":\""+esc(s.wsPath)+"\",\"last_ws_event\":\""+esc(s.lastWsEvent)+"\"";
  j+=",\"requests\":"+String(s.requests)+",\"responses\":"+String(s.responses)+",\"heartbeats_sent\":"+String(s.heartbeatsSent)+",\"firmware_update\":true";
  j+=",\"last_ws_attempt_age_ms\":"+(s.lastWsAttemptMs?String((uint32_t)(millis()-s.lastWsAttemptMs)):String("null"));
  j+=",\"last_activity_age_ms\":"+(s.lastActivityMs?String((uint32_t)(millis()-s.lastActivityMs)):String("null"));
  j+=",\"last_error\":\""+esc(s.lastError)+"\"}";
  return j;
}
