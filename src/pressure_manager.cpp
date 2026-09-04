#include "pressure_manager.h"

#include <Adafruit_BME280.h>
#include <Wire.h>
#include <math.h>

#include "board_config.h"
#include "config.h"
#include "runtime_config.h"

namespace {
Adafruit_BME280 bme;
PressureSensorStatus status;

constexpr uint32_t READ_INTERVAL_MS = 5000UL;
constexpr uint32_t TREND_SAMPLE_MS = 10UL * 60UL * 1000UL;
constexpr uint8_t TREND_SAMPLES = 20;
struct TrendSample { uint32_t ms; float hpa; };
TrendSample trend[TREND_SAMPLES]{};
uint8_t trendHead = 0;
uint8_t trendCount = 0;
uint32_t lastTrendMs = 0;

float seaLevelPressure(float stationHpa, float altitudeM) {
  if (!isfinite(stationHpa)) return NAN;
  const float ratio = 1.0f - altitudeM / 44330.0f;
  if (ratio <= 0.0f) return stationHpa;
  return stationHpa / powf(ratio, 5.255f);
}

void configureWire() {
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(100000UL);
#if defined(ARDUINO_ARCH_ESP32)
  Wire.setTimeOut(80);
#endif
}

bool tryAddress(uint8_t address) {
  if (!bme.begin(address, &Wire)) return false;
  status.detected = true;
  status.address = address;
  status.detectFailures = 0;
  status.consecutiveInvalid = 0;
  bme.setSampling(Adafruit_BME280::MODE_NORMAL,
                  Adafruit_BME280::SAMPLING_X2,
                  Adafruit_BME280::SAMPLING_X16,
                  Adafruit_BME280::SAMPLING_X1,
                  Adafruit_BME280::FILTER_X4,
                  Adafruit_BME280::STANDBY_MS_500);
  return true;
}

bool detectBme() {
  status.lastDetectAttemptMs = millis();
  status.detected = false;
  status.address = 0;
  const bool ok = tryAddress(0x76) || tryAddress(0x77);
  if (!ok && status.detectFailures < 250) status.detectFailures++;
  return ok;
}

uint32_t retryDelayMs() {
  if (status.detectFailures <= 1) return 5000UL;
  if (status.detectFailures == 2) return 15000UL;
  if (status.detectFailures <= 5) return 60000UL;
  return 300000UL;
}

void updateTrend(StationState &s, uint32_t now, float hpa) {
  if (lastTrendMs && (uint32_t)(now - lastTrendMs) < TREND_SAMPLE_MS) return;
  lastTrendMs = now;
  trend[trendHead] = {now, hpa};
  trendHead = (uint8_t)((trendHead + 1U) % TREND_SAMPLES);
  if (trendCount < TREND_SAMPLES) trendCount++;
  if (trendCount < 4) { s.pressureTrendValid = false; return; }

  const int newest = (int(trendHead) - 1 + TREND_SAMPLES) % TREND_SAMPLES;
  const int oldest = (int(trendHead) - trendCount + TREND_SAMPLES) % TREND_SAMPLES;
  const uint32_t window = trend[newest].ms - trend[oldest].ms;
  if (window < 30UL * 60UL * 1000UL || !isfinite(trend[oldest].hpa)) {
    s.pressureTrendValid = false;
    return;
  }
  const float hours = float(window) / 3600000.0f;
  s.pressureTrendHpa3h = (trend[newest].hpa - trend[oldest].hpa) * (3.0f / hours);
  s.pressureTrendWindowMin = (uint16_t)(window / 60000UL);
  s.pressureTrendValid = true;
}

String jfloat(float v, uint8_t decimals=1) {
  return isfinite(v) ? String(v, (unsigned int)decimals) : String("null");
}
} // namespace

bool initPressureSensor() {
#if BME280_ENABLE
  configureWire();
  const bool ok = detectBme();
  if (ok) {
    Serial.print(F("[BME] BME280 @0x")); Serial.println(status.address, HEX);
  } else {
    Serial.println(F("[BME] non rilevato; retry non bloccante attivo"));
  }
  return ok;
#else
  return false;
#endif
}

void servicePressureSensor(StationState &station) {
#if BME280_ENABLE
  const uint32_t now = millis();
  if (!status.detected) {
    if ((uint32_t)(now - status.lastDetectAttemptMs) >= retryDelayMs()) detectBme();
    return;
  }
  if ((uint32_t)(now - status.lastReadMs) < READ_INTERVAL_MS) return;
  status.lastReadMs = now;

  const float pressurePa = bme.readPressure();
  const float temp = bme.readTemperature();
  const float hum = bme.readHumidity();
  if (!isfinite(pressurePa) || pressurePa < 30000.0f || pressurePa > 120000.0f) {
    status.readErrors++;
    if (status.consecutiveInvalid < 250) status.consecutiveInvalid++;
    if (status.consecutiveInvalid >= 3) {
      status.detected = false;
      status.detectFailures = 1;
      status.lastDetectAttemptMs = now;
      Serial.println(F("[BME] letture non valide ripetute: avvio rediscovery"));
    }
    return;
  }
  status.consecutiveInvalid = 0;

  const float absoluteHpa = pressurePa / 100.0f;
  const float seaHpa = seaLevelPressure(absoluteHpa, runtimeConfig.bmeAltitudeM);
  station.pressureAbsoluteHpa = absoluteHpa;
  station.pressureHpa = seaHpa;
  station.pressureUpdatedMs = now;
  if (isfinite(temp) && temp > -50.0f && temp < 100.0f) station.indoorTempC = temp;
  if (isfinite(hum) && hum >= 0.0f && hum <= 100.0f) station.indoorHumidity = hum;
  updateTrend(station, now, seaHpa);
  updateDailyExtremes(station);
#else
  (void)station;
#endif
}

bool pressureSensorAvailable() { return status.detected; }
uint8_t pressureSensorAddress() { return status.address; }
const PressureSensorStatus &getPressureSensorStatus() { return status; }

const char *pressureTrendName(const StationState &s) {
  if (!s.pressureTrendValid) return "acquiring";
  if (s.pressureTrendHpa3h >= 2.0f) return "rising";
  if (s.pressureTrendHpa3h <= -2.0f) return "falling";
  return "steady";
}

const char *pressureForecastName(const StationState &s) {
  if (!isfinite(s.pressureHpa)) return "N/D";
  if (s.pressureTrendValid) {
    if (s.pressureTrendHpa3h >= 2.5f) return "Miglioramento";
    if (s.pressureTrendHpa3h <= -2.5f) return "Peggioramento";
  }
  if (s.pressureHpa >= 1022.0f) return "Stabile / sereno";
  if (s.pressureHpa <= 1000.0f) return "Instabile / pioggia";
  return "Variabile";
}

String pressureStatusJson(const StationState &s) {
  String out; out.reserve(360);
  out = "{\"detected\":"; out += status.detected ? "true" : "false";
  out += ",\"address\":" + String(status.address);
  out += ",\"absolute_hpa\":" + jfloat(s.pressureAbsoluteHpa);
  out += ",\"sea_level_hpa\":" + jfloat(s.pressureHpa);
  out += ",\"temperature_c\":" + jfloat(s.indoorTempC);
  out += ",\"humidity_pct\":" + jfloat(s.indoorHumidity,0);
  out += ",\"trend_hpa_3h\":" + jfloat(s.pressureTrendHpa3h);
  out += ",\"trend_valid\":"; out += s.pressureTrendValid ? "true" : "false";
  out += ",\"trend_window_min\":" + String(s.pressureTrendWindowMin);
  out += ",\"forecast\":\"" + String(pressureForecastName(s)) + "\"";
  out += ",\"read_errors\":" + String(status.readErrors);
  out += ",\"detect_failures\":" + String(status.detectFailures) + "}";
  return out;
}

String i2cScanJson() {
  configureWire();
  String out = "{\"clock_hz\":100000,\"devices\":[";
  bool first = true;
  for (uint8_t a=1; a<127; ++a) {
    Wire.beginTransmission(a);
    const uint8_t err = Wire.endTransmission();
    if (err == 0) {
      if (!first) out += ',';
      first = false;
      out += String(a);
    }
    delay(1);
  }
  out += "],\"bme_detected\":";
  out += status.detected ? "true" : "false";
  out += ",\"bme_address\":" + String(status.address) + "}";
  Wire.setClock(100000UL);
  return out;
}
