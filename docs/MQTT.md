# MQTT - ESP32 Davis Weather Gateway

This document describes the MQTT service introduced in firmware `0.3.0-dev`.

MQTT is an **application-layer output only**. It does not participate in Davis RF reception and cannot change the Davis frequency set, FHSS state machine or packet decoder.

## Default state

MQTT is disabled by default. Configuration is stored in ESP32 NVS under the `mqttcfg` namespace.

Default logical values:

| Setting | Default |
|---|---|
| enabled | false |
| broker | empty |
| port | 1883 |
| client ID | `DavisGateway` + unique MAC suffix at connect time |
| base topic | `davis-gateway` |
| TLS | off |
| publish interval | 10000 ms |

The Web UI never returns the stored MQTT password or CA certificate content. It only reports whether these values are present.

## TLS modes

`0` - **Off**: plain MQTT, normally port 1883.

`1` - **CA verified**: `WiFiClientSecure` validates the broker certificate against the PEM CA configured by the user. Enabling this mode without a CA is rejected.

`2` - **Insecure TLS**: TLS encryption without certificate validation. This is an explicit compatibility mode and should not be used when a trusted CA can be supplied.

## Connection behaviour

- reconnect attempt approximately every 10 seconds while enabled and disconnected;
- MQTT keepalive: 30 seconds;
- Last Will topic: `<base>/status` with retained payload `offline`;
- successful connection publishes retained `online`;
- the Davis RF service remains first in the firmware loop before MQTT processing.

## Topics

The following topics are relative to the configured base topic.

### Status / system

```text
status
system/ip
system/uptime_s
state
```

`state` is a compact JSON snapshot of the principal weather/RF values.

### Davis weather

```text
weather/temperature_c
weather/humidity_pct
weather/dewpoint_c
weather/wind_kmh
weather/gust_kmh
weather/wind_direction_deg
weather/wind_chill_c
weather/rain_rate_mm_h
weather/rain_day_mm
weather/rain_month_mm
weather/rain_year_mm
weather/uv
weather/solar_wm2
```

Unavailable values are not published as scalar topics until the firmware has a valid measurement.

### Receiver-side BME280

```text
bme/pressure_sea_hpa
bme/pressure_absolute_hpa
bme/temperature_c
bme/humidity_pct
bme/trend_hpa_3h
bme/forecast
```

The pressure sensor is local to the gateway, following the Davis receiver-side barometer architecture for the Vantage Pro2 6322/6322M reference configuration.

### Davis RF diagnostics

```text
rf/protocol
rf/rssi_dbm
rf/synchronized
rf/channel
rf/frequency_mhz
rf/station_id
rf/battery_low
rf/packets_ok
rf/crc_errors
rf/missed
rf/resyncs
```

`rf/protocol` is published as:

```text
Davis-Vantage-Pro2-EU-868-FHSS
```

No Oregon Scientific or Technoline RF topic/decoder is used by this repository.

### AS3935

```text
as3935/state
as3935/event
```

`as3935/state` is retained periodic JSON state. `as3935/event` is a non-retained JSON event emitted when the AS3935 IRQ is processed.

## Web/API configuration

GET:

```text
/api/mqtt/config
/api/mqtt/status
```

POST:

```text
/api/mqtt/config
/api/mqtt/reset
```

The browser submits a password or CA only when the operator explicitly replaces it. Empty password/CA controls preserve the stored value.

## Security notes

- keep MQTT disabled when unused;
- prefer CA-verified TLS for networks outside a trusted isolated LAN;
- do not commit broker credentials or private CA material to the repository;
- the current Web UI is intended for a trusted LAN and does not replace a network access-control boundary.
