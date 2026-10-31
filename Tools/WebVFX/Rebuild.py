"""Run as a PythonScript commandlet in DungeonWorldOpus, outside PIE."""
from pathlib import Path
import runpy,json,unreal
root=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve()
manifest=json.loads((root/'Saved/WebVFX/Source/manifest.json').read_text(encoding='utf-8'))
for _ in range((len(manifest['geometries'])+len(manifest['materials'])+7)//8+1):
 result=runpy.run_path(str(root/'Tools/WebVFX/build_native_assets.py'),init_globals={'web_vfx_asset_limit':8})['result']
 if result['done']:break
assert result['done'],result
for _ in range((len(manifest['clips'])+5)//6+1):
 result=runpy.run_path(str(root/'Tools/WebVFX/build_native_prefabs.py'),init_globals={'web_vfx_prefab_limit':6})['result']
 if result['done']:break
assert result['done'],result
(root/'Saved/WebVFX/rebuild-result.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
unreal.log('OPUS_VFX_REBUILT '+json.dumps(result))
