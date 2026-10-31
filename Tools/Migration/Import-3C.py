"""Import only 3C presentation assets/data, never the lab's training game or UI."""
import hashlib
import json
from pathlib import Path
import re
import shutil

ROOT = Path(__file__).resolve().parents[2]
LAB = ROOT / 'CustomTools/3C'
OUT = ROOT / 'Saved/Verification/ThreeC'
OUT.mkdir(parents=True, exist_ok=True)
digest = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
receipt = []

def copy_exact(relative):
    source, target = LAB / relative, ROOT / relative
    if target.exists() and digest(source) != digest(target):
        raise RuntimeError('Existing asset differs; inspect before replacing: ' + str(target))
    copied = not target.exists()
    if copied:
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
    receipt.append(dict(path=str(relative).replace('\\', '/'), copied=copied, sha256=digest(target)))

for row in json.loads((LAB / 'Docs/equipment-migration.json').read_text(encoding='utf-8-sig'))['files']:
    copy_exact(Path(row['path']))
for path in sorted((LAB / 'Content/ThreeCLab/Animation').rglob('*')):
    if path.is_file():
        copy_exact(path.relative_to(LAB))

for filename in ('Gaits.as', 'Equipment.as'):
    source = (LAB / 'Script' / filename).read_text(encoding='utf-8-sig')
    # Keep generated values and measured hand transforms unchanged.
    source = source.replace('C3Gaits', 'DWO3CGaits').replace('C3Equipment', 'DWO3CEquipment').replace('FC3Gear', 'FDWO3CGear')
    target = ROOT / 'Script/Opus/Combat3C' / filename
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(source, encoding='utf8', newline='\n')

models = []
source = (ROOT / 'Script/Opus/Models.as').read_text(encoding='utf8', newline='\n')
for model_id, body in re.findall(r'if\(Id=="([^"]+)"\)\s*\{(.*?)return M;\}', source):
    models.append(dict(id=model_id, mesh=re.search(r'M.Mesh="([^"]+)"', body)[1],
                       clips=dict(re.findall(r'M.Clips.Add\(n"([^"]+)","([^"]+)"\)', body))))
assert len(models) == 36, len(models)
ids = 'namespace DWO3CModels { TArray<FString> All() { TArray<FString> Out;\n'
ids += ''.join(' Out.Add("' + model['id'] + '");\n' for model in models)
ids += ' return Out; } }\n'
(ROOT / 'Script/Opus/Combat3C/Models.as').write_text(ids, encoding='utf8', newline='\n')
(OUT / 'models.json').write_text(json.dumps(models, ensure_ascii=False, indent=2), encoding='utf8', newline='\n')
(OUT / 'asset-migration.json').write_text(json.dumps(dict(source=str(LAB), files=receipt,
    copied=sum(row['copied'] for row in receipt), ruleEngineChanged=False, labUIImported=False), indent=2), encoding='utf8', newline='\n')
print(f'3C assets verified={len(receipt)} copied={sum(row["copied"] for row in receipt)} models={len(models)}')
