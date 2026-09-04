# ESP32 Davis Weather Gateway

[Italiano](README.md) | **English**

[![Build firmware](https://github.com/pgpaolo/esp32-davis-weather-gateway/actions/workflows/build.yml/badge.svg)](https://github.com/pgpaolo/esp32-davis-weather-gateway/actions/workflows/build.yml)
![License: LGPL v3](https://img.shields.io/badge/License-LGPL_v3-blue.svg)

Independent **ESP32/LILYGO 868 MHz** gateway for directly receiving a European **Davis Vantage Pro2 / Pro2 Plus wireless ISS**, without a Davis console or Meteobridge appliance.

> **Architecture boundary:** the weather radio engine remains exclusively **DAVIS Vantage Pro2 EU 868 MHz FHSS / 2-FSK**. Web UI, OLED, MQTT, BME280 and AS3935 are application services and do not introduce Oregon Scientific, Technoline/LaCrosse decoders or 433 MHz weather RF modes.

`develop` is the active integration branch. `main` contains reviewed states promoted through Pull Requests with successful CI.

## Project status

Current development version: **`0.3.1-dev`**.

CI builds both supported LILYGO targets. Real Davis ISS field validation is still required before declaring a stable release.

## Hardware targets

- LILYGO / TTGO LoRa32 T3 V1.6.1 with **SX1276/RFM95 868 MHz**
- LILYGO T3-S3 + SX1276/RFM95 868 MHz
- onboard **SSD1306 128x64 I2C OLED at 0x3C**
- Davis Vantage Pro2 / Pro2 Plus wireless EU ISS
- local **BME280** over I2C for receiver-side barometer and local T/H
- optional **AS3935** over I2C for lightning detection

The shared OLED/BME280/AS3935 I2C bus is kept at **100 kHz** for reliability margin.

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
          +-- Web UI / diagnostics
          +-- optional MQTT
          +-- optional HTTP mb.php
          +-- NVS / Wi-Fi provisioning
```

`serviceDavisRadio()` retains first priority in the loop. OLED refresh runs after the RF path and never changes hopping, modulation or packet decoding.

## OLED and live Davis acquisition diagnostics

The onboard display is initialized at boot and shows startup progress for configuration/NVS, network, BME280, AS3935/MQTT and Davis radio initialization.

Until a valid Davis packet is acquired and FHSS synchronization is established, the OLED stays on **DAVIS SEARCH** and displays:

- current FHSS channel and frequency
- ISS filter (`AUTO` or configured transmitter ID)
- RSSI when available
- valid packet, CRC error and missed-packet counters
- BME280 and AS3935 status
- Web IP or captive/recovery portal IP

After RF lock, the display rotates about every 6.5 seconds through:

1. Davis weather
2. wind and rain
3. BME280 barometer/trend/forecast
4. Davis RF/FHSS diagnostics
5. AS3935 lightning
6. gateway/Wi-Fi/MQTT/heap/uptime status

If Davis traffic becomes stale for about 12 seconds or synchronization is lost, the OLED automatically returns to **DAVIS SEARCH**. An SX1276 initialization failure is shown as **DAVIS RF ERROR** with the RadioLib error code.

See [docs/OLED_DISPLAY.md](docs/OLED_DISPLAY.md).

## Weather and Davis RF

The ISS provides outside temperature/humidity, wind, gust, direction, rain/rain rate, UV, solar radiation and transmitter battery status. The local gateway provides BME280 pressure/T/H, pressure trend/forecast and optional AS3935 events.

For the **6322/6322M Sensor Suite**, barometric pressure is receiver-side rather than part of the outdoor ISS RF payload. The gateway follows the same architecture with a local BME280.

The Davis receiver uses 2-FSK at 19.2 kbps, 4.8 kHz deviation, Gaussian BT=0.5, `CB 89` sync and fixed 10-byte frames with Davis CRC validation.

Implemented EU hop set: 868.066711, 868.297119, 868.527466, 868.181885 and 868.412292 MHz.

There is **no** Oregon Scientific receiver, Technoline/LaCrosse decoder or 433 MHz weather mode in this project.

## Web UI, MQTT and AS3935

The dark/tabbed UI exposes Dashboard, Hardware, Configuration and Diagnostics with **NET / RF / BME / MQTT / AS3935** status. Main APIs include `/api/state`, `/api/status`, `/api/bme`, `/api/i2c/scan`, AS3935/MQTT configuration/status APIs, `/api/config` and `/api/meteobridge`.

MQTT is disabled by default and supports plain MQTT, CA-verified TLS and explicit insecure TLS. AS3935 is optional; T3 V1.6.1 defaults to I2C `0x03` and GPIO34 IRQ, while T3-S3 keeps AS3935 disabled by default until a safe IRQ pin is validated.

The Web UI is intended for a trusted LAN and should not be exposed directly to the Internet without an authenticated access-control layer.

## Wi-Fi and HTTP receiver

Without stored Wi-Fi configuration, the gateway creates `DavisGateway-XXXX` and serves the captive portal at `http://192.168.4.1`. DHCP is default; `192.168.1.120` is only a suggested static profile. The public firmware contains no installation-specific HTTP endpoint.

## PlatformIO build

```bash
pio run -e t3-v161-868
pio run -e t3-s3-868
```

CI `0.3.1-dev` builds both boards with OLED/U8g2, Web, MQTT, BME280 and AS3935. The classic T3 V1.6.1 build uses about **54.3 kB RAM** and **1.169 MB flash** in the current application partition.

## Documentation

- [OLED display and Davis acquisition](docs/OLED_DISPLAY.md)
- [Application architecture 0.3](docs/ARCHITECTURE_0.3.md)
- [MQTT](docs/MQTT.md)
- [AS3935 Lightning](docs/AS3935.md)
- [RF protocol notes - Italian](docs/RF_PROTOCOL_IT.md)
- [RF protocol notes - English](docs/RF_PROTOCOL_EN.md)

RF documentation is independent interoperability/reverse-engineering material and is not an official Davis Instruments specification.

## Security, branches and license

See [SECURITY.md](SECURITY.md), [CONTRIBUTING.md](CONTRIBUTING.md), and [docs/BRANCH_POLICY.md](docs/BRANCH_POLICY.md). Project-original code and documentation are distributed under **GNU LGPL v3.0 only (`LGPL-3.0-only`)**, unless otherwise stated.
