"""Reload the saved editor map and verify the placed battle participants."""
import collections
import json
import pathlib
import unreal

ROOT = pathlib.Path(__file__).resolve().parents[1]
MAP = '/Game/ThreeCLab/Maps/L_ThreeCArena'
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not levels.load_level(MAP):
    raise RuntimeError('Saved arena could not be reloaded')
cls = unreal.load_class(None, '/Script/Angelscript.ThreeCFighter')
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
fighters = [actor for actor in actors if actor.get_class() == cls]
indices = {actor.get_editor_property('index') for actor in fighters}
teams = collections.Counter(actor.get_editor_property('team') for actor in fighters)
models = {str(actor.get_editor_property('model_id')) for actor in fighters}
assert len(fighters) == 100 and indices == set(range(100)), (len(fighters), indices)
assert dict(teams) == {0: 34, 1: 33, 2: 33}, teams
assert len(models) == 15, models
assert all(actor.get_component_by_class(unreal.SkeletalMeshComponent).skeletal_mesh is not None for actor in fighters)
receipt = {'map': MAP, 'reloaded': True, 'fighters': len(fighters),
           'factions': [teams[i] for i in range(3)], 'models': sorted(models)}
(ROOT / 'Docs/map-reload.json').write_text(json.dumps(receipt, indent=2), encoding='utf8')
unreal.log('C3_MAP_RELOAD_PASS fighters=100 factions=34/33/33 models=15')
