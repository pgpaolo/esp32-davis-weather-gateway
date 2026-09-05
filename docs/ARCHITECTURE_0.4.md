# Application Architecture 0.4

Firmware reference: **`0.4.0-dev`**.

## Goal

ESP32 Davis Weather Gateway is a dedicated Davis EU 868 MHz receiver plus local sensing, storage and network services. The working Davis RF engine is intentionally isolated from optional application features.

## RF boundary

`davis_radio.*` and `davis_decoder.*` remain the authoritative weather RF layer. Version 0.4 does not change:

- EU carrier frequencies;
- 2-FSK parameters;
- sync bytes;
- frame length;
- bit normalization;
- CRC algorithm;
- packet type mapping;
- FHSS acquisition/tracking strategy.

The current hardware setup has successfully acquired real Davis station data. New 0.4 work is concentrated outside this validated RF path.

## Main loop priority

```text
1. Davis RF service
2. BME280 / rain persistence / AS3935
3. microSD logger service
4. Davis RF service again
5. network
6. Web UI
7. MQTT
8. HTTP upload
9. OLED refresh
```

The second RF service immediately after storage work reduces the chance that filesystem latency delays packet processing.

## Components

### Davis radio and decoder

Responsibilities:

- SX1276 FSK configuration;
- five-channel EU hop tracking;
- station ID filtering;
- raw packet diagnostics;
- CRC verification;
- weather field decoding;
- per-channel quality/timing diagnostics.

### BME280

Local receiver-side barometer and environmental sensor. Provides absolute pressure, sea-level pressure, T/H, trend and local barometric indication.

### AS3935

Optional lightning detector on the shared I²C bus plus IRQ GPIO. It is independent from Davis RF.

### OLED

SSD1306 128x64 local display for boot state, acquisition, raw packet diagnostics, weather and system status.

### microSD

`sd_logger.*` uses SdFat on the onboard microSD through a dedicated SPI bus. It receives data indirectly from `StationState` and the RF diagnostic history, not by modifying the decoder.

Writes are buffered in a fixed RAM queue and batched outside the RF path. Missing storage is a non-fatal condition.

### Web UI

`web/dashboard.html` contains the local dashboard. During PlatformIO build, `scripts/generate_web_ui.py` gzip-compresses it into a generated header stored in the build directory. `web_manager.cpp` serves the compressed payload from flash.

This keeps the UI maintainable while reducing firmware flash usage.

### MQTT

Optional telemetry output with NVS configuration and plain/TLS modes.

### HTTP upload

Generic configurable realtime receiver output. Installation-specific endpoints are never hard-coded in the public firmware.

### Remote access preparation

`remote_access.*` stores a future outbound relay profile in NVS: HTTPS URL, Device ID, token, CA, heartbeat and administration permission.

Version 0.4.0-dev does not start a remote tunnel. No inbound listener beyond the local LAN Web server is created for remote management.

## Shared I²C bus

OLED, BME280 and AS3935 use the board I²C pins at 100 kHz.

## SPI separation

Radio and microSD use separate SPI buses/pin groups.

### T3 V1.6.1

- SX1276: SCLK 5, MISO 19, MOSI 27, CS 18
- microSD: SCLK 14, MISO 2, MOSI 15, CS 13

### T3-S3

- SX1276: SCLK 5, MISO 3, MOSI 6, CS 7
- microSD: SCLK 14, MISO 2, MOSI 11, CS 13

## Persistent configuration

| Namespace | Purpose |
|---|---|
| `davisgw` | network, Davis, BME altitude, HTTP output, timezone |
| rain persistence namespace | accumulated rain state |
| `mqttcfg` | MQTT settings and credentials |
| `as3935cfg` | AS3935 settings |
| `sdlog` | microSD logger settings |
| `remote` | remote relay readiness profile |

Stored Wi-Fi/MQTT/remote secrets are not rendered back into the browser.

## Security boundary

The local Web UI remains intended for a trusted LAN. Remote access is designed around a future outbound authenticated relay rather than direct exposure of TCP/80.

The remote profile is disabled by default and `allow_remote_admin` is a separate explicit setting.

## Validation status

- Davis RF acquisition: working on the current real-hardware test setup.
- Web UI 0.4: CI-compiled; browser/device validation required.
- microSD/SdFat: CI-compiled; physical-card validation required.
- remote profile: configuration layer only; relay transport intentionally not active yet.
- AS3935: physical installation/tuning remains installation-dependent.
