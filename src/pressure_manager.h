#pragma once
#include <Arduino.h>
#include "station_state.h"

struct PressureSensorStatus {
  bool detected = false;
  uint8_t address = 0;
  uint32_t lastReadMs = 0;
  uint32_t lastDetectAttemptMs = 0;
  uint32_t readErrors = 0;
  uint8_t consecutiveInvalid = 0;
  uint8_t detectFailures = 0;
};

bool initPressureSensor();
void servicePressureSensor(StationState &station);
bool pressureSensorAvailable();
uint8_t pressureSensorAddress();
const PressureSensorStatus &getPressureSensorStatus();
const char *pressureTrendName(const StationState &station);
const char *pressureForecastName(const StationState &station);
String pressureStatusJson(const StationState &station);
String i2cScanJson();
