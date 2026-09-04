# Extended Davis RF / FHSS diagnostics

Reference firmware: **0.3.2-dev**.

This diagnostic layer is intended for real-hardware validation of a Davis Vantage Pro2 EU 868 MHz receiver. It does not change the decoder, frequencies, CRC algorithm or hopping strategy; it only records and exposes what the receiver is already doing.

## RF states

- **ERROR**: SX1276 initialization failed.
- **SEARCH**: radio is ready but no 10-byte candidate frame has been received.
- **CANDIDATE**: at least one candidate frame has been received, but no stable Davis lock exists yet.
- **SYNC**: valid Davis frames are being received with FHSS synchronization.

## Per-channel diagnostics

For each of the five Davis EU channels the firmware keeps:

- frequency;
- RAW candidate count;
- CRC-valid count;
- CRC-error count;
- RadioLib read errors;
- last / average / minimum / maximum RSSI;
- age of the most recent reception.

Implemented hop set:

1. 868.066711 MHz
2. 868.297119 MHz
3. 868.527466 MHz
4. 868.181885 MHz
5. 868.412292 MHz

## RAW, normalized bytes and CRC

For every candidate frame the gateway stores both the 10 bytes returned by the SX1276 and the 10 bytes after Davis bit reversal.

The diagnostic page exposes:

- `RAW` bytes;
- `NORMALIZED` bytes;
- received CRC;
- calculated CRC16-CCITT over bytes 0..5;
- CRC OK / KO result.

This makes it possible to determine whether a mismatch occurs before or after Davis bit normalization.

## Recent-frame ring buffer

The gateway keeps the latest **24 candidate frames** in RAM. Each entry contains sequence number, age, channel/frequency, RSSI, received/calculated CRC, RAW bytes, normalized bytes and RadioLib result.

The ring buffer is RAM-only and does not continuously write to flash.

## Packet timing and jitter

The firmware calculates expected Davis interval (2555 ms), latest interval, minimum, maximum, average and mean absolute jitter against the expected interval.

Large timing deviations may indicate missing hops, intermittent reception or an incorrect framing assumption.

## SX1276 radio path

The Web UI also exposes DIO0 IRQ count, successful/failed `readData()` operations, tune count/errors, hop count, `startReceive()` errors, current miss streak and the latest RadioLib error code.

## System health

The diagnostics page includes uptime, free heap, minimum free heap, CPU frequency, Wi-Fi RSSI and ESP32 reset reason. The downloadable report additionally includes BME280, AS3935, MQTT and HTTP upload state.

## 60-second diagnostic capture

**Diagnostic capture 60 s** resets the RF diagnostic window and collects one minute of traffic. With a nominal Davis interval around 2.555 seconds, the 24-frame ring buffer covers approximately one minute.

After the capture, download `davis-diagnostic.txt` and attach it to a test report or issue.

## Quick interpretation

### RAW = 0

No candidate frame is reaching the packet path. Check ISS power/transmission, 868 MHz hardware, antenna, SX1276 pinout, distance/shielding and radio initialization.

### RAW increases, CRC always KO

RF candidates are reaching the receiver. Compare RAW versus normalized bytes, received/calculated CRC, RSSI, timing and per-channel distribution before changing the decoder.

### CRC OK but no SYNC

Check the configured ISS ID, hopping continuity, miss streak and packet timing.

### Stable SYNC

The RF path is substantially operational. Validation can then focus on weather-field conversions and long-term continuity.

## Reset levels and return to initial state

Version 0.3.2-dev intentionally separates different reset scopes.

### RF diagnostic reset

`POST /api/rf/reset` or **Diagnostic capture 60 s** clears only the observational RF window: diagnostic counters, timing data and recent-frame history.

It does not erase Wi-Fi, ISS ID, BME280, MQTT, AS3935, HTTP endpoint or persistent rain totals.

### Network reset

The Web **Reset network** action removes the stored Wi-Fi/network profile and restarts the gateway. On the next boot the device returns to provisioning with SSID `DavisGateway-XXXX` and portal `http://192.168.4.1`.

Holding the BOOT button forces provisioning/recovery but is not a full flash erase.

### MQTT / AS3935 reset

Their dedicated reset actions clear only the corresponding NVS configuration.

### Complete flash reset

Version 0.3.2-dev does not yet provide a Web factory-reset button that erases all persistent state. To return the board to a completely blank flash, erase it with `esptool` and then flash the firmware again:

```text
python -m esptool --chip esp32 --port COMx erase_flash
pio run -e t3-v161-868 -t upload
```

Use the corresponding PlatformIO environment for T3-S3. Replace `COMx` with the actual serial port.

A full flash erase removes firmware, NVS, Wi-Fi, MQTT, AS3935, Davis/HTTP configuration and persistent state.

## API

- `/api/rf`
- `/api/rf/diagnostics`
- `POST /api/rf/reset`
- `/api/diag/report`
- `/api/system`
- `/api/state`

## Licensing and provenance

The diagnostic layer is project-original material distributed under `LGPL-3.0-only` unless a file states otherwise. See `NOTICE.md`, `THIRD_PARTY_NOTICES.md` and `docs/LICENSING_EN.md`.

The weather RF engine remains Davis Vantage Pro2 EU 868 MHz FHSS / 2-FSK only. Extended diagnostics do not add Oregon Scientific, Technoline/LaCrosse or 433 MHz weather decoding.
