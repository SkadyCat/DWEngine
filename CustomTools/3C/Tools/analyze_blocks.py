"""Check native capsule/RVO blockage, stable waiting and clearance recovery."""
import argparse
import collections
import csv
import json
import math
import pathlib

ROOT = pathlib.Path(__file__).resolve().parents[1]


def inspect(tag):
    groups = collections.defaultdict(list)
    with (ROOT / 'Saved/Diagnostics' / (tag + '-block-probe.csv')).open(encoding='utf8') as stream:
        for row in csv.DictReader(stream):
            groups[(row['model'], row['obstacle'], int(row['guarded']))].append(row)
    assert len(groups) == 12, (tag, 'missing cases', len(groups))
    results = []
    for (model, obstacle, guarded), rows in groups.items():
        held = [row for row in rows if 2.5 <= float(row['time']) <= 5.8]
        assert len(held) >= 40, (tag, model, obstacle, 'missing hold samples')
        pairs = list(zip(held, held[1:]))
        yaw_motion = sum(abs((float(b['yaw']) - float(a['yaw']) + 180) % 360 - 180) for a, b in pairs)
        path = sum(math.hypot(float(b['x']) - float(a['x']), float(b['y']) - float(a['y'])) for a, b in pairs)
        release = next(row for row in rows if float(row['time']) >= 6)
        recovery = next((float(row['time']) - float(release['time']) for row in rows
                         if float(row['time']) >= 6 and float(row['x']) - float(release['release_x']) >= 25), None)
        result = {'model': model, 'obstacle': obstacle, 'guarded': bool(guarded),
                  'stateSwitchesWhileHeld': sum(a['state'] != b['state'] for a, b in pairs),
                  'yawMotionDegreesWhileHeld': yaw_motion, 'bodyPathCmWhileHeld': path,
                  'blockedSamplePercent': 100 * sum(row['blocked'] == '1' for row in held) / len(held),
                  'maxForwardXWhileHeld': max(float(row['x']) for row in held),
                  'resume25CmSeconds': recovery, 'statesWhileHeld': dict(collections.Counter(row['state'] for row in held))}
        limit = 211 if obstacle == 'wall' else 245
        assert result['maxForwardXWhileHeld'] < limit, (tag, 'obstruction did not block', result)
        assert recovery is not None and recovery < 1.0, (tag, 'did not recover after opening', result)
        if guarded:
            assert result['blockedSamplePercent'] >= 99, (tag, 'did not remain blocked', result)
            assert result['stateSwitchesWhileHeld'] == 0 and set(result['statesWhileHeld']) == {'idle'}, (tag, 'idle/gait chatter', result)
            assert yaw_motion < .1, (tag, 'blocked facing chatter', result)
            assert path < 4, (tag, 'blocked body jitter', result)
        results.append(result)
    return {'tag': tag, 'cases': len(results), 'results': results}


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('tags', nargs='+')
    args = parser.parse_args()
    report = {'verification': 'PASS', 'runs': [inspect(tag) for tag in args.tags]}
    (ROOT / 'Docs/block-analysis.json').write_text(json.dumps(report, indent=2), encoding='utf8')
    print(json.dumps(report, indent=2))
