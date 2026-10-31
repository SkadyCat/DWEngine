"""Copy the existing Beta wizard presentation and its exact asset dependencies.
No UI, scripts, rule data or legacy gameplay plugin is migrated.
"""
from pathlib import Path
import hashlib,json,re,shutil
source=Path('G:/DungeonWorldBeta/Content')
target=Path(__file__).resolve().parents[2]/'Content'
base='/Game/Clean3C/Animation/Mage/Set/'
clips=['Idle','Fireball_L','Hurt','Death']+['Run_'+d for d in ['F','FR','R','BR','B','BL','L','FL']]+['Dodge_'+d for d in ['F','FR','R','BR','B','BL','L','FL']]
seeds=[
 '/Game/Characters/WizardHero/FingerRig/SK_WizardHero',
 '/Game/Characters/WizardHero/StanceFix20261018/ABP_WizardRetarget',
 '/Game/Characters/WizardHero/Materials/Shimmer20261018/MI_WizardHero_Shimmer',
 '/Game/G2_Mage/Meshes/Characters/Combines/SK_Judy_D3',
]+[base+'Set_Mage_'+name+'_IP' for name in clips]
queue=list(seeds);seen=set();missing=set();files=[];modules=set()
while queue:
 pkg=queue.pop()
 if pkg in seen:continue
 seen.add(pkg)
 src=source/(pkg[6:]+'.uasset')
 if not src.is_file():missing.add(pkg);continue
 data=src.read_bytes();modules.update(x.decode() for x in re.findall(rb'/Script/[A-Za-z0-9_]+',data))
 dst=target/src.relative_to(source)
 # Shared monster/material assets may already exist: never overwrite another task.
 if not dst.exists():dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,dst)
 for ext in ('.uexp','.ubulk','.uptnl'):
  extra=src.with_suffix(ext);copy=dst.with_suffix(ext)
  if extra.exists() and not copy.exists():shutil.copy2(extra,copy)
 files.append({'package':pkg,'file':dst.relative_to(target.parent).as_posix(),'bytes':len(data),'sourceSha256':hashlib.sha256(data).hexdigest()})
 for dep in set(re.findall(rb'/Game/[A-Za-z0-9_/-]+',data)):
  name=dep.decode()
  if (source/(name[6:]+'.uasset')).is_file():queue.append(name)
assert all(x not in missing for x in seeds),missing
assert not any('/UI/' in f['package'] or '/Rules/' in f['package'] for f in files)
report={'source':'G:/DungeonWorldBeta','seeds':seeds,'files':sorted(files,key=lambda x:x['file']),'scriptModules':sorted(modules),'assetCount':len(files),'bytes':sum(f['bytes'] for f in files),'presentationOnly':True}
(target.parent/'Docs/HeroAssets.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print(json.dumps({k:report[k] for k in ('assetCount','bytes','scriptModules')}))
