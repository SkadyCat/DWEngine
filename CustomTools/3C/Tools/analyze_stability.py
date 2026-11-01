"""Check actual 100-unit battle telemetry, including low frame-rate runs."""
import argparse
import collections
import csv
import json
import pathlib
import statistics

ROOT = pathlib.Path(__file__).resolve().parents[1]
COUNTERS = ('attacks', 'hits', 'deaths', 'target_changes', 'short_target_changes',
            'action_retargets', 'anim_starts', 'hit_restarts', 'short_clip_cuts',
            'action_turn_frames', 'abrupt_turn_frames', 'turn_reversals')


def inspect(tag):
    rows = list(csv.DictReader((ROOT / 'Saved/Diagnostics' / (tag + '.csv')).open(encoding='utf8')))
    grouped = collections.defaultdict(list)
    for row in rows:
        grouped[int(row['id'])].append(row)
    assert set(grouped) == set(range(100)), (tag, 'missing fighters')
    teams = collections.Counter(int(values[0]['team']) for values in grouped.values())
    assert dict(teams) == {0: 34, 1: 33, 2: 33}, (tag, teams)
    assert len({row['model'] for row in rows}) == 15, (tag, 'missing models')
    states = collections.Counter(row['state'] for row in rows)
    assert all(state in states for state in ('idle', 'run', 'attack1', 'hit', 'death')), (tag, states)
    totals = {key: sum(max(int(row[key]) for row in values) for values in grouped.values()) for key in COUNTERS}
    active = [row for row in rows if float(row['time']) > 2.5 and float(row['health']) > 0]
    attacks = [row for row in active if row['state'] in ('attack1', 'attack2') and float(row['anim_time']) > .15]
    moving = [row for row in attacks if float(row['speed']) > 25]
    result = {
        'tag': tag, 'mode': rows[-1]['mode'], 'units': 100, 'factions': [34, 33, 33],
        'models': 15, 'samples': len(rows), 'simulationSeconds': float(rows[-1]['time']),
        'realSeconds': float(rows[-1]['real_time']),
        'medianFrameMS': statistics.median(float(row['frame_ms']) for row in rows),
        'states': dict(states), 'events': totals,
        'maxTurnDegreesPerSecond': max(float(row['turn_deg_s']) for row in active),
        'movingAttackPercent': 100 * len(moving) / max(1, len(attacks)),
    }
    assert result['simulationSeconds'] >= 39, (tag, 'battle stopped early')
    if result['mode'] == 'controlled':
        for key in ('hit_restarts', 'short_clip_cuts', 'action_retargets', 'action_turn_frames', 'short_target_changes'):
            assert totals[key] == 0, (tag, key, totals[key])
        assert result['maxTurnDegreesPerSecond'] <= 300.1, (tag, 'turn limit', result)
        assert result['movingAttackPercent'] < 5, (tag, 'attack sliding', result)
        assert abs(result['simulationSeconds'] - result['realSeconds']) < .01, (tag, 'clock drift', result)
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('tags', nargs='+')
    parser.add_argument('--output', default='stability-analysis.json')
    args = parser.parse_args()
    report = {'verification': 'PASS', 'runs': [inspect(tag) for tag in args.tags]}
    (ROOT / 'Docs' / pathlib.Path(args.output).name).write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf8')
    print(json.dumps(report, ensure_ascii=False, indent=2))
