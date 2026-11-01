"""Read native equipment bounds, character sockets, evaluated grips and dependencies."""
import json
import pathlib
import unreal

ROOT = pathlib.Path(__file__).resolve().parents[1]
models = json.loads((ROOT / 'Config/models.json').read_text('utf8'))
migration = json.loads((ROOT / 'Docs/equipment-migration.json').read_text('utf8'))
registry = unreal.AssetRegistryHelpers.get_asset_registry()
registry.scan_paths_synchronous(['/Game/Melee/Wand', '/Game/Skeleton_Collection'], force_rescan=True)
registry.search_all_assets(True)
registry.wait_for_completion()
options = unreal.AssetRegistryDependencyOptions(
    include_hard_package_references=True, include_soft_package_references=True,
    include_searchable_names=False, include_hard_management_references=False,
    include_soft_management_references=False)
pending = list(migration['seeds'])
seen = set()
while pending:
    package = pending.pop().split('.')[0]
    if package in seen or not package.startswith('/Game/'):
        continue
    seen.add(package)
    assert (ROOT / 'Content' / (package[6:] + '.uasset')).is_file(), ('Missing UE dependency', package)
    assert unreal.load_asset(package) is not None, ('Could not load dependency', package)
    pending.extend(str(p) for p in (registry.get_dependencies(package, options) or []))


def vec(v):
    return [v.x, v.y, v.z]


def transform(t):
    r = t.rotation.rotator()
    return {'location': vec(t.translation), 'rotation': [r.pitch, r.yaw, r.roll], 'scale': vec(t.scale3d)}


equipment = []
for path in migration['seeds']:
    asset = unreal.load_asset(path)
    item = {'asset': path, 'class': asset.get_class().get_name()}
    if isinstance(asset, unreal.StaticMesh):
        box = asset.get_bounding_box()
        item['bounds'] = {'min': vec(box.min), 'max': vec(box.max)}
        item['materials'] = [str(s.material_interface.get_path_name()) for s in asset.static_materials if s.material_interface]
    else:
        item['materials'] = [str(s.material_interface.get_path_name()) for s in asset.materials if s.material_interface]
    equipment.append(item)

characters = []
for model in models:
    mesh = unreal.load_asset(model['mesh'])
    item = {'id': model['id'],
            'materials': [str(s.material_interface.get_path_name()) for s in mesh.materials if s.material_interface], 'poses': {}}
    opts = unreal.AnimPoseEvaluationOptions()
    opts.set_editor_property('optional_skeletal_mesh', mesh)
    opts.set_editor_property('extract_root_motion', True)
    opts.set_editor_property('incorporate_root_motion_into_pose', False)
    for state in ('idle', 'attack1'):
        clip = unreal.load_asset(model['clips'][state])
        pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(clip, clip.get_play_length() * .25, opts)
        names = {str(n) for n in unreal.AnimPoseExtensions.get_bone_names(pose)}
        socket_names = unreal.AnimPoseExtensions.get_socket_names(pose)
        item['sockets'] = [str(n) for n in socket_names]
        item['poses'][state] = {name: transform(unreal.AnimPoseExtensions.get_bone_pose(pose, name, unreal.AnimPoseSpaces.WORLD))
                                for name in ('hand_l', 'hand_r', 'middle_01_l', 'middle_01_r', 'index_01_l', 'index_01_r', 'weapon', 'weapon2', 'Bow1') if name in names}
        item['poses'][state].update({str(n): transform(unreal.AnimPoseExtensions.get_socket_pose(pose, n, unreal.AnimPoseSpaces.WORLD)) for n in socket_names})
    characters.append(item)

report = {'verification': 'PASS', 'dependencies': sorted(seen), 'equipment': equipment, 'characters': characters}
(ROOT / 'Docs/equipment-assets.json').write_text(json.dumps(report, indent=2), encoding='utf8')
unreal.log('C3_EQUIPMENT_AUDIT_PASS assets=%d models=%d dependencies=%d' % (len(equipment), len(characters), len(seen)))
