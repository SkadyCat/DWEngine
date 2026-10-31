"""Import only the two Studio presentation factories, outside PIE."""
import runpy,json,unreal
from pathlib import Path
root=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve()
assert not unreal.EditorLevelLibrary.get_pie_worlds(True)
manifest=root/'Saved/WebVFX/Source/studio-manifest.json'
options={'web_vfx_manifest_path':manifest,'web_vfx_asset_limit':128,'web_vfx_prefab_limit':2}
result=runpy.run_path(str(root/'Tools/WebVFX/build_native_assets.py'),init_globals=options)['result']
assert result['done'],result
result=runpy.run_path(str(root/'Tools/WebVFX/build_native_prefabs.py'),init_globals=options)['result']
assert result['done'],result
unreal.log('STUDIO_MODELS_IMPORTED '+json.dumps(result))
