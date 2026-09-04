# Documentation build tools

Current firmware baseline on `develop`: **0.3.2-dev**.

## `build_protocol_pdfs.py`

Generates the historical RF protocol PDF editions from the protocol Markdown sources. These PDFs document the RF layer and are not a complete description of the current application firmware.

## `technical_guide_framework.py` and `technical_guide_content_*.py`

These files are the source of the graphical **Technical Guide v1.2** created during the earlier 0.2.x project phase.

They are retained for reproducibility and future editing, but their generated v1.2 manual must currently be considered **historical** because later firmware added:

- 0.3 application-layer Web UI;
- MQTT;
- AS3935;
- extended BME280 handling;
- OLED runtime/search/raw display;
- 0.3.2 five-channel RF diagnostics;
- RAW/NORMALIZED/CRC history and timing;
- 60-second diagnostic capture and report;
- current reset procedures;
- updated LGPL attribution/provenance policy.

Until a newer unified graphical guide is generated and explicitly linked from `docs/README.md`, the authoritative documentation for the current firmware is the Markdown documentation indexed in:

[`docs/README.md`](../docs/README.md)

Do not publish a newly generated v1.2 PDF as if it described the complete 0.3.2-dev firmware.

## Licensing

Documentation-generator code original to this repository is covered by the project `LGPL-3.0-only` license unless a file states otherwise. See `NOTICE.md`, `THIRD_PARTY_NOTICES.md` and `docs/LICENSING_IT.md` / `docs/LICENSING_EN.md`.
