#!/usr/bin/env python3
from pathlib import Path
import re
from reportlab import rl_config
from reportlab.lib import colors
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.lib.enums import TA_CENTER
from reportlab.lib.units import mm
from reportlab.platypus import SimpleDocTemplate, Paragraph, Spacer, Preformatted, Table, TableStyle

# Stable object IDs/timestamps so the committed PDFs can be reproduced and
# compared byte-for-byte by CI from the Markdown sources.
rl_config.invariant = 1

ROOT = Path(__file__).resolve().parents[1]
DOCS = ROOT / "docs"
PAIRS = [
    (DOCS / "RF_PROTOCOL_IT.md", DOCS / "Davis_RF_Protocol_Guide_IT_v1.0.pdf", "Guida alle codifiche RF Davis Vantage Pro2 / Pro2 Plus EU - 868 MHz", "Edizione 1.0 - 31 agosto 2026"),
    (DOCS / "RF_PROTOCOL_EN.md", DOCS / "Davis_RF_Protocol_Guide_EN_v1.0.pdf", "Davis RF Encoding Guide - Vantage Pro2 / Pro2 Plus EU - 868 MHz", "Edition 1.0 - 31 August 2026"),
]

styles = getSampleStyleSheet()
styles.add(ParagraphStyle(name="DocTitle", parent=styles["Title"], alignment=TA_CENTER, fontSize=19, leading=23, spaceAfter=8))
styles.add(ParagraphStyle(name="DocSub", parent=styles["Normal"], alignment=TA_CENTER, fontSize=10, leading=13, spaceAfter=6))
styles.add(ParagraphStyle(name="H1X", parent=styles["Heading1"], fontSize=14, leading=17, spaceBefore=7, spaceAfter=6))
styles.add(ParagraphStyle(name="H2X", parent=styles["Heading2"], fontSize=11.5, leading=14, spaceBefore=6, spaceAfter=5))
styles.add(ParagraphStyle(name="BodyX", parent=styles["BodyText"], fontSize=9, leading=12, spaceAfter=5))
styles.add(ParagraphStyle(name="BulletX", parent=styles["BodyText"], fontSize=9, leading=12, leftIndent=12, firstLineIndent=-7, spaceAfter=3))
styles.add(ParagraphStyle(name="CodeX", parent=styles["Code"], fontName="Courier", fontSize=7.8, leading=10, leftIndent=6, spaceAfter=5))


def clean_inline(text):
    text = re.sub(r"`([^`]+)`", r'<font name="Courier">\1</font>', text)
    text = re.sub(r"\*\*([^*]+)\*\*", r"<b>\1</b>", text)
    text = re.sub(r"\[([^\]]+)\]\(([^)]+)\)", r"\1", text)
    return text


def parse_table(lines):
    rows = []
    for line in lines:
        cells = [clean_inline(c.strip()) for c in line.strip().strip("|").split("|")]
        if all(re.fullmatch(r":?-{3,}:?", re.sub(r"\s+", "", c)) for c in cells):
            continue
        rows.append([Paragraph(c, styles["BodyX"]) for c in cells])
    if not rows:
        return None
    n = max(len(r) for r in rows)
    for row in rows:
        while len(row) < n:
            row.append(Paragraph("", styles["BodyX"]))
    widths = [(A4[0] - 36 * mm) / n] * n
    table = Table(rows, colWidths=widths, repeatRows=1, hAlign="LEFT")
    table.setStyle(TableStyle([
        ("BACKGROUND", (0, 0), (-1, 0), colors.HexColor("#eeeeee")),
        ("FONTNAME", (0, 0), (-1, 0), "Helvetica-Bold"),
        ("GRID", (0, 0), (-1, -1), 0.35, colors.HexColor("#bdbdbd")),
        ("VALIGN", (0, 0), (-1, -1), "TOP"),
        ("FONTSIZE", (0, 0), (-1, -1), 7.8),
        ("LEFTPADDING", (0, 0), (-1, -1), 4),
        ("RIGHTPADDING", (0, 0), (-1, -1), 4),
        ("TOPPADDING", (0, 0), (-1, -1), 3),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 3),
    ]))
    return table


def build(src, out, title, edition):
    lines = src.read_text(encoding="utf-8").splitlines()
    story = [
        Spacer(1, 18 * mm),
        Paragraph(title, styles["DocTitle"]),
        Paragraph(edition, styles["DocSub"]),
        Paragraph("<b>Project:</b> ESP32 Davis Weather Gateway", styles["DocSub"]),
        Spacer(1, 7 * mm),
    ]
    in_code = False
    code = []
    i = 0
    while i < len(lines):
        raw = lines[i]
        text = raw.strip()
        if i < 6 and (text.startswith("# ") or text.startswith("Edizione ") or text.startswith("Edition ")):
            i += 1
            continue
        if text.startswith("```"):
            if not in_code:
                in_code = True
                code = []
            else:
                story.append(Preformatted("\n".join(code), styles["CodeX"]))
                in_code = False
            i += 1
            continue
        if in_code:
            code.append(raw)
            i += 1
            continue
        if text.startswith("|") and i + 1 < len(lines) and lines[i + 1].strip().startswith("|"):
            block = []
            while i < len(lines) and lines[i].strip().startswith("|"):
                block.append(lines[i])
                i += 1
            table = parse_table(block)
            if table:
                story.extend([table, Spacer(1, 5)])
            continue
        if not text:
            i += 1
            continue
        if text.startswith("## "):
            story.append(Paragraph(clean_inline(text[3:]), styles["H1X"]))
        elif text.startswith("### "):
            story.append(Paragraph(clean_inline(text[4:]), styles["H2X"]))
        elif re.match(r"^[-*] ", text):
            story.append(Paragraph("• " + clean_inline(text[2:]), styles["BulletX"]))
        elif re.match(r"^\d+\. ", text):
            story.append(Paragraph(clean_inline(text), styles["BulletX"]))
        else:
            story.append(Paragraph(clean_inline(text), styles["BodyX"]))
        i += 1

    def footer(canvas, doc):
        canvas.saveState()
        canvas.setFont("Helvetica", 7.2)
        canvas.setFillColor(colors.HexColor("#666666"))
        canvas.drawString(18 * mm, 10 * mm, "ESP32 Davis Weather Gateway - RF protocol documentation")
        canvas.drawRightString(A4[0] - 18 * mm, 10 * mm, f"Page {doc.page}")
        canvas.restoreState()

    document = SimpleDocTemplate(str(out), pagesize=A4, leftMargin=18 * mm, rightMargin=18 * mm, topMargin=16 * mm, bottomMargin=17 * mm, title=title, author="ESP32 Davis Weather Gateway")
    document.build(story, onFirstPage=footer, onLaterPages=footer)


for src, out, title, edition in PAIRS:
    build(src, out, title, edition)
    print(out.relative_to(ROOT))
