"""UE commandlet: repair only the existing stun progress raster and its default.

Use the approved RuleStudio3D earth fill at full integrity, then let AS clip it.
Keep all unrelated current HUD nodes, layouts, scenes and resources intact.
"""
from pathlib import Path
import hashlib
import json
import shutil
import unreal as u

root = Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir())).resolve()
proof = root / 'Saved/Verification/PlayerStun'
backup = proof / 'BeforeWebSync'
backup.mkdir(parents=True, exist_ok=True)
for name in ('DA_CurrentCanvas', 'WBP_CurrentCanvas'):
    source = root / 'Content/UICreator' / (name + '.uasset')
    if not (backup / source.name).exists():
        shutil.copy2(source, backup / source.name)

doc = u.load_asset('/Game/UICreator/DA_CurrentCanvas')
bp = u.load_asset('/Game/UICreator/WBP_CurrentCanvas')
assert doc and bp
project = json.loads(doc.get_editor_property('project_json'))
tree = u.find_object(bp, 'WidgetTree')
widget = u.find_object(tree, 'N_' + hashlib.md5(project['root']['id'].encode()).hexdigest()[:24])
assert widget

def walk(w):
    result = [w] if isinstance(w, u.SvgCanvasNode) else []
    if isinstance(w, u.PanelWidget):
        for i in range(w.get_children_count()):
            result.extend(walk(w.get_child_at(i)))
    return result

native = {w.get_editor_property('source_id'): w for w in walk(widget)}
sid = next(k for k in native if k.endswith('::rs-stun'))
node = native[sid]
states = dict(doc.get_editor_property('states'))
rasters = dict(doc.get_editor_property('rasters'))

def snapshot():
    return {k: [[n.id, n.parent_id, [n.rect.x, n.rect.y, n.rect.z, n.rect.w],
                 n.layout_scale, n.visibility, n.local_visibility, n.interactable,
                 n.text, n.paint_order, n.raster if n.id != sid else None,
                 n.hovered_raster, n.pressed_raster, n.disabled_raster] for n in state.nodes]
            for k, state in states.items()}

before = snapshot()
texture_path = '/Game/UICreator/Stun/T_stun_bar_web_v2'
texture = u.load_asset(texture_path)
if not texture:
    task = u.AssetImportTask()
    task.filename = str(proof / 'HudRasters/stun-bar.png')
    task.destination_path = '/Game/UICreator/Stun'
    task.destination_name = 'T_stun_bar_web_v2'
    task.automated = True
    task.save = False
    u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = u.load_asset(texture_path)
    assert texture
    for key, value in [('srgb', False), ('never_stream', True),
                       ('lod_group', u.TextureGroup.TEXTUREGROUP_UI),
                       ('mip_gen_settings', u.TextureMipGenSettings.TMGS_NO_MIPMAPS),
                       ('compression_settings', u.TextureCompressionSettings.TC_EDITOR_ICON),
                       ('filter', u.TextureFilter.TF_NEAREST)]:
        texture.set_editor_property(key, value)
    assert u.EditorAssetLibrary.save_loaded_asset(texture, False)
svg = (proof / 'HudRasters/stun-bar.svg').read_text(encoding='utf-8')
for key, state in states.items():
    items = list(state.nodes)
    target = next((n for n in items if n.id == sid), None)
    if target is None:
        continue
    r = u.SvgCanvasRaster()
    r.set_editor_property('texture', texture)
    r.set_editor_property('pixel_origin', u.Vector2D(target.rect.x, target.rect.y))
    r.set_editor_property('pixel_size', u.Vector2D(target.rect.z, target.rect.w))
    r.set_editor_property('source_svg', svg)
    raster_key = 'player-stun-earth-full-v2-' + key
    rasters[raster_key] = r
    target.set_editor_property('raster', raster_key)
    state.set_editor_property('nodes', items)
    states[key] = state
assert before == snapshot(), 'Unrelated state or layout changed'

props = json.loads(node.get_editor_property('source_properties_json'))
props['value'] = 1
props['style']['color'] = '#b29665'
node.set_editor_property('source_properties_json', json.dumps(props, ensure_ascii=False))
selected = states.get('selected-1', states['canvas'])
selected_node = next(n for n in selected.nodes if n.id == sid)
raster = rasters[selected_node.raster]
node.set_editor_property('normal', raster)
for i in range(node.get_children_count()):
    image = node.get_child_at(i)
    if isinstance(image, u.Image):
        image.set_brush_from_texture(texture)
        image.slot.set_position(u.Vector2D(0, 0))
        image.slot.set_size(raster.pixel_size)

def defaults(data):
    row = next(n for n in data['prefabs']['rs-prefab-resources']['root']['children'] if n['id'] == 'rs-stun')
    row['value'] = 1
    row['style']['color'] = '#b29665'

defaults(project)
serialized = json.dumps(project, ensure_ascii=False, separators=(',', ':'))
doc.set_editor_property('project_json', serialized)
doc.set_editor_property('source_hash', hashlib.sha256(serialized.encode()).hexdigest())
doc.set_editor_property('states', states)
doc.set_editor_property('rasters', rasters)
assert u.OpusUI.finalize_editor_hud(bp)
assert u.EditorAssetLibrary.save_loaded_asset(doc, False)
assert u.EditorAssetLibrary.save_loaded_asset(bp, False)

authoring = root / 'CustomTools/UICreatorAlpha/Screen/RuleStudio-HUD/hud.uialpha.json'
data = json.loads(authoring.read_text(encoding='utf-8'))
defaults(data)
authoring.write_text(json.dumps(data, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
receipt = {'ok': True, 'node': sid, 'scenes': len(states), 'unrelated_states_and_layout_preserved': True,
           'texture': texture_path, 'source_hash': doc.get_editor_property('source_hash'),
           'raster_png_sha256': hashlib.sha256((proof / 'HudRasters/stun-bar.png').read_bytes()).hexdigest()}
(proof / 'hud-fill-sync.json').write_text(json.dumps(receipt, indent=2), encoding='utf-8')
u.log('STUN_HUD_FILL_SAVED ' + str(receipt))
