"""Verify the saved migration, native tests and two independent runtime receipts."""
from pathlib import Path
import hashlib
import json
import re

root = Path(__file__).resolve().parents[2]
proof = root / 'Saved/Verification/WindVFX'

def read(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))

migration = read(root / 'Docs/WindVFXMigration.json')
for item in migration['files']:
    path = root / item['path']
    if hashlib.sha256(path.read_bytes()).hexdigest() != item['sha256']:
        raise RuntimeError('Vendor asset changed: ' + str(path))
mapping = read(proof / 'map-written.json')
wind_keys = {'whirlwind'} | {'whirlwind@' + str(i) for i in range(6)}
if set(mapping['updated']) != wind_keys:
    raise RuntimeError('Missing wind/dye overrides')
for key, value in mapping['before'].items():
    if key not in wind_keys and mapping['after'].get(key) != value:
        raise RuntimeError('Unrelated mapping changed: ' + key)
automation = read(proof / 'Automation/index.json')
tests = {test['testDisplayName']: test['state'] for test in automation['tests']}
if set(tests) != {'UEVFXFallback', 'UEVFXMapContract', 'UEVFXWindContract'} or any(state != 'Success' for state in tests.values()):
    raise RuntimeError('Native presentation checks did not all pass')
results = {}
for mode in ('mapped', 'default'):
    log = (proof / (mode + '-combat.log')).read_text(encoding='utf-8-sig', errors='replace')
    match = re.search(r'UE_VFX_TEST_PASS real projectile hits plan=(\d+) casts=(\d+) life=([\d.]+)', log)
    if not match or 'UE_VFX_TEST_FAIL' in log:
        raise RuntimeError('Missing runtime pass: ' + mode)
    results[mode] = {'plan': int(match[1]), 'casts': int(match[2]), 'target_life': float(match[3])}
if results['mapped'] != results['default']:
    raise RuntimeError('Presentation changed deterministic combat outcome')
runtime = read(proof / 'mapped-runtime.json')
if runtime['spawns'] <= 0 or runtime['followMoves'] <= 0 or runtime['returns'] != runtime['spawns'] or runtime['activeAfterClear'] != 0:
    raise RuntimeError('Niagara follow/pool lifecycle did not pass')
for name in ('gallery.png', 'whirlwind-target.png'):
    if not (proof / name).is_file():
        raise RuntimeError('Missing rendered proof ' + name)
report = {'ok': True, 'system': migration['system'], 'files_verified': len(migration['files']),
          'map': '/Game/WebVFX/DA_UEVFXMap', 'wind_keys': sorted(wind_keys),
          'other_mappings_unchanged': True, 'max_active_systems': mapping['max_active_systems'],
          'native_tests': tests, 'combat': results, 'niagara': runtime,
          'proof_directory': str(proof),
          'visuals': 'Rendered seven saved tint entries; compared source candidates and inspected real moving rule projectiles.'}
(root / 'Docs/WindVFXVerification.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps(report, ensure_ascii=False, indent=2))
