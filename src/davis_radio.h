#pragma once

#include <Arduino.h>
#include "station_state.h"

constexpr uint8_t DAVIS_DIAG_CHANNEL_COUNT = 5U;
constexpr uint8_t DAVIS_DIAG_FRAME_HISTORY = 24U;

struct DavisChannelDiagnostics {
  uint32_t rawPackets = 0;
  uint32_t crcValid = 0;
  uint32_t crcErrors = 0;
  uint32_t readErrors = 0;
  uint32_t lastRxMs = 0;
  float lastRssi = NAN;
  float minRssi = NAN;
  float maxRssi = NAN;
  float rssiSum = 0.0f;
  uint32_t rssiSamples = 0;
};

struct DavisFrameDiagnostic {
  bool valid = false;
  uint32_t sequence = 0;
  uint32_t timestampMs = 0;
  uint8_t channel = 0;
  float frequencyMhz = 0.0f;
  float rssi = NAN;
  int16_t radioError = 0;
  bool crcOk = false;
  uint16_t crcReceived = 0;
  uint16_t crcCalculated = 0;
  uint8_t raw[10] = {0};
  uint8_t normalized[10] = {0};
};

struct DavisRadioStatus {
  bool initialized = false;
  bool synchronized = false;
  uint8_t channel = 0;
  float frequencyMhz = 0.0f;
  int16_t lastRadioError = 0;
  uint32_t lastHopMs = 0;

  // Live receive diagnostics. Raw bytes are exactly as returned by SX1276,
  // before Davis bit normalization/decoding.
  bool haveRawPacket = false;
  bool lastRawCrcOk = false;
  uint8_t lastRaw[10] = {0};
  uint8_t lastNormalized[10] = {0};
  uint16_t lastCrcReceived = 0;
  uint16_t lastCrcCalculated = 0;
  uint32_t rawPackets = 0;
  uint32_t lastRawMs = 0;
  float lastRawRssi = NAN;

  // Radio-path and FHSS diagnostics.
  uint32_t irqCount = 0;
  uint32_t readOk = 0;
  uint32_t readErrors = 0;
  uint32_t tuneCount = 0;
  uint32_t tuneErrors = 0;
  uint32_t hopCount = 0;
  uint32_t startReceiveErrors = 0;
  uint8_t missStreak = 0;

  // Packet timing diagnostics, based on raw candidate frames.
  uint32_t lastRawIntervalMs = 0;
  uint32_t minRawIntervalMs = 0;
  uint32_t maxRawIntervalMs = 0;
  uint32_t avgRawIntervalMs = 0;
  uint32_t avgJitterMs = 0;
  uint32_t timingSamples = 0;
};

bool initDavisRadio();
void serviceDavisRadio(StationState &station, uint8_t configuredIssId, float rainMmPerTip);
const DavisRadioStatus &getDavisRadioStatus();
const DavisChannelDiagnostics &getDavisChannelDiagnostics(uint8_t channel);
uint8_t getDavisFrameHistoryCount();
bool getDavisFrameHistory(uint8_t newestIndex, DavisFrameDiagnostic &out);
void resetDavisDiagnosticWindow();
String davisRadioDiagnosticsJson(const StationState &station, uint8_t configuredIssId, bool includeHistory=false);
String davisRadioDiagnosticReport(const StationState &station, uint8_t configuredIssId);
const char *davisPacketTypeName(uint8_t type);
