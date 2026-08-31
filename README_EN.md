# ESP32 Davis Weather Gateway

[Italiano](README.md) | **English**

[![Build firmware](https://github.com/pgpaolo/esp32-davis-weather-gateway/actions/workflows/build.yml/badge.svg)](https://github.com/pgpaolo/esp32-davis-weather-gateway/actions/workflows/build.yml)
![License: LGPL v3](https://img.shields.io/badge/License-LGPL_v3-blue.svg)

An independent **ESP32/LILYGO 868 MHz** gateway designed to receive a European **Davis Vantage Pro2 / Pro2 Plus wireless ISS** directly, without a Davis console and without a Meteobridge appliance.

The firmware receives Davis ISS traffic over **868 MHz FHSS**, decodes weather data, adds atmospheric pressure from a local **BME280**, exposes a web configuration/diagnostics interface, and can upload a Meteobridge/Weather34-compatible record to a user-defined HTTP endpoint.

> `develop` is the active integration branch. `main` is intended to contain reviewed, CI-validated project states.

## Status

Current development version: `0.2.0-dev`.

The firmware builds successfully in CI for the supported LILYGO targets. RF decoding is still subject to field validation against real Davis hardware; protocol details that come from public reverse engineering are documented as such.

## Supported hardware

- LILYGO / TTGO LoRa32 T3 V1.6.1 with **SX1276/RFM95 868 MHz**
- optional LILYGO T3-S3 + SX1276 868 MHz profile
- Davis Vantage Pro2 / Pro2 Plus wireless EU ISS
- BME280 on I2C for atmospheric pressure and local temperature/humidity

A 433 MHz-only radio board is not suitable. Use hardware designed for the European 868 MHz band.

## Weather data

From the Davis ISS:

- outside temperature
- outside humidity
- wind speed and direction
- 10-minute gust
- rain counter and rain rate
- UV index
- solar radiation
- transmitter battery flag

From the local gateway:

- BME280 atmospheric pressure reduced to sea level
- local BME280 temperature/humidity

## Davis EU RF profile

The receiver is configured for 2-FSK at 19.2 kbps, 4.8 kHz deviation, Gaussian BT=0.5 shaping, sync word `CB 89`, and a fixed 10-byte payload with software-validated Davis CRC16-CCITT.

Implemented EU hop set:

1. 868.066711 MHz
2. 868.297119 MHz
3. 868.527466 MHz
4. 868.181885 MHz
5. 868.412292 MHz

The current SX1276 strategy follows the hop sequence using an approximately 2.555 s nominal packet interval and falls back to acquisition when synchronization is lost.

## First boot and Wi-Fi provisioning

Wi-Fi credentials are **not embedded in the public source code**.

When no valid configuration exists, the gateway creates a temporary access point:

```text
DavisGateway-XXXX
```

The setup AP is open by default because it is temporary. It can be protected by defining `PROVISION_AP_PASSWORD` in a private configuration file.

Captive portal:

```text
http://192.168.4.1
```

The portal configures SSID, password, hostname and LAN addressing. **DHCP is the default**. If static addressing is selected, the UI proposes:

```text
IP       192.168.1.120
Gateway  192.168.1.1
Netmask  255.255.255.0
DNS      192.168.1.1
```

`192.168.1.120` is only a suggested value and must be checked against the user's LAN.

If the configured Wi-Fi network cannot be reached for 60 seconds, the gateway automatically enables the recovery provisioning portal while preserving the stored configuration. Settings are stored in **NVS**.

## HTTP receiver / `mb.php`

The public firmware contains **no installation-specific endpoint**. Each user configures their own receiver URL from the web interface, for example:

```text
https://server.example/weather/mb.php
```

The gateway appends:

```text
?d=<URL-encoded Meteobridge/Weather34-compatible record>
```

An upload is considered successful only when the receiver returns HTTP `200` with body `success`.

The web UI provides receiver URL configuration, upload interval, manual upload test, generated-record preview, and TLS mode selection.

## Davis and BME280 configuration

The web UI also provides:

- Davis transmitter ID: `0` for auto-lock or `1..8` for explicit filtering
- rain tip size in millimetres
- BME280 installation altitude for sea-level pressure reduction
- POSIX timezone string

Daily/monthly/yearly rain totals and the RF rain counter are persisted in NVS to avoid incorrect resets after reboot.

## Web endpoints

When connected to the LAN:

- `/` - dashboard
- `/config` - complete configuration
- `/api/status` - JSON status
- `/api/meteobridge` - generated record preview
- manual **TEST UPLOAD** from configuration
- **RESET NETWORK / SETUP PORTAL** to clear Wi-Fi configuration only

## PlatformIO build

```bash
pio run -e t3-v161-868
pio run -e t3-s3-868
```

GitHub Actions builds both targets on `main` and `develop`.

## Protocol documentation

- [RF protocol guide - Italian PDF](docs/Davis_RF_Protocol_Guide_IT_v1.0.pdf)
- [RF protocol guide - English PDF](docs/Davis_RF_Protocol_Guide_EN_v1.0.pdf)
- [RF protocol notes - Italian](docs/RF_PROTOCOL_IT.md)
- [RF protocol notes - English](docs/RF_PROTOCOL_EN.md)

These documents are independent technical notes based on the firmware implementation and public/reverse-engineered protocol information. They are **not official Davis Instruments specifications**.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md). Development work should target `develop`; changes intended for `main` should arrive through a reviewed pull request with successful CI checks.

## License

Project-original source code and documentation are released under the **GNU Lesser General Public License v3.0 only (`LGPL-3.0-only`)**, unless a file states otherwise.

See [LICENSE](LICENSE), [COPYING](COPYING), and [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
