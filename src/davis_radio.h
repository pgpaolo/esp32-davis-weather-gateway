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
};

bool initDavisRadio();
void serviceDavisRadio(StationState &station, uint8_t configuredIssId, float rainMmPerTip);
const DavisRadioStatus &getDavisRadioStatus();
const char *davisPacketTypeName(uint8_t type);
