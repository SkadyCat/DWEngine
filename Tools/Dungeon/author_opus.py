"""Create an Opus-owned scene from Zero's geometry and environment export.

Run in a dedicated Opus editor using -ExecutePythonScript; never in the source editor.
The post-tick save waits for real PCG instances and working navigation.
"""
import json
import time
import traceback
from collections import Counter
from pathlib import Path
import unreal as u

ROOT = Path('G:/DungeonWorldOpus')
assert Path(u.Paths.project_dir()).resolve() == ROOT
OUT = ROOT/'Saved/Verification/DungeonExtraction'
source = json.loads((OUT/'zero-source.json').read_text('utf8'))
MAP = '/Game/ClaudeDungeon/Maps/L_OpusDungeon'
actors = u.get_editor_subsystem(u.EditorActorSubsystem)
levels = u.get_editor_subsystem(u.LevelEditorSubsystem)
assert not u.EditorAssetLibrary.does_asset_exist(MAP), 'Refusing to overwrite existing dungeon map'
assert levels.new_level(MAP)
world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
assert world.get_name() == 'L_OpusDungeon'
world.get_world_settings().set_editor_property('default_game_mode', u.load_class(None,'/Script/Angelscript.DWOTowerGameMode'))
gen = actors.spawn_actor_from_class(u.DungeonGenerator, u.Vector())
gen.set_actor_label('Opus_ZeroDungeon')
params = gen.get_editor_property('params')
assert params.import_text(source['params'])
gen.set_editor_property('params', params)
gen.get_editor_property('root').set_mobility(u.ComponentMobility.STATIC)
graph = u.load_asset('/Game/ClaudeDungeon/PCG/PCG_ClaudeDungeon')
assert graph
gen.setup_graph(graph)

def decoded(value, old):
    if isinstance(value,dict):
        if 'enum' in value:
            return getattr(getattr(u,value['enum']),value['name'])
        if 'asset' in value:
            return u.load_asset(value['asset'])
        if 'text' in value:
            assert old.import_text(value['text'])
            return old
    if isinstance(value,list):
        return type(old)(*value)
    return value

def apply(obj, values):
    for name,value in values.items():
        obj.set_editor_property(name, decoded(value,obj.get_editor_property(name)))

for row in source['environment']:
    cls = u.load_class(None,row['class'])
    a = actors.spawn_actor_from_class(cls,u.Vector(*row['location']),u.Rotator(**row['rotation']))
    a.set_actor_scale3d(u.Vector(*row['scale']))
    a.set_actor_label(row['label'])
    if 'light' in row:
        c = a.get_component_by_class(u.LightComponentBase)
        c.set_mobility(u.ComponentMobility.STATIONARY)
        apply(c,row['light'])
    if 'fog' in row:
        apply(a.get_component_by_class(u.ExponentialHeightFogComponent),row['fog'])
    if 'postprocess' in row:
        apply(a,row['postprocess'])

start = actors.spawn_actor_from_class(u.PlayerStart,gen.get_player_start_location()+u.Vector(0,0,120))
start.set_actor_label('Opus_DungeonStart')
nav = actors.spawn_actor_from_class(u.NavMeshBoundsVolume,u.Vector(5200,5200,100))
nav.set_actor_label('Opus_DungeonNavigation')
assert u.DungeonPCGLibrary.configure_navigation_bounds(nav,u.Vector(5400,5400,700))
gen.regenerate()
started = time.monotonic()
last_report = 0
nav_configured = False
geometry_nav_rebuilt = False
finalizing = False

def tick(dt):
    global last_report,nav_configured,geometry_nav_rebuilt,finalizing
    if finalizing:
        return
    try:
        elapsed = time.monotonic()-started
        all_actors = list(actors.get_all_level_actors())
        if not nav_configured:
            recasts = [a for a in all_actors if isinstance(a,u.RecastNavMesh)]
            for r in recasts:
                r.set_editor_property('runtime_generation',u.RuntimeGenerationType.DYNAMIC)
                r.set_editor_property('force_rebuild_on_load',True)
            if recasts:
                nav_configured=True
                u.SystemLibrary.execute_console_command(world,'RebuildNavigation')
        count = sum(c.get_instance_count() for a in all_actors for c in a.get_components_by_class(u.InstancedStaticMeshComponent))
        if elapsed-last_report>10:
            last_report=elapsed
            u.log('DUNGEON_AUTHOR_WAIT seconds=%.1f instances=%d' % (elapsed,count))
        if elapsed>240:
            raise RuntimeError('Timed out waiting for dungeon/navigation')
        if not geometry_nav_rebuilt and count==source['instances'] and not gen.is_generation_in_progress():
            geometry_nav_rebuilt=True
            u.SystemLibrary.execute_console_command(world,'RebuildNavigation')
            return
        if elapsed<5 or gen.is_generation_in_progress() or count != source['instances'] or not u.OpusDungeon.ready(gen):
            return
        points = u.OpusDungeon.walkable_points(gen,95)
        assert len(points)>=20, len(points)
        assert len(gen.get_rooms())==len(source['rooms'])
        finalizing=True
        assert u.EditorLoadingAndSavingUtils.save_map(world,MAP)
        u.SystemLibrary.execute_console_command(world,'MAP CHECK')
        report = {'map':MAP,'rooms':len(gen.get_rooms()),'instances':count,'walkablePoints':len(points),
            'classes':dict(Counter(a.get_class().get_path_name() for a in all_actors)),
            'gameMode':world.get_world_settings().get_editor_property('default_game_mode').get_path_name(),
            'saved':True,'params':params.export_text(),'sourceMapCopied':False}
        (OUT/'author-result.json').write_text(json.dumps(report,indent=2),'utf8')
        u.log('DUNGEON_AUTHOR_PASS rooms=%d instances=%d points=%d' % (len(gen.get_rooms()),count,len(points)))
        u.unregister_slate_post_tick_callback(callback)
        u.SystemLibrary.quit_editor()
    except Exception:
        finalizing=True
        (OUT/'author-error.txt').write_text(traceback.format_exc(),'utf8')
        u.log_error(traceback.format_exc())
        u.unregister_slate_post_tick_callback(callback)
        u.SystemLibrary.quit_editor()

callback = u.register_slate_post_tick_callback(tick)
