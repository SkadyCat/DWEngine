"""Deploy the exact UICreator loot export; do not draw or restyle controls."""
import json, shutil, struct, sys
from pathlib import Path

root = Path(__file__).resolve().parents[2]
job = sys.argv[1] if len(sys.argv) > 1 else 'wbp-6f2cace1-eec6-430f-b1f8-fa5fd0448236'
assert job.startswith('wbp-') and all(c.isalnum() or c in '-_' for c in job)
bundle = root / 'CustomTools/UICreatorAlpha/exports' / job / 'bundle.json'
doc = json.loads(bundle.read_text(encoding='utf-8'))
assert doc['schema'] == 'uicreator-alpha/native-wbp'
assert len(doc['nodes']) == len({n['id'] for n in doc['nodes']})
out = root / 'Content/UICreatorLoot'
out.mkdir(exist_ok=True); (out / 'rasters').mkdir(exist_ok=True)
for key, raster in doc['rasters'].items():
    source = bundle.parent / 'rasters' / f'{key}.png'
    assert struct.unpack('>II', source.read_bytes()[16:24]) == (raster['width'], raster['height'])
    shutil.copy2(source, out / 'rasters' / source.name)
    raster.pop('png', None); raster.pop('svg', None)
for key in ('references', 'responsiveLayouts', 'sourceSvg'):
    doc.pop(key, None)
(out / 'contract.json').write_text(json.dumps(doc, ensure_ascii=False), encoding='utf-8')
(out / 'source.uialpha.json').write_text(json.dumps(doc['project'], ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps({'sourceHash': doc['sourceHash'], 'nodes': len(doc['nodes']), 'rasters': len(doc['rasters'])}))
