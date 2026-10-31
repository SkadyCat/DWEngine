"""Author legacy monster defaults and the shared spawn exclusion; explicit range traits take priority."""
import json
from pathlib import Path
import unreal

folder = '/Game/Monsters/Config'
name = 'DA_GlobalMonsterSettings'
asset_path = folder + '/' + name
asset_class = unreal.load_class(None, '/Script/Angelscript.DWOMonsterSettings')
assert asset_class is not None, 'Missing AngelScript monster settings class'
asset = unreal.load_asset(asset_path)
created = asset is None
if created:
    unreal.EditorAssetLibrary.make_directory(folder)
    factory = unreal.DataAssetFactory()
    factory.set_editor_property('data_asset_class', asset_class)
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, asset_class, factory)
assert asset is not None and asset.get_class() == asset_class, asset_path
if created:
    asset.set_editor_property('AggroRadiusMeters', 20.0)
    asset.set_editor_property('PlayerSpawnExclusionRadiusMeters', 10.0)
assert unreal.EditorAssetLibrary.save_loaded_asset(asset, False), 'Could not save ' + asset_path
receipt = {
    'asset': asset.get_path_name(),
    'class': asset.get_class().get_path_name(),
    'aggroRadiusMeters': asset.get_editor_property('AggroRadiusMeters'),
    'playerSpawnExclusionRadiusMeters': asset.get_editor_property('PlayerSpawnExclusionRadiusMeters'),
}
proof = Path(unreal.Paths.project_saved_dir()) / 'Verification' / 'MonsterAggro'
proof.mkdir(parents=True, exist_ok=True)
(proof / 'settings-asset.json').write_text(json.dumps(receipt, indent=2), encoding='utf-8')
web_settings = Path(unreal.Paths.project_dir()) / 'CustomTools' / 'RuleStudio' / 'public' / 'data' / 'global-monster-settings.mjs'
web_settings.parent.mkdir(parents=True, exist_ok=True)
web_settings.write_text('// Exported from /Game/Monsters/Config/DA_GlobalMonsterSettings by Tools/AI/Create-GlobalMonsterSettings.py.\nexport const GLOBAL_MONSTER_SETTINGS = Object.freeze(' + json.dumps({
    'aggroRadiusMeters': receipt['aggroRadiusMeters'],
    'playerSpawnExclusionRadiusMeters': receipt['playerSpawnExclusionRadiusMeters'],
}) + ');\n', encoding='utf-8')
unreal.log('MONSTER_SETTINGS_ASSET_SAVED ' + json.dumps(receipt))
