"""Compare real RuleStudio HTML capture with a UICreator or UE capture.
No camera/geometry fitting: only the documented iframe and screenshot surface scale.
"""
import argparse,json
from pathlib import Path
from PIL import Image,ImageChops,ImageStat
p=argparse.ArgumentParser();p.add_argument('candidate');p.add_argument('--label',default='uicreator');p.add_argument('--native',action='store_true');args=p.parse_args()
root=Path(__file__).resolve().parents[2];proof=root/'Saved/Verification/Talents/faithful-source'
reference=Image.open(proof/'default-full.png').convert('RGB');candidate=Image.open(args.candidate).convert('RGBA')
assert candidate.size==(1600,900),candidate.size
candidate=Image.alpha_composite(Image.new('RGBA',candidate.size,(7,10,16,255)),candidate).convert('RGB')
# Browser DOM viewport was measured at 1280x720; its screenshot is 1253x705.
surface=reference.height/720;factor=.7*surface
candidate=candidate.resize((round(1600*factor),round(900*factor)),Image.Resampling.LANCZOS)
box=(17,17,1087,607);top=round(38*surface)
reference=reference.crop((17,top+17,1087,top+607));candidate=candidate.crop(box)
diff=ImageChops.difference(reference,candidate);metric=sum(ImageStat.Stat(diff).mean)/3
combined=Image.new('RGB',(reference.width*2,reference.height));combined.paste(reference,(0,0));combined.paste(candidate,(reference.width,0));combined.save(proof/('source-vs-'+args.label+'.png'));diff.save(proof/(args.label+'-diff.png'))
report={'reference':'unmodified RuleStudio HTML: default-full.png','candidate':str(Path(args.candidate).resolve()),'native':args.native,'viewportCSS':[1280,720],'iframeCSSScale':.7,'screenshotSurfaceScale':surface,'comparison':'same panel interior; no per-element fitting','meanAbsoluteChannelError255':metric,'pixels':reference.width*reference.height}
(proof/(args.label+'-comparison.json')).write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8');print(json.dumps(report,ensure_ascii=False))
