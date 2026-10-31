"""Merge the treasury dependency closure verbatim, preserving unrelated native data."""
import hashlib,json,shutil
from pathlib import Path
root=Path(__file__).resolve().parents[2]
source=root/'Content/Rules';target=root/'Content/RulesTower'
traits={};builds={};equipment={};statuses={}
for table,folders in [(traits,['Traits','Affixes','TimedTraits','WebTraits']),(builds,['Builds','WebBuilds']),(equipment,['Equipment']),(statuses,['Statuses','WebStatuses'])]:
 for folder in folders:
  for file in (source/folder).glob('*.json'):
   doc=json.loads(file.read_text(encoding='utf-8-sig'))
   if isinstance(doc,dict) and 'id' in doc:
    assert doc['id'] not in table,(folder,doc['id'])
    table[doc['id']]=(file,doc)
selected={};edges=[]
def add(table,key,reason):
 assert key in table,(reason,key)
 file,doc=table[key];rel=file.relative_to(source).as_posix();edges.append({'from':reason,'to':rel})
 if rel in selected:return
 selected[rel]=file
 parent=doc.get('parent')
 if parent:add(traits,parent,rel)
 walk(doc,rel)
def walk(value,reason):
 if isinstance(value,list):
  for item in value:walk(item,reason)
 elif isinstance(value,dict):
  if value.get('op') in ('apply_status','consume_status') and value.get('key') in statuses:add(statuses,value['key'],reason)
  for key,v in value.items():
   if key in ('traits','bases','with'):
    for item in v:
     name=item if isinstance(item,str) else item.get('id')
     if name:add(traits,name,reason)
   if key=='legendary' and v:
    name=v if isinstance(v,str) else v.get('id')
    if name:add(traits,name,reason)
   if key=='skills':
    for name in v:
     if name:add(builds,name,reason)
   if key=='skill' and v:add(builds,v,reason)
   if key=='baseId' and v:add(equipment,v,reason)
   walk(v,reason)
add(builds,'royal_treasury','requested build')
add(equipment,'royal_key','bound weapon')
walk(json.loads((source/'Presets/royal_treasury.json').read_text(encoding='utf-8-sig')),'royal_treasury preset')
backup=root/'Saved/Verification/UEIntegration/treasury-data-before';proof=[]
for rel,file in sorted(selected.items()):
 dst=target/rel;changed=not dst.exists() or dst.read_bytes()!=file.read_bytes()
 if changed:
  if dst.exists():
   old=backup/rel;old.parent.mkdir(parents=True,exist_ok=True)
   if not old.exists():shutil.copy2(dst,old)
  dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(file,dst)
 digest=hashlib.sha256(file.read_bytes()).hexdigest();assert hashlib.sha256(dst.read_bytes()).hexdigest()==digest
 proof.append({'file':rel,'sha256':digest,'changed':changed})
receipt={'files':proof,'dependencies':edges,'unknownReferences':0}
(root/'Saved/Verification/UEIntegration/treasury-data-sync.json').write_text(json.dumps(receipt,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'files':len(proof),'changed':[p['file'] for p in proof if p['changed']],'dependencies':len(edges),'unknownReferences':0}))
