#pragma once

#include <Arduino.h>
#include "station_state.h"

struct SdLoggerConfig {
  bool enabled = false;
  bool logRfFrames = true;
  bool logWeatherSnapshots = true;
  bool logBme280 = true;
  bool logAs3935 = true;
  uint16_t snapshotIntervalSec = 300;
};

struct SdLoggerStatus {
  bool supported = false;
  bool mounted = false;
  bool timeSynced = false;
  bool retryPending = false;
  uint64_t cardSizeBytes = 0;
  uint64_t totalBytes = 0;
  uint64_t usedBytes = 0;
  uint32_t mountAttempts = 0;
  uint32_t recordsQueued = 0;
  uint32_t recordsWritten = 0;
  uint32_t recordsDropped = 0;
  uint32_t writeErrors = 0;
  uint32_t lastWriteMs = 0;
  uint32_t retryInMs = 0;
  uint32_t spiFrequencyHz = 0;
  uint8_t sdErrorCode = 0;
  uint8_t sdErrorData = 0;
  uint8_t queueDepth = 0;
  char currentFile[80] = {0};
};

void initSdLogger();
void serviceSdLogger(const StationState &station);
void shutdownSdLogger();

SdLoggerConfig getSdLoggerConfig();
SdLoggerStatus getSdLoggerStatus();
bool saveSdLoggerConfig(const SdLoggerConfig &cfg, bool &changed);
bool resetSdLoggerConfig(bool &changed);
bool remountSdLogger();
bool formatSdLogger();

String sdLoggerConfigJson();
String sdLoggerStatusJson();
