"""Read the authoring project's registry; export only two effects' /Game dependency closure."""
from pathlib import Path
import json
import unreal

source=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve()
target=Path('G:/DungeonWorldOpus')
roots=[
 '/Game/IceSkill_VFX/Niagara/Portal_Spear/NS_IceSpear_Projectile',
 '/Game/IceSkill_VFX/Niagara/Portal_Spear/NS_Ice_Explosion_Spear',
]
registry=unreal.AssetRegistryHelpers.get_asset_registry()
registry.search_all_assets(True)
options=unreal.AssetRegistryDependencyOptions(include_soft_package_references=True,include_hard_package_references=True,include_game_package_references=True,include_editor_only_package_references=True)
pending=list(roots);seen=set();packages=[]
while pending:
 name=pending.pop()
 if name in seen:continue
 seen.add(name)
 if not name.startswith('/Game/'):continue
 assets=registry.get_assets_by_package_name(name,include_only_on_disk_assets=True)
 if not assets:raise RuntimeError('Missing dependency '+name)
 classes=sorted({str(a.asset_class_path.asset_name) for a in assets})
 if any(c in ('World','Blueprint','AnimBlueprint','SkeletalMesh') for c in classes):
  raise RuntimeError('Unexpected gameplay/demo dependency '+name+' '+str(classes))
 packages.append({'package':name,'classes':classes})
 pending.extend(str(dep) for dep in registry.get_dependencies(name,options))
systems=[]
for name in roots:
 asset=unreal.load_asset(name)
 if not isinstance(asset,unreal.NiagaraSystem):raise RuntimeError(name)
 params=unreal.NiagaraFunctionLibrary.get_all_user_parameters(asset)
 emitters=unreal.NiagaraFunctionLibrary.get_all_emitters(asset)
 systems.append({'package':name,'parameters':[{'name':str(p.parameter_name),'type':str(p.type_name)} for p in params],'emitters':[{'name':str(e.emitter_name),'enabled':e.is_enabled,'meshes':[m.get_path_name() for m in e.used_renderer_meshes],'materials':[m.get_path_name() for m in e.used_renderer_materials]} for e in emitters]})
report={'source_project':str(source),'roots':roots,'packages':sorted(packages,key=lambda p:p['package']),'systems':systems,'package_count':len(packages)}
(target/'Saved/Verification/UEVFX/migration-plan.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
unreal.log('ICE_VFX_MIGRATION '+json.dumps(report,ensure_ascii=False))
