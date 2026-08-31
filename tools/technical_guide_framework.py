from pathlib import Path
from reportlab import rl_config
rl_config.invariant = 1

from reportlab.lib import colors
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.lib.enums import TA_CENTER, TA_LEFT
from reportlab.lib.units import mm
from reportlab.platypus import (BaseDocTemplate, PageTemplate, Frame, Paragraph, Spacer, Table, TableStyle,
                                PageBreak, KeepTogether, Flowable, ListFlowable, ListItem, Preformatted,
                                NextPageTemplate)
from reportlab.platypus.tableofcontents import TableOfContents
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.pdfbase import pdfmetrics
from reportlab.lib.colors import HexColor
from math import pi, cos, sin
ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "docs" / "ESP32_Davis_Weather_Gateway_Guida_Tecnica_v1.2.pdf"

# Palette
NAVY = HexColor('#17365D')
BLUE = HexColor('#2F75B5')
CYAN = HexColor('#D9EAF7')
SKY = HexColor('#EFF6FB')
GREEN = HexColor('#2E7D32')
GREEN_BG = HexColor('#EAF5EC')
ORANGE = HexColor('#C55A11')
ORANGE_BG = HexColor('#FFF1E8')
RED = HexColor('#B91C1C')
RED_BG = HexColor('#FDECEC')
GRAY = HexColor('#5B6573')
LIGHT = HexColor('#F4F6F8')
GRID = HexColor('#CFD6DE')
DARK = HexColor('#1F2937')
WHITE = colors.white

PAGE_W, PAGE_H = A4
LEFT = 18*mm
RIGHT = 18*mm
TOP = 18*mm
BOTTOM = 17*mm
CONTENT_W = PAGE_W - LEFT - RIGHT

styles = getSampleStyleSheet()
styles.add(ParagraphStyle(name='TitleMain', parent=styles['Title'], fontName='Helvetica-Bold', fontSize=24, leading=29,
                          textColor=WHITE, alignment=TA_LEFT, spaceAfter=8))
styles.add(ParagraphStyle(name='TitleSub', parent=styles['Normal'], fontName='Helvetica', fontSize=12, leading=16,
                          textColor=HexColor('#DCE8F5'), alignment=TA_LEFT))
styles.add(ParagraphStyle(name='CoverMeta', parent=styles['Normal'], fontName='Helvetica', fontSize=9.5, leading=13,
                          textColor=WHITE))
styles.add(ParagraphStyle(name='H1x', parent=styles['Heading1'], fontName='Helvetica-Bold', fontSize=16, leading=20,
                          textColor=NAVY, spaceBefore=4, spaceAfter=8, keepWithNext=True))
styles.add(ParagraphStyle(name='H2x', parent=styles['Heading2'], fontName='Helvetica-Bold', fontSize=12.3, leading=15,
                          textColor=BLUE, spaceBefore=8, spaceAfter=5, keepWithNext=True))
styles.add(ParagraphStyle(name='H3x', parent=styles['Heading3'], fontName='Helvetica-Bold', fontSize=10.4, leading=13,
                          textColor=DARK, spaceBefore=6, spaceAfter=4, keepWithNext=True))
styles.add(ParagraphStyle(name='Bodyx', parent=styles['BodyText'], fontName='Helvetica', fontSize=9.15, leading=12.3,
                          textColor=DARK, spaceAfter=5.5))
styles.add(ParagraphStyle(name='Smallx', parent=styles['BodyText'], fontName='Helvetica', fontSize=7.8, leading=10.2,
                          textColor=GRAY, spaceAfter=3.5))
styles.add(ParagraphStyle(name='Captionx', parent=styles['BodyText'], fontName='Helvetica-Oblique', fontSize=7.6, leading=10,
                          textColor=GRAY, alignment=TA_CENTER, spaceBefore=3, spaceAfter=6))
styles.add(ParagraphStyle(name='TOCHeading', parent=styles['Heading1'], fontName='Helvetica-Bold', fontSize=18, leading=22,
                          textColor=NAVY, spaceAfter=12))
styles.add(ParagraphStyle(name='Refx', parent=styles['BodyText'], fontName='Helvetica', fontSize=7.6, leading=10.3,
                          textColor=DARK, leftIndent=7*mm, firstLineIndent=-7*mm, spaceAfter=3))
styles.add(ParagraphStyle(name='Codex', parent=styles['Code'], fontName='Courier', fontSize=7.6, leading=9.6,
                          leftIndent=6, rightIndent=4, backColor=HexColor('#F6F8FA'), borderColor=GRID,
                          borderWidth=0.5, borderPadding=5, spaceBefore=3, spaceAfter=7))

# Helpers

def P(txt, style='Bodyx'):
    return Paragraph(txt, styles[style])

def h1(txt, key=None):
    p = Paragraph(txt, styles['H1x'])
    p._toc_level = 0
    p._toc_text = txt
    p._bookmark = key
    return p

def h2(txt, key=None):
    p = Paragraph(txt, styles['H2x'])
    p._toc_level = 1
    p._toc_text = txt
    p._bookmark = key
    return p

def h3(txt):
    return Paragraph(txt, styles['H3x'])

def bullets(items):
    return ListFlowable([ListItem(P(i), leftIndent=5*mm) for i in items], bulletType='bullet', start='circle',
                        leftIndent=8*mm, bulletFontName='Helvetica', bulletFontSize=6, spaceAfter=6)

def table(data, widths=None, header=True, font=7.7, alignments=None):
    pdata=[]
    for r,row in enumerate(data):
        prow=[]
        for c,cell in enumerate(row):
            st = ParagraphStyle('tbl', parent=styles['Smallx'], fontSize=font, leading=font+2.2,
                                textColor=DARK, spaceAfter=0)
            if header and r==0:
                st.fontName='Helvetica-Bold'; st.textColor=WHITE
            if alignments and c < len(alignments):
                st.alignment=alignments[c]
            prow.append(Paragraph(str(cell), st))
        pdata.append(prow)
    t=Table(pdata, colWidths=widths, repeatRows=1 if header else 0, hAlign='LEFT')
    cmds=[('VALIGN',(0,0),(-1,-1),'TOP'),('GRID',(0,0),(-1,-1),0.35,GRID),
          ('LEFTPADDING',(0,0),(-1,-1),4),('RIGHTPADDING',(0,0),(-1,-1),4),
          ('TOPPADDING',(0,0),(-1,-1),4),('BOTTOMPADDING',(0,0),(-1,-1),4)]
    if header:
        cmds += [('BACKGROUND',(0,0),(-1,0),NAVY)]
        if len(data)>1:
            cmds += [('ROWBACKGROUNDS',(0,1),(-1,-1),[WHITE,HexColor('#FAFBFC')])]
    else:
        cmds += [('ROWBACKGROUNDS',(0,0),(-1,-1),[WHITE,HexColor('#FAFBFC')])]
    t.setStyle(TableStyle(cmds))
    return t

class InfoBox(Flowable):
    def __init__(self, title, text, kind='info', width=CONTENT_W):
        Flowable.__init__(self)
        self.width=width
        self.title=title
        self.text=text
        self.kind=kind
        self.pad=7
        if kind=='warn': self.bg,self.accent=ORANGE_BG,ORANGE
        elif kind=='danger': self.bg,self.accent=RED_BG,RED
        elif kind=='ok': self.bg,self.accent=GREEN_BG,GREEN
        else: self.bg,self.accent=SKY,BLUE
        self.p_title=Paragraph(title, ParagraphStyle('ibtitle', parent=styles['Smallx'], fontName='Helvetica-Bold', fontSize=8.4, leading=10.5, textColor=self.accent))
        self.p_text=Paragraph(text, ParagraphStyle('ibtext', parent=styles['Smallx'], fontSize=8.2, leading=11, textColor=DARK))
        _,ht=self.p_title.wrap(width-2*self.pad-5,1000)
        _,hb=self.p_text.wrap(width-2*self.pad-5,1000)
        self.height=ht+hb+2*self.pad+3
    def draw(self):
        c=self.canv
        c.setFillColor(self.bg); c.setStrokeColor(self.accent); c.setLineWidth(0.7)
        c.roundRect(0,0,self.width,self.height,5,fill=1,stroke=1)
        c.setFillColor(self.accent); c.rect(0,0,3,self.height,fill=1,stroke=0)
        y=self.height-self.pad
        _,ht=self.p_title.wrap(self.width-2*self.pad-5,1000); self.p_title.drawOn(c,self.pad+5,y-ht); y-=ht+3
        _,hb=self.p_text.wrap(self.width-2*self.pad-5,1000); self.p_text.drawOn(c,self.pad+5,y-hb)

class ArchDiagram(Flowable):
    def __init__(self, width=CONTENT_W, height=72*mm):
        Flowable.__init__(self); self.width=width; self.height=height
    def box(self,c,x,y,w,h,title,lines,fill,stroke):
        c.setFillColor(fill); c.setStrokeColor(stroke); c.setLineWidth(0.8); c.roundRect(x,y,w,h,5,fill=1,stroke=1)
        c.setFillColor(stroke); c.setFont('Helvetica-Bold',8.2); c.drawCentredString(x+w/2,y+h-11,title)
        c.setFillColor(DARK); c.setFont('Helvetica',6.8)
        yy=y+h-23
        for line in lines:
            c.drawCentredString(x+w/2,yy,line); yy-=9
    def arrow(self,c,x1,y1,x2,y2,label=None):
        c.setStrokeColor(GRAY); c.setFillColor(GRAY); c.setLineWidth(1.1); c.line(x1,y1,x2,y2)
        from math import atan2
        ang=atan2(y2-y1,x2-x1)
        for da in (2.65,-2.65):
            c.line(x2,y2,x2+7*cos(ang+da),y2+7*sin(ang+da))
        if label:
            c.setFont('Helvetica',6.4); c.setFillColor(GRAY); c.drawCentredString((x1+x2)/2,(y1+y2)/2+4,label)
    def draw(self):
        c=self.canv; w=self.width; h=self.height
        bw=46*mm; bh=25*mm
        y1=h-31*mm
        self.box(c,0,y1,bw,bh,'DAVIS ISS EU',['T/H esterni - vento - pioggia','UV/solare se presenti','FHSS 868 MHz'],SKY,BLUE)
        self.box(c,(w-bw)/2,y1,bw,bh,'SX1276 / RFM95',['2-FSK - sync CB 89','hop tracking - RSSI','packet capture'],HexColor('#F3F0FA'),HexColor('#6A4FB3'))
        self.box(c,w-bw,y1,bw,bh,'ESP32 / LILYGO',['decoder Davis - CRC','NVS - Web UI - diagnostica','rete e upload HTTP'],GREEN_BG,GREEN)
        self.arrow(c,bw+2,y1+bh/2,(w-bw)/2-2,y1+bh/2,'RF')
        self.arrow(c,(w+bw)/2+2,y1+bh/2,w-bw-2,y1+bh/2,'SPI')
        bw2=52*mm; bh2=20*mm; y2=4*mm
        self.box(c,15*mm,y2,bw2,bh2,'BME280 LOCALE',['pressione lato ricevitore','T/H locale','riduzione a livello mare'],HexColor('#FFF9E6'),ORANGE)
        self.box(c,w-15*mm-bw2,y2,bw2,bh2,'SERVIZI LOCALI',['dashboard - JSON status','preview record','endpoint configurabile'],LIGHT,GRAY)
        self.arrow(c,w-bw+6*mm,y1-1,w-15*mm-bw2/2,y2+bh2+1,'dati')
        self.arrow(c,15*mm+bw2,y2+bh2/2,w-bw+6*mm,y1+4,'I2C')

class SensorDiagram(Flowable):
    def __init__(self,width=CONTENT_W,height=65*mm): Flowable.__init__(self); self.width=width; self.height=height
    def draw(self):
        c=self.canv; w=self.width; h=self.height
        cx=w/2; cy=h/2+5
        c.setFillColor(SKY); c.setStrokeColor(BLUE); c.setLineWidth(1); c.roundRect(cx-28*mm,cy-12*mm,56*mm,24*mm,7,fill=1,stroke=1)
        c.setFillColor(NAVY); c.setFont('Helvetica-Bold',10); c.drawCentredString(cx,cy+3,'DAVIS SENSOR SUITE')
        c.setFont('Helvetica',7); c.setFillColor(DARK); c.drawCentredString(cx,cy-8,'6322/6322M - 6327/6327M Plus')
        nodes=[('Temperatura','-40..+65 C',7*mm,h-16*mm),('Umidita','1..100 %RH',7*mm,17*mm),('Vento','0..322 km/h',w-49*mm,h-16*mm),('Pioggia','0.2 mm/tip',w-49*mm,17*mm),('UV*','0..16 index',cx-53*mm,3*mm),('Solare*','0..1800 W/m2',cx+16*mm,3*mm)]
        for title,sub,x,y in nodes:
            bw=42*mm; bh=15*mm
            c.setFillColor(WHITE); c.setStrokeColor(GRID); c.roundRect(x,y,bw,bh,4,fill=1,stroke=1)
            c.setFillColor(DARK); c.setFont('Helvetica-Bold',7.5); c.drawCentredString(x+bw/2,y+9.5*mm,title)
            c.setFont('Helvetica',6.3); c.setFillColor(GRAY); c.drawCentredString(x+bw/2,y+4.5*mm,sub)
            sx = x+bw/2; sy=y+bh/2
            ex=cx; ey=cy
            c.setStrokeColor(HexColor('#AAB4C0')); c.setLineWidth(0.6); c.line(sx,sy,ex,ey)
        c.setFont('Helvetica-Oblique',6.5); c.setFillColor(GRAY); c.drawString(7*mm,1*mm,'* UV e solare sono di serie sulle configurazioni Pro2 Plus o opzionali su alcune Pro2.')

class HopDiagram(Flowable):
    def __init__(self,width=CONTENT_W,height=45*mm): Flowable.__init__(self); self.width=width; self.height=height
    def draw(self):
        c=self.canv; w=self.width; h=self.height
        freqs=[868.066711,868.297119,868.527466,868.181885,868.412292]
        labels=['H1','H2','H3','H4','H5']
        margin=10*mm; y=h/2
        c.setStrokeColor(GRID); c.setLineWidth(1.2); c.line(margin,y,w-margin,y)
        for i,(f,lbl) in enumerate(zip(freqs,labels)):
            x=margin + i*(w-2*margin)/4
            c.setFillColor(BLUE if i%2==0 else NAVY); c.setStrokeColor(WHITE); c.circle(x,y,6*mm,fill=1,stroke=0)
            c.setFillColor(WHITE); c.setFont('Helvetica-Bold',7.2); c.drawCentredString(x,y-2.5,lbl)
            c.setFillColor(DARK); c.setFont('Helvetica-Bold',7.1); c.drawCentredString(x,y+10*mm,f'{f:.6f} MHz')
            if i<4:
                c.setFillColor(GRAY); c.setFont('Helvetica',6.3); c.drawCentredString(x+(w-2*margin)/8,y-9*mm,'hop')
        c.setFillColor(GRAY); c.setFont('Helvetica',6.6); c.drawCentredString(w/2,2*mm,'Sequenza implementata nel gateway; il periodo RF dipende dall\'ID trasmettitore Davis.')

class FrameDiagram(Flowable):
    def __init__(self,width=CONTENT_W,height=36*mm): Flowable.__init__(self); self.width=width; self.height=height
    def draw(self):
        c=self.canv; w=self.width; h=self.height
        labels=['B0','B1','B2','B3','B4','B5','B6','B7','B8','B9']
        desc=['TYPE/ID','WIND','DIR','DATA','DATA','CRC src','CRC','CRC','TRAIL','TRAIL']
        cell=w/10
        y=10*mm; ch=13*mm
        for i in range(10):
            x=i*cell
            fill=NAVY if i in (0,6,7) else (CYAN if i in (1,2,3,4,5) else LIGHT)
            c.setFillColor(fill); c.setStrokeColor(WHITE); c.rect(x,y,cell-1,ch,fill=1,stroke=0)
            c.setFillColor(WHITE if i in (0,6,7) else DARK); c.setFont('Helvetica-Bold',6.8); c.drawCentredString(x+(cell-1)/2,y+8*mm,labels[i])
            c.setFont('Helvetica',5.8); c.drawCentredString(x+(cell-1)/2,y+3.7*mm,desc[i])
        c.setFillColor(GRAY); c.setFont('Helvetica',6.3); c.drawString(0,2.5*mm,'Dopo la ricezione ogni byte viene normalizzato tramite bit reversal. CRC16-CCITT sui byte 0..5; confronto con byte 6..7.')

class SecurityDiagram(Flowable):
    def __init__(self,width=CONTENT_W,height=43*mm): Flowable.__init__(self); self.width=width; self.height=height
    def draw(self):
        c=self.canv; w=self.width; h=self.height
        items=[('Provisioning','AP temporaneo','NVS'),('Web UI','LAN fidata','no Internet diretto'),('HTTPS','verify default','insecure opt-in'),('GitHub','no secrets','PR + CI')]
        gap=4*mm; bw=(w-3*gap)/4; bh=25*mm; y=8*mm
        for i,(a,b,d) in enumerate(items):
            x=i*(bw+gap)
            c.setFillColor(GREEN_BG if i in (2,3) else SKY); c.setStrokeColor(GREEN if i in (2,3) else BLUE)
            c.roundRect(x,y,bw,bh,5,fill=1,stroke=1)
            c.setFillColor(DARK); c.setFont('Helvetica-Bold',7.5); c.drawCentredString(x+bw/2,y+17*mm,a)
            c.setFont('Helvetica',6.6); c.drawCentredString(x+bw/2,y+10.5*mm,b)
            c.setFillColor(GRAY); c.drawCentredString(x+bw/2,y+5.5*mm,d)

class MyDocTemplate(BaseDocTemplate):
    def __init__(self, filename, **kw):
        super().__init__(filename, **kw)
        frame = Frame(LEFT, BOTTOM, CONTENT_W, PAGE_H-TOP-BOTTOM, id='normal', leftPadding=0, rightPadding=0, topPadding=0, bottomPadding=0)
        self.addPageTemplates([PageTemplate(id='main', frames=frame, onPage=self._page)])
    def _page(self, canvas, doc):
        canvas.saveState()
        if doc.page > 1:
            canvas.setStrokeColor(HexColor('#D5DBE3')); canvas.setLineWidth(0.5)
            canvas.line(LEFT, PAGE_H-12*mm, PAGE_W-RIGHT, PAGE_H-12*mm)
            canvas.setFont('Helvetica',7.2); canvas.setFillColor(GRAY)
            canvas.drawString(LEFT, PAGE_H-9.5*mm, 'ESP32 Davis Weather Gateway - Guida tecnica v1.2')
            canvas.drawRightString(PAGE_W-RIGHT, PAGE_H-9.5*mm, '31 agosto 2026')
            canvas.line(LEFT, 12*mm, PAGE_W-RIGHT, 12*mm)
            canvas.drawString(LEFT, 7.8*mm, 'Documentazione indipendente di interoperabilita - LGPL-3.0-only')
            canvas.drawRightString(PAGE_W-RIGHT, 7.8*mm, f'Pagina {doc.page}')
        canvas.restoreState()
    def afterFlowable(self, flowable):
        if hasattr(flowable, '_toc_level'):
            text = getattr(flowable, '_toc_text', flowable.getPlainText())
            level = flowable._toc_level
            key = getattr(flowable, '_bookmark', None) or f'h{level}-{self.page}-{abs(hash(text))%100000}'
            self.canv.bookmarkPage(key)
            self.canv.addOutlineEntry(text, key, level=level, closed=False)
            self.notify('TOCEntry', (level, text, self.page, key))

story=[]

class Cover(Flowable):
    def __init__(self): Flowable.__init__(self); self.width=CONTENT_W; self.height=PAGE_H-TOP-BOTTOM
    def draw(self):
        c=self.canv; w=self.width; h=self.height
        c.setFillColor(NAVY); c.roundRect(0,h-118*mm,w,110*mm,9,fill=1,stroke=0)
        c.setFillColor(BLUE); c.rect(0,h-118*mm,5*mm,110*mm,fill=1,stroke=0)
        c.setFont('Helvetica-Bold',24); c.setFillColor(WHITE)
        c.drawString(14*mm,h-37*mm,'ESP32 Davis Weather Gateway')
        c.setFont('Helvetica-Bold',15); c.setFillColor(HexColor('#DCE8F5'))
        c.drawString(14*mm,h-50*mm,'Guida tecnica completa')
        c.setFont('Helvetica',10); c.drawString(14*mm,h-61*mm,'Davis Vantage Pro2 / Pro2 Plus EU - RF 868 MHz - ESP32/LILYGO')
        c.setFillColor(WHITE); c.roundRect(14*mm,h-86*mm,34*mm,14*mm,7,fill=1,stroke=0)
        c.setFillColor(NAVY); c.setFont('Helvetica-Bold',11); c.drawCentredString(31*mm,h-81.3*mm,'Versione 1.2')
        c.setFillColor(HexColor('#DCE8F5')); c.setFont('Helvetica',8.3)
        c.drawString(14*mm,h-104*mm,'Edizione tecnica - 31 agosto 2026')
        cx=w-42*mm; cy=h-71*mm
        c.setStrokeColor(HexColor('#8FB8DD')); c.setLineWidth(1.2)
        for r in (9*mm,15*mm,22*mm): c.circle(cx,cy,r,stroke=1,fill=0)
        c.setFillColor(WHITE); c.circle(cx,cy,3.5*mm,fill=1,stroke=0)
        for ang in [20,85,155,230,310]:
            x=cx+28*mm*cos(ang*pi/180); y=cy+28*mm*sin(ang*pi/180)
            c.setFillColor(HexColor('#DCE8F5')); c.circle(x,y,2.2*mm,fill=1,stroke=0)
        y0=h-147*mm
        c.setFillColor(DARK); c.setFont('Helvetica-Bold',10); c.drawString(2*mm,y0,'Ambito')
        c.setFont('Helvetica',8.5); c.setFillColor(GRAY)
        lines=['Stazione e sensori Davis - segnale FHSS EU - frame RF e conversioni',
               'architettura gateway - BME280 lato ricevitore - provisioning e sicurezza',
               'upload HTTP compatibile Meteobridge/Weather34 - diagnostica e limiti noti']
        yy=y0-6*mm
        for line in lines: c.drawString(2*mm,yy,line); yy-=5*mm
        c.setFillColor(LIGHT); c.roundRect(0,12*mm,w,37*mm,7,fill=1,stroke=0)
        c.setFillColor(DARK); c.setFont('Helvetica-Bold',8.5); c.drawString(8*mm,39*mm,'Nota di indipendenza')
        c.setFont('Helvetica',7.6); c.setFillColor(GRAY)
        note=('Questo documento e prodotto dal progetto open-source ESP32 Davis Weather Gateway per scopi di interoperabilita e studio. '
              'Non e documentazione ufficiale Davis Instruments e distingue esplicitamente dati ufficiali, implementazione del progetto e informazioni da reverse engineering pubblico.')
        words=note.split(); line=''; yy=32*mm
        for word in words:
            test=(line+' '+word).strip()
            if c.stringWidth(test,'Helvetica',7.6)>w-16*mm:
                c.drawString(8*mm,yy,line); yy-=4.3*mm; line=word
            else: line=test
        if line: c.drawString(8*mm,yy,line)
        c.setFillColor(GRAY); c.setFont('Helvetica',7); c.drawString(2*mm,3*mm,'Project license: GNU LGPL v3.0 only (LGPL-3.0-only) - Trademarks belong to their respective owners.')

story.append(Cover()); story.append(PageBreak())

toc=TableOfContents(); toc.levelStyles=[
    ParagraphStyle(name='TOC0', fontName='Helvetica-Bold', fontSize=9.5, leading=13, leftIndent=0, textColor=NAVY, spaceBefore=3),
    ParagraphStyle(name='TOC1', fontName='Helvetica', fontSize=8.2, leading=11, leftIndent=9*mm, textColor=DARK, spaceBefore=1)
]
story += [P('Indice','TOCHeading'), P('La numerazione delle sezioni segue il percorso dalla stazione fisica al segnale RF, quindi al firmware e ai servizi di rete.','Smallx'), Spacer(1,3*mm), toc, PageBreak()]
