"""Persist independent UE prefab data assets and placeable preview blueprints."""
import hashlib
import json
from pathlib import Path
import unreal as u

root=Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir())).resolve()
assert (root/'DungeonWorldOpus.uproject').is_file()
assert not u.EditorLevelLibrary.get_pie_worlds(True)
source=root/'Saved/WebVFX/Source'
manifest=json.loads(Path(globals().get('web_vfx_manifest_path',source/'manifest.json')).read_text('utf-8'))
receipt=root/'Saved/WebVFX/prefab-import.json'
hashes=json.loads(receipt.read_text('utf-8')).get('hashes',{}) if receipt.exists() else {}
tools=u.AssetToolsHelpers.get_asset_tools()
changed=[]
limit=int(globals().get('web_vfx_prefab_limit',6))

def signature_for(clip):
    # Geometry/material references can change while animation bytes stay equal.
    return hashlib.sha256(json.dumps(clip,sort_keys=True).encode('utf-8')).hexdigest()+':native-prefab-v2'

for clip in manifest['clips']:
    safe=clip['id'].replace('-','_').replace('@','_element_')
    folder='/Game/WebVFX/Variants' if clip['internal'] else '/Game/WebVFX/Prefabs/'+safe
    target=folder+'/DA_'+safe
    signature=signature_for(clip)
    blueprint_target=folder+'/BP_'+safe
    existing=u.load_asset(target)
    if hashes.get(target)==signature and existing and existing.get_editor_property('source_hash')==clip['sha256'] and (clip['internal'] or u.EditorAssetLibrary.does_asset_exist(blueprint_target)):
        continue
    factory=u.DataAssetFactory()
    factory.set_editor_property('data_asset_class',u.DWWebVFXPrefab)
    asset=existing or tools.create_asset('DA_'+safe,folder,u.DWWebVFXPrefab,factory)
    assert asset, target
    asset.set_editor_property('id',clip['id'])
    asset.set_editor_property('display_name',clip['name'])
    asset.set_editor_property('source_prefab',clip['prefab'])
    asset.set_editor_property('continuous',clip['continuous'])
    asset.set_animation_data(list((source/'Clips'/clip['file']).read_bytes()),float(clip['duration']),clip['sha256'])
    parts=[]
    for row in clip['parts']:
        part=u.DWWebVFXPart()
        mesh=u.load_asset('/Game/WebVFX/Meshes/'+row['geometry'])
        material=u.load_asset('/Game/WebVFX/Materials/'+row['material'])
        assert mesh and material, (clip['id'],row)
        part.set_editor_property('mesh',mesh)
        part.set_editor_property('material',material)
        part.set_editor_property('billboard',bool(row.get('billboard',False)))
        parts.append(part)
    asset.set_editor_property('parts',parts)
    # SetAnimationData mutates an intentionally non-editor byte field; it does
    # not dirty the package. Always save, including animation-only reimports.
    assert u.EditorAssetLibrary.save_loaded_asset(asset,False), target
    if not clip['internal']:
        blueprint_target=folder+'/BP_'+safe
        factory=u.BlueprintFactory()
        factory.set_editor_property('parent_class',u.DWWebVFXActor)
        blueprint=u.load_asset(blueprint_target) or tools.create_asset('BP_'+safe,folder,u.Blueprint,factory)
        u.BlueprintEditorLibrary.compile_blueprint(blueprint)
        defaults=u.get_default_object(blueprint.generated_class())
        defaults.set_editor_property('prefab',asset)
        assert u.EditorAssetLibrary.save_loaded_asset(blueprint,False),blueprint_target
    hashes[target]=signature
    changed.append(target)
    if len(changed)>=limit:
        break

receipt.write_text(json.dumps({'hashes':hashes,'changed':changed},indent=2),'utf-8')
pending=[]
for clip in manifest['clips']:
    safe=clip['id'].replace('-','_').replace('@','_element_')
    folder='/Game/WebVFX/Variants' if clip['internal'] else '/Game/WebVFX/Prefabs/'+safe
    target=folder+'/DA_'+safe
    actual=u.load_asset(target)
    if hashes.get(target)!=signature_for(clip) or not actual or actual.get_editor_property('source_hash')!=clip['sha256'] or (not clip['internal'] and not u.EditorAssetLibrary.does_asset_exist(folder+'/BP_'+safe)):
        pending.append(target)
result={'ok':True,'changed':len(changed),'imported':len(hashes),'total':len(manifest['clips']),'publicPrefabs':sum(not c['internal'] for c in manifest['clips']),'pending':len(pending),'done':not pending}
u.log('WEB_VFX_PREFABS '+json.dumps(result))
