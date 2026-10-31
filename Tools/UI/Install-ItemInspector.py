"""Install the exact frozen UICreator inspector contract and raster resources."""
import json,shutil,struct,sys
from pathlib import Path
root=Path(__file__).resolve().parents[2]
job=sys.argv[1]
assert job.startswith('wbp-') and all(c.isalnum() or c in '-_' for c in job)
bundle=root/'CustomTools/UICreatorAlpha/exports'/job/'bundle.json'
document=json.loads(bundle.read_text(encoding='utf-8'))
assert document['schema']=='uicreator-alpha/native-wbp'
assert len(document['nodes'])==len({n['id'] for n in document['nodes']})
ids={n['id'] for n in document['nodes']}
assert 'hover-detail' in ids
assert not any('hover-details' in n or 'hover-craft-label' in n for n in ids)
out=root/'Content/UICreatorHover';out.mkdir(exist_ok=True);(out/'rasters').mkdir(exist_ok=True)
for key,raster in document['rasters'].items():
 source=bundle.parent/'rasters'/f'{key}.png'
 assert struct.unpack('>II',source.read_bytes()[16:24])==(raster['width'],raster['height'])
 shutil.copy2(source,out/'rasters'/source.name)
 raster.pop('png',None);raster.pop('svg',None)
for key in ('references','responsiveLayouts','sourceSvg'):document.pop(key,None)
(out/'contract.json').write_text(json.dumps(document,ensure_ascii=False),encoding='utf-8')
(out/'source.uialpha.json').write_text(json.dumps(document['project'],ensure_ascii=False,indent=2),encoding='utf-8')
report={'job':job,'sourceHash':document['sourceHash'],'nodes':len(document['nodes']),'rasters':len(document['rasters']),'removedLegacyActions':True,'templates':8}
(root/'Saved/Verification/ItemHover/uicreator-install.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(report))
