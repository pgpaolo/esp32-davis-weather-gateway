# Notice

## ESP32 Davis Weather Gateway

Copyright (C) 2026 **Gianpaolo P. (`@pgpaolo`) and contributors**.

ESP32 Davis Weather Gateway is an independent open-source interoperability project for ESP32/LILYGO hardware and European Davis Vantage Pro2 / Pro2 Plus wireless stations.

Unless a file states otherwise, project-original source code and documentation are licensed under the **GNU Lesser General Public License v3.0 only (`LGPL-3.0-only`)**. The canonical license text is kept unmodified in `LICENSE`; `COPYING` contains the GNU GPLv3 text incorporated by LGPLv3.

This `NOTICE.md` is an attribution/provenance notice only. It does **not** add restrictions to, replace, or modify the terms of the GNU LGPLv3.

## Attribution and redistribution

When redistributing project source code, preserve applicable copyright notices, SPDX identifiers where present, the repository license information and third-party attribution notices required by the licenses of the components being redistributed.

Modified versions should clearly identify substantial changes and must not be presented as an official Davis Instruments product, firmware or protocol specification.

## Project provenance

The Davis RF receiver/decoder in this repository is an independent ESP32/SX1276 implementation based on publicly documented interoperability facts, observed packet behaviour and historical Davis reverse-engineering references documented in `THIRD_PARTY_NOTICES.md`.

The Web UI, OLED display, microSD datalogger, MQTT integration, BME280 support, AS3935 support, diagnostics and remote-access preparation are maintained as components of this project.

## Trademarks and non-affiliation

Davis Instruments, Vantage Pro2, Vantage Vue and related names, product identifiers and trademarks belong to their respective owners. Their use here is descriptive and solely for interoperability and technical documentation.

This project is **not affiliated with, sponsored by, endorsed by, or official documentation of Davis Instruments**.

Protocol notes identified as reverse-engineered, inferred or requiring field validation must not be represented as vendor specifications.

## Warranty

The software and documentation are provided under the warranty disclaimers contained in the GNU LGPLv3/GPLv3. Weather, RF, lightning and pressure data should be independently validated before use in safety-critical, operational or commercial decision-making.
