"""Compare the frozen UICreator reference to the saved/reloaded native canvas.
No alignment, rescaling, masking or reference modification is performed.
"""
import hashlib,json,sys
from pathlib import Path
from PIL import Image,ImageChops,ImageStat

r=Path(__file__).resolve().parents[2]
job=sys.argv[1] if len(sys.argv)>1 else 'wbp-recent-d1b8b241-9167-4aa6-abf8-4354c912f885'
frozen=r/'CustomTools/UICreatorAlpha/exports'/job
proof=r/(sys.argv[2] if len(sys.argv)>2 else 'Saved/Verification/UEIntegration/UICreator')
source=frozen/'reference/canvas.png';native=proof/'native/canvas.png'
a=Image.open(source).convert('RGBA');b=Image.open(native).convert('RGBA')
assert a.size==b.size
bundle=json.loads((frozen/'bundle.json').read_text(encoding='utf-8'))
reload=json.loads((proof/'native/reload-receipt.json').read_text(encoding='utf-8'))
assert reload['ok'] and reload['reloadedInSeparateProcess'] and reload['sourceHash']==bundle['sourceHash']

def measure(left,right):
 diff=ImageChops.difference(left.convert('RGB'),right.convert('RGB'));stats=ImageStat.Stat(diff)
 channels=diff.split();maximum=ImageChops.lighter(ImageChops.lighter(channels[0],channels[1]),channels[2]);hist=maximum.histogram()
 return {'meanAbsoluteError':sum(stats.mean)/3,'fractionOver8':sum(hist[9:])/(left.width*left.height)}

rows=[];doc=json.loads((frozen/'project-record.json').read_text(encoding='utf-8'))['document']
for node in doc['root']['children']:
 offsets=node['slot']['offsets'];x=int(offsets['left']);y=int(offsets['top']);w=int(offsets['right']);h=int(offsets['bottom'])
 box=(x,y,x+w,y+h);stats=measure(a.crop(box),b.crop(box))
 rows.append({'id':node['id'],'box':box,**stats})
overall=measure(a,b)
passed=all(row['meanAbsoluteError']<=.25 and row['fractionOver8']<=.001 for row in rows)
receipt={'ok':passed,'job':job,'sourceHash':bundle['sourceHash'],'width':a.width,'height':a.height,'alignment':'none; exact original pixels','sourcePngSha256':hashlib.sha256(source.read_bytes()).hexdigest(),'nativePngSha256':hashlib.sha256(native.read_bytes()).hexdigest(),'overall':overall,'templates':rows,'thresholds':{'maximumTemplateMeanAbsoluteError':.25,'maximumTemplateFractionOver8':.001}}
(proof/'pixel-comparison.json').write_text(json.dumps(receipt,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(receipt,ensure_ascii=False));assert passed
