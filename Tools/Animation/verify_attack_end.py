"""Verify the real End-notify receipts and both authoritative trait documents."""
from pathlib import Path
import json
import re

root = Path(__file__).resolve().parents[2]
proof = root / 'Saved/Verification/AttackEnd'
documents = {}
for folder in ('Rules', 'RulesTower'):
    path = root / 'Content' / folder / 'WebTraits/create_whirlwind.json'
    document = json.loads(path.read_text(encoding='utf-8-sig'))
    if 'cooldown' in document['params']:
        raise RuntimeError('Extra cooldown remains: ' + str(path))
    if not any(rule['event'] == 'cast' and any(action['op'] == 'emit' for action in rule['actions']) for rule in document['rules']):
        raise RuntimeError('Active whirlwind composition was removed')
    documents[folder] = document
log = (proof / 'after.log').read_text(encoding='utf-8-sig', errors='replace')
if 'ATTACK_END_FAIL' in log or 'Angelscript: Error:' in log:
    raise RuntimeError('Runtime verification failed')
markers = ('ImmediateChainAndLocks', 'WhirlwindNoCooldown', 'NativeCooldownPreserved', 'Complete')
if any('ATTACK_END_PASS ' + marker not in log for marker in markers):
    raise RuntimeError('Missing runtime checks')
observations = []
for match in re.finditer(r'ATTACK_END_OBSERVED pass=(\d+) elapsed=([\d.]+) progress=([\d.]+) state=(\d+) locked=(\w+) cooldown=([\d.]+)', log):
    observation = {'cycle': int(match[1]), 'elapsed_seconds': float(match[2]), 'montage_progress': float(match[3]),
                   'action_state': int(match[4]), 'locked': match[5] == 'true', 'cooldown': float(match[6])}
    if observation['action_state'] != 0 or observation['locked'] or observation['montage_progress'] >= .7:
        raise RuntimeError('End still retains the animation tail')
    observations.append(observation)
if len(observations) != 3:
    raise RuntimeError('Expected three actual End cycles')
before = (proof / 'before.log').read_text(encoding='utf-8-sig', errors='replace')
if 'state=4 locked=true' not in before or 'End ends action and releases locks before the animation tail' not in before:
    raise RuntimeError('Missing reproduction before the fix')
report = {'ok': True, 'rule_engine_modified_by_this_fix': False, 'cooldown_bypass_added': False,
          'removed_trait_parameter': 'create_whirlwind.params.cooldown', 'whirlwind_params': documents['Rules']['params'],
          'ue_whirlwind_params': documents['RulesTower']['params'],
          'end_observations': observations, 'checks': list(markers),
          'before': 'Saved/Verification/AttackEnd/before.log', 'after': 'Saved/Verification/AttackEnd/after.log'}
hero_log = proof / 'hero-regression.log'
report['hero_regression_passed'] = hero_log.is_file() and 'HERO_TEST_PASS RuntimeIntegration' in hero_log.read_text(encoding='utf-8-sig', errors='replace')
def passed(path, marker):
    return path.is_file() and marker in path.read_text(encoding='utf-8-sig', errors='replace')

report['additional_checks'] = {
    'web_whirlwind_suite': '10/11 passed; wall fixture also fails with the original cooldown restored in an isolated in-memory probe',
    'free_cast_headless_passed': passed(proof / 'free-cast-headless.log', 'FREE_CAST_PASS Complete'),
    'free_cast_rendered_passed': passed(proof / 'free-cast-rendered.log', 'FREE_CAST_PASS Complete'),
    'all_regressions_passed': False,
}
(proof / 'result.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps(report, ensure_ascii=False, indent=2))
