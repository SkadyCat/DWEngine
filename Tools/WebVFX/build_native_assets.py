"""Run in Opus's idle editor. Import native meshes and translate the actual
small RuleStudio GLSL fragments to native unlit/PBR UE materials.
No viewport textures, browser widgets, gameplay scripts, or PIE imports.
"""
import hashlib
import json
import re
from urllib.parse import urlparse, unquote
from pathlib import Path
import unreal as u

root = Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir())).resolve()
assert (root / 'DungeonWorldOpus.uproject').is_file()
assert not u.EditorLevelLibrary.get_pie_worlds(True), 'Stop PIE before importing VFX'
source = root / 'Saved/WebVFX/Source'
manifest_path = Path(globals().get('web_vfx_manifest_path',source / 'manifest.json'))
manifest = json.loads(manifest_path.read_text('utf-8'))
receipt_file = root / 'Saved/WebVFX/asset-import.json'
previous = json.loads(receipt_file.read_text('utf-8')) if receipt_file.exists() else {}
hashes = previous.get('hashes', {})
saved = []
limit = int(globals().get('web_vfx_asset_limit', 8))
tools = u.AssetToolsHelpers.get_asset_tools()
library = u.MaterialEditingLibrary

def digest(value):
    return hashlib.sha256(value).hexdigest()

def expression(material, cls, x=0, y=0):
    return library.create_material_expression(material, cls, x, y)

def connect(source_node, dest, pin, output=''):
    connected = library.connect_material_expressions(source_node, output, dest, pin)
    if not connected and pin == 'Input':
        names = library.get_material_expression_input_names(dest)
        if len(names) == 1:
            connected = library.connect_material_expressions(source_node, output, dest, names[0])
    assert connected, (dest, pin)

def scalar(material, index, vertex=False):
    node = expression(material, u.MaterialExpressionPerInstanceCustomData, -1000, index * 60)
    node.set_editor_property('data_index', index)
    node.set_editor_property('const_default_value', 1.0 if index in (0, 1, 2, 3, 5) else 0.0)
    if vertex:
        return node
    interpolator = expression(material, u.MaterialExpressionVertexInterpolator, -800, index * 60)
    connect(node, interpolator, 'Input')
    return interpolator

def custom_inputs(names):
    values = []
    for name in names:
        value = u.CustomInput()
        value.set_editor_property('input_name', name)
        values.append(value)
    return values

import runpy
shader_code=runpy.run_path(str(root/"Tools/WebVFX/shader_translation.py"))["shader_code"]

def material_signature(data):
    return digest(('native-shader-v4:' + shader_code(data) + json.dumps({k:v for k,v in data.items() if k!='asset'}, sort_keys=True)).encode('utf-8'))

for geometry in manifest['geometries']:
    name = geometry['id']
    target = '/Game/WebVFX/Meshes/' + name
    file = source / geometry['file']
    fingerprint = digest(b'native-mesh-v2:' + file.read_bytes())
    if hashes.get(target) == fingerprint and u.EditorAssetLibrary.does_asset_exist(target):
        continue
    task = u.AssetImportTask()
    task.set_editor_property('filename', str(file))
    task.set_editor_property('destination_path', '/Game/WebVFX/Meshes')
    task.set_editor_property('destination_name', name)
    task.set_editor_property('automated', True)
    task.set_editor_property('replace_existing', True)
    task.set_editor_property('save', True)
    options = u.FbxImportUI()
    options.set_editor_property('import_materials', False)
    options.set_editor_property('import_textures', False)
    options.set_editor_property('import_as_skeletal', False)
    options.set_editor_property('automated_import_should_detect_type', False)
    options.set_editor_property('mesh_type_to_import', u.FBXImportType.FBXIT_STATIC_MESH)
    options.static_mesh_import_data.set_editor_property('auto_generate_collision', False)
    options.static_mesh_import_data.set_editor_property('generate_lightmap_u_vs', False)
    options.static_mesh_import_data.set_editor_property('normal_import_method', u.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    options.static_mesh_import_data.set_editor_property('import_uniform_scale', 1.0)
    options.static_mesh_import_data.set_editor_property('import_translation', u.Vector())
    options.static_mesh_import_data.set_editor_property('import_rotation', u.Rotator())
    task.set_editor_property('options', options)
    task.set_editor_property('factory', u.FbxFactory())
    tools.import_asset_tasks([task])
    mesh = u.load_asset(target)
    assert isinstance(mesh, u.StaticMesh), 'Native OBJ import failed: ' + target
    # Verify the importer's actual coordinate conversion, including asymmetric
    # geometry such as a translated projectile tail (symmetric spheres hide flips).
    vertices = [[float(x) for x in line.split()[1:4]] for line in file.read_text('utf-8').splitlines() if line.startswith('v ')]
    for vertex in vertices: vertex[1] *= -1
    minimum = [min(v[k] for v in vertices) for k in range(3)]
    maximum = [max(v[k] for v in vertices) for k in range(3)]
    bounds = mesh.get_bounds()
    for k, axis in enumerate(['x', 'y', 'z']):
        assert abs(getattr(bounds.origin, axis) - (minimum[k]+maximum[k])*.5) < .02, (target, 'origin', axis)
        assert abs(getattr(bounds.box_extent, axis) - (maximum[k]-minimum[k])*.5) < .02, (target, 'extent', axis)
    mesh.set_editor_property('allow_cpu_access', False)
    assert u.EditorAssetLibrary.save_loaded_asset(mesh), target
    hashes[target] = fingerprint
    saved.append(target)
    if globals().get('web_vfx_mesh_probe'):
        result = {'probe': target, 'ok': True}
        break
    if len(saved) >= limit:
        break

if not globals().get('web_vfx_mesh_probe'):
    for data in manifest['materials']:
        if len(saved) >= limit:
            break
        target = '/Game/WebVFX/Materials/' + data['id']
        fingerprint = material_signature(data)
        if hashes.get(target) == fingerprint and u.EditorAssetLibrary.does_asset_exist(target):
            continue
        material = u.load_asset(target) or tools.create_asset(data['id'], '/Game/WebVFX/Materials', u.Material, u.MaterialFactoryNew())
        library.delete_all_material_expressions(material)
        material.set_editor_property('two_sided', data['doubleSided'] or data['billboard'])
        material.set_editor_property('wireframe', data.get('wireframe', False))
        material.set_editor_property('used_with_instanced_static_meshes', True)
        blend = u.BlendMode.BLEND_MASKED if data.get('weaponClip') else u.BlendMode.BLEND_ADDITIVE if data['additive'] else u.BlendMode.BLEND_TRANSLUCENT if data['transparent'] else u.BlendMode.BLEND_OPAQUE
        material.set_editor_property('blend_mode', blend)
        pbr = data['type'] == 'MeshStandardMaterial'
        material.set_editor_property('shading_model', u.MaterialShadingModel.MSM_DEFAULT_LIT if pbr else u.MaterialShadingModel.MSM_UNLIT)
        values = [scalar(material, i) for i in range(10)]
        rg = expression(material, u.MaterialExpressionAppendVector, -600, 0)
        connect(values[0], rg, 'A'); connect(values[1], rg, 'B')
        rgb = expression(material, u.MaterialExpressionAppendVector, -450, 0)
        connect(rg, rgb, 'A'); connect(values[2], rgb, 'B')
        uv = expression(material, u.MaterialExpressionTextureCoordinate, -500, -180)
        custom = expression(material, u.MaterialExpressionCustom, 0, 0)
        custom.set_editor_property('output_type', u.CustomMaterialOutputType.CMOT_FLOAT4)
        names = ['UV', 'Color', 'Opacity', 'Age', 'Fade', 'Mode', 'Clock', 'Inner', 'Angle']
        if data.get('portal'): names.append('PortalTexture')
        if data.get('weaponClip'): names.append('LocalPosition')
        if data.get('impactSolid'): names.append('WorldNormal')
        custom.set_editor_property('inputs', custom_inputs(names))
        code = shader_code(data)
        if data.get("impactSolid"):
            normal=expression(material,u.MaterialExpressionPixelNormalWS,-450,-400)
            connect(normal,custom,"WorldNormal")
        if data.get('portal'):
            portal = data['portal']; variant = portal['variant']
            source_texture = Path(unquote(urlparse(portal['texture']).path).lstrip('/'))
            assert source_texture.is_file() and source_texture.suffix == '.png', source_texture
            tex_name = 'T_TreasuryPortal_' + str(variant)
            task = u.AssetImportTask(); task.filename = str(source_texture)
            task.destination_path = '/Game/WebVFX/Textures'; task.destination_name = tex_name
            task.automated = True; task.replace_existing = True; task.save = True
            tools.import_asset_tasks([task])
            texture = u.load_asset('/Game/WebVFX/Textures/' + tex_name)
            assert isinstance(texture, u.Texture2D), source_texture
            texture.set_editor_property('srgb',True); assert u.EditorAssetLibrary.save_loaded_asset(texture)
            obj = expression(material, u.MaterialExpressionTextureObject, -450, -400)
            obj.set_editor_property('texture',texture); connect(obj, custom, 'PortalTexture')
        if data.get('weaponClip'):
            local = expression(material, u.MaterialExpressionPreSkinnedPosition, -450, -400)
            interpolator = expression(material, u.MaterialExpressionVertexInterpolator, -200, -400)
            connect(local, interpolator, 'Input'); connect(interpolator, custom, 'LocalPosition')
        custom.set_editor_property('code', code)
        custom.set_editor_property('description', 'RuleStudio GLSL source translation: ' + data['id'])
        connect(uv, custom, 'UV'); connect(rgb, custom, 'Color')
        for index, name in zip(range(3, 10), names[2:]): connect(values[index], custom, name)
        rgb_mask = expression(material, u.MaterialExpressionComponentMask, 250, -80)
        rgb_mask.set_editor_property('r', True); rgb_mask.set_editor_property('g', True); rgb_mask.set_editor_property('b', True); rgb_mask.set_editor_property('a', False)
        connect(custom, rgb_mask, 'Input')
        alpha = expression(material, u.MaterialExpressionComponentMask, 250, 80)
        alpha.set_editor_property('r', False); alpha.set_editor_property('g', False); alpha.set_editor_property('b', False); alpha.set_editor_property('a', True)
        connect(custom, alpha, 'Input')
        library.connect_material_property(rgb_mask, '', u.MaterialProperty.MP_BASE_COLOR if pbr else u.MaterialProperty.MP_EMISSIVE_COLOR)
        if blend != u.BlendMode.BLEND_OPAQUE: library.connect_material_property(alpha, '', u.MaterialProperty.MP_OPACITY_MASK if blend == u.BlendMode.BLEND_MASKED else u.MaterialProperty.MP_OPACITY)
        if pbr:
            for key, prop in [('roughness', u.MaterialProperty.MP_ROUGHNESS), ('metalness', u.MaterialProperty.MP_METALLIC)]:
                node = expression(material, u.MaterialExpressionConstant, 300, 300)
                node.set_editor_property('r', float(data[key])); library.connect_material_property(node, '', prop)
            emissive = expression(material, u.MaterialExpressionConstant3Vector, 300, 400)
            emissive.set_editor_property('constant', u.LinearColor(*[float(x) * data['emissiveIntensity'] for x in data['emissive']], 1.0))
            library.connect_material_property(emissive, '', u.MaterialProperty.MP_EMISSIVE_COLOR)
        if 'p.x+=' in data['vertex']:
            local = expression(material, u.MaterialExpressionPreSkinnedPosition, -500, 600)
            deform = expression(material, u.MaterialExpressionCustom, -100, 600)
            deform.set_editor_property('output_type', u.CustomMaterialOutputType.CMOT_FLOAT3)
            deform.set_editor_property('inputs', custom_inputs(['LocalPosition', 'Clock']))
            deform.set_editor_property('code', 'return float3(sin(LocalPosition.z*.08+Clock*4.)*9.,0.,0.);')
            connect(local, deform, 'LocalPosition'); connect(scalar(material, 7, True), deform, 'Clock')
            transform = expression(material, u.MaterialExpressionTransform, 150, 600)
            transform.set_editor_property('transform_source_type', u.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_LOCAL)
            transform.set_editor_property('transform_type', u.MaterialVectorCoordTransform.TRANSFORM_WORLD)
            connect(deform, transform, 'Input'); library.connect_material_property(transform, '', u.MaterialProperty.MP_WORLD_POSITION_OFFSET)
        library.recompile_material(material)
        assert u.EditorAssetLibrary.save_loaded_asset(material), target
        hashes[target] = fingerprint
        saved.append(target)
    # Asset paths are stable; the runtime never imports source OBJ/GLSL.
    for geometry in manifest['geometries']: geometry['asset'] = '/Game/WebVFX/Meshes/' + geometry['id']
    for material in manifest['materials']: material['asset'] = '/Game/WebVFX/Materials/' + material['id']
    manifest_path.write_text(json.dumps(manifest, separators=(',', ':')), 'utf-8')
    pending = []
    for geometry in manifest['geometries']:
        target = '/Game/WebVFX/Meshes/' + geometry['id']
        expected = digest(b'native-mesh-v2:' + (source / geometry['file']).read_bytes())
        if hashes.get(target) != expected or not u.EditorAssetLibrary.does_asset_exist(target): pending.append(target)
    for data in manifest['materials']:
        target = '/Game/WebVFX/Materials/' + data['id']
        expected = material_signature(data)
        if hashes.get(target) != expected or not u.EditorAssetLibrary.does_asset_exist(target): pending.append(target)
    result = {'ok': True, 'meshes': len(manifest['geometries']), 'materials': len(manifest['materials']), 'clips': len(manifest['clips']), 'changed': len(saved), 'imported': len(hashes), 'pending': len(pending), 'done': not pending}

receipt_file.write_text(json.dumps({'hashes': hashes, 'saved': saved}, indent=2), 'utf-8')
u.log('WEB_VFX_ASSETS ' + json.dumps(result))
