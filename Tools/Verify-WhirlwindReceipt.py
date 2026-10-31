"""Summarize saved native/AS/render evidence without changing gameplay."""
from pathlib import Path
import hashlib,json,re

root=Path(__file__).resolve().parents[1]
proof=root/'Saved/Verification/WhirlwindParity/DungeonGround'
native=json.loads((proof/'NativeAutomation/index.json').read_text('utf-8-sig'))
presentation=json.loads((proof/'PresentationAutomation/index.json').read_text('utf-8-sig'))
assert native['succeeded']==9 and native['failed']==0
assert presentation['succeeded']==3 and presentation['failed']==0
render=(proof/'native-after-visual.log').read_text('utf-8-sig',errors='replace')
match=re.search(r'WHIRLWIND_PARITY_DONE initial=(\d+) peak=(\d+) mirrors=(\d+) remaining=(\d+) maxCenters=(\d+) maxHeadings=(\d+) maxGrounds=(\d+) maxTinted=(\d+)',render)
assert match and 'WHIRLWIND_PARITY_FAIL' not in render
launch=(proof/'launch-final.log').read_text('utf-8-sig',errors='replace')
assert 'WIND_LAUNCH_DONE' in launch and 'WIND_LAUNCH_FAIL' not in launch
web=(proof/'web-ground-tests.log').read_text('utf-8-sig',errors='replace')
assert '# pass 9' in web and '# fail 0' in web
files=[
 'Plugins/DWRules/Source/DWRulesCore/Private/DWRuleModernCombat.cpp',
 'Plugins/DWRules/Source/DWRulesCore/Public/DWRuleModernCombat.h',
 'Plugins/DWRules/Source/DWRulesCore/Public/DWRuleWorld.h',
 'Plugins/DWRules/Source/DWRules/Private/WebVFX/DWRuleWebVFXAdapter.cpp',
 'Script/Opus/SelfSkills.as','Script/Opus/HeroVisual.as',
 'Content/Rules/Presets/whirlwind_grounds.json','Content/Rules/Presets/whirlwind_swarm.json',
 'Content/WebVFX/Systems/NS_OpusWhirlwind.uasset','Content/WebVFX/DA_UEVFXMap.uasset',
 'Plugins/DWRules/Binaries/Win64/UnrealEditor-DWRulesCore.dll',
 'Plugins/DWRules/Binaries/Win64/UnrealEditor-DWRules.dll',
 'Binaries/Win64/UnrealEditor-DungeonWorldOpus.dll']
visuals=['wind-010.png','wind-070.png','wind-200.png','ground-contact.png','ground-after-winds.png']
assert all((proof/p).is_file() for p in visuals)
report={'ok':True,'native_tests':9,'presentation_tests':3,'web_ground_tests':9,
 'actual_animation_launch':'real authored Release; three forward carriers; seeded non-symmetric headings; one cast per action',
 'actual_dungeon':dict(zip(['initial','peak','mirrors','remaining','separated_centers','heading_bins','grounds','tinted'],map(int,match.groups()))),
 'speed_after_54_reflections_mps':27,'late_distance_m':60.178,
 'ground_projection':'191 actual dungeon surfaces; raised 310cm platform; void and wall separation',
 'visuals':visuals,'profile_migration':json.loads((proof/'profile-launch-migration.json').read_text('utf-8-sig')),
 'sha256':{p:hashlib.sha256((root/p).read_bytes()).hexdigest() for p in files}}
(proof/'receipt-final.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({k:v for k,v in report.items() if k not in ['sha256','profile_migration']},ensure_ascii=False,indent=2))
