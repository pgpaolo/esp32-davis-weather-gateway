# ESP32 Davis Weather Gateway - Documentation Index

Current firmware baseline on `develop`: **`0.4.0-dev`**.

This documentation describes ESP32 Davis Weather Gateway as a standalone Davis Vantage Pro2 / Pro2 Plus EU 868 MHz project.

## Current documentation

### Architecture and operation

- [`ARCHITECTURE_0.4.md`](ARCHITECTURE_0.4.md) - current service architecture, RF isolation, SPI/I²C layout, storage and remote-ready design.
- [`OLED_DISPLAY.md`](OLED_DISPLAY.md) - SSD1306 boot/runtime display and Davis acquisition diagnostics.
- [`MQTT.md`](MQTT.md) - MQTT configuration and telemetry.
- [`AS3935.md`](AS3935.md) - optional lightning detector.
- [`SD_DATALOGGER.md`](SD_DATALOGGER.md) - microSD/SdFat storage, CSV format, retry and API.
- [`REMOTE_ACCESS.md`](REMOTE_ACCESS.md) - outbound TLS relay security model and current readiness state.

### Davis RF and diagnostics

- [`RF_PROTOCOL_IT.md`](RF_PROTOCOL_IT.md) - Davis RF protocol notes in Italian.
- [`RF_PROTOCOL_EN.md`](RF_PROTOCOL_EN.md) - Davis RF protocol notes in English.
- [`DIAGNOSTICS_IT.md`](DIAGNOSTICS_IT.md) - extended five-channel diagnostics, RAW/NORMALIZED/CRC, timing and capture workflow.
- [`DIAGNOSTICS_EN.md`](DIAGNOSTICS_EN.md) - English diagnostics guide.

The current real-hardware setup successfully receives Davis station data. Diagnostic tools remain available for RF quality and long-term monitoring.

### Licensing and governance

- [`LICENSING_IT.md`](LICENSING_IT.md) - Italian licensing/attribution guide.
- [`LICENSING_EN.md`](LICENSING_EN.md) - English licensing/attribution guide.
- [`BRANCH_POLICY.md`](BRANCH_POLICY.md) - protected-main development model.
- [`../NOTICE.md`](../NOTICE.md) - project copyright and trademark disclaimer.
- [`../THIRD_PARTY_NOTICES.md`](../THIRD_PARTY_NOTICES.md) - software dependencies and Davis interoperability references.
- [`../SECURITY.md`](../SECURITY.md) - security policy.

## Historical documentation

`ARCHITECTURE_0.3.md` and the RF PDF guides v1.0/v1.1 document earlier development stages and are kept for project history.

The root graphical Technical Guide v1.2 predates the 0.4 microSD and remote-ready application layer. For the current firmware, the Markdown documents indexed here are authoritative until a newer unified graphical guide is released.

## Information classification

Documentation distinguishes:

- vendor-published Davis data;
- behaviour implemented by this project;
- public interoperability/reverse-engineering information;
- hardware-dependent or still-to-be-validated behaviour.

## License

Project-original documentation is licensed under `LGPL-3.0-only` unless a file states otherwise. Third-party specifications, software and trademarks remain under their respective ownership/licenses.
