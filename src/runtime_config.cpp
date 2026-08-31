#include "runtime_config.h"
#include "config.h"
#include <Preferences.h>

RuntimeConfig runtimeConfig;
static Preferences prefs;

void loadRuntimeConfig(){
  prefs.begin("davisgw", true);
  runtimeConfig.mbUrl = prefs.getString("mburl", MB_DEFAULT_URL);
  runtimeConfig.uploadIntervalMs = prefs.getULong("mbint", MB_UPLOAD_INTERVAL_MS);
  runtimeConfig.tlsInsecure = prefs.getBool("tlsinsec", MB_TLS_INSECURE_DEFAULT != 0);
  runtimeConfig.issId = prefs.getUChar("issid", DAVIS_ISS_ID);
  runtimeConfig.rainMmPerTip = prefs.getFloat("raintip", DAVIS_RAIN_MM_PER_TIP);
  prefs.end();
  if(runtimeConfig.uploadIntervalMs < 5000UL) runtimeConfig.uploadIntervalMs = 5000UL;
  if(runtimeConfig.uploadIntervalMs > 300000UL) runtimeConfig.uploadIntervalMs = 300000UL;
  if(runtimeConfig.issId > 8) runtimeConfig.issId = 0;
  if(runtimeConfig.rainMmPerTip < 0.05f || runtimeConfig.rainMmPerTip > 1.0f) runtimeConfig.rainMmPerTip = DAVIS_RAIN_MM_PER_TIP;
}
void saveRuntimeConfig(){
  prefs.begin("davisgw", false);
  prefs.putString("mburl", runtimeConfig.mbUrl);
  prefs.putULong("mbint", runtimeConfig.uploadIntervalMs);
  prefs.putBool("tlsinsec", runtimeConfig.tlsInsecure);
  prefs.putUChar("issid", runtimeConfig.issId);
  prefs.putFloat("raintip", runtimeConfig.rainMmPerTip);
  prefs.end();
}
