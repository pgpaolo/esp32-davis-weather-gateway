# Application Architecture 0.3.2

Firmware reference: **`0.3.2-dev`**.

## Goal

The 0.3 application line aligns the Davis gateway application layer with the richer usability and diagnostics model previously used by the maintainer's Oregon/Technoline gateway while preserving an explicit RF boundary:

> **Davis Vantage Pro2 EU 868 MHz FHSS is the only weather radio protocol implemented by this repository.**

No Oregon Scientific, Technoline or LaCrosse receive/decoder module is part of the Davis firmware.

## Service order

The primary loop is intentionally ordered as follows:

```text
1. Davis RF service
2. BME280 pressure service
3. rain persistence
4. AS3935 lightning service
5. network service
6. Web UI / HTTP API
7. MQTT publisher
8. HTTP weather upload
9. OLED refresh
```

Davis RF remains first to minimize receive/hop latency. OLED and application services run only after the radio path has been serviced.

## Components

### `davis_radio.*` / `davis_decoder.*`

Authoritative weather RF layer. Responsibilities include:

- SX1276 FSK configuration;
- European five-frequency hop set;
- packet timing/synchronization;
- Davis frame normalization and CRC;
- station ID filtering/auto-lock;
- weather packet decoding;
- observational diagnostics for raw packets, per-channel quality, timing and receive-path counters.

The 0.3.2 diagnostic additions are observational: the decoder, CRC algorithm, hop frequencies and acquisition strategy are not changed by the diagnostic layer.

This module was not replaced by Oregon/Technoline decoder code.

### RF diagnostic telemetry

The radio layer maintains RAM-only diagnostic information including:

- acquisition phase `ERROR / SEARCH / CANDIDATE / SYNC`;
- per-channel RAW/CRC/RSSI statistics for all five EU frequencies;
- RAW and bit-normalized bytes;
- received/calculated CRC;
- a 24-frame recent-history ring buffer;
- packet interval and jitter statistics;
- DIO0 IRQ, `readData()`, tune, hop, miss-streak and RadioLib error counters.

The diagnostic reset endpoint clears this observation window only; it does not change user configuration, Davis ID, decoder state or NVS settings.

### `display_manager.*`

SSD1306 128x64 OLED application layer. It provides:

- boot progress;
- `DAVIS SEARCH` live acquisition state;
- `DAVIS RX RAW` raw-frame visualization while no stable lock exists;
- rotating weather, wind/rain, barometer, RF, AS3935 and system pages after lock;
- automatic return to acquisition diagnostics when Davis traffic becomes stale.

### `pressure_manager.*`

Local receiver-side BME280 manager. Provides:

- `0x76` / `0x77` detection;
- 100 kHz shared I2C setup;
- non-blocking rediscovery after sensor absence/failure;
- absolute station pressure;
- configured-altitude sea-level pressure;
- approximate normalized three-hour pressure trend;
- simple local pressure/trend forecast indication;
- BME and I2C diagnostic JSON.

For the Vantage Pro2 6322/6322M architecture, barometric pressure is receiver-side rather than transmitted by the outdoor ISS; the local BME280 follows that design pattern.

### `lightning_manager.*`

Optional AS3935 subsystem. Uses I2C plus an IRQ GPIO, stores runtime settings in NVS, handles detector events and forwards state/events to MQTT when available.

### `mqtt_publisher.*`

Optional telemetry output. It has no control path into the Davis radio engine. Runtime configuration is isolated in NVS namespace `mqttcfg` and supports plain MQTT, CA-verified TLS and explicit insecure TLS.

### `web_manager.*`

Single-page dark dashboard with Dashboard, Hardware, Configuration and Diagnostics views. The browser polls JSON APIs rather than forcing full-page refreshes.

In 0.3.2 the diagnostics view exposes:

- live RF state;
- five channel cards;
- RAW/NORMALIZED/CRC comparison;
- recent-frame history;
- timing/jitter;
- radio-path counters;
- ESP32 system health;
- a 60-second capture workflow;
- downloadable `davis-diagnostic.txt` report.

### `network_manager.*`

Retains captive provisioning and recovery behaviour. Wi-Fi credentials remain stored in NVS and are not rendered back to the browser.

A network reset clears the saved Wi-Fi/network profile and returns the device to provisioning after restart. It is not a full NVS/factory erase.

### `meteobridge_client.*`

Optional generic HTTP receiver output remains independent of MQTT. The public firmware has no installation-specific URL.

## Shared I2C bus

OLED, BME280 and AS3935 share the configured board I2C pins. The bus is kept at 100 kHz. `/api/i2c/scan` is an operator-triggered diagnostic and should not be polled continuously.

## NVS namespaces

| Namespace | Purpose |
|---|---|
| `davisgw` | network, HTTP, Davis ID/rain tip, altitude, timezone |
| rain persistence namespace | persistent rain state |
| `mqttcfg` | MQTT configuration and credentials |
| `as3935cfg` | AS3935 runtime configuration |

The browser never receives stored Wi-Fi/MQTT password values.

## Reset levels

The project currently distinguishes several resets:

- **RF diagnostic reset**: clears only live RF counters/history;
- **network reset**: removes the Wi-Fi/network configuration and re-enters provisioning;
- **MQTT reset**: clears MQTT NVS settings;
- **AS3935 reset**: clears lightning-detector runtime settings;
- **full factory/flash reset**: currently performed with `esptool erase_flash` followed by firmware reflash when a complete NVS/firmware reset is required.

A dedicated one-click full factory-reset API is not currently part of 0.3.2-dev.

## Security boundary

The Web UI is currently a trusted-LAN management interface. TLS validation is secure-by-default for HTTP receiver configuration and CA-verified MQTT is available. This does not turn the management UI itself into an Internet-facing administrative console; external exposure should use an authenticated reverse proxy/VPN or equivalent access-control layer.

## Licensing and provenance boundary

Project-original code/documentation are distributed under `LGPL-3.0-only` unless otherwise stated. Attribution and provenance are documented in `NOTICE.md`, `THIRD_PARTY_NOTICES.md` and `docs/LICENSING_IT.md` / `docs/LICENSING_EN.md`.

The application design may reuse or reimplement project-original concepts from the maintainer's Oregon/Technoline gateway, but its Oregon/Technoline decoder code and unrelated third-party GPL-derived decoder portions are intentionally outside the Davis repository.

## Hardware validation still required

CI proves source compatibility/buildability, not RF or sensor physics. Field validation must cover:

- Davis FHSS acquisition/reacquisition against a real ISS;
- decoded sensor values and rain counters;
- five-channel distribution, CRC and timing evidence from real captures;
- BME280 pressure accuracy and enclosure thermal effects;
- AS3935 IRQ pin and tuning on actual hardware;
- MQTT reconnect behaviour on the target network;
- RF packet loss with all auxiliary wiring installed.
