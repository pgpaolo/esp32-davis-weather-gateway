# ESP32 Davis Weather Gateway - Documentation Index

Current firmware baseline on `develop`: **`0.3.2-dev`**.

This directory contains both current project documentation and historical RF guide editions. The firmware remains under field validation against real Davis Vantage Pro2 / Pro2 Plus EU hardware.

## Current documentation for 0.3.2-dev

### Architecture and operation

- [`ARCHITECTURE_0.3.md`](ARCHITECTURE_0.3.md) - application architecture updated for 0.3.2, service order, RF boundary, NVS and reset levels.
- [`OLED_DISPLAY.md`](OLED_DISPLAY.md) - onboard SSD1306 display, boot status, DAVIS SEARCH and raw-frame display.
- [`MQTT.md`](MQTT.md) - MQTT configuration and telemetry.
- [`AS3935.md`](AS3935.md) - optional AS3935 lightning detector.

### Davis RF and diagnostics

- [`RF_PROTOCOL_IT.md`](RF_PROTOCOL_IT.md) - Davis RF protocol notes in Italian.
- [`RF_PROTOCOL_EN.md`](RF_PROTOCOL_EN.md) - Davis RF protocol notes in English.
- [`DIAGNOSTICS_IT.md`](DIAGNOSTICS_IT.md) - extended 0.3.2 diagnostics, 5-channel statistics, RAW/NORMALIZED/CRC, timing, 60-second capture and reset procedures.
- [`DIAGNOSTICS_EN.md`](DIAGNOSTICS_EN.md) - English extended diagnostics.

### Licensing, attribution and project governance

- [`LICENSING_IT.md`](LICENSING_IT.md) - Italian licensing/provenance guide.
- [`LICENSING_EN.md`](LICENSING_EN.md) - English licensing/provenance guide.
- [`BRANCH_POLICY.md`](BRANCH_POLICY.md) - `develop -> main` promotion model and protected-main policy.
- Repository root [`NOTICE.md`](../NOTICE.md) - project copyright, provenance and trademark disclaimer.
- Repository root [`THIRD_PARTY_NOTICES.md`](../THIRD_PARTY_NOTICES.md) - public protocol references and third-party attribution.
- Repository root [`LICENSE`](../LICENSE) - GNU LGPL v3.0.

## Historical PDF RF guides

The following PDFs document earlier stages of the Davis RF work and are kept for project history:

- `Davis_RF_Protocol_Guide_IT_v1.0.pdf`
- `Davis_RF_Protocol_Guide_EN_v1.0.pdf`
- `Davis_RF_Protocol_Guide_IT_v1.1.pdf`
- `Davis_RF_Protocol_Guide_EN_v1.1.pdf`

They should not be treated as the complete description of the current 0.3.2 application layer. For live diagnostics, OLED, MQTT, AS3935, reset behaviour and current licensing/provenance, use the Markdown documents listed above.

## Unified technical guide

The repository also contains source material under `tools/technical_guide_*.py` for the unified graphical technical guide. Until a newer generated edition is explicitly published and linked from this index, the current Markdown documentation is authoritative for firmware changes after the historical RF PDFs.

## Validation labels

Documentation uses the following practical distinction:

- **official/vendor data** - values explicitly published by Davis Instruments;
- **project implementation** - behaviour implemented in this repository;
- **reverse-engineered/interoperability information** - public protocol knowledge not published as an official Davis specification;
- **field validation required** - behaviour that still needs confirmation against real hardware.

## RF scope guarantee

The weather RF engine in this repository is **Davis Vantage Pro2 EU 868 MHz FHSS / 2-FSK only**.

The application layer may share design patterns with the maintainer's Oregon/Technoline gateway, but this Davis repository does not include Oregon Scientific, Technoline/LaCrosse or 433 MHz weather decoders.
