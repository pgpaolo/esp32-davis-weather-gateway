# Davis Vantage Pro2 / Pro2 Plus EU RF protocol notes

Edition 1.1 - 31 August 2026

This document summarizes the RF behaviour implemented by ESP32 Davis Weather Gateway. It is an independent interoperability/reverse-engineering note and **not an official Davis Instruments specification**. Several conversions still require validation on real hardware.

## Architecture

```text
Davis ISS EU
   | 868 MHz / 2-FSK / FHSS
   v
SX1276 / RFM95
   | SPI
   v
ESP32 / LILYGO
   |-- Davis decoder
   |-- local BME280 pressure
   |-- Web UI / diagnostics
   `-- optional HTTP upload
```

The project is Davis-only; Oregon Scientific and Technoline decoders are not part of this variant.

## RF profile

| Parameter | Implemented value |
|---|---|
| Band | 868 MHz EU |
| Modulation | 2-FSK |
| Bit rate | 19.2 kbps |
| RX deviation | 4.8 kHz |
| Shaping | Gaussian BT=0.5 |
| RX bandwidth | 25 kHz |
| AFC bandwidth | 50 kHz |
| Preamble | 4 x `0xAA` |
| Sync | `0xCB 0x89` |
| Payload | fixed 10 bytes |
| CRC | CRC16-CCITT in firmware |

The SX1276 hardware CRC is disabled because the Davis frame carries its own CRC.

## European hop set

| Hop | FRF | Frequency |
|---:|---|---:|
| 1 | `D9 04 45` | 868.066711 MHz |
| 2 | `D9 13 04` | 868.297119 MHz |
| 3 | `D9 21 C2` | 868.527466 MHz |
| 4 | `D9 0B A4` | 868.181885 MHz |
| 5 | `D9 1A 63` | 868.412292 MHz |

The firmware uses an approximately 2555 ms nominal packet interval. After a Davis-shaped packet it advances to the next hop; after prolonged misses it returns to acquisition. This synchronization algorithm is a gateway implementation choice, not a guaranteed reproduction of Davis console internals.

## 10-byte frame

RF bytes are normalized by bit reversal before field/CRC interpretation.

| Byte | Current gateway use |
|---:|---|
| 0 | packet type + battery flag + transmitter ID |
| 1 | wind speed |
| 2 | wind direction |
| 3 | type-specific payload |
| 4 | type-specific payload |
| 5 | data/reserved, included in primary CRC |
| 6 | CRC MSB |
| 7 | CRC LSB |
| 8 | additional/retransmit field, not validated in current alpha |
| 9 | additional/retransmit field, not validated in current alpha |

Byte 0 is treated as `TTTT B III`: high nibble packet type, bit 3 battery flag, bits 2..0 transmitter ID 0..7. The UI displays IDs 1..8 and supports `0` as auto-lock.

## Packet types currently decoded

| Type | Name | Measurement |
|---|---|---|
| `0x4` | UV | UV index |
| `0x5` | RAIN_RATE | seconds since last rain tip / rain rate |
| `0x6` | SOLAR | solar radiation |
| `0x8` | TEMP | outside temperature |
| `0x9` | WIND_GUST | 10-minute gust |
| `0xA` | HUMIDITY | outside humidity |
| `0xE` | RAIN | rain tip counter |

Other valid packet types may be logged as `OTHER` without updating a sensor value.

## CRC16-CCITT

The gateway calculates CRC over normalized bytes 0..5 with polynomial `0x1021` and initial value `0`, then compares the result with bytes 6..7 interpreted MSB first.

## Current field conversions

Wind speed:

```text
wind_kmh = byte1 * 1.609344
```

Direction:

```text
wind_dir_deg = 9.0 + byte2 * (342.0 / 255.0)
```

This direction mapping must be checked against a real ISS, especially around the 0/360 degree wrap.

Temperature (`0x8`):

```text
raw = ((byte3 << 8) | byte4) >> 4
temp_f = raw / 10.0
temp_c = (temp_f - 32.0) * 5.0 / 9.0
```

Humidity (`0xA`):

```text
raw = ((byte4 >> 4) << 8) | byte3
humidity_pct = raw / 10.0
```

Gust (`0x9`):

```text
gust_kmh = byte3 * 1.609344
```

Solar radiation (`0x6`):

```text
raw10 = byte3 * 4 + (byte4 >> 6)
solar_wm2 = raw10
```

UV (`0x4`):

```text
raw = ((byte3 << 8) | byte4) >> 4
uv = (raw - 4.0) / 200.0
```

The UV conversion explicitly requires field comparison against a Davis reference.

Rain counter (`0xE`) uses the low 7 bits of byte 3 as a modulo-128 counter. The configurable rain-tip size converts counter deltas into millimetres. Large anomalous deltas are rejected.

Rain rate (`0x5`) reconstructs seconds since the last tip from byte 3 and the high nibble of byte 4, then calculates:

```text
rain_rate_mm_h = rain_mm_per_tip * 3600 / seconds_since_tip
```

## Atmospheric pressure and Davis architecture

For the **Wireless Vantage Pro2 Sensor Suite 6322/6322M** used as this project's reference, barometric pressure is not one of the outdoor ISS sensors. Davis lists outside temperature and humidity, wind speed and direction, and rainfall for the sensor suite; UV and solar radiation are provided by Pro2 Plus configurations or additional sensors.

In the Davis ecosystem the barometer is located on the receiving side. **WeatherLink Live** explicitly includes integrated barometric pressure, inside temperature and inside humidity sensors; **Weather Envoy** likewise includes a barometer and inside temperature/humidity. Therefore, for the 6322/6322M ISS, the gateway should not expect a pressure packet type over the ISS RF link.

ESP32 Davis Weather Gateway follows the same functional separation by using a **local BME280** on the gateway. The measured station pressure is reduced to sea level using the configured installation altitude:

```text
P0 = Pstation / (1 - altitude_m / 44330) ^ 5.255
```

The BME280 should be mounted so that it is exposed to ambient pressure. For meaningful local temperature readings it should also be kept away from the ESP32 board's main heat sources.

## FHSS synchronization diagnostics

During real-hardware validation, monitor at least:

- `packetsOk`
- `crcErrors`
- `packetsMissed`
- `resyncs`
- RSSI
- current channel/frequency
- `lastRadioError`

A stable-looking lock with many missed slots can indicate timing, RF offset, antenna, or hop-sequence problems.

## Persistence and provisioning

Rain totals and the RF rain counter are persisted in NVS. Wi-Fi credentials are also stored in NVS and are not compiled into public source. First boot uses the temporary `DavisGateway-XXXX` AP at `192.168.4.1`; DHCP is the default LAN mode, while `192.168.1.120` is only the suggested static address.

## Known limits of Edition 1.1

1. The guide describes the `0.2.0-dev` decoder, not a stable release.
2. Unlisted packet types are not decoded.
3. Bytes 8-9 are not validated in the current alpha.
4. UV, direction and some rain-rate details require field verification.
5. SX1276 synchronization is project logic, not an official Davis algorithm.
6. Pressure is intentionally local to the gateway and is not derived from the 6322/6322M ISS RF frame.

## Public technical references

Official Davis references used to distinguish ISS sensors from receiver-side sensors:

- https://www.davisinstruments.com/products/wireless-vantage-pro2-integrated-sensor-suite
- https://www.davisinstruments.com/products/weatherlink-live
- https://www.davisinstruments.com/products/cabled-weather-envoy-3

See also [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md) for attribution and licensing notes regarding historical public Davis ISS reverse-engineering projects.
