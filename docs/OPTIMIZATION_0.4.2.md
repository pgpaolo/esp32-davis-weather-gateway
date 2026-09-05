# ESP32 Davis Weather Gateway - 0.4.2-opt-dev

Branch: **`develop-optimized`**

This branch is an optimization/test line derived from the working `develop` firmware. The Davis RF decoder, FHSS frequencies, CRC handling, OLED, AdminSensor Remote, MQTT, AS3935, microSD logging/formatting and Web UI are intentionally unchanged unless stated below.

## Goals

The optimization line addresses two issues:

1. the classic T3 V1.6.1 application image was close to the default application-partition limit;
2. the BME280 dependency chain could be simplified without changing the pressure features exposed by the gateway.

## Larger application partition

Both supported PlatformIO targets use the Arduino ESP32 `huge_app.csv` partition layout.

This gives the application image a **3,145,728-byte** slot on the 4 MB flash device. The current firmware remains around 1.2 MB, so the percentage reported by PlatformIO drops substantially without deleting features.

This optimization deliberately does not remove Serial/debug output.

## Minimal BME280 driver

The Adafruit BME280 + Unified Sensor + BusIO dependency chain has been replaced by the project-local `bme280_minimal.*` driver.

The driver implements only what this gateway requires:

- chip-ID verification (`0x60`);
- I2C addresses `0x76` / `0x77`;
- calibration-register loading;
- Bosch integer compensation formulas;
- temperature, pressure and humidity;
- the same sampling/filter profile used previously;
- automatic rediscovery through `pressure_manager.*`.

The external behaviour remains the same: absolute pressure, sea-level pressure, local temperature/humidity, trend and forecast continue to be exposed to Web/OLED/MQTT/SD.

## Rain persistence remains intentionally frequent

Rain data is treated as high-value state. The existing NVS persistence policy is intentionally preserved:

- cumulative rain values are saved when they change;
- writes are rate-limited to approximately **one save every 15 seconds** while rain state is changing;
- day/month/year rollover is persisted immediately;
- explicit forced saves remain available.

The microSD provides the historical record, but NVS remains a separate recovery layer so the cumulative rain value survives reboot even if the SD card is absent, removed or damaged.

Configuration data for Wi-Fi, MQTT, AS3935, AdminSensor Remote, display state and similar services is written only on explicit configuration changes or one-time identity/token creation; it is not continuously rewritten in the main loop.

## Functions intentionally retained

The following were **not** removed to save flash:

- microSD formatting;
- microSD CSV logging and viewer;
- U8g2 OLED graphics;
- debug/Serial diagnostics;
- extended Davis RF diagnostics;
- AdminSensor Remote WebSocket/TLS support;
- MQTT;
- AS3935;
- Web UI and gzip dashboard;
- HTTPS/HTTP upload.

## CI result

Initial CI validation of the optimization core succeeded on both boards.

### T3 V1.6.1 SX1276 868

```text
RAM:   65,644 / 327,680 bytes  = 20.0%
Flash: 1,260,249 / 3,145,728   = 40.1%
```

### T3-S3 SX1276 868

```text
RAM:   64,176 / 327,680 bytes  = 19.6%
Flash: 1,200,673 / 3,145,728   = 38.2%
```

The lower percentage is primarily due to the larger application partition; the minimal BME280 driver also reduces the linked application image compared with the previous Adafruit-based build.

## Validation required before promotion

Before merging this branch back into `develop`, test on real hardware:

- Davis RF acquisition and long-term FHSS continuity;
- BME280 temperature/humidity/absolute pressure against the previous build or a known reference;
- sea-level pressure conversion;
- OLED pages;
- microSD logging and formatting;
- AdminSensor Remote;
- MQTT/HTTP outputs;
- reboot recovery of cumulative rain values.

Do not promote `develop-optimized` to `develop` until the BME280 readings have been compared on the actual device.
