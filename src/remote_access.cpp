#include "remote_access.h"

#include <Preferences.h>
#include <ctype.h>

namespace {
constexpr const char *NVS_NS = "remote";
RemoteAccessConfig cfg;
RemoteAccessStatus status;

String jsonEscape(const String &s) {
  String o; o.reserve(s.length()+8);
  for (size_t i=0;i<s.length();++i) {
    const char c=s[i];
    if (c=='\\'||c=='"') { o+='\\'; o+=c; }
    else if (c=='\n') o+="\\n";
    else if (c!='\r') o+=c;
  }
  return o;
}

bool validDeviceId(const String &s) {
  if (s.isEmpty() || s.length()>48) return false;
  for (size_t i=0;i<s.length();++i) {
    const char c=s[i];
    if (!(isalnum(static_cast<unsigned char>(c)) || c=='-' || c=='_')) return false;
  }
  return true;
}

void normalize(RemoteAccessConfig &c) {
  c.relayUrl.trim(); c.deviceId.trim();
  if (c.deviceId.isEmpty()) c.deviceId=remoteDefaultDeviceId();
  if (c.relayUrl.length()>192) c.relayUrl.remove(192);
  if (c.deviceId.length()>48) c.deviceId.remove(48);
  if (c.token.length()>160) c.token.remove(160);
  if (c.caCertificate.length()>3900) c.caCertificate.remove(3900);
  if (c.heartbeatSec<30U) c.heartbeatSec=30U;
  if (c.heartbeatSec>3600U) c.heartbeatSec=3600U;
}

bool complete(const RemoteAccessConfig &c) {
  return c.relayUrl.startsWith("https://") && validDeviceId(c.deviceId) &&
         !c.token.isEmpty() && !c.caCertificate.isEmpty();
}

void updateStatus() {
  status.initialized=true;
  status.configured=complete(cfg);
  // 0.4.0-dev deliberately prepares identity/credentials and the relay
  // contract without opening an inbound listener or creating a tunnel yet.
  status.transportActive=false;
  if (!cfg.enabled) { status.state="OFF"; status.lastError=""; }
  else if (!status.configured) { status.state="CONFIG_REQUIRED"; status.lastError="relay HTTPS, token e CA richiesti"; }
  else { status.state="READY"; status.lastError="transport relay non ancora attivato in questa build"; }
}

void loadConfig() {
  RemoteAccessConfig d;
  Preferences p;
  if (!p.begin(NVS_NS,true)) { cfg=d; normalize(cfg); return; }
  cfg.enabled=p.getBool("enabled",d.enabled);
  cfg.relayUrl=p.getString("url",d.relayUrl);
  cfg.deviceId=p.getString("device",d.deviceId);
  cfg.token=p.getString("token",d.token);
  cfg.caCertificate=p.getString("ca",d.caCertificate);
  cfg.heartbeatSec=p.getUShort("heartbeat",d.heartbeatSec);
  cfg.allowRemoteAdmin=p.getBool("admin",d.allowRemoteAdmin);
  p.end(); normalize(cfg);
}
} // namespace

String remoteDefaultDeviceId() {
  char b[32];
  snprintf(b,sizeof(b),"davis-%06llX",static_cast<unsigned long long>(ESP.getEfuseMac() & 0xFFFFFFULL));
  return String(b);
}

void initRemoteAccess() { loadConfig(); updateStatus(); }
RemoteAccessConfig getRemoteAccessConfig(){return cfg;}
const RemoteAccessStatus &getRemoteAccessStatus(){return status;}

bool saveRemoteAccessConfig(const RemoteAccessConfig &input,bool replaceToken,bool replaceCa) {
  RemoteAccessConfig next=input;
  if (!replaceToken) next.token=cfg.token;
  if (!replaceCa) next.caCertificate=cfg.caCertificate;
  normalize(next);
  if (!validDeviceId(next.deviceId)) return false;
  if (next.enabled && !complete(next)) return false;
  Preferences p; if(!p.begin(NVS_NS,false)) return false;
  p.putBool("enabled",next.enabled);p.putString("url",next.relayUrl);p.putString("device",next.deviceId);
  p.putString("token",next.token);p.putString("ca",next.caCertificate);p.putUShort("heartbeat",next.heartbeatSec);p.putBool("admin",next.allowRemoteAdmin);p.end();
  cfg=next;updateStatus();return true;
}

bool resetRemoteAccessConfig() {
  Preferences p;if(!p.begin(NVS_NS,false))return false;const bool ok=p.clear();p.end();
  if(!ok)return false;cfg=RemoteAccessConfig{};normalize(cfg);updateStatus();return true;
}

String remoteAccessConfigJson() {
  String j;j.reserve(520);j="{\"enabled\":";j+=cfg.enabled?"true":"false";
  j+=",\"relay_url\":\""+jsonEscape(cfg.relayUrl)+"\",\"device_id\":\""+jsonEscape(cfg.deviceId)+"\",\"heartbeat_s\":"+String(cfg.heartbeatSec);
  j+=",\"allow_remote_admin\":";j+=cfg.allowRemoteAdmin?"true":"false";
  j+=",\"has_token\":";j+=cfg.token.isEmpty()?"false":"true";j+=",\"has_ca\":";j+=cfg.caCertificate.isEmpty()?"false":"true";j+="}";return j;
}

String remoteAccessStatusJson() {
  String j;j.reserve(300);j="{\"initialized\":";j+=status.initialized?"true":"false";
  j+=",\"enabled\":";j+=cfg.enabled?"true":"false";j+=",\"configured\":";j+=status.configured?"true":"false";
  j+=",\"transport_active\":";j+=status.transportActive?"true":"false";
  j+=",\"state\":\""+jsonEscape(status.state)+"\",\"device_id\":\""+jsonEscape(cfg.deviceId)+"\",\"last_error\":\""+jsonEscape(status.lastError)+"\"}";return j;
}
