#include "pressure_manager.h"

#include <Adafruit_BME280.h>
#include <Wire.h>
#include <math.h>

#include "board_config.h"
#include "config.h"
#include "runtime_config.h"

namespace {
Adafruit_BME280 bme;
bool available = false;
uint32_t lastReadMs = 0;

float seaLevelPressure(float stationHpa, float altitudeM) {
  if (!isfinite(stationHpa)) return NAN;
  const float ratio = 1.0f - altitudeM / 44330.0f;
  if (ratio <= 0.0f) return stationHpa;
  return stationHpa / powf(ratio, 5.255f);
}
}

bool initPressureSensor() {
#if BME280_ENABLE
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  available = bme.begin(0x76, &Wire);
  if (!available) available = bme.begin(0x77, &Wire);
  if (available) {
    bme.setSampling(Adafruit_BME280::MODE_NORMAL,
                    Adafruit_BME280::SAMPLING_X2,
                    Adafruit_BME280::SAMPLING_X2,
                    Adafruit_BME280::SAMPLING_X2,
                    Adafruit_BME280::FILTER_X4,
                    Adafruit_BME280::STANDBY_MS_1000);
  }
#endif
  return available;
}

void servicePressureSensor(StationState &station) {
  if (!available) return;
  const uint32_t now = millis();
  if ((uint32_t)(now - lastReadMs) < 5000UL) return;
  lastReadMs = now;

  const float temp = bme.readTemperature();
  const float hum = bme.readHumidity();
  const float stationHpa = bme.readPressure() / 100.0f;
  const float seaHpa = seaLevelPressure(stationHpa, runtimeConfig.bmeAltitudeM);

  if (isfinite(temp)) station.indoorTempC = temp;
  if (isfinite(hum) && hum >= 0.0f && hum <= 100.0f) station.indoorHumidity = hum;
  if (isfinite(seaHpa) && seaHpa > 800.0f && seaHpa < 1150.0f) station.pressureHpa = seaHpa;
  updateDailyExtremes(station);
}

bool pressureSensorAvailable() { return available; }
