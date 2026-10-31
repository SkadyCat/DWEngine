"""Migrate Zero's exact 3C runtime/graph and the referenced presentation closure.

No source workspace is edited. TopDownRuntime's account/UI character base is
replaced by ACharacter; movement prediction and animation code stay unchanged.
"""
from pathlib import Path
import hashlib,json,re,shutil,sys

root=Path(__file__).resolve().parents[2]
zero=Path('G:/DungeonWorldZero')
dry='--dry-run' in sys.argv
plugin=root/'Plugins/MoonScholar3C'
seeds=[
 '/Game/Clean3C/Animation/Channel/ABP_Clean3C_ChannelMobile',
 '/Game/Clean3C/Animation/WeaponLayers/DA_WeaponLayer_0',
 '/Game/Clean3C/Animation/Mage/Rolls/DA_Roll_Mocap114',
 '/Game/Clean3C/Animation/Mage/Set/Set_Mage_Fireball_L_RM',
 '/Game/Clean3C/Animation/Montages/AM_Set_Mage_Fireball_L_RM',
 '/Game/FlexibleCombatSystem/Animation/UE5-Mannequin/Common/Roll',
 '/Game/Clean3C/Animation/Montages/AM_Roll',
]+['/Game/Clean3C/Animation/Montages/AM_Set_Mage_'+c+'_IP' for c in ['Fireball_L','Hurt','Death']]+[
 '/Game/Clean3C/Animation/Montages/AM_Mocap114_Dive_'+d for d in ['F_0','F_R_45','R_90','B_R_45','B_180','B_L_45','L_90','F_L_45']]
queue=seeds.copy();seen=set();files=[];modules=set();missing=set()
while queue:
 pkg=queue.pop()
 if pkg in seen:continue
 seen.add(pkg)
 src=zero/'Content'/(pkg[6:]+'.uasset')
 if not src.is_file():missing.add(pkg);continue
 data=src.read_bytes()
 modules.update(x.decode() for x in re.findall(rb'/Script/[A-Za-z0-9_]+',data))
 dst=root/'Content'/src.relative_to(zero/'Content')
 if not dry and not dst.exists():
  dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,dst)
  for ext in ('.uexp','.ubulk','.uptnl'):
   extra=src.with_suffix(ext)
   if extra.exists():shutil.copy2(extra,dst.with_suffix(ext))
 files.append({'package':pkg,'file':dst.relative_to(root).as_posix(),'bytes':len(data),'sourceSha256':hashlib.sha256(data).hexdigest(),'alreadyPresent':dst.exists()})
 for dep in set(re.findall(rb'/Game/[A-Za-z0-9_/-]+',data)):
  name=dep.decode()
  candidate=zero/'Content'/(name[6:]+'.uasset')
  if candidate.is_file():queue.append(name)
  elif candidate.parent.is_dir():
   # UE's FName table can serialize a numeric suffix separately from the text.
   # Resolve those numbered packages by actual filenames, then recurse normally.
   for numbered in candidate.parent.glob(candidate.stem+'_*.uasset'):
    suffix=numbered.stem[len(candidate.stem)+1:]
    if suffix.isdigit():queue.append('/Game/'+numbered.relative_to(zero/'Content').with_suffix('').as_posix())
assert not missing,missing
assert not any('/UI/' in f['package'] for f in files)
sources=[]
if not dry:
 for module in ('FCS3C','FCS3CAnimGraph'):
  for src in (zero/'Plugins/MoonScholar3C/Source'/module).glob('*'):
   if not src.is_file():continue
   data=src.read_text(encoding='utf-8-sig')
   if src.name=='CleanNetworkCharacter.h':
    data=data.replace('#include "TDNativeBridge.h"','#include "GameFramework/Character.h"').replace(': public ATDNativeCharacter',': public ACharacter')
    yaw='    UFUNCTION(BlueprintCallable, Category="Root Motion") void SetRootMotionTranslationYaw(float WorldDeltaYaw);'
    data=data.replace(yaw,yaw+'\n    UFUNCTION(BlueprintCallable, Category="Root Motion") void SetCleanRootMotionScale(float Scale) { SetAnimRootMotionTranslationScale(Scale); }')
   elif src.name=='FCS3C.Build.cs':data=data.replace('"TopDownRuntime",','')
   dst=plugin/'Source'/module/src.name
   dst.parent.mkdir(parents=True,exist_ok=True)
   if not dst.exists() or dst.read_text(encoding='utf-8')!=data:dst.write_text(data,encoding='utf-8')
   sources.append({'file':dst.relative_to(root).as_posix(),'sourceSha256':hashlib.sha256(src.read_bytes()).hexdigest(),'adapted':src.name in ('CleanNetworkCharacter.h','FCS3C.Build.cs')})
 desc=json.loads((zero/'Plugins/MoonScholar3C/MoonScholar3C.uplugin').read_text(encoding='utf-8-sig'))
 desc['Modules']=[m for m in desc['Modules'] if m['Name']!='FCS3CEditor']
 desc['Plugins']=[p for p in desc['Plugins'] if p['Name']!='TopDownRuntime']
 (plugin/'MoonScholar3C.uplugin').write_text(json.dumps(desc,indent=2)+'\n',encoding='utf-8')
 script=root/'Script/Opus/Zero3C';script.mkdir(parents=True,exist_ok=True)
 shutil.copy2(zero/'Script/MoonScholar/CleanRollAnimationSet.as',script/'CleanRollAnimationSet.as')
 camera=(zero/'Script/MoonScholar/CleanCameraFollow.as').read_text(encoding='utf-8-sig')
 # The source's temporary green debug path is not part of the game camera.
 start=camera.index('        // Show the actual capsule path')
 end=camera.index('        float Speed =',start)
 camera=camera[:start]+camera[end:]
 camera=camera.replace('    void Tick(float DeltaSeconds)\n    {','    void Tick(float DeltaSeconds)\n    {\n        AdvanceFollow(DeltaSeconds);\n    }\n\n    // Share the exact follow step with deterministic integration probes;\n    // BlueprintOverride events cannot be called from another AS component.\n    void AdvanceFollow(float DeltaSeconds)\n    {')
 (script/'CleanCameraFollow.as').write_text(camera,encoding='utf-8')
 report={'source':str(zero),'seeds':seeds,'files':sorted(files,key=lambda f:f['file']),'sources':sources,'assetCount':len(files),'bytes':sum(f['bytes'] for f in files),'modules':sorted(modules),'accountOrUIImported':False}
 (root/'Docs/Zero3CAssets.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print(json.dumps({'dryRun':dry,'assetCount':len(files),'bytes':sum(f['bytes'] for f in files),'modules':sorted(modules),'missing':sorted(missing)}))
