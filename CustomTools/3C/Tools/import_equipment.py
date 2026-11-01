"""Copy existing visual equipment into this isolated lab; never overwrite assets."""
import hashlib
import json
import pathlib
import re
import shutil

ROOT = pathlib.Path(__file__).resolve().parents[1]
SOURCE = pathlib.Path('G:/DungeonWorldBeta/Content')
SEEDS = [
    'Skeleton_Collection/Skeleton_Samurai/Meshes/SM_Katana',
    'Skeleton_Collection/Skeleton_Spearman/Meshes/Spear',
    'Skeleton_Collection/Skeleton_Spearman/Meshes/Shield',
    'Skeleton_Collection/Skeleton_swordsman/Meshes/SM_Sword',
    'Skeleton_Collection/Skeleton_with_axe/Meshes/SM_Axe',
    'Skeleton_Collection/Skeleton_Archer/Meshes/SeparateMesh/SK_Bow',
    'Melee/Wand/SM_CoalLamp_Wand',
]


def digest(path):
    return hashlib.file_digest(path.open('rb'), 'sha256').hexdigest() if hasattr(hashlib, 'file_digest') else hashlib.sha256(path.read_bytes()).hexdigest()


pending = list(SEEDS)
packages = set()
while pending:
    package = pending.pop()
    if package in packages:
        continue
    path = SOURCE / (package + '.uasset')
    assert path.is_file(), path
    packages.add(package)
    # Seed the local copy from serialized package names. The UE asset audit then
    # validates the complete dependency closure with the actual Asset Registry.
    for ref in re.findall(rb'/Game/[A-Za-z0-9_/]+', path.read_bytes()):
        relative = ref.decode('ascii')[6:]
        if (SOURCE / (relative + '.uasset')).is_file():
            pending.append(relative)

files = []
for package in sorted(packages):
    for suffix in ('.uasset', '.uexp', '.ubulk', '.uptnl'):
        source = SOURCE / (package + suffix)
        if not source.is_file():
            continue
        target = ROOT / 'Content' / source.relative_to(SOURCE)
        copied = not target.exists()
        if copied:
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, target)
        same = digest(source) == digest(target)
        assert same, ('Existing asset differs; left untouched', target)
        files.append({'path': str(target.relative_to(ROOT)).replace('\\', '/'),
                      'copied': copied, 'sha256': digest(target)})
report = {'source': str(SOURCE), 'seeds': ['/Game/' + p for p in SEEDS],
          'packages': len(packages), 'copiedFiles': sum(f['copied'] for f in files), 'files': files}
(ROOT / 'Docs/equipment-migration.json').write_text(json.dumps(report, indent=2), encoding='utf8')
print(json.dumps({'packages': report['packages'], 'copiedFiles': report['copiedFiles']}))
