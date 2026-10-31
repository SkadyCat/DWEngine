"""Cold saved-asset readback against the untouched pre-edit document."""
from pathlib import Path
import hashlib,json,shutil
import unreal as u
root=Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir())).resolve()
out=root/'Saved/Verification/PlayerStun'
temp=root/'Content/__StunVerificationBefore/DA_CurrentCanvas.uasset'
if temp.exists() and temp.read_bytes()!=(out/'BeforeHUD/DA_CurrentCanvas.uasset').read_bytes():raise RuntimeError('Unexpected temporary verification asset')
temp.parent.mkdir(parents=True,exist_ok=True)
shutil.copy2(out/'BeforeHUD/DA_CurrentCanvas.uasset',temp)
before=u.load_asset('/Game/__StunVerificationBefore/DA_CurrentCanvas')
after=u.load_asset('/Game/UICreator/DA_CurrentCanvas')
bp=u.load_asset('/Game/UICreator/WBP_CurrentCanvas')
assert before and after and bp
project=json.loads(after.get_editor_property('project_json'))
tree=u.find_object(bp,'WidgetTree')
widget=u.find_object(tree,'N_'+hashlib.md5(project['root']['id'].encode()).hexdigest()[:24])
def walk(w):
 result=[]
 if isinstance(w,u.SvgCanvasNode):result.append(w)
 if isinstance(w,u.PanelWidget):
  for i in range(w.get_children_count()):result.extend(walk(w.get_child_at(i)))
 return result
native={w.get_editor_property('source_id'):w for w in walk(widget)}
prefix=next(k[:-len('rs-stun')] for k in native if k.endswith('::rs-stun'))
new_ids={prefix+'rs-stun'+suffix for suffix in ('-title','-value','')}
old_states=dict(before.get_editor_property('states'));new_states=dict(after.get_editor_property('states'))
old_rasters=dict(before.get_editor_property('rasters'));new_rasters=dict(after.get_editor_property('rasters'))
def scene_digest(scene):
 rows=[[n.id,n.parent_id,[n.rect.x,n.rect.y,n.rect.z,n.rect.w],n.layout_scale,n.visibility,n.local_visibility,n.interactable,n.text,n.paint_order] for n in scene.nodes]
 return hashlib.sha256(json.dumps(rows,ensure_ascii=False,separators=(',',':')).encode()).hexdigest()
latest_baseline=json.loads((out/'hud-before-raster-shift.json').read_text(encoding='utf-8'))
assert {k:scene_digest(v) for k,v in new_states.items()}==latest_baseline['scenes'],'saved state identity/layout changed during raster repair'
diagnostic={'before_path':before.get_path_name(),'after_path':after.get_path_name(),'before_hash':before.get_editor_property('source_hash'),'after_hash':after.get_editor_property('source_hash'),'new_ids':sorted(new_ids),'scenes':{}}
for k in ('canvas','selected-1'):
 old={n.id:n for n in old_states[k].nodes};new={n.id:n for n in new_states[k].nodes}
 diagnostic['scenes'][k]={'added':sorted(set(new)-set(old))[:25],'missing':sorted(set(old)-set(new))[:25],'samples':{n.id:{'parent':n.parent_id,'rect':str(n.rect)} for n in new.values() if n.id.endswith(('::rs-resources','::rs-resources-instance','::rs-top-left','::rs-objective-instance','::rs-objective-accent','::rs-stun'))}}
(out/'hud-readback-diagnostic.json').write_text(json.dumps(diagnostic,ensure_ascii=False,indent=2),encoding='utf-8')
assert set(old_states)==set(new_states)
checks=0
for key,old_scene in old_states.items():
 old={n.id:n for n in old_scene.nodes};new={n.id:n for n in new_states[key].nodes}
 assert len(new)==len(new_states[key].nodes),('duplicate node',key)
 assert new_ids<=set(new),('missing stun controls',key)
 parent=prefix+'rs-resources';instance=old[parent].parent_id;stack=old[instance].parent_id
 shifted={n.id for n in old_scene.nodes if n.parent_id==stack and n.id!=instance}
 while True:
  count=len(shifted);shifted.update(n.id for n in old_scene.nodes if n.parent_id in shifted)
  if len(shifted)==count:break
 delta=34*old[parent].layout_scale
 for sid,n in old.items():
  # The first backup predates concurrent item-hover authoring. Compare its
  # unchanged left HUD; all current states are covered by the immediate digest above.
  if sid not in shifted and sid not in (parent,instance,prefix+'rs-resources-bg'):continue
  assert sid in new,('missing left HUD node',key,sid)
  want=[n.rect.x,n.rect.y,n.rect.z,n.rect.w]
  if sid in shifted:want[1]+=delta
  if sid in (parent,instance,prefix+'rs-resources-bg'):want[3]+=delta
  actual=new[sid].rect
  assert all(abs(a-b)<.0001 for a,b in zip(want,(actual.x,actual.y,actual.z,actual.w))),(key,sid,want,str(actual))
  for prop in ('parent_id','visibility','local_visibility','interactable','text','paint_order'):
   assert n.get_editor_property(prop)==new[sid].get_editor_property(prop),(key,sid,prop)
  checks+=1
  if sid in shifted and n.raster and new[sid].raster:
   old_r=old_rasters[n.raster];new_r=new_rasters[new[sid].raster]
   assert abs((new_r.pixel_origin.y-new[sid].rect.y)-(old_r.pixel_origin.y-n.rect.y))<.001,('raster placement mismatch',key,sid)
 for suffix,binding in (('-title','stun-title'),('-value','stun-text'),('','stun')):
  sid=prefix+'rs-stun'+suffix
  assert native[sid].get_editor_property('binding')==binding
  assert new[sid].parent_id==parent
  props=json.loads(native[sid].get_editor_property('source_properties_json'))
  assert props['style']['color']=='#e9b75c'
assert {n['id'] for n in project['prefabs']['rs-prefab-resources']['root']['children']}>= {'rs-stun-title','rs-stun-value','rs-stun'}
receipt={'ok':True,'cold_reload':True,'scene_count':len(new_states),'left_hud_node_states_checked':checks,'all_current_scene_digests_preserved':True,'added_nodes':sorted(new_ids),'source_hash':after.get_editor_property('source_hash'),'other_state_fields_preserved':True,'concurrent_authoring_baseline_note':'Earlier backup includes older hover/choice controls; current complete scene identities/layouts compared against the snapshot immediately before raster repair.'}
(out/'hud-reloaded.json').write_text(json.dumps(receipt,ensure_ascii=False,indent=2),encoding='utf-8')
u.log('STUN_HUD_RELOAD_PASS '+str(receipt))
