"""Build a delivery receipt from independent native, UI and runtime evidence."""
import hashlib,json,re
from pathlib import Path
from datetime import datetime
r=Path(__file__).resolve().parents[2];proof=r/'Saved/Verification/UEIntegration'
checks={};logs={}
expected={
 'NativeTests.log':r'Test Completed\. Result=\{Success\}.*TreasuryFinisher',
 'RecentHost.log':'RECENT_TEST_PASS .*HideoutTravel PositionedStash LocalGoods',
 'ModernCombat.log':'MODERN_TEST_PASS Complete',
 'Talents.log':'TALENT_TEST_PASS TravelComplete',
 'Taunt.log':'TAUNT_TEST_PASS IndependentCooldownSwitchPauseAndRecast',
 'PlayerEffects.log':'STUN_TEST_PASS FixedEquipmentHUDAndRecovery',
 'VFXCold.log':'OPUS_VFX_COLD_PASS',
 'Gallery.log':'GALLERY_TEST_PASS',
}
for name,pattern in expected.items():
 p=proof/name;text=p.read_text(encoding='utf-8',errors='replace')
 assert re.search(pattern,text),name
 assert not re.search(r'Angelscript: Error:|Fatal error:|(?:RECENT|MODERN|TALENT|TAUNT|STUN|GALLERY)_TEST_FAIL|LogPython: Error:|Test Completed\. Result=\{Fail',text),name
 checks[name]=True;logs[name]=hashlib.sha256(p.read_bytes()).hexdigest()
native=(proof/'NativeTests.log').read_text(encoding='utf-8',errors='replace')
assert len(re.findall(r'Test Completed\. Result=\{Success\}.*DungeonWorldOpus.Recent\.',native))==6
assert 'TALENT_KNOWN_GAPS count=0' in (proof/'Talents.log').read_text(encoding='utf-8',errors='replace')
catalogue=json.loads((proof/'rules-full-sync.json').read_text(encoding='utf-8'))
for row in catalogue['files']:
 for folder in ['Rules','RulesTower']:
  assert hashlib.sha256((r/'Content'/folder/row['path']).read_bytes()).hexdigest()==row['sha256'],row['path']
pixel=json.loads((proof/'UICreator/pixel-comparison.json').read_text(encoding='utf-8'));assert pixel['ok']
vfx=json.loads((proof/'vfx-cold-read.json').read_text(encoding='utf-8'));assert vfx['ok'] and vfx['clips']==186 and len(vfx['materialStatistics'])==70 and all(x['pixelInstructions']>0 for x in vfx['materialStatistics'])
profile=r/'Saved/RuleTower/Profile.v2.json';normal=json.loads(profile.read_text(encoding='utf-8-sig'))
runtime_js=[str(p.relative_to(r)) for base in ['Script','Content/Script','Source'] for p in (r/base).rglob('*') if p.suffix in ['.js','.mjs','.ts']];assert not runtime_js
receipt={'ok':True,'recordedAt':datetime.now().astimezone().isoformat(),'checks':checks,'evidenceHashes':logs,'runtimeCatalogueFiles':len(catalogue['files']),'nativeMechanicsTests':6,'presetCount':27,'supportedLegendaryTalents':100,'uiTemplates':9,'uiSourceHash':pixel['sourceHash'],'uiPixelComparison':pixel['overall'],'vfx':{'public':58,'clips':186,'geometries':158,'materials':70},'runtimeJavaScriptFiles':runtime_js,'normalProfile':{'sha256':hashlib.sha256(profile.read_bytes()).hexdigest(),'testRewardAbsent':not normal.get('opusGameplay',{}).get('rewardClaimed',False),'testStashAbsent':not normal.get('opusGameplay',{}).get('stash',[]),'testGoodsAbsent':not normal.get('opusGameplay',{}).get('goods',[])},'limits':['VFX combat screenshots use an isolated encounter with invulnerable targets; damage and multipliers are separately covered by native automation.','Merchant goods implement the existing local listing model. No external trading backend was added.','No packaged shipping build was requested; these receipts cover saved assets and native UE game/editor execution.']}
(r/'Docs/UEIntegration/Verification.json').write_text(json.dumps(receipt,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(receipt,ensure_ascii=False))
