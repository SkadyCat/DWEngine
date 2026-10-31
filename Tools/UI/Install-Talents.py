"""Install the immutable UICreator export; no gameplay or image synthesis."""
import hashlib,json,shutil,struct,sys,copy
from pathlib import Path
root=Path(__file__).resolve().parents[2]
job=sys.argv[1]
assert job.startswith('wbp-') and all(c.isalnum() or c in '-_' for c in job)
bundle=root/'CustomTools/UICreatorAlpha/exports'/job/'bundle.json'
doc=json.loads(bundle.read_text(encoding='utf-8'))
assert doc['schema']=='uicreator-alpha/native-wbp'
nodes=doc['nodes']; ids={n['id'] for n in nodes}
assert len(ids)==len(nodes)
tree=json.loads((root/'Content/Rules/Talents/constellation.json').read_text(encoding='utf-8'))
assert len(tree['nodes'])==1000
for n in tree['nodes']:
 assert 'point-'+n['id'] in ids
 if n['kind']=='legendary': assert 'legend-sample-'+n['id'] in ids
out=root/'Content/UICreatorTalents';out.mkdir(exist_ok=True);(out/'rasters').mkdir(exist_ok=True)
for key,raster in doc['rasters'].items():
 source=bundle.parent/'rasters'/f'{key}.png'
 assert struct.unpack('>II',source.read_bytes()[16:24])==(raster['width'],raster['height'])
 shutil.copy2(source,out/'rasters'/source.name)
 raster.pop('png',None);raster.pop('svg',None)
for asset in doc['project']['provenance']['sourceAssets']:
 source=bundle.parent/'source-assets'/asset['name']
 assert hashlib.sha256(source.read_bytes()).hexdigest()==asset['sha256']
sources=out/'source-assets';sources.mkdir(exist_ok=True)
for source in (bundle.parent/'source-assets').glob('*.png'):shutil.copy2(source,sources/source.name)
for key in ('references','responsiveLayouts','sourceSvg'):doc.pop(key,None)
(out/'contract.json').write_text(json.dumps(doc,ensure_ascii=False),encoding='utf-8')
(out/'source.uialpha.json').write_text(json.dumps(doc['project'],ensure_ascii=False,indent=2),encoding='utf-8')
# Derived projection retains the complete frozen contract for editor/readback verification.
from CompileTalentRuntime import compile_runtime
runtime=compile_runtime(doc)
(out/'runtime.json').write_text(json.dumps(runtime,ensure_ascii=False,separators=(',',':')),encoding='utf-8')
report={'job':job,'sourceHash':doc['sourceHash'],'nodes':len(nodes),'rasters':len(doc['rasters']),'talents':1000,'legendaryIcons':100,'sourcePNGsUnmodified':16,'scope':str(out),'newAssetScope':not(out/'DA_CurrentCanvas.uasset').exists()}
(root/'Saved/Verification/Talents/uicreator-install.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(report))
