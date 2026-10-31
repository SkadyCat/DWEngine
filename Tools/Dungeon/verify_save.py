"""Verify a direct map save with source control disabled in our isolated editor."""
import json
from pathlib import Path
import unreal as u

ROOT=Path('G:/DungeonWorldOpus')
assert Path(u.Paths.project_dir()).resolve()==ROOT
world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
assert world.get_name()=='L_OpusDungeon'
actors=u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors()
count=sum(c.get_instance_count() for a in actors for c in a.get_components_by_class(u.InstancedStaticMeshComponent))
assert count==7448,count
assert u.EditorLoadingAndSavingUtils.save_map(world,'/Game/ClaudeDungeon/Maps/L_OpusDungeon')
u.SystemLibrary.execute_console_command(world,'MAP CHECK')
(ROOT/'Saved/Verification/DungeonExtraction/save-verified.json').write_text(json.dumps({'saved':True,'instances':count,'map':world.get_path_name()}),'utf8')
u.log('DUNGEON_SAVE_PASS instances=7448')
u.SystemLibrary.quit_editor()
