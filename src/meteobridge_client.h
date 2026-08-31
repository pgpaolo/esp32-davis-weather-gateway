#pragma once
#include <Arduino.h>
#include "station_state.h"

struct UploadStatus {
  uint32_t attempts = 0;
  uint32_t successes = 0;
  int lastHttpCode = 0;
  String lastMessage;
  uint32_t lastAttemptMs = 0;
  uint32_t lastSuccessMs = 0;
};

String buildMeteobridgeCompatibleRecord(const StationState &station);
bool sendWeatherRecordNow(const StationState &station);
void serviceWeatherUpload(const StationState &station);
const UploadStatus &getUploadStatus();
