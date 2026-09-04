# Licensing, Attribution and Provenance

Documentation version: **0.3.2-dev**  
Project: **ESP32 Davis Weather Gateway**

## 1. Primary license

Unless a specific file states otherwise, project-original source code and documentation in this repository are distributed under:

**GNU Lesser General Public License v3.0 only (`LGPL-3.0-only`)**.

The canonical LGPLv3 text is stored in `LICENSE`. `COPYING` contains the GPLv3 text incorporated by reference by LGPLv3.

The GNU license text is kept unmodified. Project-specific attribution and provenance are maintained separately in `NOTICE.md`, `THIRD_PARTY_NOTICES.md` and this guide.

## 2. Project copyright and attribution

Copyright (C) 2026 **Gianpaolo P. (`@pgpaolo`) and contributors**.

Source redistributions must preserve applicable copyright and license notices and all third-party attribution required by the licenses of the components actually being redistributed.

`NOTICE.md` documents authorship/provenance, trademarks and the independent nature of the project. It does not add restrictions to LGPLv3.

## 3. Attribution rule inherited from the Oregon / Technoline project

The earlier ESP32 Oregon Scientific + Technoline project followed an important rule: **provenance and third-party attribution must not be hidden**. When work derives from or materially relies on third-party code, the source and documentation should clearly identify origin and license where required.

The same attribution practice is used here, but the GPL license of that other project is not automatically transferred to the Davis gateway.

In the Davis project:

- the weather RF engine is exclusively Davis EU 868 MHz FHSS;
- no Oregon Scientific decoder is included;
- no Technoline/LaCrosse decoder is included;
- GPL-derived decoder portions associated with `rtl_433` or PracticalArduino in the other project are not imported here;
- original application-layer work owned by the same maintainer may be adapted or reimplemented and distributed here under LGPLv3 where the copyright holder has the right to do so.

## 4. Public Davis references

Davis decoding is an independent ESP32/SX1276 implementation based on publicly available interoperability and reverse-engineering information.

Historical references documented by the project include:

- **DavisRFM69** by DeKay;
- **ISS-MQTT-Gateway** by dcbo.

They are technical references. This repository does not intend to vendor their source code. Their license/provenance notes are documented in `THIRD_PARTY_NOTICES.md`.

## 5. Software dependencies

Libraries declared in `platformio.ini` — RadioLib, U8g2, Adafruit BME280/Unified Sensor/BusIO, PubSubClient, AS3935MI and the ESP32/Arduino framework dependencies — remain under their respective upstream licenses.

The project LGPLv3 license does not replace or alter those licenses.

## 6. Modified versions and forks

A published fork or modified version should:

1. preserve required copyright/license notices;
2. clearly identify substantial modifications and preferably their date;
3. not present itself as official Davis Instruments firmware or documentation;
4. retain applicable third-party attribution;
5. distinguish verified facts, reverse-engineered behaviour and assumptions still requiring hardware validation.

## 7. SPDX

New original source files may use:

```text
SPDX-License-Identifier: LGPL-3.0-only
```

SPDX identifiers improve machine-readable license identification but do not replace `LICENSE`, `NOTICE.md` or required third-party attribution.

## 8. Trademarks and non-affiliation

Davis Instruments, Vantage Pro2, Vantage Vue and related product names belong to their respective owners.

This project is independent and is not affiliated with, sponsored by or endorsed by Davis Instruments. Product names are used descriptively for interoperability and technical documentation.

## 9. Warranty and use of data

The warranty disclaimers of GNU LGPLv3/GPLv3 apply. The firmware remains under development, and RF, weather, pressure and lightning data must be independently validated before operational, commercial or safety-critical use.

## 10. Reference files

- `LICENSE` — GNU LGPLv3
- `COPYING` — GNU GPLv3 incorporated by LGPLv3
- `NOTICE.md` — project copyright, provenance, trademarks and disclaimer
- `THIRD_PARTY_NOTICES.md` — third-party references and attribution
- `CONTRIBUTING.md` — contribution and provenance rules
- `SECURITY.md` — firmware security boundary
