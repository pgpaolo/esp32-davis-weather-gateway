# ESP32 Davis Weather Gateway

[Italiano](README.md) | **English**

[![Build firmware](https://github.com/pgpaolo/esp32-davis-weather-gateway/actions/workflows/build.yml/badge.svg)](https://github.com/pgpaolo/esp32-davis-weather-gateway/actions/workflows/build.yml)
![License: LGPL v3](https://img.shields.io/badge/License-LGPL_v3-blue.svg)

An independent **ESP32/LILYGO 868 MHz** gateway for receiving a European **Davis Vantage Pro2 / Pro2 Plus wireless ISS** directly, without a Davis console and without a Meteobridge appliance.

> **Architecture boundary:** the radio engine remains exclusively **DAVIS Vantage Pro2 EU 868 MHz FHSS / 2-FSK**. The `0.3.x` line ports the mature application-layer UI and services from the Oregon/Technoline 6.4 reference project, but it does **not** import Oregon Scientific / Technoline decoders, parsers or RF modes.

`develop` is the active integration branch. `main` contains reviewed states promoted through Pull Request and successful CI.

## Project status

Current development version: **`0.3.0-dev`**.

CI builds both supported LILYGO targets. The Davis RF decoder still requires field validation against real ISS hardware before a stable release can be declared.

## Hardware targets

- LILYGO / TTGO LoRa32 T3 V1.6.1 with **SX1276/RFM95 868 MHz**
- LILYGO T3-S3 + SX1276/RFM95 868 MHz
- Davis Vantage Pro2 / Pro2 Plus wireless EU ISS
- local **BME280** over I2C for receiver-side barometer and local T/H
- optional **AS3935** over I2C for lightning detection

A 433 MHz-only radio board is not suitable. The Davis receiver requires hardware for the European 868 MHz band.

## 0.3 architecture

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
          +-- BME280                 <-- receiver-side barometer
          +-- AS3935                 <-- lightning, I2C + IRQ
          +-- Web UI / diagnostics
          +-- optional MQTT
          +-- optional HTTP mb.php
          +-- NVS / Wi-Fi provisioning
```

`serviceDavisRadio()` retains first priority in the main loop. MQTT, Web, BME280 and AS3935 are auxiliary services and never change Davis modulation, hop table or packet decoding.

## Weather data

From the Davis ISS:

- outside temperature and humidity
- wind speed and direction
- 10-minute gust
- rain counter and rain rate
- UV and solar radiation when available
- transmitter battery flag

From the local gateway:

- BME280 absolute and sea-level pressure
- local BME280 temperature/humidity
- barometric trend and indicative local forecast
- AS3935 lightning/noise/disturber events, distance and energy

### Davis pressure architecture

For the **Vantage Pro2 Sensor Suite 6322/6322M**, barometric pressure is not an outdoor ISS sensor. Davis places the barometer on the receiving side; this gateway follows the same separation with a local BME280. The enhanced BME manager supports `0x76/0x77`, non-blocking rediscovery, absolute/sea-level pressure, trend estimation and I2C diagnostics.

## Davis EU RF

The receiver uses 2-FSK at 19.2 kbps, 4.8 kHz deviation, Gaussian BT=0.5 shaping, `CB 89` sync and a fixed 10-byte frame with Davis CRC validation in software.

Implemented EU hop set:

1. 868.066711 MHz
2. 868.297119 MHz
3. 868.527466 MHz
4. 868.181885 MHz
5. 868.412292 MHz

There is **no** `oregon_receiver`, Oregon Scientific decoder, Technoline/LaCrosse decoder or 433 MHz RF operating mode in this project.

## Web UI

The `0.3.0-dev` dashboard adopts the dark/tabbed design language of the 6.4 reference project, adapted specifically to Davis data. Pages include:

- **Dashboard**: live weather, barometer, local forecast and lightning
- **Hardware**: Davis RF/FHSS, BME280/I2C, AS3935, network and MQTT
- **Configuration**: gateway/Davis, HTTP, MQTT and AS3935
- **Diagnostics**: live JSON views of the subsystems

Quick status chips show **NET / RF / BME / MQTT / AS3935**.

Main APIs:

- `/api/state` and `/api/status` - aggregate state
- `/api/bme` - BME280 and pressure trend
- `/api/i2c/scan` - on-demand I2C scan
- `/api/as3935/state` and `/api/as3935/config`
- `/api/mqtt/status` and `/api/mqtt/config`
- `/api/config` - gateway configuration
- `/api/meteobridge` - generated record preview

The Web UI is intended for a **trusted LAN** and should not be exposed directly to the public Internet without a suitable authentication/reverse-proxy access-control layer.

## AS3935 lightning detector

AS3935 support is optional and configurable through Web UI/NVS: indoor/outdoor mode, I2C address, IRQ GPIO, noise floor, watchdog threshold, spike rejection, minimum strikes, disturber masking, tuning capacitor and auto tuning.

For **T3 V1.6.1**, the default is I2C address `0x03` with IRQ on **GPIO34**. For **T3-S3**, AS3935 is deliberately **disabled by default** until a free IRQ GPIO is selected and validated for the specific hardware revision.

See [docs/AS3935.md](docs/AS3935.md).

## MQTT

MQTT is **disabled by default** and fully configurable from the Web UI. It supports:

- broker/port
- username/password
- client ID and base topic
- publish interval
- plain MQTT
- TLS with a configured CA
- insecure TLS only as an explicit installer choice

Stored MQTT passwords and CA data are not returned to the browser. Topics are published for weather, BME, Davis RF, system and AS3935, plus a JSON `state` snapshot.

See [docs/MQTT.md](docs/MQTT.md).

## First boot and Wi-Fi

Wi-Fi credentials are not embedded in public source. When no valid configuration exists, the gateway creates the temporary AP:

```text
DavisGateway-XXXX
```

Captive portal: `http://192.168.4.1`.

DHCP is the default. `192.168.1.120` is only a suggested static address. After 60 seconds without connectivity, the recovery portal is enabled. Runtime settings are stored in NVS and stored passwords are never rendered back to the browser.

## HTTP / mb.php

Public firmware contains no installation-specific endpoint. Each installation configures its own receiver URL, for example:

```text
https://server.example/weather/mb.php
```

The gateway appends `?d=<URL-encoded Meteobridge/Weather34-compatible record>`. HTTPS verifies certificates by default; insecure mode requires explicit opt-in.

## PlatformIO build

```bash
pio run -e t3-v161-868
pio run -e t3-s3-868
```

The `0.3.0-dev` integration with Web/MQTT/BME/AS3935 builds successfully in CI for both supported boards. Main dependencies are pinned in `platformio.ini`.

## Documentation

- [Application architecture 0.3](docs/ARCHITECTURE_0.3.md)
- [MQTT](docs/MQTT.md)
- [AS3935 Lightning](docs/AS3935.md)
- [RF protocol notes - Italian](docs/RF_PROTOCOL_IT.md)
- [RF protocol notes - English](docs/RF_PROTOCOL_EN.md)
- [Davis RF guide Italian PDF v1.1](docs/Davis_RF_Protocol_Guide_IT_v1.1.pdf)
- [Davis RF guide English PDF v1.1](docs/Davis_RF_Protocol_Guide_EN_v1.1.pdf)

RF protocol documentation is independent interoperability/reverse-engineering documentation and is not an official Davis Instruments specification.

## Security

See [SECURITY.md](SECURITY.md). Public source must never contain passwords, tokens, private keys, private CA material or installation-specific private endpoints. MQTT/HTTP TLS paths are secure-by-default where applicable; insecure modes require explicit configuration.

## Branches and contributions

See [CONTRIBUTING.md](CONTRIBUTING.md). Regular development targets `develop`; promotion to `main` happens only through Pull Request with successful CI.

## License

Project-original source code and documentation are distributed under **GNU Lesser General Public License v3.0 only (`LGPL-3.0-only`)**, unless a file states otherwise. Dependencies retain their own licenses.

See [LICENSE](LICENSE), [COPYING](COPYING), and [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
