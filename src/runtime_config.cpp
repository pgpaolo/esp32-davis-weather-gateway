#include "runtime_config.h"
#include "config.h"
#include <Preferences.h>

RuntimeConfig runtimeConfig;
static Preferences prefs;

void loadRuntimeConfig(){
  prefs.begin("davisgw", true);
  runtimeConfig.wifiSsid = prefs.getString("ssid", "");
  runtimeConfig.wifiPassword = prefs.getString("wpass", "");
  runtimeConfig.useDhcp = prefs.getBool("dhcp", true);
  runtimeConfig.staticIp = prefs.getString("ip", STATIC_IP_DEFAULT);
  runtimeConfig.gateway = prefs.getString("gw", STATIC_GATEWAY_DEFAULT);
  runtimeConfig.netmask = prefs.getString("mask", STATIC_MASK_DEFAULT);
  runtimeConfig.dns = prefs.getString("dns", STATIC_DNS_DEFAULT);
  runtimeConfig.hostname = prefs.getString("host", DEVICE_HOSTNAME_DEFAULT);

  runtimeConfig.mbUrl = prefs.getString("mburl", MB_DEFAULT_URL);
  runtimeConfig.uploadIntervalMs = prefs.getULong("mbint", MB_UPLOAD_INTERVAL_MS);
  runtimeConfig.tlsInsecure = prefs.getBool("tlsinsec", MB_TLS_INSECURE_DEFAULT != 0);
  runtimeConfig.issId = prefs.getUChar("issid", DAVIS_ISS_ID);
  runtimeConfig.rainMmPerTip = prefs.getFloat("raintip", DAVIS_RAIN_MM_PER_TIP);
  runtimeConfig.bmeAltitudeM = prefs.getFloat("altm", BME280_ALTITUDE_M_DEFAULT);
  runtimeConfig.tzInfo = prefs.getString("tz", TZ_INFO_DEFAULT);
  prefs.end();

  if(runtimeConfig.hostname.isEmpty()) runtimeConfig.hostname = DEVICE_HOSTNAME_DEFAULT;
  if(runtimeConfig.staticIp.isEmpty()) runtimeConfig.staticIp = STATIC_IP_DEFAULT;
  if(runtimeConfig.gateway.isEmpty()) runtimeConfig.gateway = STATIC_GATEWAY_DEFAULT;
  if(runtimeConfig.netmask.isEmpty()) runtimeConfig.netmask = STATIC_MASK_DEFAULT;
  if(runtimeConfig.dns.isEmpty()) runtimeConfig.dns = STATIC_DNS_DEFAULT;
  if(runtimeConfig.tzInfo.isEmpty()) runtimeConfig.tzInfo = TZ_INFO_DEFAULT;
  if(runtimeConfig.uploadIntervalMs < 5000UL) runtimeConfig.uploadIntervalMs = 5000UL;
  if(runtimeConfig.uploadIntervalMs > 300000UL) runtimeConfig.uploadIntervalMs = 300000UL;
  if(runtimeConfig.issId > 8) runtimeConfig.issId = 0;
  if(runtimeConfig.rainMmPerTip < 0.05f || runtimeConfig.rainMmPerTip > 1.0f) runtimeConfig.rainMmPerTip = DAVIS_RAIN_MM_PER_TIP;
  if(runtimeConfig.bmeAltitudeM < -500.0f || runtimeConfig.bmeAltitudeM > 9000.0f) runtimeConfig.bmeAltitudeM = BME280_ALTITUDE_M_DEFAULT;
}

void saveRuntimeConfig(){
  prefs.begin("davisgw", false);
  prefs.putString("ssid", runtimeConfig.wifiSsid);
  prefs.putString("wpass", runtimeConfig.wifiPassword);
  prefs.putBool("dhcp", runtimeConfig.useDhcp);
  prefs.putString("ip", runtimeConfig.staticIp);
  prefs.putString("gw", runtimeConfig.gateway);
  prefs.putString("mask", runtimeConfig.netmask);
  prefs.putString("dns", runtimeConfig.dns);
  prefs.putString("host", runtimeConfig.hostname);

  prefs.putString("mburl", runtimeConfig.mbUrl);
  prefs.putULong("mbint", runtimeConfig.uploadIntervalMs);
  prefs.putBool("tlsinsec", runtimeConfig.tlsInsecure);
  prefs.putUChar("issid", runtimeConfig.issId);
  prefs.putFloat("raintip", runtimeConfig.rainMmPerTip);
  prefs.putFloat("altm", runtimeConfig.bmeAltitudeM);
  prefs.putString("tz", runtimeConfig.tzInfo);
  prefs.end();
}

bool runtimeWifiConfigured(){ return !runtimeConfig.wifiSsid.isEmpty(); }

void clearRuntimeWifiConfig(){
  runtimeConfig.wifiSsid = "";
  runtimeConfig.wifiPassword = "";
  runtimeConfig.useDhcp = true;
  runtimeConfig.staticIp = STATIC_IP_DEFAULT;
  runtimeConfig.gateway = STATIC_GATEWAY_DEFAULT;
  runtimeConfig.netmask = STATIC_MASK_DEFAULT;
  runtimeConfig.dns = STATIC_DNS_DEFAULT;
  saveRuntimeConfig();
}
