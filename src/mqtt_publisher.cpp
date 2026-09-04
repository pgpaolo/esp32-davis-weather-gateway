#include "mqtt_publisher.h"

#include <Preferences.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <math.h>

#include "davis_radio.h"
#include "network_manager.h"
#include "pressure_manager.h"

namespace {
Preferences prefs;
WiFiClient plainClient;
WiFiClientSecure secureClient;
PubSubClient client(plainClient);
MqttRuntimeConfig cfg;
MqttRuntimeStatus status;
uint32_t lastAttemptMs = 0;

String jsonEscape(const String &s) {
  String o; o.reserve(s.length()+8);
  for (size_t i=0;i<s.length();++i) {
    const char c=s[i];
    if (c=='\\' || c=='"') { o += '\\'; o += c; }
    else if (c=='\n') o += "\\n";
    else if (c!='\r') o += c;
  }
  return o;
}

String trimTopic(String s) {
  s.trim();
  while (s.startsWith("/")) s.remove(0,1);
  while (s.endsWith("/")) s.remove(s.length()-1);
  return s;
}

void normalize(MqttRuntimeConfig &c) {
  c.broker.trim(); c.user.trim(); c.clientId.trim(); c.baseTopic = trimTopic(c.baseTopic);
  if (c.port == 0) c.port = c.tlsMode == MqttTlsMode::Off ? 1883 : 8883;
  if (c.clientId.isEmpty()) c.clientId = "DavisGateway";
  if (c.baseTopic.isEmpty()) c.baseTopic = "davis-gateway";
  if (c.broker.length()>96) c.broker.remove(96);
  if (c.user.length()>64) c.user.remove(64);
  if (c.password.length()>96) c.password.remove(96);
  if (c.clientId.length()>64) c.clientId.remove(64);
  if (c.baseTopic.length()>96) c.baseTopic.remove(96);
  if (c.caCertificate.length()>3900) c.caCertificate.remove(3900);
  if (c.publishIntervalMs < 5000UL) c.publishIntervalMs = 5000UL;
  if (c.publishIntervalMs > 300000UL) c.publishIntervalMs = 300000UL;
  if ((uint8_t)c.tlsMode > 2U) c.tlsMode = MqttTlsMode::Off;
}

void loadConfig() {
  MqttRuntimeConfig d;
  if (!prefs.begin("mqttcfg", true)) { cfg=d; return; }
  cfg.enabled = prefs.getBool("enabled", d.enabled);
  cfg.broker = prefs.getString("broker", d.broker);
  cfg.port = prefs.getUShort("port", d.port);
  cfg.user = prefs.getString("user", d.user);
  cfg.password = prefs.getString("pass", d.password);
  cfg.clientId = prefs.getString("client", d.clientId);
  cfg.baseTopic = prefs.getString("topic", d.baseTopic);
  cfg.tlsMode = (MqttTlsMode)prefs.getUChar("tls", (uint8_t)d.tlsMode);
  cfg.caCertificate = prefs.getString("ca", d.caCertificate);
  cfg.publishIntervalMs = prefs.getULong("period", d.publishIntervalMs);
  prefs.end();
  normalize(cfg);
}

String topic(const char *suffix) {
  if (!suffix || !*suffix) return cfg.baseTopic;
  return cfg.baseTopic + "/" + suffix;
}

String effectiveClientId() {
  String id=cfg.clientId;
  char suffix[10];
  snprintf(suffix,sizeof(suffix),"-%06llX",(unsigned long long)(ESP.getEfuseMac() & 0xFFFFFFULL));
  id += suffix;
  return id;
}

void applyTransport() {
  if (client.connected()) client.disconnect();
  secureClient.stop(); plainClient.stop();
  if (cfg.tlsMode == MqttTlsMode::CaVerified) {
    if (!cfg.caCertificate.isEmpty()) secureClient.setCACert(cfg.caCertificate.c_str());
    client.setClient(secureClient);
  } else if (cfg.tlsMode == MqttTlsMode::Insecure) {
    secureClient.setInsecure();
    client.setClient(secureClient);
  } else client.setClient(plainClient);
  client.setServer(cfg.broker.c_str(), cfg.port);
  client.setKeepAlive(30);
  client.setSocketTimeout(2);
  client.setBufferSize(3072);
  lastAttemptMs = 0;
}

void publishText(const char *suffix, const String &value, bool retained=true) {
  if (!client.connected()) return;
  if (client.publish(topic(suffix).c_str(), value.c_str(), retained)) status.publishes++;
}

void publishFloat(const char *suffix, float value, uint8_t decimals=1) {
  if (!isfinite(value)) return;
  publishText(suffix, String(value,(unsigned int)decimals), true);
}

void publishStation(const StationState &s) {
  const DavisRadioStatus &rf = getDavisRadioStatus();
  publishFloat("weather/temperature_c",s.outTempC);
  publishFloat("weather/humidity_pct",s.outHumidity,0);
  publishFloat("weather/dewpoint_c",calcDewPointC(s.outTempC,s.outHumidity));
  publishFloat("weather/wind_kmh",s.windKmh);
  publishFloat("weather/gust_kmh",s.windGustKmh);
  publishFloat("weather/wind_direction_deg",s.windDirDeg,0);
  publishFloat("weather/wind_chill_c",calcWindChillC(s.outTempC,s.windKmh));
  publishFloat("weather/rain_rate_mm_h",s.rainRateMmH);
  publishFloat("weather/rain_day_mm",s.rainDayMm);
  publishFloat("weather/rain_month_mm",s.rainMonthMm);
  publishFloat("weather/rain_year_mm",s.rainYearMm);
  publishFloat("weather/uv",s.uv);
  publishFloat("weather/solar_wm2",s.solarWm2,0);

  publishFloat("bme/pressure_sea_hpa",s.pressureHpa);
  publishFloat("bme/pressure_absolute_hpa",s.pressureAbsoluteHpa);
  publishFloat("bme/temperature_c",s.indoorTempC);
  publishFloat("bme/humidity_pct",s.indoorHumidity,0);
  if (s.pressureTrendValid) publishFloat("bme/trend_hpa_3h",s.pressureTrendHpa3h);
  publishText("bme/forecast",pressureForecastName(s));

  publishFloat("rf/rssi_dbm",s.rssi);
  publishText("rf/synchronized",rf.synchronized?"true":"false");
  publishText("rf/channel",String(rf.channel+1));
  publishText("rf/frequency_mhz",String(rf.frequencyMhz,6));
  publishText("rf/station_id",s.locked?String(s.stationId+1):String("0"));
  publishText("rf/battery_low",s.batteryLow?"true":"false");
  publishText("rf/packets_ok",String(s.packetsOk));
  publishText("rf/crc_errors",String(s.crcErrors));
  publishText("rf/missed",String(s.packetsMissed));
  publishText("rf/resyncs",String(s.resyncs));

  publishText("system/ip",networkIp());
  publishText("system/uptime_s",String(millis()/1000UL));

  String json; json.reserve(850);
  json = "{\"temperature_c\":" + (isfinite(s.outTempC)?String(s.outTempC,1):String("null"));
  json += ",\"humidity_pct\":" + (isfinite(s.outHumidity)?String(s.outHumidity,0):String("null"));
  json += ",\"wind_kmh\":" + (isfinite(s.windKmh)?String(s.windKmh,1):String("null"));
  json += ",\"gust_kmh\":" + (isfinite(s.windGustKmh)?String(s.windGustKmh,1):String("null"));
  json += ",\"rain_day_mm\":" + String(s.rainDayMm,1);
  json += ",\"pressure_hpa\":" + (isfinite(s.pressureHpa)?String(s.pressureHpa,1):String("null"));
  json += ",\"uv\":" + (isfinite(s.uv)?String(s.uv,1):String("null"));
  json += ",\"solar_wm2\":" + (isfinite(s.solarWm2)?String(s.solarWm2,0):String("null"));
  json += ",\"rf_sync\":"; json += rf.synchronized?"true":"false";
  json += ",\"rssi_dbm\":" + (isfinite(s.rssi)?String(s.rssi,1):String("null"));
  json += "}";
  publishText("state",json);
  status.lastPublishMs = millis();
}

bool connectNow() {
  if (!cfg.enabled || cfg.broker.isEmpty() || !networkConnected()) return false;
  status.connectAttempts++;
  const String id=effectiveClientId();
  const String will=topic("status");
  bool ok;
  if (!cfg.user.isEmpty()) ok=client.connect(id.c_str(),cfg.user.c_str(),cfg.password.c_str(),will.c_str(),0,true,"offline");
  else ok=client.connect(id.c_str(),will.c_str(),0,true,"offline");
  if (ok) {
    status.connected=true; status.connects++; status.lastConnectMs=millis(); status.lastError="";
    publishText("status","online");
    publishText("rf/protocol","Davis-Vantage-Pro2-EU-868-FHSS");
    publishText("system/ip",networkIp());
  } else {
    status.connected=false;
    status.lastError="connect rc=" + String(client.state());
  }
  return ok;
}
} // namespace

void initMqtt() {
  loadConfig(); applyTransport(); status.initialized=true;
  Serial.print(F("[MQTT] ")); Serial.print(cfg.enabled?F("enabled "):F("disabled "));
  if (cfg.enabled) { Serial.print(cfg.broker); Serial.print(':'); Serial.print(cfg.port); Serial.print(F(" tls=")); Serial.println(mqttTlsModeName(cfg.tlsMode)); }
  else Serial.println();
}

void serviceMqtt(const StationState &station) {
  if (!status.initialized || !cfg.enabled) return;
  if (!networkConnected()) { status.connected=false; return; }
  client.loop(); status.connected=client.connected();
  const uint32_t now=millis();
  if (!client.connected()) {
    if ((uint32_t)(now-lastAttemptMs) >= 10000UL) { lastAttemptMs=now; connectNow(); }
    return;
  }
  if ((uint32_t)(now-status.lastPublishMs) >= cfg.publishIntervalMs) publishStation(station);
}

MqttRuntimeConfig getMqttConfig(){ return cfg; }
const MqttRuntimeStatus &getMqttStatus(){ return status; }

bool saveMqttConfig(const MqttRuntimeConfig &input, bool replacePassword, bool replaceCaCertificate) {
  MqttRuntimeConfig next=input;
  if (!replacePassword) next.password=cfg.password;
  if (!replaceCaCertificate) next.caCertificate=cfg.caCertificate;
  normalize(next);
  if (next.enabled && next.broker.isEmpty()) return false;
  if (next.enabled && next.tlsMode==MqttTlsMode::CaVerified && next.caCertificate.isEmpty()) return false;
  if (!prefs.begin("mqttcfg",false)) return false;
  prefs.putBool("enabled",next.enabled); prefs.putString("broker",next.broker); prefs.putUShort("port",next.port);
  prefs.putString("user",next.user); prefs.putString("pass",next.password); prefs.putString("client",next.clientId);
  prefs.putString("topic",next.baseTopic); prefs.putUChar("tls",(uint8_t)next.tlsMode); prefs.putString("ca",next.caCertificate);
  prefs.putULong("period",next.publishIntervalMs); prefs.end();
  cfg=next; applyTransport(); return true;
}

bool resetMqttConfig() {
  if (!prefs.begin("mqttcfg",false)) return false;
  const bool ok=prefs.clear(); prefs.end(); if (!ok) return false;
  cfg=MqttRuntimeConfig{}; normalize(cfg); applyTransport(); return true;
}

const char *mqttTlsModeName(MqttTlsMode mode) {
  switch(mode){case MqttTlsMode::CaVerified:return "TLS-CA";case MqttTlsMode::Insecure:return "TLS-INSECURE";default:return "OFF";}
}

String mqttConfigJson() {
  String out; out.reserve(650);
  out="{\"enabled\":"; out+=cfg.enabled?"true":"false";
  out+=",\"broker\":\""+jsonEscape(cfg.broker)+"\",\"port\":"+String(cfg.port)+",\"user\":\""+jsonEscape(cfg.user)+"\"";
  out+=",\"client_id\":\""+jsonEscape(cfg.clientId)+"\",\"base_topic\":\""+jsonEscape(cfg.baseTopic)+"\"";
  out+=",\"tls_mode\":"+String((uint8_t)cfg.tlsMode)+",\"tls_name\":\""+String(mqttTlsModeName(cfg.tlsMode))+"\"";
  out+=",\"publish_interval_ms\":"+String(cfg.publishIntervalMs);
  out+=",\"has_password\":"; out+=cfg.password.isEmpty()?"false":"true";
  out+=",\"has_ca\":"; out+=cfg.caCertificate.isEmpty()?"false":"true"; out+="}"; return out;
}

String mqttStatusJson() {
  String out; out.reserve(330);
  out="{\"initialized\":"; out+=status.initialized?"true":"false";
  out+=",\"enabled\":"; out+=cfg.enabled?"true":"false";
  out+=",\"connected\":"; out+=status.connected?"true":"false";
  out+=",\"connect_attempts\":"+String(status.connectAttempts)+",\"connects\":"+String(status.connects)+",\"publishes\":"+String(status.publishes);
  out+=",\"last_connect_ms\":"+String(status.lastConnectMs)+",\"last_publish_ms\":"+String(status.lastPublishMs)+",\"last_error\":\""+jsonEscape(status.lastError)+"\"}"; return out;
}

void mqttPublishLightningState(const String &json) { if (cfg.enabled && client.connected()) publishText("as3935/state",json,true); }
void mqttPublishLightningEvent(const String &json) { if (cfg.enabled && client.connected()) publishText("as3935/event",json,false); }
