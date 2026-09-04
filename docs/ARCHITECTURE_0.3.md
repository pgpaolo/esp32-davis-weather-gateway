# Application Architecture 0.3

## Goal

Firmware `0.3.0-dev` aligns the Davis gateway application layer with the richer usability and diagnostics model used by the Oregon/Technoline 6.4 reference project while preserving an explicit RF boundary:

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
6. Web UI
7. MQTT publisher
8. HTTP weather upload
```

Davis RF is first to minimize receive/hop latency. Auxiliary services are non-blocking or rate-limited where practical.

## Components

### `davis_radio.*` / `davis_decoder.*`

Authoritative weather RF layer. Responsibilities include:

- SX1276 FSK configuration;
- European five-frequency hop set;
- packet timing/synchronization;
- Davis frame normalization and CRC;
- station ID filtering/auto-lock;
- weather packet decoding.

This module was not replaced by code from the Oregon/Technoline reference project.

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

### `lightning_manager.*`

Optional AS3935 subsystem. Uses I2C plus an IRQ GPIO, stores runtime settings in NVS, handles detector events and forwards state/events to MQTT when available.

### `mqtt_publisher.*`

Optional telemetry output. It has no control path into the Davis radio engine. Runtime configuration is isolated in NVS namespace `mqttcfg` and supports plain MQTT, CA-verified TLS and explicit insecure TLS.

### `web_manager.*`

Single-page dark dashboard with Dashboard, Hardware, Configuration and Diagnostics views. The browser polls JSON APIs rather than forcing full-page refreshes.

### `network_manager.*`

Retains captive provisioning and recovery behaviour. Wi-Fi credentials remain stored in NVS and are not rendered back to the browser.

### `meteobridge_client.*`

Optional generic HTTP receiver output remains independent of MQTT. The public firmware has no installation-specific URL.

## Shared I2C bus

BME280 and AS3935 share the configured board I2C pins. The bus is kept at 100 kHz. `/api/i2c/scan` is an operator-triggered diagnostic and should not be polled continuously.

## NVS namespaces

| Namespace | Purpose |
|---|---|
| `davisgw` | network, HTTP, Davis ID/rain tip, altitude, timezone |
| `davisstate` / rain persistence namespace | persistent rain state |
| `mqttcfg` | MQTT configuration and credentials |
| `as3935cfg` | AS3935 runtime configuration |

The browser never receives stored Wi-Fi/MQTT password values.

## Security boundary

The Web UI is currently a trusted-LAN management interface. TLS validation is secure-by-default for HTTP receiver configuration and CA-verified MQTT is available. This does not turn the management UI itself into an Internet-facing administrative console; external exposure should use an authenticated reverse proxy/VPN or equivalent access-control layer.

## Hardware validation still required

CI proves source compatibility/buildability, not RF or sensor physics. Field validation must cover:

- Davis FHSS acquisition/reacquisition against a real ISS;
- decoded sensor values and rain counters;
- BME280 pressure accuracy and enclosure thermal effects;
- AS3935 IRQ pin and tuning on actual hardware;
- MQTT reconnect behaviour on the target network;
- RF packet loss with all auxiliary wiring installed.
