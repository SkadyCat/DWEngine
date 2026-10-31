"""Read Zero's current dungeon without saving or changing its source packages."""
import json
import re
from collections import Counter
from pathlib import Path
import unreal as u

SOURCE = Path('G:/DungeonWorldZero')
OUT = Path('G:/DungeonWorldOpus/Saved/Verification/DungeonExtraction')
assert Path(u.Paths.project_dir()).resolve() == SOURCE
OUT.mkdir(parents=True, exist_ok=True)
world = u.EditorLoadingAndSavingUtils.load_map('/Game/ClaudeDungeon/Maps/L_ClaudeDungeon')
assert world
actors = list(u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors())
gen, = [a for a in actors if isinstance(a, u.DungeonGenerator)]

def val(v):
    if isinstance(v, (str, int, float, bool)) or v is None:
        return v
    if isinstance(v, u.EnumBase):
        enum = re.match(r'<([\w]+)\.([\w]+):',str(v))
        assert enum, str(v)
        return {'enum':enum.group(1),'name':enum.group(2)}
    if isinstance(v, u.Rotator):
        return {'pitch': v.pitch, 'yaw': v.yaw, 'roll': v.roll}
    if hasattr(v, 'export_text') and not isinstance(v, (u.Vector,u.Vector2D,u.Color,u.LinearColor)):
        return {'text': v.export_text()}
    if hasattr(v, 'to_tuple'):
        return list(v.to_tuple())
    if hasattr(v, 'get_path_name'):
        return {'asset': v.get_path_name()}
    return {'text': v.export_text()} if hasattr(v, 'export_text') else str(v)

def props(obj, names):
    result = {}
    for name in names:
        try:
            result[name] = val(obj.get_editor_property(name))
        except Exception:
            pass
    return result

environment = []
for a in actors:
    row = {'class': a.get_class().get_path_name(), 'name': a.get_name(), 'label': a.get_actor_label(),
           'location': val(a.get_actor_location()), 'rotation': val(a.get_actor_rotation()),
           'scale': val(a.get_actor_scale3d())}
    if isinstance(a, (u.Light, u.SkyLight)):
        c = a.get_component_by_class(u.LightComponentBase)
        row['light'] = props(c, ['intensity','light_color','cast_shadows','cast_static_shadows',
            'cast_dynamic_shadows','indirect_lighting_intensity','volumetric_scattering_intensity',
            'source_radius','soft_source_radius','attenuation_radius','temperature','use_temperature',
            'affects_world','source_angle','lower_hemisphere_is_black','lower_hemisphere_color',
            'real_time_capture','source_type','cubemap','sky_distance_threshold'])
        row['mobility'] = str(c.get_editor_property('mobility'))
        environment.append(row)
    elif isinstance(a, u.PostProcessVolume):
        row['postprocess'] = props(a, ['unbound','blend_weight','priority','settings'])
        environment.append(row)
    elif isinstance(a, u.ExponentialHeightFog):
        row['fog'] = props(a.get_component_by_class(u.ExponentialHeightFogComponent),
            ['fog_density','fog_height_falloff','fog_inscattering_luminance','start_distance',
             'enable_volumetric_fog','volumetric_fog_scattering_distribution','volumetric_fog_extinction_scale',
             'volumetric_fog_albedo','volumetric_fog_emissive','volumetric_fog_distance'])
        environment.append(row)

registry = u.AssetRegistryHelpers.get_asset_registry()
registry.search_all_assets(True)
roots = {'/Game/ClaudeDungeon/PCG/PCG_ClaudeDungeon'}
# The dresser selects these meshes by dynamic soft paths; AssetRegistry alone cannot see them.
roots.update(str(a.package_name) for a in registry.get_assets_by_path('/Game/ClaudeDungeonKit/Meshes', True))
roots.add('/Game/ClaudeDungeonKit/Particles/P_Fire')
options = u.AssetRegistryDependencyOptions(include_soft_package_references=True,
    include_hard_package_references=True, include_searchable_names=False,
    include_soft_management_references=False, include_hard_management_references=False)
pending = list(roots)
closure = set()
external = set()
while pending:
    pkg = pending.pop()
    if pkg in closure:
        continue
    closure.add(pkg)
    for dep in registry.get_dependencies(pkg, options):
        dep = str(dep)
        if dep.startswith('/Game/'):
            assert dep.startswith(('/Game/ClaudeDungeonKit/', '/Game/ClaudeDungeon/PCG/')), dep
            pending.append(dep)
        else:
            external.add(dep)
report = {'source': str(SOURCE), 'map': world.get_path_name(),
    'classes': dict(Counter(a.get_class().get_path_name() for a in actors)),
    'params': gen.get_editor_property('params').export_text(),
    'generatorTransform': val(gen.get_actor_transform()),
    'start': val(gen.get_player_start_location()), 'center': val(gen.get_dungeon_center()),
    'extent': val(gen.get_dungeon_extent()),
    'rooms': [{'center': val(r.center), 'size': val(r.size), 'theme': str(r.theme)} for r in gen.get_rooms()],
    'environment': environment, 'packages': sorted(closure), 'externalPackages': sorted(external),
    'instances': sum(c.get_instance_count() for a in actors for c in a.get_components_by_class(u.InstancedStaticMeshComponent)),
    'actors': [{'class': a.get_class().get_path_name(), 'name': a.get_name()} for a in actors]}
(OUT/'zero-source.json').write_text(json.dumps(report, ensure_ascii=False, indent=2, default=val), 'utf8')
u.log('DUNGEON_EXPORT_PASS packages=%d rooms=%d instances=%d' % (len(closure), len(report['rooms']), report['instances']))
