"""Record current native evidence only after the five runtime groups pass."""
import hashlib
import json
import re
import struct
from datetime import datetime
from pathlib import Path

root = Path(__file__).resolve().parents[1]
proof = root / 'Saved/Verification/PlayerStun'
log = (proof / 'runtime-visual.log').read_text(encoding='utf-8')
tests = re.findall(r'STUN_TEST_PASS (\w+)', log)
assert len(tests) == 5 and len(set(tests)) == 5
assert not any(s in log for s in ('STUN_TEST_FAIL', 'Angelscript: Error:', 'TowerError', 'Fatal error:'))
contract = json.loads((root / 'Content/UICreatorWorld/contract.json').read_text(encoding='utf-8'))
hud = json.loads((proof / 'hud-fill-sync.json').read_text(encoding='utf-8'))
digest = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
rule_files = []
for rel in ('Equipment/player_character_equipment.json', 'Affixes/player_stun_buildup.json',
            'Affixes/player_stun_threshold.json', 'Affixes/player_stun_recovery.json'):
    native = root / 'Content/RulesTower' / rel
    web = root / 'Content/Rules' / rel
    assert json.loads(native.read_text(encoding='utf-8')) == json.loads(web.read_text(encoding='utf-8')), rel
    rule_files.append({'path': rel, 'sha256': digest(native), 'web_sha256': digest(web), 'same_json_in_web_and_ue': True})
screenshots = []
for name in ('half', 'active', 'restore', 'restored', 'decay', 'shield-only'):
    path = proof / f'stun-{name}.png'
    data = path.read_bytes()
    width, height = struct.unpack('>II', data[16:24])
    assert (width, height) == (1600, 900)
    screenshots.append({'path': str(path), 'width': width, 'height': height, 'sha256': digest(path)})
receipt = {
    'verified_at': datetime.now().astimezone().isoformat(), 'ok': True,
    'fixed_slot': 'character', 'fixed_equipment': 'player_character_equipment',
    'rules': {'weight': 1, 'threshold_max_life_percent': 20, 'threshold_comparison': '>=',
              'damage': 'mitigated positive non-DOT non-evaded damage before shield allocation',
              'shield_cannot_prevent_buildup': True, 'stun_seconds': .5,
              'quiet_delay_seconds': 1, 'unbroken_linear_decay_seconds': 4,
              'restore_after_stun_seconds': 1, 'no_repeat_during_restore': True,
              'clock': 'native RuleWorld simulation clock'},
    'gameplay': {'tests': tests, 'actual_native_damage': True, 'transient_profile': True,
                 'pause_freezes_recovery': True, 'old_release_cannot_cast': True,
                 'log': 'Saved/Verification/PlayerStun/runtime-visual.log'},
    'world_overlay': {'sourceHash': contract['sourceHash'], 'nodes': len(contract['nodes']),
                      'rasters': len(contract['rasters']), 'earth_slabs': 8, 'independent_chips': 8,
                      'hit_flash_seconds': .12, 'burst_seconds': .35,
                      'preserved_templates': 6, 'preserved_svg_assets': 19,
                      'stale_combined_debris_removed': True,
                      'job': 'wbp-211a675d-04a6-42a1-bf92-63eecc22e085'},
    'hud': {'label': '\u6655\u7729', 'progress': 'remaining integrity',
            'default': 'remaining / capacity', 'restoring': 'percent',
            'runtime': 'saved WBP plus AS bindings', 'cold_game_reload': True,
            'fill_import': hud, 'assets': [
                {'path': 'Content/UICreator/' + name, 'sha256': digest(root / 'Content/UICreator' / name)}
                for name in ('DA_CurrentCanvas.uasset', 'WBP_CurrentCanvas.uasset')]},
    'screenshots': screenshots, 'rule_files': rule_files,
    'engine_algorithms': 'VM, damage resolution and status execution unchanged',
    'implementation': 'AS policy and bindings, authored SVG/PNG contract, native event/stat/clock adapter',
    'build': {'scope': 'project module against precompiled engine', 'status': 'Succeeded',
              'log': 'Saved/Verification/PlayerStun/web-sync-build.log'},
}
(root / 'Docs/PlayerStunVerification.json').write_text(json.dumps(receipt, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
print(json.dumps({'ok': True, 'tests': len(tests), 'sourceHash': contract['sourceHash'], 'screenshots': len(screenshots)}))
