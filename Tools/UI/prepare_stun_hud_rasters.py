"""Rasterize the four new SVG controls using the HUD's existing visual constants."""
from pathlib import Path
from io import BytesIO
import cairosvg
from PIL import Image

out=Path(__file__).resolve().parents[2]/'Saved/Verification/PlayerStun/HudRasters'
out.mkdir(parents=True,exist_ok=True)
specs={
 'stun-title':(60,18,'<text x="1" y="13" font-family="Microsoft YaHei" font-size="12" fill="#e9b75c">晕眩</text>'),
 'stun-value':(146,18,'<text x="145" y="13" text-anchor="end" font-family="Consolas" font-size="12" fill="#e9b75c">80 / 80</text>'),
 'stun-bar':(212,4,'<defs><linearGradient id="earth" x2="0" y2="1"><stop stop-color="#d2b77f"/><stop offset="1" stop-color="#907049"/></linearGradient><pattern id="grain" width="21" height="21" patternUnits="userSpaceOnUse" patternTransform="rotate(-20)"><path d="M12 0v21" stroke="#60452b" stroke-opacity=".53"/></pattern><clipPath id="round"><rect width="212" height="4" rx="2"/></clipPath></defs><g clip-path="url(#round)"><rect width="212" height="4" fill="url(#earth)"/><rect width="212" height="4" fill="url(#grain)"/></g>'),
 'stun-panel':(236,160,'<rect x=".5" y=".5" width="235" height="159" rx="10" fill="#0c1018" fill-opacity=".9294" stroke="#ffffff" stroke-opacity=".0784"/><path d="M10 1h216" stroke="white" stroke-opacity=".045"/>')
}
for name,(width,height,body) in specs.items():
 svg=f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">{body}</svg>'
 (out/(name+'.svg')).write_text(svg,encoding='utf-8')
 image=Image.open(BytesIO(cairosvg.svg2png(bytestring=svg.encode()))).convert('RGBA')
 # Match SvgCanvas's NoGamma/premultiplied-alpha texture import, exactly once.
 image.putdata([((r*a+127)//255,(g*a+127)//255,(b*a+127)//255,a) for r,g,b,a in image.getdata()])
 image.save(out/(name+'.png'))
print('STUN_HUD_RASTERS_READY controls=4')
