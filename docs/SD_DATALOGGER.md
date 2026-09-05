# microSD Datalogger

Firmware reference: **`0.4.0-dev`**.

The microSD subsystem is an optional storage layer. It does not participate in Davis packet decoding and a missing, invalid or full card must not prevent RF reception, Web access, MQTT or HTTP upload.

## Hardware

The supported LILYGO boards use the onboard microSD through a **dedicated SPI bus**, separate from the SX1276/RFM95 bus.

| Board | MOSI | MISO | SCLK | CS |
|---|---:|---:|---:|---:|
| T3 / LoRa32 V1.6.1 | GPIO15 | GPIO2 | GPIO14 | GPIO13 |
| T3-S3 V1.2/V1.3 | GPIO11 | GPIO2 | GPIO14 | GPIO13 |

`board_config.h` is the authoritative source for these pin assignments.

## Filesystem backend

The logger uses **SdFat 2.3.1** with a dedicated `SPIClass`.

Mount sequence:

1. CS is held high while the SPI pins are initialized;
2. first attempt at 4 MHz;
3. fallback attempt at 400 kHz if card initialization fails;
4. SdFat error code/data are exposed in the Web diagnostics;
5. if mount fails, the rest of the gateway continues normally.

No automatic formatting is performed.

## Fail-safe write path

Filesystem I/O is kept outside the Davis receive/decoder path.

- fixed RAM queue: 16 records;
- queue overflow increments `dropped` rather than blocking RF;
- write service interval: about 750 ms;
- maximum records per write batch: 6;
- Davis RF is serviced before SD work and immediately again after SD work;
- storage is initialized only after the Davis radio has already started.

This design reduces the risk that filesystem latency interferes with FHSS reception.

## Automatic retry

If logging is enabled but the card cannot be mounted, retries are scheduled without blocking the main loop:

- 5 seconds;
- 15 seconds;
- 60 seconds;
- then every 300 seconds.

The Web UI reports whether a retry is pending and the approximate time until the next attempt.

## Directory layout

With a valid system clock, files are created in UTC:

```text
/weather/YYYY/MM/YYYY-MM-DD.csv
```

Example:

```text
/weather/2026/09/2026-09-05.csv
```

Before NTP/time is valid, records go to:

```text
/weather/unsynced.csv
```

UTC is intentional so daily files are independent of daylight-saving transitions.

## CSV records

The common CSV header is:

```text
timestamp_utc,uptime_ms,record_type,station_id,packet_type,channel,frequency_mhz,rssi_dbm,crc_ok,crc_received,crc_calculated,temperature_c,humidity_pct,wind_kmh,gust_kmh,wind_dir_deg,rain_day_mm,rain_rate_mmh,rain_month_mm,rain_year_mm,uv,solar_wm2,pressure_abs_hpa,pressure_sl_hpa,local_temp_c,local_humidity_pct,lightning_distance_km,lightning_energy,lightning_count,battery_low,raw,normalized
```

Record types currently include:

- `davis_rf` - raw candidate frame plus normalized bytes and CRC information;
- `weather` - periodic station/environment snapshot;
- `lightning` - AS3935 lightning event summary.

Unused columns remain empty for that record type.

## Configuration

NVS namespace: `sdlog`.

Default configuration:

| Option | Default |
|---|---|
| logger enabled | false |
| log Davis RF frames | true |
| weather snapshots | true |
| BME280 fields | true |
| AS3935 fields | true |
| snapshot interval | 300 s |

Allowed snapshot interval: **30..3600 seconds**.

## Web UI and API

The Configuration page contains a dedicated microSD card with enable/logging options, mount, reset and format actions.

API:

- `GET /api/sd` - configuration + live status;
- `POST /api/sd` - save logger configuration;
- `POST /api/sd/remount` - immediate mount attempt;
- `POST /api/sd/reset` - reset logger configuration to defaults;
- `POST /api/sd/format` - explicit FAT format, requires `confirm=FORMAT`.

The format operation is destructive. The Web UI requires both a confirmation dialog and the literal word `FORMAT` before calling the endpoint.

## Status fields

The Web interface exposes:

- supported / mounted;
- time synchronized;
- retry pending / retry delay;
- card size, filesystem size and used bytes;
- mount attempts;
- queued, written and dropped records;
- write errors and queue depth;
- active SPI clock;
- SdFat error code/data;
- current output file;
- last successful write.

## Operational test

For the first hardware test:

1. insert a FAT-formatted microSD;
2. boot `0.4.0-dev` with the Davis receiver already working;
3. enable the logger from Configuration;
4. verify `SD ON` in the top status bar;
5. leave the gateway running for at least 10 minutes;
6. confirm that `written` increases while Davis RF remains synchronized;
7. inspect the daily CSV and compare timestamps, packet types and weather values with the live dashboard.

During this test watch Davis `missed`, `CRC error` and `resync` counters. SD logging should not materially worsen RF continuity.
