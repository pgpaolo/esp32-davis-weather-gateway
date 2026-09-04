# OLED display and live Davis diagnostics

Firmware reference: **`0.3.2-dev`**.

The supported LILYGO boards use the onboard **SSD1306 128x64 OLED** on the shared I2C bus at address `0x3C`. T3 V1.6.1 uses SDA GPIO21 / SCL GPIO22; the T3-S3 profile uses SDA GPIO18 / SCL GPIO17. The runtime I2C clock is intentionally kept at 100 kHz so the OLED can coexist with the BME280 and optional AS3935.

## Boot sequence

The display is brought up before the weather radio and reports the initialization sequence instead of remaining blank:

1. firmware start;
2. runtime configuration/NVS loaded;
3. network/provisioning initialized;
4. BME280 detection or non-blocking retry mode;
5. AS3935 and MQTT initialization;
6. Davis SX1276 initialization;
7. Davis ISS acquisition.

If the OLED is not detected at `0x3C`, the firmware continues normally and logs the condition on Serial.

## DAVIS SEARCH page

While the receiver has no valid Davis FHSS lock, the display stays in acquisition diagnostics. The primary page shows:

- current channel and frequency;
- configured ISS filter (`AUTO` or 1..8);
- last raw RSSI when available;
- RAW, valid and CRC-error counters;
- missed packets;
- BME280 and AS3935 state;
- Web/provisioning IP address.

The acquisition strategy itself is unchanged: while unsynchronized the Davis receiver remains on EU channel 1 / 868.066711 MHz and waits for the transmitter to revisit that channel. The OLED reports this state; it does not independently retune or scan the radio.

## DAVIS RX RAW page

Version 0.3.2 alternates the search page with a raw-reception page roughly every 2.5 seconds while no stable lock exists.

It shows:

- current FHSS channel/frequency;
- raw-frame sequence number;
- CRC OK/KO;
- RSSI;
- the complete 10-byte SX1276 frame split over two lines;
- age of the last raw frame;
- latest RadioLib result code.

This makes the OLED useful even before full Davis synchronization: `RAW = 0` indicates no packet reaches the packet path, while increasing RAW with CRC failures confirms that RF candidates are being received and should be analysed in the Web diagnostics.

## Normal page cycle

After synchronization, the OLED cycles through six pages at roughly 6.5-second intervals:

- Davis outside weather;
- wind and rain;
- BME280 barometer, trend and forecast;
- Davis RF/FHSS diagnostics;
- AS3935 lightning diagnostics;
- gateway/network/MQTT/system status.

Display refresh is throttled to about 750 ms and is serviced after the Davis RF code path so screen updates do not take priority over packet reception.

## Loss of synchronization

If the receiver loses synchronization, or no valid Davis packet has been received for about 12 seconds, the display automatically returns to **DAVIS SEARCH / DAVIS RX RAW**. Packet, CRC, missed, raw and resync information remains available for troubleshooting.

## RF initialization failure

If `initDavisRadio()` fails, the OLED shows **DAVIS RF ERROR**, the RadioLib error code, board profile and Web/network address when available. This allows basic diagnosis even without a serial terminal.

## Status letters

The header uses compact indicators:

- `W` = Wi-Fi connected;
- `M` = MQTT connected;
- `R` = Davis RF synchronized.

A dash means that subsystem is not currently ready.

## Relation with extended Web diagnostics

The OLED is intentionally concise. For per-channel RSSI, RAW/NORMALIZED comparison, calculated/received CRC, 24-frame history, packet timing/jitter and 60-second captures, use the Web **Diagnostica** page described in [`DIAGNOSTICS_IT.md`](DIAGNOSTICS_IT.md) / [`DIAGNOSTICS_EN.md`](DIAGNOSTICS_EN.md).

## Licensing and scope guarantee

The OLED manager is project-original application/diagnostic code distributed under `LGPL-3.0-only` unless a file states otherwise. It does not implement any weather RF protocol and does not change the Davis hop table, packet decoder or 2-FSK configuration.

Davis Vantage Pro2 EU 868 MHz FHSS remains the only weather RF engine in this repository. See [`LICENSING_IT.md`](LICENSING_IT.md) and [`LICENSING_EN.md`](LICENSING_EN.md) for attribution/provenance details.
