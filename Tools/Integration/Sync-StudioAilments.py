"""Promote only the three explicitly approved Studio reactions, with identical values."""
import json,shutil,hashlib
from pathlib import Path
root=Path(__file__).resolve().parents[2]
source=root/'Content/Rules';target=root/'Content/RulesTower'
proof=root/'Saved/Verification/StudioSync';backup=proof/'rules-before';backup.mkdir(parents=True,exist_ok=True)
ids=['chill_consume_burst','shock_consume_strike','freeze_consume_shards']
tier_file=Path('Values/trait_tiers.json')
before=json.loads((target/tier_file).read_text(encoding='utf-8-sig'))
author=json.loads((source/tier_file).read_text(encoding='utf-8-sig'))
assert before['tiers']==author['tiers'] and before['version']==author['version']
old_tiers=json.loads(json.dumps(before))
for id in ids:before['traits'][id]=author['traits'][id]
if old_tiers!=before:
 b=backup/tier_file;b.parent.mkdir(parents=True,exist_ok=True)
 if not b.exists():shutil.copy2(target/tier_file,b)
 (target/tier_file).write_text(json.dumps(before,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
receipt=[]
for id in ids:
 rel=Path('Affixes')/(id+'.json');dst=target/rel
 if dst.exists() and dst.read_bytes()!=(source/rel).read_bytes():
  b=backup/rel;b.parent.mkdir(parents=True,exist_ok=True)
  if not b.exists():shutil.copy2(dst,b)
 shutil.copy2(source/rel,dst)
 assert dst.read_bytes()==(source/rel).read_bytes()
 receipt.append({'id':id,'sha256':hashlib.sha256(dst.read_bytes()).hexdigest(),'tiersEqual':before['traits'][id]==author['traits'][id]})
(proof/'approved-reactions.json').write_text(json.dumps({'ruleEngineChanged':False,'reactions':receipt},ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'copied':ids,'otherTraitValuesUnchanged':all(before['traits'][k]==v for k,v in old_tiers['traits'].items() if k not in ids)}))
