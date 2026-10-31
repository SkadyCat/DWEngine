"""Stage the supplied archive in the existing VFX library, preserving package paths."""
from pathlib import Path
import hashlib
import json
import shutil

root = Path(__file__).resolve().parents[2]
source = root / 'Saved/AssetImports/TornadoWind/_TornadoWindVFX'
target = root / 'CustomTools/特效/Content/_TornadoWindVFX'
proof = root / 'Saved/Verification/WindVFX'
proof.mkdir(parents=True, exist_ok=True)
plan = []
for path in sorted(source.rglob('*')):
    if not path.is_file():
        continue
    relative = path.relative_to(source)
    if relative.parts[0] == 'DEMO':
        continue
    if path.suffix not in ('.uasset', '.uexp', '.ubulk'):
        raise RuntimeError('Unexpected file: ' + str(relative))
    destination = target / relative
    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    if destination.exists() and hashlib.sha256(destination.read_bytes()).hexdigest() != digest:
        raise RuntimeError('Existing library asset differs: ' + str(destination))
    plan.append((path, destination, digest))
for source_path, destination, digest in plan:
    if not destination.exists():
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source_path, destination)
    if hashlib.sha256(destination.read_bytes()).hexdigest() != digest:
        raise RuntimeError('Copy hash mismatch: ' + str(destination))
(proof / 'library-staging.json').write_text(json.dumps({'files': [
    {'path': str(destination.relative_to(root)), 'sha256': digest}
    for _, destination, digest in plan
]}, indent=2), encoding='utf-8')
print('WIND_LIBRARY_STAGED files=' + str(len(plan)))
