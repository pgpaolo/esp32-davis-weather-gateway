#include "bme280_minimal.h"

#include <math.h>

namespace {
constexpr uint8_t REG_ID = 0xD0;
constexpr uint8_t REG_RESET = 0xE0;
constexpr uint8_t REG_CTRL_HUM = 0xF2;
constexpr uint8_t REG_STATUS = 0xF3;
constexpr uint8_t REG_CTRL_MEAS = 0xF4;
constexpr uint8_t REG_CONFIG = 0xF5;
constexpr uint8_t REG_DATA = 0xF7;
constexpr uint8_t CHIP_ID_BME280 = 0x60;
}

uint16_t Bme280Minimal::u16le(const uint8_t *p) {
  return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

int16_t Bme280Minimal::s16le(const uint8_t *p) {
  return (int16_t)u16le(p);
}

int16_t Bme280Minimal::signExtend12(uint16_t v) {
  if (v & 0x0800U) v |= 0xF000U;
  return (int16_t)v;
}

bool Bme280Minimal::readBytes(uint8_t reg, uint8_t *dst, size_t len) {
  if (!wire_ || !dst || len == 0) return false;
  wire_->beginTransmission(address_);
  wire_->write(reg);
  if (wire_->endTransmission(false) != 0) return false;
  const size_t got = wire_->requestFrom((int)address_, (int)len);
  if (got != len) {
    while (wire_->available()) wire_->read();
    return false;
  }
  for (size_t i = 0; i < len; ++i) dst[i] = (uint8_t)wire_->read();
  return true;
}

bool Bme280Minimal::write8(uint8_t reg, uint8_t value) {
  if (!wire_) return false;
  wire_->beginTransmission(address_);
  wire_->write(reg);
  wire_->write(value);
  return wire_->endTransmission() == 0;
}

bool Bme280Minimal::loadCalibration() {
  uint8_t a[26] = {0};
  uint8_t h[7] = {0};
  if (!readBytes(0x88, a, sizeof(a))) return false;
  if (!readBytes(0xE1, h, sizeof(h))) return false;

  cal_.t1 = u16le(&a[0]);
  cal_.t2 = s16le(&a[2]);
  cal_.t3 = s16le(&a[4]);
  cal_.p1 = u16le(&a[6]);
  cal_.p2 = s16le(&a[8]);
  cal_.p3 = s16le(&a[10]);
  cal_.p4 = s16le(&a[12]);
  cal_.p5 = s16le(&a[14]);
  cal_.p6 = s16le(&a[16]);
  cal_.p7 = s16le(&a[18]);
  cal_.p8 = s16le(&a[20]);
  cal_.p9 = s16le(&a[22]);
  cal_.h1 = a[25];

  cal_.h2 = s16le(&h[0]);
  cal_.h3 = h[2];
  cal_.h4 = signExtend12(((uint16_t)h[3] << 4) | (h[4] & 0x0FU));
  cal_.h5 = signExtend12(((uint16_t)h[5] << 4) | (h[4] >> 4));
  cal_.h6 = (int8_t)h[6];

  return cal_.t1 != 0 && cal_.p1 != 0;
}

bool Bme280Minimal::configure() {
  // Soft-reset first so a sensor previously configured by another firmware
  // always starts from a known state.
  if (!write8(REG_RESET, 0xB6)) return false;
  delay(3);

  // Wait for NVM calibration copy to complete (im_update bit clears).
  for (uint8_t i = 0; i < 20; ++i) {
    uint8_t s = 0;
    if (!readBytes(REG_STATUS, &s, 1)) return false;
    if ((s & 0x01U) == 0) break;
    delay(2);
  }

  if (!loadCalibration()) return false;

  // Humidity x1; temperature x2; pressure x16; normal mode;
  // filter x4; standby 500 ms. Matches the former Adafruit setup.
  if (!write8(REG_CTRL_HUM, 0x01)) return false;
  if (!write8(REG_CONFIG, 0x88)) return false;
  if (!write8(REG_CTRL_MEAS, 0x57)) return false;
  return true;
}

bool Bme280Minimal::begin(uint8_t address, TwoWire &wire) {
  ready_ = false;
  wire_ = &wire;
  address_ = address;
  uint8_t id = 0;
  if (!readBytes(REG_ID, &id, 1) || id != CHIP_ID_BME280) return false;
  ready_ = configure();
  return ready_;
}

float Bme280Minimal::compensateTemperature(int32_t adcT) {
  const int32_t var1 = (((adcT >> 3) - ((int32_t)cal_.t1 << 1)) * (int32_t)cal_.t2) >> 11;
  const int32_t x = (adcT >> 4) - (int32_t)cal_.t1;
  const int32_t var2 = (((x * x) >> 12) * (int32_t)cal_.t3) >> 14;
  tFine_ = var1 + var2;
  const int32_t t = (tFine_ * 5 + 128) >> 8;
  return (float)t / 100.0f;
}

float Bme280Minimal::compensatePressure(int32_t adcP) const {
  int64_t var1 = (int64_t)tFine_ - 128000;
  int64_t var2 = var1 * var1 * (int64_t)cal_.p6;
  var2 += (var1 * (int64_t)cal_.p5) << 17;
  var2 += ((int64_t)cal_.p4) << 35;
  var1 = ((var1 * var1 * (int64_t)cal_.p3) >> 8) + ((var1 * (int64_t)cal_.p2) << 12);
  var1 = (((((int64_t)1) << 47) + var1) * (int64_t)cal_.p1) >> 33;
  if (var1 == 0) return NAN;

  int64_t p = 1048576 - adcP;
  p = (((p << 31) - var2) * 3125) / var1;
  var1 = ((int64_t)cal_.p9 * (p >> 13) * (p >> 13)) >> 25;
  var2 = ((int64_t)cal_.p8 * p) >> 19;
  p = ((p + var1 + var2) >> 8) + (((int64_t)cal_.p7) << 4);
  return (float)p / 256.0f;
}

float Bme280Minimal::compensateHumidity(int32_t adcH) const {
  int32_t v = tFine_ - 76800;
  v = (((((adcH << 14) - ((int32_t)cal_.h4 << 20) - ((int32_t)cal_.h5 * v)) + 16384) >> 15) *
       (((((((v * (int32_t)cal_.h6) >> 10) * (((v * (int32_t)cal_.h3) >> 11) + 32768)) >> 10) + 2097152) *
          (int32_t)cal_.h2 + 8192) >> 14));
  v -= (((((v >> 15) * (v >> 15)) >> 7) * (int32_t)cal_.h1) >> 4);
  if (v < 0) v = 0;
  if (v > 419430400) v = 419430400;
  return (float)(v >> 12) / 1024.0f;
}

bool Bme280Minimal::read(float &temperatureC, float &pressurePa, float &humidityPct) {
  temperatureC = pressurePa = humidityPct = NAN;
  if (!ready_) return false;

  uint8_t d[8] = {0};
  if (!readBytes(REG_DATA, d, sizeof(d))) return false;

  const int32_t adcP = ((int32_t)d[0] << 12) | ((int32_t)d[1] << 4) | (d[2] >> 4);
  const int32_t adcT = ((int32_t)d[3] << 12) | ((int32_t)d[4] << 4) | (d[5] >> 4);
  const int32_t adcH = ((int32_t)d[6] << 8) | d[7];
  if (adcT == 0x80000 || adcP == 0x80000) return false;

  temperatureC = compensateTemperature(adcT);
  pressurePa = compensatePressure(adcP);
  humidityPct = compensateHumidity(adcH);
  return isfinite(temperatureC) && isfinite(pressurePa) && isfinite(humidityPct);
}
