# Documentation build tools

Current firmware baseline on `develop`: **0.4.0-dev**.

## `build_protocol_pdfs.py`

Generates the historical RF protocol PDF editions from the protocol Markdown sources. These PDFs document the RF layer and are not a complete description of the current application firmware.

## `technical_guide_framework.py` and `technical_guide_content_*.py`

These files are the source of the graphical **Technical Guide v1.2** created during an earlier firmware phase.

They are retained for reproducibility and future editing, but the generated v1.2 manual predates later firmware features including MQTT, AS3935, OLED live diagnostics, extended five-channel RF diagnostics, microSD logging and remote-access preparation.

Until a newer unified graphical guide is generated and explicitly linked from `docs/README.md`, the authoritative documentation for the current firmware is the Markdown documentation indexed in:

[`docs/README.md`](../docs/README.md)

Do not publish a newly generated v1.2 PDF as if it described the complete 0.4.0-dev firmware.

## Web UI generator

The active Web dashboard is stored in `web/dashboard.html`. `scripts/generate_web_ui.py` produces a deterministic gzip C header inside the PlatformIO build directory. The generated header is intentionally not maintained as a hand-edited source file.

## Licensing

Project-original documentation-generator code is covered by `LGPL-3.0-only` unless a file states otherwise. See `NOTICE.md`, `THIRD_PARTY_NOTICES.md` and the licensing guides under `docs/`.
