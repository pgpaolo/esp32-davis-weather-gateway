#pragma once

#include <Arduino.h>
#include <Wire.h>

// Minimal BME280 I2C driver used by the Davis gateway.
// Supports only the features required by this firmware: chip detection,
// calibration loading and compensated temperature/pressure/humidity reads.
// This intentionally avoids the larger Adafruit dependency chain.
class Bme280Minimal {
 public:
  bool begin(uint8_t address, TwoWire &wire = Wire);
  bool read(float &temperatureC, float &pressurePa, float &humidityPct);
  uint8_t address() const { return address_; }
  bool ready() const { return ready_; }

 private:
  struct Calibration {
    uint16_t t1 = 0;
    int16_t t2 = 0, t3 = 0;
    uint16_t p1 = 0;
    int16_t p2 = 0, p3 = 0, p4 = 0, p5 = 0, p6 = 0, p7 = 0, p8 = 0, p9 = 0;
    uint8_t h1 = 0, h3 = 0;
    int16_t h2 = 0, h4 = 0, h5 = 0;
    int8_t h6 = 0;
  } cal_;

  TwoWire *wire_ = nullptr;
  uint8_t address_ = 0;
  bool ready_ = false;
  int32_t tFine_ = 0;

  bool readBytes(uint8_t reg, uint8_t *dst, size_t len);
  bool write8(uint8_t reg, uint8_t value);
  bool loadCalibration();
  bool configure();

  static uint16_t u16le(const uint8_t *p);
  static int16_t s16le(const uint8_t *p);
  static int16_t signExtend12(uint16_t v);

  float compensateTemperature(int32_t adcT);
  float compensatePressure(int32_t adcP) const;
  float compensateHumidity(int32_t adcH) const;
};
