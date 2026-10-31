"""UE Python: add the authored stun row, preserving all unrelated saved widgets/states."""
from pathlib import Path
from copy import deepcopy
import hashlib,json,sys,shutil,xml.etree.ElementTree as ET
import unreal as u

root=Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir())).resolve()
sys.path.insert(0,str(root/'Tools/UI'))
from stun_hud_source import add_stun_row
out=root/'Saved/Verification/PlayerStun';out.mkdir(parents=True,exist_ok=True)
backup=out/'BeforeHUD';backup.mkdir(exist_ok=True)
for name in ('DA_CurrentCanvas','WBP_CurrentCanvas'):
 src=root/'Content/UICreator'/(name+'.uasset')
 if not (backup/src.name).exists():shutil.copy2(src,backup/src.name)
doc=u.load_asset('/Game/UICreator/DA_CurrentCanvas');bp=u.load_asset('/Game/UICreator/WBP_CurrentCanvas')
project=json.loads(doc.get_editor_property('project_json'))
tree=u.find_object(bp,'WidgetTree')
if not tree:raise RuntimeError('Missing saved WidgetTree')
def widgets(node):
 result=[]
 if isinstance(node,u.SvgCanvasNode):result.append(node)
 if isinstance(node,u.PanelWidget):
  for i in range(node.get_children_count()):result.extend(widgets(node.get_child_at(i)))
 return result
root_widget=u.find_object(tree,'N_'+hashlib.md5(project['root']['id'].encode()).hexdigest()[:24])
if not root_widget:raise RuntimeError('Missing saved HUD root')
native={n.get_editor_property('source_id'):n for n in widgets(root_widget)}
prefix=next(k[:-len('rs-mana')] for k in native if k.endswith('::rs-mana'))
parent_id=prefix+'rs-resources';parent=native[parent_id]
already=prefix+'rs-stun' in native
add_stun_row(project)
source={n['id']:n for n in project['prefabs']['rs-prefab-resources']['root']['children']}
states=dict(doc.get_editor_property('states'))
rasters=dict(doc.get_editor_property('rasters'))
prepared=root/'Saved/Verification/PlayerStun/HudRasters'
textures={};changed_ids={parent_id,prefix+'rs-resources-bg'}
shift_applied=project.get('provenance',{}).get('opusPlayerStunHUD',{}).get('rasterShiftApplied',False)
def scene_digest(scene):
 rows=[]
 for n in scene.nodes:
  rows.append([n.id,n.parent_id,[n.rect.x,n.rect.y,n.rect.z,n.rect.w],n.layout_scale,n.visibility,n.local_visibility,n.interactable,n.text,n.paint_order])
 return hashlib.sha256(json.dumps(rows,ensure_ascii=False,separators=(',',':')).encode()).hexdigest()
if already and not shift_applied:
 snapshot={'source_hash':doc.get_editor_property('source_hash'),'scenes':{k:scene_digest(v) for k,v in states.items()}}
 (out/'hud-before-raster-shift.json').write_text(json.dumps(snapshot,indent=2),encoding='utf-8')

def translated_raster(key,delta):
 if not key:return key
 shifted_key=hashlib.sha256((key+'|stun-offset-y|'+str(delta)).encode()).hexdigest()
 if shifted_key not in rasters:
  old=rasters[key];r=u.SvgCanvasRaster()
  for prop in ('texture','pixel_origin','pixel_size','source_svg'):r.set_editor_property(prop,old.get_editor_property(prop))
  r.set_editor_property('pixel_origin',u.Vector2D(old.pixel_origin.x,old.pixel_origin.y+delta))
  if old.source_svg:
   svg=ET.fromstring(old.source_svg);view=list(map(float,svg.attrib['viewBox'].split()));view[1]+=delta;svg.set('viewBox',' '.join(map(str,view)))
   group=ET.Element('{http://www.w3.org/2000/svg}g',{'transform':'translate(0 '+str(delta)+')'})
   for child in list(svg):
    if child.tag.endswith('defs'):continue
    svg.remove(child);group.append(child)
   svg.append(group);r.set_editor_property('source_svg',ET.tostring(svg,encoding='unicode'))
  rasters[shifted_key]=r
 return shifted_key

def raster(name,rect):
 asset_name='T_'+name.replace('-','_')
 texture=textures.get(name) or u.load_asset('/Game/UICreator/Stun/'+asset_name)
 created=not texture
 if not texture:
  task=u.AssetImportTask();task.filename=str(prepared/(name+'.png'));task.destination_path='/Game/UICreator/Stun';task.destination_name=asset_name;task.automated=True;task.save=False
  u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
  texture=u.load_asset('/Game/UICreator/Stun/'+asset_name)
 if not texture:raise RuntimeError('Texture import '+name)
 if created:
  for key,value in [('srgb',False),('never_stream',True),('lod_group',u.TextureGroup.TEXTUREGROUP_UI),('mip_gen_settings',u.TextureMipGenSettings.TMGS_NO_MIPMAPS),('compression_settings',u.TextureCompressionSettings.TC_EDITOR_ICON),('filter',u.TextureFilter.TF_NEAREST)]:texture.set_editor_property(key,value)
  if not u.EditorAssetLibrary.save_loaded_asset(texture,False):raise RuntimeError('Texture save '+name)
 textures[name]=texture
 r=u.SvgCanvasRaster()
 for prop,value in [('texture',texture),('pixel_origin',u.Vector2D(rect.x,rect.y)),('pixel_size',u.Vector2D(rect.z,rect.w)),('source_svg',(prepared/(name+'.svg')).read_text(encoding='utf-8'))]:r.set_editor_property(prop,value)
 return r

for key,state in states.items():
 items=list(state.nodes);by={n.id:n for n in items}
 if parent_id not in by:continue
 resource_instance=by[parent_id].parent_id;top_left=by[resource_instance].parent_id
 shifted={n.id for n in items if n.parent_id==top_left and n.id!=resource_instance}
 while True:
  count=len(shifted);shifted.update(n.id for n in items if n.parent_id in shifted)
  if len(shifted)==count:break
 scale=by[parent_id].layout_scale
 changed_ids.update(shifted);changed_ids.add(resource_instance)
 if not already:
  resource_instance=by[parent_id].parent_id
  top_left=by[resource_instance].parent_id
  shifted={n.id for n in items if n.parent_id==top_left and n.id!=resource_instance}
  while True:
   old=len(shifted);shifted.update(n.id for n in items if n.parent_id in shifted)
   if len(shifted)==old:break
  scale=by[parent_id].layout_scale
  changed_ids.update(shifted);changed_ids.add(resource_instance)
  for n in items:
   rect=n.rect
   if n.id in shifted:n.set_editor_property('rect',u.Vector4(rect.x,rect.y+34*scale,rect.z,rect.w))
   if n.id in (parent_id,resource_instance,prefix+'rs-resources-bg'):n.set_editor_property('rect',u.Vector4(rect.x,rect.y,rect.z,rect.w+34*scale))
  for suffix in ('-title','-value',''):
   old=by[prefix+'rs-mana'+suffix]
   n=u.SvgCanvasNodeState()
   for prop in ('parent_id','rect','layout_scale','visibility','local_visibility','raster','hovered_raster','pressed_raster','disabled_raster','interactable','text','paint_order'):n.set_editor_property(prop,old.get_editor_property(prop))
   n.set_editor_property('id',prefix+'rs-stun'+suffix);n.set_editor_property('rect',u.Vector4(old.rect.x,old.rect.y+34*scale,old.rect.z,old.rect.w));n.set_editor_property('paint_order',old.paint_order+3)
   n.set_editor_property('text',source['rs-stun'+suffix]['text']);items.append(n)
  by={n.id:n for n in items}
 if not shift_applied:
  for n in items:
   if n.id in shifted:
    for prop in ('raster','hovered_raster','pressed_raster','disabled_raster'):n.set_editor_property(prop,translated_raster(n.get_editor_property(prop),34*scale))
 changed_ids.update(prefix+'rs-stun'+s for s in ('-title','-value',''))
 # Every scene retains its existing positions except the enlarged resources stack.
 for suffix,name in (('-title','stun-title'),('-value','stun-value'),('','stun-bar')):
  n=by[prefix+'rs-stun'+suffix];rk='player-stun-'+name+'-'+key;n.set_editor_property('raster',rk);rasters[rk]=raster(name,n.rect)
 bg=by[prefix+'rs-resources-bg'];bg.set_editor_property('raster','player-stun-bg-'+key);rasters[bg.raster]=raster('stun-panel',bg.rect)
 state.set_editor_property('nodes',items);states[key]=state

selected=states.get('selected-1',states['canvas']);selected_nodes={n.id:n for n in selected.nodes}
for suffix in ('-title','-value',''):
 sid=prefix+'rs-stun'+suffix;authored=deepcopy(source['rs-stun'+suffix]);authored['id']=sid;authored['parentId']=parent_id
 node=native.get(sid)
 if not node:
  name='N_'+hashlib.md5(sid.encode()).hexdigest()[:24]
  node=u.OpusUI.create_editor_node(bp,name)
  if not node:raise RuntimeError('Native editor node creation failed')
  parent.add_child_to_canvas(node);native[sid]=node
 for prop,value in [('source_id',sid),('source_name',authored['name']),('source_type',authored['type']),('source_properties_json',json.dumps(authored,ensure_ascii=False)),('source_text',authored['text']),('binding',authored['binding'])]:node.set_editor_property(prop,value)
 node.set_visibility(u.SlateVisibility.SELF_HIT_TEST_INVISIBLE)

for sid,node in native.items():
 state=selected_nodes.get(sid)
 if not state:continue
 # Retain all source widgets, including current map-core and hover additions.
 if sid in changed_ids:
  node.set_editor_property('design_rect',state.rect);node.set_editor_property('design_scale',state.layout_scale)
  if state.raster in rasters:
   r=rasters[state.raster];node.set_editor_property('normal',r)
   image=u.find_object(tree,node.get_name()+'_SVG');image.set_brush_from_texture(r.texture)
   slot=image.slot;slot.set_position(u.Vector2D(r.pixel_origin.x-state.rect.x,r.pixel_origin.y-state.rect.y));slot.set_size(r.pixel_size)
  par=node.get_parent()
  if isinstance(par,u.SvgCanvasNode) and isinstance(node.slot,u.CanvasPanelSlot):
   pr=par.get_editor_property('design_rect');rect=state.rect
   node.slot.set_position(u.Vector2D(rect.x-pr.x,rect.y-pr.y));node.slot.set_size(u.Vector2D(rect.z,rect.w))

if not already:
 for sid in (parent_id,prefix+'rs-resources-bg',parent.get_parent().get_editor_property('source_id')):
  node=native[sid];props=json.loads(node.get_editor_property('source_properties_json'))
  props['layout']['height']+=34;props['slot']['offsets']['bottom']+=34
  node.set_editor_property('source_properties_json',json.dumps(props,ensure_ascii=False))

doc.set_editor_property('states',states);doc.set_editor_property('rasters',rasters)
project.setdefault('provenance',{})['opusPlayerStunHUD']={'version':2,'rasterShiftApplied':True}
serialized=json.dumps(project,ensure_ascii=False,separators=(',',':'))
doc.set_editor_property('project_json',serialized);doc.set_editor_property('source_hash',hashlib.sha256(serialized.encode()).hexdigest())
if not u.OpusUI.finalize_editor_hud(bp):raise RuntimeError('HUD structural compile failed')
if not u.EditorAssetLibrary.save_loaded_asset(doc,False) or not u.EditorAssetLibrary.save_loaded_asset(bp,False):raise RuntimeError('HUD save failed')
authoring=root/'CustomTools/UICreatorAlpha/Screen/RuleStudio-HUD/hud.uialpha.json'
source_project=json.loads(authoring.read_text(encoding='utf-8'))
add_stun_row(source_project);source_project.setdefault('provenance',{})['opusPlayerStunHUD']={'version':2,'rasterShiftApplied':True}
authoring.write_text(json.dumps(source_project,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
report={'ok':True,'new_nodes':[prefix+'rs-stun'+s for s in ('-title','-value','')],'scene_count':len(states),'source_hash':doc.get_editor_property('source_hash'),'unrelated_nodes_preserved':True,'authoring':str(authoring),'backup':str(backup)}
(out/'hud-patch.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8');u.log('PLAYER_STUN_HUD_SAVED '+str(report))
