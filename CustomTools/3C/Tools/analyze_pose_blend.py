"""Compare evaluated native bone poses, not just montage API success."""
import collections
import csv
import json
import math
import pathlib

ROOT = pathlib.Path(__file__).resolve().parents[1]
rows = list(csv.DictReader((ROOT / 'Saved/Diagnostics/pose-probe.csv').open(encoding='utf8')))
groups = collections.defaultdict(list)
for row in rows:
    groups[(row['model'], int(row['blended']))].append(row)
keys = [bone + '_' + axis for bone in ('head', 'left', 'right') for axis in 'xyz']


def pose(row):
    return [float(row[key]) for key in keys]


def distance(a, b):
    return math.sqrt(sum((x-y)**2 for x, y in zip(a, b)))


def inspect(values):
    before = pose([row for row in values if float(row['transition_time']) < 0][-1])
    after = pose(values[-1])
    changing = [row for row in values if 0 <= float(row['transition_time']) <= .4]
    path = [before] + [pose(row) for row in changing]
    steps = [distance(a, b) for a, b in zip(path, path[1:])]
    span = distance(before, after)
    middle = [row for row in changing if .001 < float(row['transition_time']) < .22
              and distance(pose(row), before) > .001 * max(1, span)
              and distance(pose(row), after) > .001 * max(1, span)]
    return {'spanCm': span, 'maxFrameStepCm': max(steps), 'intermediatePoses': len(middle),
            'minimumSlotWeight': min(float(row['slot_weight']) for row in values),
            'destination': after}


report = []
assert len(groups) == 30, ('Expected all 15 models in both modes', len(groups))
for model in sorted({key[0] for key in groups}):
    hard, blend = inspect(groups[(model, 0)]), inspect(groups[(model, 1)])
    assert distance(hard['destination'], blend['destination']) < .05, (model, 'different destination', hard, blend)
    assert blend['minimumSlotWeight'] > .98, (model, 'reference-pose leakage', blend)
    if hard['spanCm'] > .1:
        assert blend['intermediatePoses'] >= 3, (model, 'missing evaluated blend', hard, blend)
        assert blend['maxFrameStepCm'] < hard['maxFrameStepCm'] * .75, (model, 'no continuity improvement', hard, blend)
    for result in (hard, blend):
        del result['destination']
    report.append({'model': model, 'hardCut': hard, 'crossfade': blend})
output = {'verification': 'PASS', 'models': 15, 'cases': 30, 'blendSeconds': .22,
          'probe': 'Frozen attack recovery -> frozen idle, actual head and both hand positions every frame',
          'results': report}
(ROOT / 'Docs/pose-blend-analysis.json').write_text(json.dumps(output, indent=2), encoding='utf8')
print(json.dumps(output, indent=2))
