"""Run in the isolated compiled Opus staging project, outside PIE."""
from pathlib import Path
import json
import unreal as u

root=Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir())).resolve()
tools=u.AssetToolsHelpers.get_asset_tools()
factory=u.DataAssetFactory()
factory.set_editor_property('data_asset_class',u.DWUEVFXMap)
path='/Game/WebVFX/DA_UEVFXMap'
asset=u.load_asset(path) if u.EditorAssetLibrary.does_asset_exist(path) else tools.create_asset('DA_UEVFXMap','/Game/WebVFX',u.DWUEVFXMap,factory)
entries=dict(asset.get_editor_property('effects'))
specs=[
 ('ice_lance.flight','/Game/IceSkill_VFX/Niagara/Portal_Spear/NS_IceSpear_Projectile',True,u.Vector(.5,.5,.5),u.Rotator(0,0,0),u.Vector(0,0,0),3),
 ('ice_lance.impact','/Game/IceSkill_VFX/Niagara/Projectile_Small/NS_Ice_Hit_Small',False,u.Vector(.5,.5,.5),u.Rotator(pitch=0,yaw=0,roll=0),u.Vector(0,0,0),1),
]
receipt=[]
for key,system_path,follow,scale,rotation,offset,limit in specs:
 system=u.load_asset(system_path)
 if not isinstance(system,u.NiagaraSystem):raise RuntimeError('Missing system '+system_path)
 entry=u.DWUEVFXEntry()
 for prop,value in [('system',system),('scale',scale),('rotation',rotation),('offset',offset),('follow_source',follow),('max_visual_seconds',limit)]:
  entry.set_editor_property(prop,value)
 entries[key]=entry
 receipt.append({'id':key,'system':system_path,'follow':follow,'scale':str(scale),'rotation':str(rotation),'offset':str(offset)})
asset.set_editor_property('effects',entries)
asset.set_editor_property('max_active_systems',128)
if not u.EditorAssetLibrary.save_loaded_asset(asset,False):raise RuntimeError('Map save failed')
report={'ok':True,'asset':path,'effects':receipt,'max_active_systems':128,'project':str(root)}
Path('G:/DungeonWorldOpus/Saved/Verification/UEVFX/map-created.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
u.log('ICE_VFX_MAP_CREATED '+json.dumps(report))
