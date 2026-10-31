from pathlib import Path
import json
import unreal as u

world=u.EditorLoadingAndSavingUtils.new_blank_map(False)
actors=u.get_editor_subsystem(u.EditorActorSubsystem)
report=[]
for name in ['BP_Ice_Projectile_Spear','BP_Ice_Spear']:
 cls=u.EditorAssetLibrary.load_blueprint_class('/Game/IceSkill_VFX/Blueprint/'+name)
 actor=actors.spawn_actor_from_class(cls,u.Vector(0,0,0))
 row={'blueprint':name,'components':[]}
 for c in actor.get_components_by_class(u.NiagaraComponent):
  system=c.get_asset()
  row['components'].append({'system':system.get_path_name() if system else None,'rotation':str(c.get_editor_property('relative_rotation')),'scale':str(c.get_editor_property('relative_scale3d')),'location':str(c.get_editor_property('relative_location'))})
 report.append(row)
 actors.destroy_actor(actor)
mesh=u.load_asset('/Game/IceSkill_VFX/Resource/Mesh/SM_TwistIce_02')
bounds=mesh.get_bounds()
report.append({'mesh':mesh.get_path_name(),'origin':str(bounds.origin),'extent':str(bounds.box_extent)})
Path('G:/DungeonWorldOpus/Saved/Verification/UEVFX/source-orientation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
