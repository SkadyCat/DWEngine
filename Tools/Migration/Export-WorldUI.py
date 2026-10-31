"""Install a frozen UICreator export; never draw or restyle its controls."""
import json, shutil, struct, sys
from pathlib import Path

bundle = Path(sys.argv[1]).resolve()
root = Path(__file__).resolve().parents[2]
out = root / 'Content/UICreatorWorld'
doc = json.loads(bundle.read_text(encoding='utf-8'))
assert doc['schema'] == 'uicreator-alpha/native-wbp'
out.mkdir(parents=True, exist_ok=True)
(out / 'rasters').mkdir(exist_ok=True)
for key, raster in doc['rasters'].items():
    source = bundle.parent / 'rasters' / f'{key}.png'
    data = source.read_bytes()
    width, height = struct.unpack('>II', data[16:24])
    assert (width, height) == (raster['width'], raster['height'])
    shutil.copy2(source, out / 'rasters' / source.name)
    raster.pop('png', None)
    raster.pop('svg', None)
for key in ('references', 'responsiveLayouts', 'sourceSvg'):
    doc.pop(key, None)
(out / 'contract.json').write_text(json.dumps(doc, ensure_ascii=False), encoding='utf-8')
(out / 'source.uialpha.json').write_text(json.dumps(doc['project'], ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps({'sourceHash':doc['sourceHash'], 'nodes':len(doc['nodes']), 'rasters':len(doc['rasters']), 'dimensionMismatches':0}))
