"""Import only the UICreator-authored stun artwork into the existing saved HUD."""
import hashlib,json,shutil
from pathlib import Path
import unreal as u
root=Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir())).resolve()
proof=root/'Saved/Verification/StunOverlay'
for name in ('DA_CurrentCanvas','WBP_CurrentCanvas'):
    target=proof/(name+'-before.uasset')
    if not target.exists():shutil.copy2(root/'Content/UICreator'/(name+'.uasset'),target)
doc=u.load_asset('/Game/UICreator/DA_CurrentCanvas');bp=u.load_asset('/Game/UICreator/WBP_CurrentCanvas')
assert doc and bp
project=json.loads(doc.get_editor_property('project_json'))
author=json.loads((root/'UIAuthoring/RuleStudio3D.uialpha.json').read_text(encoding='utf-8'))
asset=author['assets']['rs-svg-stun-yellow-fill'];svg=asset['source']
project['assets'].pop('rs-svg-stun-earth-fill',None);project['assets']['rs-svg-stun-yellow-fill']=asset
bar=next(n for n in project['prefabs']['rs-prefab-resources']['root']['children'] if n['id']=='rs-stun')
bar.update(type='Svg',svgAsset='rs-svg-stun-yellow-fill',value=0);bar['style']['color']='#f7d046'
value=next(n for n in project['prefabs']['rs-prefab-resources']['root']['children'] if n['id']=='rs-stun-value');value['text']=value['name']='0 / 400'
png=proof/'yellow.png';assert png.exists()
task=u.AssetImportTask();task.filename=str(png);task.destination_path='/Game/UICreator/Stun';task.destination_name='T_stun_yellow_20261027';task.automated=True;task.replace_existing=True;task.save=False
u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task]);texture=u.load_asset('/Game/UICreator/Stun/T_stun_yellow_20261027');assert texture
for key,val in [('srgb',False),('never_stream',True),('lod_group',u.TextureGroup.TEXTUREGROUP_UI),('mip_gen_settings',u.TextureMipGenSettings.TMGS_NO_MIPMAPS),('compression_settings',u.TextureCompressionSettings.TC_EDITOR_ICON),('filter',u.TextureFilter.TF_NEAREST)]:texture.set_editor_property(key,val)
assert u.EditorAssetLibrary.save_loaded_asset(texture,False)
tree=u.find_object(bp,'WidgetTree');widget=u.find_object(tree,'N_'+hashlib.md5(project['root']['id'].encode()).hexdigest()[:24]);assert widget
def walk(w):
    result=[w] if isinstance(w,u.SvgCanvasNode) else []
    if isinstance(w,u.PanelWidget):
        for i in range(w.get_children_count()):result.extend(walk(w.get_child_at(i)))
    return result
native={n.get_editor_property('source_id'):n for n in walk(widget)}
sid=next(k for k in native if k.endswith('::rs-stun'));value_id=sid+'-value';node=native[sid]
states=dict(doc.get_editor_property('states'));rasters=dict(doc.get_editor_property('rasters'))
def unrelated():
    return {k:[[n.id,n.parent_id,[n.rect.x,n.rect.y,n.rect.z,n.rect.w],n.layout_scale,n.visibility,n.local_visibility,n.interactable,n.paint_order,str(n.text),n.raster,n.hovered_raster,n.pressed_raster,n.disabled_raster] for n in s.nodes if n.id not in (sid,value_id)] for k,s in states.items()}
before=unrelated()
for key,state in states.items():
    items=list(state.nodes);target=next((n for n in items if n.id==sid),None)
    if target is None:continue
    raster=u.SvgCanvasRaster();raster.set_editor_property('texture',texture);raster.set_editor_property('pixel_origin',u.Vector2D(target.rect.x,target.rect.y));raster.set_editor_property('pixel_size',u.Vector2D(target.rect.z,target.rect.w));raster.set_editor_property('source_svg',svg)
    raster_key='stun-yellow-'+key;rasters[raster_key]=raster;target.set_editor_property('raster',raster_key)
    text=next((n for n in items if n.id==value_id),None)
    if text:text.set_editor_property('text','0 / 400')
    state.set_editor_property('nodes',items);states[key]=state
after=unrelated()
if before!=after:
    changed={k:[(a,b) for a,b in zip(before[k],after[k]) if a!=b] for k in before if before[k]!=after[k]}
    (proof/'unrelated-diff.json').write_text(json.dumps(changed,default=str,ensure_ascii=False,indent=2),encoding='utf-8')
assert before==after,'Unrelated states or layout changed'
props=json.loads(node.get_editor_property('source_properties_json'));props.update(type='Svg',svgAsset='rs-svg-stun-yellow-fill',value=0);props['style']['color']='#f7d046'
node.set_editor_property('source_properties_json',json.dumps(props,ensure_ascii=False));node.set_editor_property('normal',rasters[states.get('selected-1',states['canvas']).nodes[[n.id for n in states.get('selected-1',states['canvas']).nodes].index(sid)].raster])
for i in range(node.get_children_count()):
    child=node.get_child_at(i)
    if isinstance(child,u.Image):child.set_brush_from_texture(texture)
serialized=json.dumps(project,ensure_ascii=False,separators=(',',':'));doc.set_editor_property('project_json',serialized);doc.set_editor_property('source_hash',hashlib.sha256(serialized.encode()).hexdigest());doc.set_editor_property('states',states);doc.set_editor_property('rasters',rasters)
assert u.OpusUI.finalize_editor_hud(bp)
assert u.EditorAssetLibrary.save_loaded_asset(doc,False) and u.EditorAssetLibrary.save_loaded_asset(bp,False)
(proof/'hud-import.json').write_text(json.dumps({'ok':True,'states':len(states),'unrelatedStatesPreserved':True,'sourceHash':doc.get_editor_property('source_hash'),'texture':texture.get_path_name()},indent=2),encoding='utf-8')
u.log('STUN_YELLOW_HUD_SAVED')
