"""Merge the cold-validated authoring catalogue, retaining native-only definitions."""
import json,hashlib,shutil,sys
from pathlib import Path
r=Path(__file__).resolve().parents[2]
source=r/'Content/Rules';out=r/'Content/RulesTower';proof=r/'Saved/Verification/UEIntegration'
logs=sorted(proof.glob('content-audit-*.log'),key=lambda p:p.stat().st_mtime,reverse=True)
assert logs and 'CONTENT_TEST_PASS Catalog ' in logs[0].read_text(encoding='utf-8',errors='replace'),'Full current authoring catalogue must pass before runtime promotion'
assert '--apply' in sys.argv or '--check' in sys.argv,'Use --check or --apply after the cold catalogue audit'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
files=sorted(source.rglob('*.json'));source_ids={p.relative_to(source).as_posix() for p in files}
def identity(p,root):
 rel=p.relative_to(root);family=rel.parts[0].removeprefix('Web')
 doc=json.loads(p.read_text(encoding='utf-8-sig'))
 return (family,doc.get('id')) if isinstance(doc,dict) and doc.get('id') else None,doc
source_keys={}
for p in files:
 key,doc=identity(p,source)
 if key:source_keys[key]=(p,doc)
preserved=[];duplicates=[]
for p in out.rglob('*.json'):
 if p.relative_to(out).as_posix() in source_ids:continue
 key,doc=identity(p,out);canonical=source_keys.get(key)
 if canonical:
  assert doc==canonical[1],f'Conflicting native-only definition {p}: source {canonical[0]}'
  duplicates.append((p,canonical[0]))
 else:preserved.append(p.relative_to(out).as_posix())
if '--check' in sys.argv:
 print(json.dumps({'files':len(files),'preservedNativeOnly':preserved,'identicalDuplicates':[p.relative_to(out).as_posix() for p,_ in duplicates]}));sys.exit(0)
backup=proof/'rules-before-full-sync';changed=[];manifest=[]
for p in files:
 rel=p.relative_to(source);dst=out/rel;before=sha(dst) if dst.exists() else None
 if before!=sha(p):
  if dst.exists():
   b=backup/rel;b.parent.mkdir(parents=True,exist_ok=True)
   if not b.exists():shutil.copy2(dst,b)
  dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,dst);changed.append(rel.as_posix())
 assert sha(dst)==sha(p)
 manifest.append({'path':rel.as_posix(),'sha256':sha(p)})
deduplicated=[]
for p,canonical in duplicates:
 rel=p.relative_to(out);b=backup/'duplicates'/rel;b.parent.mkdir(parents=True,exist_ok=True)
 if not b.exists():shutil.copy2(p,b)
 p.unlink();deduplicated.append({'path':rel.as_posix(),'canonical':canonical.relative_to(source).as_posix(),'backup':b.relative_to(r).as_posix()})
receipt={'source':'Content/Rules','runtime':'Content/RulesTower','audit':str(logs[0].relative_to(r)),'changed':changed,'preservedNativeOnly':preserved,'deduplicated':deduplicated,'files':manifest}
(proof/'rules-full-sync.json').write_text(json.dumps(receipt,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'files':len(files),'changed':len(changed),'preservedNativeOnly':len(preserved)}))
