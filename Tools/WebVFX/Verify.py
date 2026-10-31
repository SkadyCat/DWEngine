"""Cold-read every saved native VFX asset; run in a separate editor commandlet."""
import json,hashlib
from pathlib import Path
import unreal as u
r=Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir())).resolve()
assert (r/'DungeonWorldOpus.uproject').is_file()
assert not u.EditorLevelLibrary.get_pie_worlds(True)
m=json.loads((r/'Saved/WebVFX/Source/manifest.json').read_text(encoding='utf-8'))
checked=[];statistics=[];failed=[]
for row in m['geometries']:
 path='/Game/WebVFX/Meshes/'+row['id'];asset=u.load_asset(path)
 assert isinstance(asset,u.StaticMesh),path
 bounds=asset.get_bounds();assert max(bounds.box_extent.x,bounds.box_extent.y,bounds.box_extent.z)>0,path
for row in m['materials']:
 path='/Game/WebVFX/Materials/'+row['id'];asset=u.load_asset(path)
 assert isinstance(asset,u.Material),path
 expected=u.BlendMode.BLEND_MASKED if row.get('weaponClip') else u.BlendMode.BLEND_ADDITIVE if row['additive'] else u.BlendMode.BLEND_TRANSLUCENT if row['transparent'] else u.BlendMode.BLEND_OPAQUE
 assert asset.get_editor_property('blend_mode')==expected,path
 stats=u.MaterialEditingLibrary.get_statistics(asset)
 row_stats={'id':row['id'],'vertexInstructions':stats.num_vertex_shader_instructions,'pixelInstructions':stats.num_pixel_shader_instructions,'interpolatorScalars':stats.num_interpolator_scalars}
 statistics.append(row_stats)
 if stats.num_pixel_shader_instructions<=0:failed.append(row_stats)
 u.log('OPUS_MATERIAL_STATS '+json.dumps(row_stats))
for row in m['clips']:
 safe=row['id'].replace('-','_').replace('@','_element_')
 folder='/Game/WebVFX/Variants' if row['internal'] else '/Game/WebVFX/Prefabs/'+safe
 path=folder+'/DA_'+safe;asset=u.load_asset(path)
 assert isinstance(asset,u.DWWebVFXPrefab),path
 assert str(asset.get_editor_property('id'))==row['id'],path
 assert asset.get_editor_property('source_hash')==row['sha256'],path
 # The byte array deliberately has no Python/editor write exposure. Match its
 # complete serialized payload in the saved package, then exercise native decode
 # separately in the rendered gameplay probe.
 animation=(r/'Saved/WebVFX/Source/Clips'/row['file']).read_bytes()
 assert hashlib.sha256(animation).hexdigest()[:20]==row['sha256'],path
 saved_package=r/'Content'/(path.removeprefix('/Game/')+'.uasset')
 assert animation in saved_package.read_bytes(),path+' saved animation payload'
 parts=asset.get_editor_property('parts');assert len(parts)==len(row['parts']),path
 for part,source in zip(parts,row['parts']):
  assert part.get_editor_property('mesh').get_path_name().split('.')[-1]==source['geometry'],path
  assert part.get_editor_property('material').get_path_name().split('.')[-1]==source['material'],path
  assert part.get_editor_property('billboard')==bool(source.get('billboard',False)),path
 if not row['internal']:
  blueprint=u.load_asset(folder+'/BP_'+safe);assert blueprint,path
  assert u.get_default_object(blueprint.generated_class()).get_editor_property('prefab')==asset,path
 checked.append(row['id'])
for i in range(3):assert isinstance(u.load_asset('/Game/WebVFX/Textures/T_TreasuryPortal_'+str(i)),u.Texture2D)
receipt={'ok':not failed,'coldReload':True,'savedAnimationPayloadsMatched':True,'clips':len(checked),'geometries':len(m['geometries']),'materials':len(m['materials']),'ids':checked,'materialStatistics':statistics,'failedMaterials':failed}
(r/'Saved/Verification/UEIntegration/vfx-cold-read.json').write_text(json.dumps(receipt,ensure_ascii=False,indent=2),encoding='utf-8')
assert not failed,failed
u.log('OPUS_VFX_COLD_PASS '+json.dumps({k:v for k,v in receipt.items() if k not in ['ids','materialStatistics']}))
