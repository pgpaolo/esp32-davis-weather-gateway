# Third-Party Notices and Protocol References

ESP32 Davis Weather Gateway uses third-party libraries and public interoperability references. Those components remain subject to their own licenses and are **not relicensed** by this repository.

The repository license applies only to project-original material unless a file states otherwise.

## Direct software dependencies

The firmware dependencies declared in `platformio.ini` include:

- **RadioLib** - SX1276/RFM95 radio control;
- **U8g2** - SSD1306 OLED rendering;
- **Adafruit BME280 / Unified Sensor / BusIO** - local environmental sensor support;
- **PubSubClient** - MQTT client;
- **SdFat** - microSD filesystem and SPI storage backend;
- **AS3935MI** - optional AS3935 lightning detector support;
- ESP32 / Arduino framework components supplied by the PlatformIO Espressif32 platform.

These projects remain under their respective upstream licenses and copyright notices. No ownership or relicensing claim is made over those dependencies.

## Public Davis protocol / interoperability references

Historical public work used as technical/interoperability reference includes:

- **DavisRFM69** by DeKay - historical Davis ISS/RFM69 reverse-engineering work; its repository notices identify CC BY-SA 3.0 material.
- **ISS-MQTT-Gateway** by dcbo - ESP32/RFM69 Davis ISS receiver project; its repository metadata identifies GPL-3.0 licensing.

These projects are cited as protocol/interoperability references. This repository is intended to contain an independent ESP32/SX1276 implementation and does not intentionally vendor their source files.

Protocol parameters, observed frame layouts and interoperability facts are documented so behaviour can be tested against real hardware. Where a formula or packet interpretation is not an official Davis specification, the documentation identifies it as project implementation or reverse-engineered interoperability information.

## Davis trademarks

Davis, Davis Instruments, Vantage Pro2, Vantage Vue and related product names or identifiers may be trademarks of Davis Instruments or their respective owners. Their use is descriptive and for interoperability/documentation purposes only.

This project is independent and is not affiliated with, sponsored by or endorsed by Davis Instruments.

## Redistribution

Redistributors are responsible for preserving all notices required by the licenses of the specific source files and third-party components they redistribute. See also:

- `LICENSE`
- `COPYING`
- `NOTICE.md`
- `docs/LICENSING_IT.md`
- `docs/LICENSING_EN.md`
