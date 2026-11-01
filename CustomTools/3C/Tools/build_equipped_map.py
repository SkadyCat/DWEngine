"""Apply measured loadouts to the saved 100-actor editor scene and reload it."""
import json
import math
import pathlib
import runpy
import unreal

ROOT = pathlib.Path(__file__).resolve().parents[1]
runpy.run_path(str(ROOT / 'Tools/prepare_equipment.py'))
profiles = {p['id']: p for p in json.loads((ROOT / 'Config/equipment.json').read_text('utf8'))}
MAP = '/Game/ThreeCLab/Maps/L_ThreeCArena'
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert levels.load_level(MAP)
cls = unreal.load_class(None, '/Script/Angelscript.ThreeCFighter')


def component(actor, name):
    found = [c for c in actor.get_components_by_class(unreal.SceneComponent) if c.get_name() == name]
    assert len(found) == 1, (actor.get_name(), name, [c.get_name() for c in actor.get_components_by_class(unreal.SceneComponent)])
    return found[0]


def fit(part, body, data):
    assert part.attach_to_component(body, data['socket'], unreal.AttachmentRule.SNAP_TO_TARGET,
                                   unreal.AttachmentRule.SNAP_TO_TARGET, unreal.AttachmentRule.SNAP_TO_TARGET, False)
    t = data['transform']
    part.set_relative_transform(unreal.Transform(location=unreal.Vector(*t['location']),
                                               rotation=unreal.Rotator(pitch=t['rotation'][0],yaw=t['rotation'][1],roll=t['rotation'][2]),
                                               scale=unreal.Vector(*t['scale'])), False, True)
    part.set_collision_profile_name('NoCollision')
    part.set_editor_property('generate_overlap_events', False)
    part.set_visibility(True)


fighters = [a for a in actors.get_all_level_actors() if a.get_class() == cls]
assert len(fighters) == 100, len(fighters)
external = 0
built_in = 0
for actor in fighters:
    at = actor.get_actor_location()
    actor.set_actor_rotation(unreal.Rotator(pitch=0,yaw=math.degrees(math.atan2(-at.y,-at.x)),roll=0), False)
    profile = profiles[str(actor.get_editor_property('model_id'))]
    body = component(actor, 'CharacterMesh0')
    main, off, rig = [component(actor, name) for name in ('MainGear', 'OffGear', 'GearRig')]
    for c in (main, off, rig):
        c.set_visibility(False)
    built_in += bool(profile['builtIn'])
    for i, data in enumerate(profile['parts']):
        c = rig if data['skeletal'] else main if i == 0 else off
        asset = unreal.load_asset(data['asset'])
        assert asset is not None, data['asset']
        if data['skeletal']:
            c.set_skeletal_mesh_asset(asset)
            c.play_animation(unreal.load_asset(data['animation']), False)
            c.set_position(data['freezeTime'], False)
            c.set_play_rate(0)
            c.set_update_animation_in_editor(True)
        else:
            c.set_static_mesh(asset)
        fit(c, body, data)
        external += 1
world = unreal.EditorLevelLibrary.get_editor_world()
assert unreal.EditorLoadingAndSavingUtils.save_map(world, MAP)
assert levels.load_level('/Engine/Maps/Entry')
assert levels.load_level(MAP)
reloaded = [a for a in actors.get_all_level_actors() if a.get_class() == cls]
assert len(reloaded) == 100
seen = []
for actor in reloaded:
    profile = profiles[str(actor.get_editor_property('model_id'))]
    body = component(actor, 'CharacterMesh0')
    for i, data in enumerate(profile['parts']):
        c = component(actor, 'GearRig' if data['skeletal'] else 'MainGear' if i == 0 else 'OffGear')
        assert c.get_attach_parent() == body and str(c.get_attach_socket_name()) == data['socket']
        assert str(c.get_collision_profile_name()) == 'NoCollision'
        asset = c.get_skinned_asset() if data['skeletal'] else c.static_mesh
        assert asset.get_path_name().split('.')[0] == data['asset']
        local = c.get_relative_transform()
        expected = unreal.Vector(*data['transform']['location'])
        assert (local.translation - expected).length() < .001
        rotation = local.rotation.rotator()
        assert all(abs((a-b+180)%360-180)<.01 for a,b in zip((rotation.pitch,rotation.yaw,rotation.roll),data['transform']['rotation']))
        assert (local.scale3d-unreal.Vector(*data['transform']['scale'])).length()<.001
    actor_rotation = actor.get_actor_rotation()
    assert abs(actor_rotation.pitch)<.001 and abs(actor_rotation.roll)<.001
    seen.append({'id': int(actor.get_editor_property('index')), 'model': profile['id'], 'parts': len(profile['parts']), 'builtIn': profile['builtIn']})
report = {'verification': 'PASS', 'map': MAP, 'reloaded': True, 'actors': 100,
          'externalParts': external, 'builtInUnits': built_in, 'units': sorted(seen, key=lambda x: x['id'])}
(ROOT / 'Docs/equipment-map.json').write_text(json.dumps(report, indent=2), encoding='utf8')
unreal.log('C3_EQUIPMENT_MAP_PASS actors=100 external=%d builtin=%d reloaded=1' % (external, built_in))
