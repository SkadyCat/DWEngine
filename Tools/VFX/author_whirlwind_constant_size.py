"""Derive a wind presentation with constant mesh size; retain vendor assets and rules."""
from pathlib import Path
import hashlib
import json
import unreal as u

root = Path('G:/DungeonWorldOpus')
proof = root / 'Saved/Verification/WhirlwindParity/DungeonGround'
proof.mkdir(parents=True, exist_ok=True)
vendor_file = root / 'Content/_TornadoWindVFX/NS/NS_TopDown_Whirlwind.uasset'
vendor_hash = hashlib.sha256(vendor_file.read_bytes()).hexdigest()
source = '/Game/_TornadoWindVFX/NS/NS_TopDown_Whirlwind'
destination = '/Game/WebVFX/Systems/NS_OpusWhirlwind'
system = u.load_asset(destination)
if system is None:
    u.EditorAssetLibrary.make_directory('/Game/WebVFX/Systems')
    system = u.EditorAssetLibrary.duplicate_asset(source, destination)
if not isinstance(system, u.NiagaraSystem):
    raise RuntimeError('Could not create derived wind system')

changes = []
for obj in u.ObjectIterator():
    if isinstance(obj, type):
        continue
    path = u.SystemLibrary.get_path_name(obj)
    if not path.startswith(system.get_path_name() + ':NE_Cycclone'):
        continue
    if obj.get_class() is None or obj.get_class().get_name() != 'NiagaraDataInterfaceCurve':
        continue
    curve = obj.get_editor_property('curve')
    keys = list(curve.get_editor_property('keys'))
    values = [(k.get_editor_property('time'), k.get_editor_property('value')) for k in keys]
    # Graph and compiled DI copies share the same unambiguous size ramp.
    if len(values) != 2 or abs(values[0][0]) > 1e-6 or abs(values[1][0]-1) > 1e-6 or abs(values[1][1]-1) > 1e-6:
        continue
    if abs(values[0][1]-.5) > 1e-6 and abs(values[0][1]-1) > 1e-6:
        continue
    for key in keys:
        key.set_editor_property('value', 1.)
    curve.set_editor_property('keys', keys)
    obj.set_editor_property('curve', curve)
    # This property notification rebuilds the curve's serialized GPU LUT.
    obj.set_editor_property('CurveAsset', None)
    changes.append({'path': path, 'before': values, 'after': [(k.get_editor_property('time'), k.get_editor_property('value')) for k in obj.get_editor_property('curve').get_editor_property('keys')]})
if len(changes) < 12:
    raise RuntimeError('Expected the size curve for all twelve wind emitters: ' + str(len(changes)))
if not u.EditorAssetLibrary.save_loaded_asset(system, False):
    raise RuntimeError('Derived system save failed')
task = u.AssetExportTask()
task.object = system
task.filename = str(proof / 'wind-constant.t3d')
task.exporter = u.ObjectExporterT3D()
task.automated = True
task.prompt = False
task.replace_identical = True
u.Exporter.run_asset_export_task(task)

mapping = u.load_asset('/Game/WebVFX/DA_UEVFXMap')
entries = dict(mapping.get_editor_property('effects'))
updated = ['whirlwind'] + ['whirlwind@' + str(i) for i in range(6)]
for key in updated:
    entry = entries[u.Name(key)]
    entry.set_editor_property('system', system)
    entries[u.Name(key)] = entry
mapping.set_editor_property('effects', entries)
if not u.EditorAssetLibrary.save_loaded_asset(mapping, False):
    raise RuntimeError('Wind mapping save failed')
if hashlib.sha256(vendor_file.read_bytes()).hexdigest() != vendor_hash:
    raise RuntimeError('Vendor asset changed')
report = {'source': source, 'derived': destination, 'vendor_sha256': vendor_hash, 'size_ramps': changes,
          'updated_map_keys': updated, 'warmup': system.get_editor_property('warmup_time')}
(proof / 'constant-size-authored.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
u.log('WIND_CONSTANT_SIZE_AUTHORED curves=' + str(len(changes)))
