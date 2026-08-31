#pragma once
#include <Arduino.h>

struct RuntimeConfig {
  String mbUrl;
  uint32_t uploadIntervalMs;
  bool tlsInsecure;
  uint8_t issId; // 0 auto, 1..8 manual
  float rainMmPerTip;
};

extern RuntimeConfig runtimeConfig;
void loadRuntimeConfig();
void saveRuntimeConfig();
