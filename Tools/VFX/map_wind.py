"""Author presentation-only overrides in Opus, preserving every existing key."""
from pathlib import Path
import json
import unreal as u

root = Path('G:/DungeonWorldOpus')
if Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir())).resolve() != root:
    raise RuntimeError('Run in the compiled Opus project, outside PIE')
path = '/Game/WebVFX/DA_UEVFXMap'
asset = u.load_asset(path)
if not isinstance(asset, u.DWUEVFXMap):
    raise RuntimeError('Existing override map is required')
system_path = '/Game/WebVFX/Systems/NS_OpusWhirlwind' if u.EditorAssetLibrary.does_asset_exist('/Game/WebVFX/Systems/NS_OpusWhirlwind') else '/Game/_TornadoWindVFX/NS/NS_TopDown_Whirlwind'
system = u.load_asset(system_path)
if not isinstance(system, u.NiagaraSystem):
    raise RuntimeError('Missing vendor wind system')
expected = {'Color01': 'LinearColor', 'Color2': 'LinearColor', 'ColorBottom': 'LinearColor',
            '_ColorHue': 'NiagaraFloat', '_Size': 'NiagaraFloat', '_Tall': 'NiagaraFloat',
            'Duration': 'NiagaraFloat', 'Saturation': 'NiagaraFloat', 'Brightness': 'NiagaraFloat',
            'LightOn': 'NiagaraBool', 'AudioOn': 'NiagaraBool', 'Optimized': 'NiagaraBool'}
available = {str(p.parameter_name): str(p.type_name) for p in u.NiagaraFunctionLibrary.get_all_user_parameters(system)}
if any(available.get(name) != kind for name, kind in expected.items()):
    raise RuntimeError('Vendor parameter contract changed: ' + str(available))

def snapshot(entries):
    result = {}
    for k, v in entries.items():
        scale, rotation, offset = (v.get_editor_property(p) for p in ('scale', 'rotation', 'offset'))
        result[str(k)] = {'system': str(v.get_editor_property('system')),
            'scale': [scale.x, scale.y, scale.z], 'rotation': [rotation.pitch, rotation.yaw, rotation.roll],
            'offset': [offset.x, offset.y, offset.z], 'follow': v.get_editor_property('follow_source'),
            'limit': v.get_editor_property('max_visual_seconds'),
            'floats': {str(a): b for a, b in v.get_editor_property('user_floats').items()},
            'bools': {str(a): b for a, b in v.get_editor_property('user_bools').items()},
            'colors': {str(a): [b.r, b.g, b.b, b.a] for a, b in v.get_editor_property('user_colors').items()}}
    return result

entries = dict(asset.get_editor_property('effects'))
before = snapshot(entries)
budget = asset.get_editor_property('max_active_systems')
# Physical wind is pale blue-gray; all six ground dye elements keep separate colors.
specs = [('whirlwind', (.45, .72, .86)),
         ('whirlwind@0', (1., .18, .025)),
         ('whirlwind@1', (.15, .65, 1.)),
         ('whirlwind@2', (1., .75, .07)),
         ('whirlwind@3', (.52, .68, .78)),
         ('whirlwind@4', (.15, 1., .08)),
         ('whirlwind@5', (.62, .12, 1.))]
for key, rgb in specs:
    entry = u.DWUEVFXEntry()
    entry.set_editor_property('system', system)
    entry.set_editor_property('follow_source', True)
    entry.set_editor_property('scale', u.Vector(.5, .5, .5))
    # The vendor hue control rotates the explicitly supplied colors; keep it neutral.
    entry.set_editor_property('user_floats', {'User._ColorHue': 0., 'User._Size': 1., 'User._Tall': 1.6,
                              'User.Duration': 30., 'User.Saturation': 4., 'User.Brightness': .6})
    entry.set_editor_property('user_colors', {'User.Color01': u.LinearColor(*rgb, 1.),
                              'User.Color2': u.LinearColor(*rgb, 1.),
                              'User.ColorBottom': u.LinearColor(*(c * .12 for c in rgb), 1.)})
    entry.set_editor_property('user_bools', {'User.AudioOn': False, 'User.LightOn': False, 'User.Optimized': True})
    entries[key] = entry
asset.set_editor_property('effects', entries)
after = snapshot(dict(asset.get_editor_property('effects')))
updated = {key for key, _ in specs}
if any(after.get(k) != v for k, v in before.items() if k not in updated):
    raise RuntimeError('Unrelated mapping changed')
if asset.get_editor_property('max_active_systems') != budget:
    raise RuntimeError('Budget changed')
if not u.EditorAssetLibrary.save_loaded_asset(asset, False):
    raise RuntimeError('Map save failed')
report = {'before': before, 'after': after, 'updated': sorted(updated), 'max_active_systems': budget}
(root / 'Saved/Verification/WindVFX/map-written.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
u.log('WIND_MAP_SAVED count=' + str(len(after)))
