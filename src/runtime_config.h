#pragma once
#include <Arduino.h>

struct RuntimeConfig {
  String wifiSsid;
  String wifiPassword;
  bool useDhcp = true;
  String staticIp;
  String gateway;
  String netmask;
  String dns;
  String hostname;

  String mbUrl;
  uint32_t uploadIntervalMs = 10000UL;
  bool tlsInsecure = false;
  uint8_t issId = 0; // 0 auto, 1..8 manual
  float rainMmPerTip = 0.2f;
  float bmeAltitudeM = 0.0f;
  String tzInfo;
};

extern RuntimeConfig runtimeConfig;
void loadRuntimeConfig();
void saveRuntimeConfig();
bool runtimeWifiConfigured();
void clearRuntimeWifiConfig();
