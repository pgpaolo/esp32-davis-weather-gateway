# ESP32 Davis Weather Gateway

[Italiano](README.md) | **English**

[![Build firmware](https://github.com/pgpaolo/esp32-davis-weather-gateway/actions/workflows/build.yml/badge.svg)](https://github.com/pgpaolo/esp32-davis-weather-gateway/actions/workflows/build.yml)
![License: LGPL v3](https://img.shields.io/badge/License-LGPL_v3-blue.svg)

Independent **ESP32/LILYGO 868 MHz** gateway for directly receiving a European **Davis Vantage Pro2 / Pro2 Plus wireless ISS**, without a Davis console or Meteobridge appliance.

> **Architecture boundary:** the weather radio engine remains exclusively **DAVIS Vantage Pro2 EU 868 MHz FHSS / 2-FSK**. Web UI, OLED, MQTT, BME280, AS3935 and diagnostics are application services and do not add Oregon Scientific, Technoline/LaCrosse decoders or 433 MHz weather RF modes.

`develop` is the active integration branch. `main` contains reviewed states promoted through Pull Requests with successful CI.

## Project status

Current development version: **`0.3.2-dev`**.

CI builds both supported LILYGO targets. Real Davis ISS field validation is still required before declaring a stable release.

Version 0.3.2 adds extended RF/FHSS diagnostics without changing the Davis decoder or five-frequency hop sequence.

## Hardware targets

- LILYGO / TTGO LoRa32 T3 V1.6.1 with **SX1276/RFM95 868 MHz**
- LILYGO T3-S3 + SX1276/RFM95 868 MHz
- onboard **SSD1306 128x64 I2C OLED at 0x3C**
- Davis Vantage Pro2 / Pro2 Plus wireless EU ISS
- local **BME280** over I2C for receiver-side barometer and local T/H
- optional **AS3935** over I2C for lightning detection

The shared OLED/BME280/AS3935 I2C bus is kept at **100 kHz**.

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
          +-- Davis decoder / FHSS   <-- only weather RF engine
          +-- OLED 128x64            <-- data + live search diagnostics
          +-- BME280                 <-- receiver-side barometer
          +-- AS3935                 <-- lightning, I2C + IRQ
          +-- Web UI / extended diagnostics
          +-- optional MQTT
          +-- optional HTTP mb.php
          +-- NVS / Wi-Fi provisioning
```

`serviceDavisRadio()` retains first priority in the loop. OLED, Web, MQTT and auxiliary sensors do not change Davis hopping, modulation or packet decoding.

See [docs/ARCHITECTURE_0.3.md](docs/ARCHITECTURE_0.3.md).

## OLED and Davis acquisition

At boot the OLED shows configuration/NVS, network, BME280, AS3935/MQTT and Davis radio initialization. While acquiring the ISS it alternates **DAVIS SEARCH** and **DAVIS RX RAW**, showing channel/frequency, RSSI, counters, latest raw frame and CRC state.

After lock it rotates through weather, wind/rain, barometer, RF/FHSS, AS3935 and gateway status. If Davis traffic becomes stale or lock is lost, the display automatically returns to acquisition mode.

See [docs/OLED_DISPLAY.md](docs/OLED_DISPLAY.md).

## Extended RF diagnostics 0.3.2

The **Diagnostics** page is intended to identify where the receive chain stops before changing the decoder.

It exposes:

- `ERROR / SEARCH / CANDIDATE / SYNC` acquisition phases;
- separate statistics for all five Davis EU channels;
- RAW and NORMALIZED bytes plus received/calculated CRC;
- a **24-frame RAM ring buffer**;
- per-channel last/average/minimum/maximum RSSI;
- packet timing, min/max/average interval and jitter against the expected ~2555 ms;
- DIO0 IRQ, `readData()`, tune, hop, miss streak and RadioLib error counters;
- ESP32 health: uptime, free/minimum heap, CPU, Wi-Fi RSSI and reset reason;
- **Diagnostic capture 60 s**;
- downloadable `davis-diagnostic.txt` report.

Quick interpretation:

- `RAW = 0`: no candidate frame reaches the radio packet path;
- `RAW > 0` with `CRC KO`: RF traffic is present; inspect framing/bit order/CRC;
- `CRC OK` without `SYNC`: inspect ISS ID and FHSS continuity;
- `SYNC`: Davis RF acquisition is substantially working.

See [docs/DIAGNOSTICS_EN.md](docs/DIAGNOSTICS_EN.md) and [docs/DIAGNOSTICS_IT.md](docs/DIAGNOSTICS_IT.md).

## Weather data and pressure

The ISS provides outside temperature/humidity, wind, gust, direction, rain/rain rate, UV, solar radiation and transmitter battery state. The gateway provides local BME280 pressure/T/H, pressure trend/forecast and optional AS3935 events.

For the **6322/6322M Sensor Suite**, barometric pressure is receiver-side rather than part of the outdoor ISS RF payload. The gateway follows the same architecture with a local BME280.

## Davis EU RF

The receiver uses 2-FSK at 19.2 kbps, 4.8 kHz deviation, Gaussian BT=0.5, `CB 89` sync and fixed 10-byte frames with Davis CRC validation.

Implemented EU hop set:

1. 868.066711 MHz
2. 868.297119 MHz
3. 868.527466 MHz
4. 868.181885 MHz
5. 868.412292 MHz

There is **no** Oregon Scientific receiver, Technoline/LaCrosse decoder or 433 MHz weather mode in this project.

## Web UI and API

The dark/tabbed UI exposes Dashboard, Hardware, Configuration and Diagnostics with **NET / RF / BME / MQTT / AS3935** status.

Main APIs:

- `/api/state` and `/api/status`
- `/api/rf`
- `/api/rf/diagnostics`
- `POST /api/rf/reset`
- `/api/diag/report`
- `/api/system`
- `/api/bme` and `/api/i2c/scan`
- AS3935 and MQTT APIs
- `/api/config`
- `/api/meteobridge`

The Web UI is intended for a trusted LAN and should not be exposed directly to the Internet without an authenticated access-control layer.

## MQTT and AS3935

MQTT is disabled by default and supports plain MQTT, CA-verified TLS and explicit insecure TLS.

AS3935 is optional; T3 V1.6.1 defaults to I2C `0x03` and GPIO34 IRQ, while T3-S3 keeps AS3935 disabled by default until a safe IRQ pin is validated.

See [docs/MQTT.md](docs/MQTT.md) and [docs/AS3935.md](docs/AS3935.md).

## Wi-Fi, provisioning and reset levels

Without stored Wi-Fi configuration, the gateway creates `DavisGateway-XXXX` and serves the captive portal at `http://192.168.4.1`. DHCP is default; `192.168.1.120` is only a suggested static profile.

Current reset scopes:

- `POST /api/rf/reset` / 60-second capture: clears only RF diagnostics;
- **Reset network**: removes the Wi-Fi/network profile and returns to provisioning;
- MQTT reset: clears MQTT configuration only;
- AS3935 reset: clears lightning-detector configuration only;
- full flash reset: `esptool erase_flash` followed by a firmware upload.

Version 0.3.2-dev does not yet provide a one-click Web factory-reset action. The complete procedure is documented in [docs/DIAGNOSTICS_EN.md](docs/DIAGNOSTICS_EN.md).

## HTTP receiver

The public firmware contains no installation-specific HTTP endpoint. Each installation configures its own receiver; HTTPS certificate validation is enabled by default and insecure mode requires explicit opt-in.

## PlatformIO build

```bash
pio run -e t3-v161-868
pio run -e t3-s3-868
```

CI builds both boards with OLED/U8g2, Web, extended diagnostics, MQTT, BME280 and AS3935.

## Documentation

The current `develop` documentation index is [docs/README.md](docs/README.md).

Main documents:

- [Extended Davis diagnostics](docs/DIAGNOSTICS_EN.md)
- [Diagnostica estesa Davis](docs/DIAGNOSTICS_IT.md)
- [OLED display and Davis acquisition](docs/OLED_DISPLAY.md)
- [Application architecture 0.3.2](docs/ARCHITECTURE_0.3.md)
- [MQTT](docs/MQTT.md)
- [AS3935 Lightning](docs/AS3935.md)
- [RF protocol notes - Italian](docs/RF_PROTOCOL_IT.md)
- [RF protocol notes - English](docs/RF_PROTOCOL_EN.md)
- [Licensing and provenance - English](docs/LICENSING_EN.md)
- [Licenza e provenance - Italiano](docs/LICENSING_IT.md)

Historical RF PDF guides v1.0/v1.1 are kept for project history and do not by themselves describe the complete 0.3.2 application layer.

## License, attribution and provenance

Copyright (C) 2026 **Gianpaolo P. (`@pgpaolo`) and contributors**.

Project-original code and documentation are distributed under **GNU LGPL v3.0 only (`LGPL-3.0-only`)** unless a file states otherwise.

The explicit-attribution practice used in the maintainer's Oregon/Technoline project is preserved here: technical provenance and third-party references are documented openly, without importing the Oregon/Technoline GPL decoder engine or its `rtl_433` / PracticalArduino-derived portions into the Davis weather RF engine.

The canonical LGPL text is not modified. Project-specific notices and attribution are kept separately in:

- [LICENSE](LICENSE)
- [NOTICE.md](NOTICE.md)
- [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)
- [docs/LICENSING_EN.md](docs/LICENSING_EN.md)
- [docs/LICENSING_IT.md](docs/LICENSING_IT.md)

Davis Instruments, Vantage Pro2 and related names belong to their respective owners. This project is independent and is not affiliated with or endorsed by Davis Instruments.

See also [SECURITY.md](SECURITY.md), [CONTRIBUTING.md](CONTRIBUTING.md) and [docs/BRANCH_POLICY.md](docs/BRANCH_POLICY.md).
