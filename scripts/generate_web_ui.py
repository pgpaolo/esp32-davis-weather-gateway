Import("env")

from pathlib import Path
import gzip
import re

project_dir = Path(env.subst("$PROJECT_DIR"))
pioenv = env.subst("$PIOENV")
build_root = Path(env.subst("$BUILD_DIR"))
generated_dir = build_root / pioenv / "generated"
generated_dir.mkdir(parents=True, exist_ok=True)

source_path = project_dir / "web" / "dashboard.html"
html = source_path.read_text(encoding="utf-8")

# Normalize the project footer at build time. This operation is intentionally
# idempotent: both an older dashboard and a dashboard already carrying the
# current attribution must compile successfully.
old_footer = '<div class="footer">ESP32 Davis Weather Gateway · firmware open source · interfaccia locale</div>'
new_footer = '<div class="footer">ESP32 Davis Weather Gateway · Gianpaolo P. · firmware open source · interfaccia locale</div>'
if new_footer not in html:
    if old_footer in html:
        html = html.replace(old_footer, new_footer, 1)
    else:
        # Accept minor footer wording changes while keeping the canonical
        # project attribution. Fail only if the footer element itself vanished.
        pattern = r'<div class="footer">ESP32 Davis Weather Gateway[^<]*</div>'
        html, replaced = re.subn(pattern, new_footer, html, count=1)
        if replaced != 1:
            raise RuntimeError("Dashboard footer element not found")

# Add the current-log viewer only when the source dashboard does not already
# contain it. This prevents duplicate buttons when UI changes are promoted
# back into web/dashboard.html.
viewer = '<a class="btn secondary" href="/api/sd/current" target="_blank" rel="noopener">Visualizza log SD</a>'
if '/api/sd/current' not in html:
    hardware_anchor = '<button onclick="sdRemount()">Rimonta</button>'
    config_anchor = '<button class="secondary" onclick="sdRemount()">Rimonta</button>'
    if hardware_anchor not in html or config_anchor not in html:
        raise RuntimeError("microSD action anchor not found")
    html = html.replace(hardware_anchor, hardware_anchor + viewer, 1)
    html = html.replace(config_anchor, config_anchor + viewer, 1)

payload = gzip.compress(html.encode("utf-8"), compresslevel=9, mtime=0)

rows = []
for offset in range(0, len(payload), 16):
    chunk = payload[offset:offset + 16]
    rows.append("    " + ", ".join(f"0x{value:02X}" for value in chunk) + ",")

header = (
    "#pragma once\n"
    "#include <Arduino.h>\n"
    "// Auto-generated from web/dashboard.html; deterministic gzip (mtime=0).\n"
    "static const uint8_t WEB_UI_GZ[] PROGMEM = {\n"
    + "\n".join(rows)
    + "\n};\n"
    + f"static constexpr size_t WEB_UI_GZ_LEN = {len(payload)}U;\n"
)

header_path = generated_dir / "web_ui_generated.h"
if not header_path.exists() or header_path.read_text(encoding="ascii") != header:
    header_path.write_text(header, encoding="ascii", newline="\n")

env.Append(CPPPATH=[str(generated_dir)])
print(f"Compressed Web UI: {len(html.encode('utf-8'))} -> {len(payload)} bytes")
