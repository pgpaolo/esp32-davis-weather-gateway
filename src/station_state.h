#pragma once
#include <Arduino.h>
#include <math.h>

struct StationState {
  bool locked = false;
  uint8_t stationId = 0; // internal 0..7
  bool batteryLow = false;
  float rssi = NAN;
  uint32_t packetsOk = 0;
  uint32_t crcErrors = 0;
  uint32_t packetsMissed = 0;
  uint32_t resyncs = 0;
  uint8_t lastPacketType = 0;
  uint32_t lastPacketMs = 0;

  float outTempC = NAN;
  float outHumidity = NAN;
  float windKmh = NAN;
  float windGustKmh = NAN;
  float windDirDeg = NAN;
  float rainRateMmH = 0.0f;
  float rainDayMm = 0.0f;
  float rainMonthMm = 0.0f;
  float rainYearMm = 0.0f;
  float rainYesterdayMm = 0.0f;
  float uv = NAN;
  float solarWm2 = NAN;

  float pressureHpa = NAN;
  float indoorTempC = NAN;
  float indoorHumidity = NAN;

  float tempDayHighC = NAN;
  float tempDayLowC = NAN;
  float windDayMaxKmh = NAN;
  float gustDayMaxKmh = NAN;
  float pressureDayHighHpa = NAN;
  float pressureDayLowHpa = NAN;
  float uvDayMax = NAN;
  float solarDayMax = NAN;

  uint32_t tempDayHighEpoch = 0;
  uint32_t tempDayLowEpoch = 0;
  uint32_t windDayMaxEpoch = 0;
  uint32_t gustDayMaxEpoch = 0;
  uint32_t pressureDayHighEpoch = 0;
  uint32_t pressureDayLowEpoch = 0;

  uint8_t rainCounter = 0;
  bool haveRainCounter = false;
  uint32_t lastRainTipMs = 0;
};

float calcDewPointC(float tempC, float rh);
float calcHeatIndexC(float tempC, float rh);
float calcWindChillC(float tempC, float windKmh);
uint8_t calcBeaufort(float windMs);
void updateDailyExtremes(StationState &s);
void resetDailyExtremes(StationState &s);
