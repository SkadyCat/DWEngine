"""Create a single SVG icon from the local system font's 烈 outline. No raster art."""
from pathlib import Path
import sys
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.boundsPen import BoundsPen

target = Path(__file__).resolve().parents[1] / 'src/assets/hud-darksteel/svg/flame-glyph.svg'
font_path = Path('C:/Windows/Fonts/msyhbd.ttc')
font = TTFont(font_path, fontNumber=0)
glyphs = font.getGlyphSet()
glyph_name = font.getBestCmap()[ord('烈')]
glyph = glyphs[glyph_name]
bounds_pen = BoundsPen(glyphs)
glyph.draw(bounds_pen)
x0, y0, x1, y1 = bounds_pen.bounds
pen = SVGPathPen(glyphs)
glyph.draw(pen)
scale = min(28 / (x1 - x0), 28 / (y1 - y0))
tx = (32 - (x1 - x0) * scale) / 2 - x0 * scale
ty = (34 - (y1 - y0) * scale) / 2 + y1 * scale
svg = f'''<svg xmlns="http://www.w3.org/2000/svg" width="32" height="34" viewBox="0 0 32 34"><defs><linearGradient id="cyan" x2="0" y2="1"><stop stop-color="#60ddff"/><stop offset="1" stop-color="#29b6e8"/></linearGradient><filter id="glow" x="-30%" y="-30%" width="160%" height="160%"><feGaussianBlur stdDeviation="1.1"/></filter></defs><path d="{pen.getCommands()}" transform="translate({tx:.5f} {ty:.5f}) scale({scale:.7f} {-scale:.7f})" fill="#33bfee" opacity=".48" filter="url(#glow)"/><path d="{pen.getCommands()}" transform="translate({tx:.5f} {ty:.5f}) scale({scale:.7f} {-scale:.7f})" fill="url(#cyan)"/></svg>\n'''
target.parent.mkdir(parents=True, exist_ok=True)
if target.exists() and '--replace-owned' not in sys.argv:
    raise SystemExit(f'Already exists; source SVG was not overwritten: {target}')
target.write_text(svg, encoding='utf-8')
print(f'Created {target.name}: vector outline, 32x34, glyph U+70C8')
