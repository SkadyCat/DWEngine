"""Only replace ice_lance.impact; preserve flight, all other entries and budgets."""
from pathlib import Path
import json
import unreal as u

root=Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir())).resolve()
asset=u.load_asset('/Game/WebVFX/DA_UEVFXMap')
if not isinstance(asset,u.DWUEVFXMap):raise RuntimeError('Missing map')
effects=dict(asset.get_editor_property('effects'))
def snapshot(entries):
 def value(v):
  if isinstance(v,u.Object):return v.get_path_name()
  if isinstance(v,u.Vector):return [v.x,v.y,v.z]
  if isinstance(v,u.Rotator):return [v.pitch,v.yaw,v.roll]
  if isinstance(v,(bool,int,float)):return v
  return str(v)
 return {str(k):{p:value(v.get_editor_property(p)) for p in ('system','scale','rotation','offset','follow_source','max_visual_seconds')} for k,v in entries.items()}
before=snapshot(effects)
system_path='/Game/IceSkill_VFX/Niagara/Projectile_Small/NS_Ice_Hit_Small'
system=u.load_asset(system_path)
if not isinstance(system,u.NiagaraSystem):raise RuntimeError(system_path)
entry=u.DWUEVFXEntry()
for prop,value in [('system',system),('scale',u.Vector(.5,.5,.5)),('rotation',u.Rotator(pitch=0,yaw=0,roll=0)),('offset',u.Vector(0,0,0)),('follow_source',False),('max_visual_seconds',1.0)]:
 entry.set_editor_property(prop,value)
effects[u.Name('ice_lance.impact')]=entry
asset.set_editor_property('effects',effects)
after=snapshot(dict(asset.get_editor_property('effects')))
(root/'Saved/Verification/UEVFX/ice-hit-map-readback.json').write_text(json.dumps({'before':before,'after':after},ensure_ascii=False,indent=2),encoding='utf-8')
assert {k:v for k,v in before.items() if k!='ice_lance.impact'}=={k:v for k,v in after.items() if k!='ice_lance.impact'}
if not u.EditorAssetLibrary.save_loaded_asset(asset,False):raise RuntimeError('Map save failed')
report={'ok':True,'before':before,'after':after,'preserved_other_entries':True,'system':system_path,'scale':[.5,.5,.5],'offset':[0,0,0],'max_visual_seconds':1.0}
(root/'Saved/Verification/UEVFX/ice-hit-map.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
u.log('ICE_HIT_MAP_REPLACED '+system_path)
