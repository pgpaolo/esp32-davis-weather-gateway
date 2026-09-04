# Third-Party Notices and Protocol References

ESP32 Davis Weather Gateway uses third-party libraries and public interoperability references. Those components and references remain subject to their own licenses and are **not relicensed** by this repository.

The repository license applies only to project-original material unless a file states otherwise.

## Direct software dependencies

The firmware currently depends on libraries declared in `platformio.ini`, including RadioLib, U8g2, Adafruit BME280/Unified Sensor/BusIO, PubSubClient and AS3935MI. These projects remain under their respective upstream licenses and copyright notices.

No ownership or relicensing claim is made over those dependencies.

## Public Davis protocol / reverse-engineering references

Historical public work used as technical/interoperability reference includes:

- **DavisRFM69** by DeKay — historical Davis ISS/RFM69 reverse-engineering work; its repository notices identify CC BY-SA 3.0 material.
- **ISS-MQTT-Gateway** by dcbo — ESP32/RFM69 Davis ISS receiver project; its repository metadata identifies GPL-3.0 licensing.

These projects are cited as protocol/interoperability references. This repository is intended to contain an independent ESP32/SX1276 implementation and does not intentionally vendor their source files.

Protocol parameters, observed frame layouts and interoperability facts are documented so behaviour can be tested against real hardware. Where a formula, packet interpretation or synchronization behaviour has not yet been confirmed on physical Davis hardware, the documentation marks it as reverse-engineered, inferred or requiring validation.

## Relationship with the Oregon / Technoline gateway project

The maintainer previously developed an ESP32 Oregon Scientific + Technoline weather gateway. Its application-layer concepts — Web dashboard, OLED status, MQTT integration, BME280 handling, AS3935 management and diagnostic workflows — were used as a design reference when expanding this Davis project.

For this Davis repository:

- the **weather RF engine is Davis-only**;
- no Oregon Scientific receiver/decoder is included;
- no Technoline/LaCrosse receiver/decoder is included;
- no 433 MHz weather mode is included;
- GPL-derived decoder portions associated with `rtl_433` or PracticalArduino in the other project were **not imported** into the Davis RF engine.

Where project-original application-layer work is owned by the same maintainer and was adapted or reimplemented here, it is distributed in this repository under `LGPL-3.0-only` where the maintainer has the right to do so.

This distinction mirrors the attribution practice used in the Oregon/Technoline project: provenance is preserved explicitly, while code subject to incompatible or unrelated third-party licensing is not silently absorbed into the Davis project.

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
