#pragma once
#include <Arduino.h>
#include "station_state.h"

struct DavisDecodeResult {
  bool valid = false;
  uint8_t stationId = 0;
  uint8_t packetType = 0;
};

uint8_t reverseBits8(uint8_t b);
uint16_t davisCrc16(const uint8_t *data, size_t len, uint16_t init=0);
bool validateAndNormalizeDavisPacket(const uint8_t *raw, size_t len, uint8_t out[10]);
DavisDecodeResult decodeDavisPacket(const uint8_t data[10], StationState &s, float rainMmPerTip);
