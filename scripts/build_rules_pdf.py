"""Render the two Simple Hulk Markdown booklets. Requires reportlab."""
from pathlib import Path
import re
from xml.sax.saxutils import escape
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.lib import colors
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.lib.enums import TA_LEFT
from reportlab.lib.pagesizes import letter
from reportlab.platypus import (SimpleDocTemplate, Paragraph, Spacer, PageBreak,
                              Preformatted, Table, TableStyle, KeepTogether, Image)

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'output' / 'pdf'
OUT.mkdir(parents=True, exist_ok=True)
FONT_DIR = Path('/Users/barbalet/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/pdfjs-dist/standard_fonts')
for name, filename in [('Helvetica','LiberationSans-Regular.ttf'), ('Helvetica-Bold','LiberationSans-Bold.ttf'), ('Helvetica-Oblique','LiberationSans-Italic.ttf'), ('Helvetica-BoldOblique','LiberationSans-BoldItalic.ttf')]:
    pdfmetrics.registerFont(TTFont(name, str(FONT_DIR / filename)))
pdfmetrics.registerFontFamily('Helvetica', normal='Helvetica', bold='Helvetica-Bold', italic='Helvetica-Oblique', boldItalic='Helvetica-BoldOblique')
pdfmetrics.registerFont(TTFont('Courier', '/Users/barbalet/.cache/codex-runtimes/codex-primary-runtime/dependencies/native/poppler/poppler/fonts/UbuntuMono-R.ttf'))
styles = getSampleStyleSheet()
styles.add(ParagraphStyle(name='Text', fontName='Helvetica', fontSize=10,
                         leading=13, spaceAfter=6))
styles.add(ParagraphStyle(name='TitleSH', fontName='Helvetica-Bold', fontSize=21,
                         leading=24, spaceAfter=10, textColor=colors.HexColor('#202020')))
styles.add(ParagraphStyle(name='H2SH', fontName='Helvetica-Bold', fontSize=14,
                         leading=17, spaceBefore=8, spaceAfter=8,
                         textColor=colors.HexColor('#202020'), keepWithNext=True))
styles.add(ParagraphStyle(name='H3SH', fontName='Helvetica-Bold', fontSize=11,
                         leading=14, spaceBefore=6, spaceAfter=5, keepWithNext=True))
styles.add(ParagraphStyle(name='CellSH', fontName='Helvetica', fontSize=8.3,
                         leading=10.6, spaceAfter=0))
styles.add(ParagraphStyle(name='CodeSH', fontName='Courier', fontSize=11,
                         leading=12.5, spaceAfter=7))
styles.add(ParagraphStyle(name='ListSH', parent=styles['Text'], leftIndent=12,
                         firstLineIndent=-10, spaceAfter=4))


def ascii_punctuation(s):
    return s.translate(str.maketrans({'—':'-', '–':'-', '’':"'", '“':'"', '”':'"', '…':'...'}))


def inline(s):
    s = escape(ascii_punctuation(s))
    s = re.sub(r'\[([^\]]+)\]\(([^)]+)\)', r'\1', s)
    s = re.sub(r'\*\*(.+?)\*\*', r'<b>\1</b>', s)
    s = re.sub(r'(?<!\*)\*([^*]+)\*(?!\*)', r'<i>\1</i>', s)
    return re.sub(r'`([^`]+)`', r'<font name="Courier">\1</font>', s)


def footer(canvas, doc):
    canvas.saveState()
    canvas.setStrokeColor(colors.HexColor('#cccccc'))
    canvas.line(42, 36, 570, 36)
    canvas.setFont('Helvetica', 8)
    canvas.setFillColor(colors.HexColor('#555555'))
    canvas.drawString(42, 23, 'SIMPLE HULK  /  ' + doc.booklet)
    page_label = ('COVER' if doc.page == 1 else str(doc.page - 1) + ' / 6') if doc.booklet == 'RULES' else str(doc.page)
    canvas.drawRightString(570, 23, page_label)
    canvas.restoreState()


def render(name):
    lines = (ROOT / 'rules' / (name + '.md')).read_text().splitlines()
    story = []
    i = 0
    while i < len(lines):
        line = lines[i]
        if not line.strip():
            i += 1
            continue
        if line == '<!-- pagebreak -->':
            story.append(PageBreak()); i += 1; continue
        if line.startswith('!['):
            match = re.fullmatch(r'!\[([^\]]*)\]\(([^)]+)\)', line)
            if not match:
                raise ValueError('Invalid image reference: ' + line)
            source = ROOT / 'rules' / match.group(2)
            illustration = Image(str(source))
            scale = min(528 / illustration.imageWidth, 565 / illustration.imageHeight)
            illustration.drawWidth = illustration.imageWidth * scale
            illustration.drawHeight = illustration.imageHeight * scale
            illustration.hAlign = 'CENTER'
            story.extend([Spacer(1,12), illustration, Spacer(1,14)])
            i += 1; continue
        if line.startswith('```'):
            code = []; i += 1
            while i < len(lines) and not lines[i].startswith('```'):
                code.append(lines[i]); i += 1
            story.append(Preformatted('\n'.join(code), styles['CodeSH']))
            i += 1; continue
        if line.startswith('|'):
            rows = []
            while i < len(lines) and lines[i].startswith('|'):
                cells = [c.strip() for c in lines[i].strip('|').split('|')]
                if not all(re.fullmatch(r'[:\-]+', c) for c in cells):
                    rows.append([Paragraph(inline(c), styles['CellSH']) for c in cells])
                i += 1
            count = len(rows[0])
            widths = {3:[320,104,104],6:[120,52,45,120,87,104],5:[92,36,54,90,256]}[count]
            table = Table(rows, colWidths=widths, repeatRows=1, hAlign='LEFT')
            table.setStyle(TableStyle([
                ('BACKGROUND',(0,0),(-1,0),colors.HexColor('#eeeeee')),
                ('VALIGN',(0,0),(-1,-1),'TOP'),
                ('LINEBELOW',(0,0),(-1,0),0.6,colors.HexColor('#999999')),
                ('LINEBELOW',(0,1),(-1,-1),0.25,colors.HexColor('#dddddd')),
                ('TOPPADDING',(0,0),(-1,-1),5),('BOTTOMPADDING',(0,0),(-1,-1),5),
            ]))
            story.extend([table, Spacer(1,8)]); continue
        if line.startswith('#'):
            level = len(line) - len(line.lstrip('#'))
            style = {1:'TitleSH',2:'H2SH',3:'H3SH'}[level]
            story.append(Paragraph(inline(line[level:].strip()), styles[style]))
            i += 1; continue
        is_list = bool(re.match(r'^(?:- |\d+\. )', line))
        parts = [line]; i += 1
        while i < len(lines) and lines[i].strip() and not re.match(r'^(?:#|!\[|\||```|<!--|- |\d+\. )', lines[i]):
            parts.append(lines[i].strip()); i += 1
        text = ' '.join(parts)
        if text.startswith('- '): text = '- ' + text[2:]
        story.append(Paragraph(inline(text), styles['ListSH' if is_list else 'Text']))
    doc = SimpleDocTemplate(str(OUT / (name + '.pdf')), pagesize=letter,
                            rightMargin=42,leftMargin=42,topMargin=36,bottomMargin=47,
                            title='Simple Hulk - ' + name, author='Simple Hulk')
    doc.booklet = name
    doc.build(story, onFirstPage=footer, onLaterPages=footer)


if __name__ == '__main__':
    for booklet in ('RULES','FLUFF'):
        render(booklet)
