#include "meteobridge_client.h"

#include <HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <math.h>
#include <time.h>

#include "config.h"
#include "network_manager.h"
#include "runtime_config.h"

namespace {
UploadStatus status;
uint32_t lastUploadMs = 0;

String fval(float v, uint8_t decimals=1) {
  if(!isfinite(v)) return "--";
  return String(v, decimals);
}

String epochText(uint32_t epoch) {
  if(epoch == 0) return "--";
  time_t t=(time_t)epoch;
  struct tm tmv;
  localtime_r(&t,&tmv);
  char b[20];
  strftime(b,sizeof(b),"%d/%m/%Y_%H:%M:%S",&tmv);
  return String(b);
}

String urlEncode(const String &s) {
  static const char hex[]="0123456789ABCDEF";
  String out; out.reserve(s.length()*2);
  for(size_t i=0;i<s.length();++i) {
    uint8_t c=(uint8_t)s[i];
    if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'||c=='~') out+=(char)c;
    else { out+='%'; out+=hex[c>>4]; out+=hex[c&15]; }
  }
  return out;
}

String makeUrl(const String &base, const String &record) {
  String u=base;
  u += (u.indexOf('?') >= 0) ? '&' : '?';
  u += "d=";
  u += urlEncode(record);
  return u;
}
}

String buildMeteobridgeCompatibleRecord(const StationState &s) {
  String f[192];
  for(size_t i=0;i<192;i++) f[i]="--";

  time_t now=time(nullptr);
  if(now > 1700000000) {
    struct tm t; localtime_r(&now,&t);
    char d[11], ti[9];
    strftime(d,sizeof(d),"%d/%m/%Y",&t);
    strftime(ti,sizeof(ti),"%H:%M:%S",&t);
    f[0]=d; f[1]=ti;
  } else { f[0]="01/01/1970"; f[1]="00:00:00"; }

  const float windMs=isfinite(s.windKmh)?s.windKmh/3.6f:NAN;
  const float gustMs=isfinite(s.windGustKmh)?s.windGustKmh/3.6f:NAN;
  const float dew=calcDewPointC(s.outTempC,s.outHumidity);
  const float chill=calcWindChillC(s.outTempC,s.windKmh);

  f[2]=fval(s.outTempC,1);
  f[3]=fval(s.outHumidity,0);
  f[4]=fval(dew,1);
  f[5]=fval(windMs,1);
  f[6]=fval(windMs,1);
  f[7]=fval(s.windDirDeg,0);
  f[8]=fval(s.rainRateMmH,1);
  f[9]=fval(s.rainDayMm,1);
  f[10]=fval(s.pressureHpa,1);
  f[12]=isfinite(windMs)?String(calcBeaufort(windMs)):"--";
  f[15]="hPa";
  f[16]="mm";
  f[19]=fval(s.rainMonthMm,1);
  f[20]=fval(s.rainYearMm,1);
  f[22]=fval(s.indoorTempC,1);
  f[23]=fval(s.indoorHumidity,0);
  f[24]=fval(chill,1);
  f[26]=fval(s.tempDayHighC,1);
  f[27]=epochText(s.tempDayHighEpoch);
  f[28]=fval(s.tempDayLowC,1);
  f[29]=epochText(s.tempDayLowEpoch);
  f[30]=fval(isfinite(s.windDayMaxKmh)?s.windDayMaxKmh/3.6f:NAN,1);
  f[31]=epochText(s.windDayMaxEpoch);
  f[32]=fval(isfinite(s.gustDayMaxKmh)?s.gustDayMaxKmh/3.6f:NAN,1);
  f[33]=epochText(s.gustDayMaxEpoch);
  f[34]=fval(s.pressureDayHighHpa,1);
  f[36]=fval(s.pressureDayLowHpa,1);
  f[38]=FIRMWARE_VERSION;
  f[39]="develop";
  f[40]=fval(gustMs,1);
  f[41]="ESP32";
  f[42]="Davis-VP2";
  f[43]=fval(s.uv,1);
  f[45]=fval(s.solarWm2,0);
  f[46]=fval(s.windDirDeg,0);
  f[58]=fval(s.uvDayMax,1);
  f[80]=fval(s.solarDayMax,0);
  f[81]=String(millis()/1000UL);

  String out;
  out.reserve(1400);
  for(size_t i=0;i<192;i++) {
    if(i) out+=' ';
    out+=f[i];
  }
  return out;
}

bool sendWeatherRecordNow(const StationState &station) {
  status.attempts++;
  status.lastAttemptMs=millis();
  status.lastMessage="";
  if(!networkConnected()) { status.lastMessage="network offline"; return false; }
  if(runtimeConfig.mbUrl.isEmpty()) { status.lastMessage="endpoint not configured"; return false; }

  const String record=buildMeteobridgeCompatibleRecord(station);
  const String url=makeUrl(runtimeConfig.mbUrl,record);
  HTTPClient http;
  http.setConnectTimeout(5000);
  http.setTimeout(7000);

  bool begun=false;
  int code=0;
  String body;
  if(url.startsWith("https://")) {
    WiFiClientSecure client;
    if(runtimeConfig.tlsInsecure) client.setInsecure();
    else if(strlen(MB_ROOT_CA) > 0) client.setCACert(MB_ROOT_CA);
    else { status.lastMessage="HTTPS requires CA or insecure mode"; return false; }
    begun=http.begin(client,url);
    if(begun) { code=http.GET(); if(code>0) body=http.getString(); }
    http.end();
  } else {
    WiFiClient client;
    begun=http.begin(client,url);
    if(begun) { code=http.GET(); if(code>0) body=http.getString(); }
    http.end();
  }

  status.lastHttpCode=code;
  body.trim();
  if(!begun) { status.lastMessage="HTTP begin failed"; return false; }
  const bool ok=code==200 && body.equalsIgnoreCase("success");
  status.lastMessage=body.isEmpty()?String("HTTP ")+code:body;
  if(ok){ status.successes++; status.lastSuccessMs=millis(); }
  return ok;
}

void serviceWeatherUpload(const StationState &station) {
  if(runtimeConfig.mbUrl.isEmpty() || !station.locked || !networkConnected()) return;
  const uint32_t now=millis();
  if((uint32_t)(now-lastUploadMs) < runtimeConfig.uploadIntervalMs) return;
  lastUploadMs=now;
  sendWeatherRecordNow(station);
}

const UploadStatus &getUploadStatus(){ return status; }
