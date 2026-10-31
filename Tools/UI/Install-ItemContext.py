"""Deploy the frozen UICreator contract and raster files without redrawing UI."""
import json,shutil,struct,sys
from pathlib import Path
root=Path(__file__).resolve().parents[2]
job=sys.argv[1] if len(sys.argv)>1 else 'wbp-context-8605178c-a5a2-40ad-b2e6-6a3223d35e3d'
bundle=root/'CustomTools/UICreatorAlpha/exports'/job/'bundle.json'
doc=json.loads(bundle.read_text(encoding='utf8'));assert doc['schema']=='uicreator-alpha/native-wbp'
dest=root/'Content/UICreatorContext';(dest/'rasters').mkdir(parents=True,exist_ok=True)
for key,raster in doc['rasters'].items():
 source=bundle.parent/'rasters'/(key+'.png');assert struct.unpack('>II',source.read_bytes()[16:24])==(raster['width'],raster['height'])
 shutil.copy2(source,dest/'rasters'/source.name);raster.pop('svg',None);raster.pop('png',None)
for key in ('references','responsiveLayouts','sourceSvg'):doc.pop(key,None)
(dest/'contract.json').write_text(json.dumps(doc,ensure_ascii=False),encoding='utf8')
(dest/'source.uialpha.json').write_text(json.dumps(doc['project'],ensure_ascii=False,indent=2),encoding='utf8')
print(json.dumps(dict(hash=doc['sourceHash'],nodes=len(doc['nodes']),rasters=len(doc['rasters']))))
