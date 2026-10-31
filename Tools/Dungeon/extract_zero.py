"""Copy only the inspected geometry plugin and explicit dungeon asset closure."""
import hashlib
import json
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SOURCE = Path('G:/DungeonWorldZero')
PROOF = ROOT/'Saved/Verification/DungeonExtraction'
report = json.loads((PROOF/'zero-source.json').read_text('utf8'))
paths = [Path('Plugins/ClaudeDungeon/ClaudeDungeon.uplugin')]
paths += [p.relative_to(SOURCE) for p in (SOURCE/'Plugins/ClaudeDungeon/Source').rglob('*') if p.is_file()]
for package in report['packages']:
    assert package.startswith(('/Game/ClaudeDungeonKit/','/Game/ClaudeDungeon/PCG/'))
    rel = Path('Content')/package.removeprefix('/Game/')
    files = [p for p in (SOURCE/rel).parent.glob(rel.name+'.*') if p.suffix in ('.uasset','.ubulk','.uexp')]
    assert files, package
    paths += [p.relative_to(SOURCE) for p in files]
entries = []
for rel in sorted(set(paths)):
    src, dst = SOURCE/rel, ROOT/rel
    digest = hashlib.sha256(src.read_bytes()).hexdigest()
    if dst.exists():
        assert hashlib.sha256(dst.read_bytes()).hexdigest() == digest, 'Refusing overwrite '+str(dst)
    else:
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src,dst)
    assert hashlib.sha256(dst.read_bytes()).hexdigest() == digest
    entries.append({'path':rel.as_posix(),'bytes':src.stat().st_size,'sha256':digest})
out = ROOT/'Docs/ZeroDungeon'
out.mkdir(parents=True,exist_ok=True)
(out/'source-assets.json').write_text(json.dumps({'source':str(SOURCE),'files':entries,'packages':report['packages'],
    'externalPackages':report['externalPackages'],'sourceMapCopied':False},indent=2),'utf8')
print('DUNGEON_EXTRACT_PASS files=%d bytes=%d' % (len(entries),sum(e['bytes'] for e in entries)))
