# ESP32 Davis Weather Gateway

[Italiano](README.md) | **English**

[![Build firmware](https://github.com/pgpaolo/esp32-davis-weather-gateway/actions/workflows/build.yml/badge.svg)](https://github.com/pgpaolo/esp32-davis-weather-gateway/actions/workflows/build.yml)
![License: LGPL v3](https://img.shields.io/badge/License-LGPL_v3-blue.svg)

Independent **ESP32/LILYGO 868 MHz** gateway for directly receiving a European **Davis Vantage Pro2 / Pro2 Plus wireless station**, adding local sensors and distributing data through Web, MQTT, HTTP and microSD.

`develop` is the active integration branch. `main` contains states promoted through Pull Requests and CI.

## Project status

Current development version: **`0.4.0-dev`**.

Davis EU 868 MHz/FHSS reception has been successfully tested on the current real-hardware setup and the gateway acquires station data. Version 0.4.0-dev leaves the working RF engine unchanged and adds application-layer features: microSD, a redesigned Web UI and secure remote-access preparation.

The new SD and remote-ready functions should be validated on the device before a stable release.

## Hardware

- LILYGO / TTGO LoRa32 T3 V1.6.1 + SX1276/RFM95 868 MHz
- LILYGO T3-S3 + SX1276/RFM95 868 MHz
- Davis Vantage Pro2 / Pro2 Plus wireless EU
- SSD1306 128x64 I2C OLED at `0x3C`
- local BME280 for receiver-side pressure and T/H
- optional AS3935 lightning detector
- onboard microSD on a dedicated SPI bus

## Architecture

```text
Davis ISS EU 868 MHz FHSS
          |
          v
   SX1276 / RFM95
          |
          v
       ESP32
          |
          +-- Davis decoder / FHSS
          +-- OLED 128x64
          +-- BME280
          +-- AS3935
          +-- microSD datalogger
          +-- Web UI / diagnostics
          +-- MQTT
          +-- HTTP upload
          +-- remote-ready TLS profile
          +-- NVS / Wi-Fi provisioning
```

The Davis RF path retains first priority in the loop. Application services do not change frequencies, CRC, packet mapping or FHSS logic.

## Web UI 0.4

The dashboard has been reorganized with a clearer weather overview, prominent RF/last-update state, grouped weather sections, live `NET / RF / BME / SD / MQTT / AS3935 / REMOTE` badges, a cleaner Hardware view and separate configuration cards for Station, Network, HTTP, MQTT, microSD, AS3935, Remote and System.

The HTML is no longer embedded as a large raw C++ string. `web/dashboard.html` is deterministically gzip-compressed during the PlatformIO build by `scripts/generate_web_ui.py`.

## microSD datalogger

The datalogger is **disabled by default** and uses SdFat over a dedicated SPI bus separate from the radio.

Features include a fixed RAM queue, UTC daily CSV files under `/weather/YYYY/MM/YYYY-MM-DD.csv`, `/weather/unsynced.csv` before valid time, optional Davis RAW/NORMALIZED frame logging, weather/BME280/AS3935 snapshots, non-blocking mount retries, 4 MHz SPI with 400 kHz fallback, SdFat diagnostics and explicit double-confirmed FAT formatting.

Pin mapping:

| Board | MOSI | MISO | SCLK | CS |
|---|---:|---:|---:|---:|
| T3 V1.6.1 | 15 | 2 | 14 | 13 |
| T3-S3 V1.2/V1.3 | 11 | 2 | 14 | 13 |

See [docs/SD_DATALOGGER.md](docs/SD_DATALOGGER.md).

## Remote access preparation

Version 0.4.0-dev introduces configuration for a future **outbound TLS relay**, disabled by default.

The gateway does not open Internet-facing ports or automatically expose its local Web UI. NVS stores an HTTPS relay URL, Device ID, per-device token, relay CA, future heartbeat interval and a separate future remote-admin flag. Token and CA are never returned to the browser.

In this version `READY` means the profile is complete; **the remote tunnel is not active yet**. The transport will be implemented together with the relay service, preferably using outbound TLS/WSS and server-authorized sessions.

See [docs/REMOTE_ACCESS.md](docs/REMOTE_ACCESS.md).

## OLED and diagnostics

During acquisition the OLED alternates `DAVIS SEARCH` and `DAVIS RX RAW`. After synchronization it rotates through weather, wind/rain, barometer, RF, AS3935 and gateway status.

The Web diagnostics expose all five Davis channels, RAW/NORMALIZED bytes, received/calculated CRC, recent-frame history, RSSI, timing/jitter, IRQ counters and a downloadable report.

## Pressure

For the Vantage Pro2 6322/6322M sensor suite, barometric pressure is receiver-side. The gateway uses a local BME280 and provides absolute pressure, sea-level reduction, trend and a local barometric indication.

## MQTT and HTTP

MQTT is optional and disabled by default. It supports plain MQTT, CA-verified TLS and explicit insecure TLS.

HTTP upload uses a configurable endpoint; the public firmware contains no installation-specific URL. HTTPS certificate validation is the default.

## First boot

Without stored Wi-Fi, the gateway creates `DavisGateway-XXXX` and serves the captive portal at `http://192.168.4.1`. DHCP is default; `192.168.1.120` is only a suggested static profile.

## Build

```bash
pio run -e t3-v161-868
pio run -e t3-s3-868
```

CI builds both targets with Davis radio, OLED, BME280, AS3935, MQTT, microSD/SdFat and gzip Web UI.

## Documentation

Current index: [docs/README.md](docs/README.md)

Main documents include [Architecture 0.4](docs/ARCHITECTURE_0.4.md), [microSD datalogger](docs/SD_DATALOGGER.md), [Remote access](docs/REMOTE_ACCESS.md), [Davis diagnostics](docs/DIAGNOSTICS_EN.md), [OLED](docs/OLED_DISPLAY.md), [MQTT](docs/MQTT.md), [AS3935](docs/AS3935.md), RF protocol notes and licensing/attribution guides.

## License

Copyright (C) 2026 **Gianpaolo P. (`@pgpaolo`) and contributors**.

Project-original material is distributed under **GNU LGPL v3.0 only (`LGPL-3.0-only`)** unless a file states otherwise. The official GNU license text remains unmodified; copyright, attribution, third-party references and disclaimers are maintained separately in `NOTICE.md` and `THIRD_PARTY_NOTICES.md`.

Davis Instruments, Vantage Pro2 and related names belong to their respective owners. This project is independent and is not affiliated with or endorsed by Davis Instruments.
