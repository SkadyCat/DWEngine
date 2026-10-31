"""Cold-load all dungeon packages and the saved target map in a separate commandlet."""
import json
from collections import Counter
from pathlib import Path
import unreal as u

ROOT=Path('G:/DungeonWorldOpus')
assert Path(u.Paths.project_dir()).resolve()==ROOT
OUT=ROOT/'Saved/Verification/DungeonExtraction'
manifest=json.loads((ROOT/'Docs/ZeroDungeon/source-assets.json').read_text('utf8'))
loaded=[]
for package in manifest['packages']:
    asset=u.load_asset(package)
    assert asset,package
    loaded.append(package)
world=u.EditorLoadingAndSavingUtils.load_map('/Game/ClaudeDungeon/Maps/L_OpusDungeon')
assert world
actors=list(u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors())
classes=Counter(a.get_class().get_path_name() for a in actors)
assert not any(c.startswith('/Game/') for c in classes),classes
assert not any('TowerRoom' in c for c in classes),classes
assert classes['/Script/ClaudePCG.DungeonGenerator']==1
gen,=[a for a in actors if isinstance(a,u.DungeonGenerator)]
assert len(gen.get_rooms())==11
count=sum(c.get_instance_count() for a in actors for c in a.get_components_by_class(u.InstancedStaticMeshComponent))
assert count==7448,count
assert world.get_world_settings().get_editor_property('default_game_mode').get_path_name()=='/Script/Angelscript.DWOTowerGameMode'
for nav in [a for a in actors if isinstance(a,u.RecastNavMesh)]:
    assert nav.get_editor_property('force_rebuild_on_load')
    assert nav.get_editor_property('runtime_generation')==u.RuntimeGenerationType.DYNAMIC
registry=u.AssetRegistryHelpers.get_asset_registry()
options=u.AssetRegistryDependencyOptions(include_hard_package_references=True,include_soft_package_references=True)
deps=[str(p) for p in registry.get_dependencies('/Game/ClaudeDungeon/Maps/L_OpusDungeon',options)]
for dep in deps:
    if dep.startswith('/Game/'):
        assert dep.startswith(('/Game/ClaudeDungeon/','/Game/ClaudeDungeonKit/')),dep
    if dep.startswith('/Script/'):
        assert dep.split('/')[2] in ('Engine','CoreUObject','ClaudePCG','PCG','NavigationSystem','Angelscript'),dep
report={'loadedPackages':len(loaded),'map':world.get_path_name(),'rooms':11,'instances':count,'classes':dict(classes),'mapDependencies':deps,'savedReloadVerified':True}
(OUT/'cold-audit.json').write_text(json.dumps(report,indent=2),'utf8')
u.log('DUNGEON_COLD_AUDIT_PASS packages=%d instances=%d' % (len(loaded),count))
