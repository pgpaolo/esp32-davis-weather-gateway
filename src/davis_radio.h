#pragma once

#include <Arduino.h>
#include "station_state.h"

struct DavisRadioStatus {
  bool initialized = false;
  bool synchronized = false;
  uint8_t channel = 0;
  float frequencyMhz = 0.0f;
  int16_t lastRadioError = 0;
  uint32_t lastHopMs = 0;

  // Live receive diagnostics. These fields contain the most recent 10 bytes
  // exactly as returned by the SX1276 before Davis bit normalization/decoding.
  bool haveRawPacket = false;
  bool lastRawCrcOk = false;
  uint8_t lastRaw[10] = {0};
  uint32_t rawPackets = 0;
  uint32_t lastRawMs = 0;
  float lastRawRssi = NAN;
};

bool initDavisRadio();
void serviceDavisRadio(StationState &station, uint8_t configuredIssId, float rainMmPerTip);
const DavisRadioStatus &getDavisRadioStatus();
const char *davisPacketTypeName(uint8_t type);
