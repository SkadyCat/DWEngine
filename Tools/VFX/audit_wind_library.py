"""Run in the VFX library; audit candidate Niagara systems and their exact dependencies."""
from pathlib import Path
import json
import unreal as u

root = Path('G:/DungeonWorldOpus')
project = Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir())).resolve()
if project != (root / 'CustomTools/特效').resolve():
    raise RuntimeError('Run the wind audit in the VFX library')
registry = u.AssetRegistryHelpers.get_asset_registry()
registry.search_all_assets(True)
options = u.AssetRegistryDependencyOptions(include_soft_package_references=True,
    include_hard_package_references=True, include_game_package_references=True,
    include_editor_only_package_references=True)
candidates = []
for name in ('NS_TopDown_Whirlwind', 'NS_Air_AeroTornado', 'NS_Realistic_GushTwister'):
    path = '/Game/_TornadoWindVFX/NS/' + name
    system = u.load_asset(path)
    if not isinstance(system, u.NiagaraSystem):
        raise RuntimeError('Missing Niagara system: ' + path)
    parameters = u.NiagaraFunctionLibrary.get_all_user_parameters(system)
    emitters = u.NiagaraFunctionLibrary.get_all_emitters(system)
    pending, seen, packages = [path], set(), []
    while pending:
        package = pending.pop()
        if package in seen or not package.startswith('/Game/'):
            continue
        seen.add(package)
        assets = registry.get_assets_by_package_name(package, include_only_on_disk_assets=True)
        if not assets:
            raise RuntimeError('Missing dependency: ' + package)
        classes = sorted({str(asset.asset_class_path.asset_name) for asset in assets})
        if any(c in ('World', 'Blueprint', 'AnimBlueprint', 'SkeletalMesh') for c in classes):
            raise RuntimeError('Unexpected gameplay/demo dependency: ' + package)
        packages.append({'package': package, 'classes': classes})
        pending.extend(str(p) for p in (registry.get_dependencies(package, options) or []))
    row = {'system': path, 'parameters': [
        {'name': str(p.parameter_name), 'type': str(p.type_name)} for p in parameters
    ], 'emitters': [], 'packages': sorted(packages, key=lambda p: p['package'])}
    for emitter in emitters:
        row['emitters'].append({'name': str(emitter.emitter_name), 'enabled': emitter.is_enabled,
            'meshes': [m.get_path_name() for m in emitter.used_renderer_meshes],
            'materials': [m.get_path_name() for m in emitter.used_renderer_materials]})
    try:
        row['fixed_bounds'] = str(system.get_editor_property('fixed_bounds'))
    except Exception:
        pass
    candidates.append(row)
output = root / 'Saved/Verification/WindVFX/library-audit.json'
output.write_text(json.dumps({'project': str(project), 'candidates': candidates}, indent=2), encoding='utf-8')
u.log('WIND_LIBRARY_AUDIT_PASS systems=' + str(len(candidates)))
