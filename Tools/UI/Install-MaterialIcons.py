"""Copy the exact UICreator raster exports into the native item-icon mapping."""
import json,shutil,hashlib,struct,sys
from pathlib import Path
root=Path(__file__).resolve().parents[2]
job=sys.argv[1]
assert job.startswith('wbp-') and all(c.isalnum() or c in '-_' for c in job)
folder=root/'CustomTools/UICreatorAlpha/exports'/job
bundle=json.loads((folder/'bundle.json').read_text(encoding='utf-8-sig'))
layout={n['id']:n for n in bundle['states']['canvas']['nodes']}
backup=root/'Saved/Verification/ItemHover/material-icon-backup';backup.mkdir(exist_ok=True)
rows=[]
for node,item in (('stone-green','chaos_orb'),('stone-blue','spirit_stone_mid'),('stone-purple','spirit_stone_high')):
 key=layout[node]['raster'];src=folder/'rasters'/f'{key}.png'
 assert struct.unpack('>II',src.read_bytes()[16:24])==(40,40)
 target=root/'Content/OpusIcons/item'/f'{item}.png'
 if not (backup/target.name).exists():shutil.copy2(target,backup/target.name)
 shutil.copy2(src,target)
 rows.append({'item':item,'sourceNode':node,'raster':key,'target':str(target.relative_to(root)),'sha256':hashlib.sha256(target.read_bytes()).hexdigest()})
receipt={'exportJob':job,'sourceHash':bundle['sourceHash'],'sourceProject':'opus-native-material-icons','icons':rows}
(root/'Saved/Verification/ItemHover/material-icon-install.json').write_bytes((json.dumps(receipt,ensure_ascii=False,indent=2)+'\n').encode('utf-8'))
print(json.dumps(receipt,ensure_ascii=False))
