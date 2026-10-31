"""Copy the selected library hit's registry-audited closure, never overwrite assets."""
from pathlib import Path
import hashlib
import json
import shutil

target=Path(__file__).resolve().parents[2]
source=target/'CustomTools/特效'
selected='/Game/IceSkill_VFX/Niagara/Projectile_Small/NS_Ice_Hit_Small'
audit=json.loads((source/'Docs/IceHitCandidates.json').read_text(encoding='utf-8'))
candidate=next(c for c in audit['candidates'] if c['system']==selected)
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
plan=[]
for package in candidate['packages']:
 if any(c in ('World','Blueprint','AnimBlueprint','SkeletalMesh') for c in package['classes']):
  raise RuntimeError('Unexpected demo dependency '+package['package'])
 rel=Path(package['package'].removeprefix('/Game/'))
 files=[(source/'Content'/rel).with_suffix(ext) for ext in ('.uasset','.uexp','.ubulk')]
 if not files[0].is_file():raise RuntimeError(str(files[0]))
 for src in files:
  if not src.is_file():continue
  dst=target/'Content'/src.relative_to(source/'Content')
  sha=digest(src)
  if dst.exists() and digest(dst)!=sha:raise RuntimeError('Existing asset differs: '+str(dst))
  plan.append((src,dst,sha,not dst.exists()))
for src,dst,sha,added in plan:
 if added:
  dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,dst)
 if digest(dst)!=sha:raise RuntimeError('Copy hash mismatch '+str(dst))
report={'system':selected,'registry_package_count':len(candidate['packages']),'file_count':len(plan),'new_files':sum(int(p[3]) for p in plan),'files':[{'path':str(dst.relative_to(target)).replace('\\','/'),'sha256':sha,'bytes':dst.stat().st_size,'added':added} for src,dst,sha,added in plan]}
(target/'Docs/IceHitMigration.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({k:v for k,v in report.items() if k!='files'}))
