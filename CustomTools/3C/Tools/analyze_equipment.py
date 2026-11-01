"""Validate actual evaluated attachments across all models and action clips."""
import collections
import csv
import json
import math
import pathlib

ROOT = pathlib.Path(__file__).resolve().parents[1]
profiles = json.loads((ROOT / 'Config/equipment.json').read_text('utf8'))
with (ROOT / 'Saved/Diagnostics/equipment-probe.csv').open(encoding='utf8') as stream:
    rows = list(csv.DictReader(stream))
groups = collections.defaultdict(list)
for row in rows:
    groups[(row['model'], row['state'], int(row['slot']))].append(row)
states = {'idle', 'run', 'attack1', 'hit', 'death'}
expected = {(p['id'], s, i) for p in profiles for s in states
            for i in (range(len(p['parts'])) if p['parts'] else [-1])}
assert set(groups) == expected, ('Missing model/action/equipment samples', expected - set(groups))
assert all(len(rs) >= 5 for rs in groups.values())
for key, rs in groups.items():
    positions = [float(r['pose_time']) for r in rs]
    assert max(positions)-min(positions) > float(rs[0]['clip_length'])*.75, ('Clip range was skipped', key)
for profile in profiles:
    death = [r for r in rows if r['model'] == profile['id'] and r['state'] == 'death']
    span = math.sqrt(sum((max(float(r[k]) for r in death)-min(float(r[k]) for r in death))**2 for k in ('hand_x','hand_y','hand_z')))
    assert span > 2, ('Native skeletal pose did not update', profile['id'], span)
assert all(r['ready'] == '1' and r['collision'] == '0' and r['parent_ok'] == '1' for r in rows)
assert max(float(r['follow_error']) for r in rows) < .01
assert max(float(r['grip_error']) for r in rows) < .02, ('Equipment is no longer in the calibrated hand grip', sorted(rows, key=lambda r: float(r['grip_error']), reverse=True)[:3])
report = {'verification': 'PASS', 'models': len(profiles), 'modelStates': len(profiles) * len(states),
          'slotCases': len(groups), 'samples': len(rows),
          'maxFollowErrorCm': max(float(r['follow_error']) for r in rows),
          'maxGripErrorCm': max(float(r['grip_error']) for r in rows),
          'results': [{'model': p['id'], 'externalParts': len(p['parts']), 'builtIn': p['builtIn'],
                       'maxGripErrorCm': max(float(r['grip_error']) for r in rows if r['model'] == p['id'])}
                      for p in profiles]}
(ROOT / 'Docs/equipment-analysis.json').write_text(json.dumps(report, indent=2), encoding='utf8')
print(json.dumps({k: v for k, v in report.items() if k != 'results'}))
