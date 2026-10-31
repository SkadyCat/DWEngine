"""Run with the Opus editor Python commandlet; edits Opus presentation clips only."""
import unreal,json
from pathlib import Path
unreal.load_module('AnimationBlueprintLibrary')
root=Path(unreal.Paths.project_dir())
base='/Game/Clean3C/Animation/Mage/Set/'
names=['Idle','Fireball_L','Hurt','Death']+['Run_'+d for d in ['F','FR','R','BR','B','BL','L','FL']]+['Dodge_'+d for d in ['F','FR','R','BR','B','BL','L','FL']]
results=[]
for name in names:
 path=base+'Set_Mage_'+name+'_IP'
 clip=unreal.load_asset(path)
 assert isinstance(clip,unreal.AnimSequence),path
 # Opus rules already emit successful casts and damage. Imported legacy action
 # notifies must never release a second projectile or run the old combat bridge.
 notify_count=len(unreal.AnimationLibrary.get_animation_notify_events(clip))
 unreal.AnimationLibrary.remove_all_animation_notify_tracks(clip)
 unreal.AnimationLibrary.set_root_motion_enabled(clip,False)
 unreal.AnimationLibrary.set_is_root_motion_lock_forced(clip,True)
 assert unreal.EditorAssetLibrary.save_loaded_asset(clip,False),path
 results.append({'asset':path,'legacyNotifiesRemoved':notify_count,'length':clip.get_play_length()})
mesh=unreal.load_asset('/Game/Characters/WizardHero/FingerRig/SK_WizardHero')
source=unreal.load_asset('/Game/G2_Mage/Meshes/Characters/Combines/SK_Judy_D3')
anim=unreal.load_class(None,'/Game/Characters/WizardHero/StanceFix20261018/ABP_WizardRetarget.ABP_WizardRetarget_C')
material=unreal.load_asset('/Game/Characters/WizardHero/Materials/Shimmer20261018/MI_WizardHero_Shimmer')
assert mesh and source and anim and material,'Missing wizard presentation asset'
report={'mesh':mesh.get_path_name(),'sourceMesh':source.get_path_name(),'retargetClass':anim.get_path_name(),'material':material.get_path_name(),'clips':results,'rulesChanged':False}
(root/'Docs/HeroPreparedAssets.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
unreal.log('HERO_ASSET_PREPARE_PASS clips='+str(len(results)))
