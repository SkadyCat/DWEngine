"""Copy the selected wind system's audited closure. Keep vendor assets unchanged."""
from pathlib import Path
import hashlib
import json
import shutil

root = Path(__file__).resolve().parents[2]
library = root / 'CustomTools/特效/Content'
selected = '/Game/_TornadoWindVFX/NS/NS_TopDown_Whirlwind'
audit = json.loads((root / 'Saved/Verification/WindVFX/library-audit.json').read_text(encoding='utf-8'))
candidate = next(c for c in audit['candidates'] if c['system'] == selected)
plan = []
for package in candidate['packages']:
    name = package['package']
    if not name.startswith('/Game/_TornadoWindVFX/') or any(c in ('World', 'Blueprint', 'AnimBlueprint', 'SkeletalMesh') for c in package['classes']):
        raise RuntimeError('Unexpected dependency ' + name)
    relative = Path(name.removeprefix('/Game/'))
    if not (library / relative).with_suffix('.uasset').is_file():
        raise RuntimeError('Missing ' + name)
    for ext in ('.uasset', '.uexp', '.ubulk'):
        source = (library / relative).with_suffix(ext)
        if not source.is_file():
            continue
        destination = root / 'Content' / source.relative_to(library)
        sha = hashlib.sha256(source.read_bytes()).hexdigest()
        if destination.exists() and hashlib.sha256(destination.read_bytes()).hexdigest() != sha:
            raise RuntimeError('Existing asset differs: ' + str(destination))
        plan.append((source, destination, sha))
for source, destination, sha in plan:
    if not destination.exists():
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)
    if hashlib.sha256(destination.read_bytes()).hexdigest() != sha:
        raise RuntimeError('Copy mismatch ' + str(destination))
receipt = {'archive': 'G:/Baiduyun/Tornado and Wind VFX with SFX 5.2 - 5.7.rar',
           'system': selected, 'package_count': len(candidate['packages']),
           'files': [{'path': str(dst.relative_to(root)).replace('\\', '/'), 'sha256': sha, 'bytes': dst.stat().st_size} for _, dst, sha in plan]}
(root / 'Docs/WindVFXMigration.json').write_text(json.dumps(receipt, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps({'system': selected, 'packages': len(candidate['packages']), 'files': len(plan), 'bytes': sum(d.stat().st_size for _, d, _ in plan)}))
