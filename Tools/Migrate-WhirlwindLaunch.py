"""Migrate only the two authored whirlwind preset rings after the game exits."""
from pathlib import Path
from datetime import datetime
import copy, hashlib, json, shutil

root=Path(__file__).resolve().parents[1]
proof=root/'Saved/Verification/WhirlwindParity/DungeonGround'
profile=root/'Saved/RuleTower/Profile.v2.json'
uids={'whirlwind_grounds_v2_ring_bdt1','whirlwind_swarm_v3_ring_bdt1'}

def update(value):
    if isinstance(value,dict):return {k:update(v) for k,v in value.items()}
    if isinstance(value,list):return [update(v) for v in value]
    if value=='proj_layout_ring':return 'proj_layout_fan'
    if value=='投射物向角色周围环形发射':return '投射物以扇形排列发射'
    return value

before=json.loads(profile.read_text('utf-8-sig'));after=copy.deepcopy(before);changed=[]
for uid in sorted(uids):
    item=after.get('instances',{}).get(uid)
    if item is not None:
        revised=update(item)
        if revised!=item:after['instances'][uid]=revised;changed.append(uid)
expected=copy.deepcopy(after)
for uid in changed:expected['instances'][uid]=before['instances'][uid]
assert expected==before,'Unexpected profile mutation'
backup=proof/('Profile-launch-migration-'+datetime.now().strftime('%Y%m%d-%H%M%S')+'.json')
shutil.copy2(profile,backup)
if changed:
    temporary=profile.with_name('Profile.v2.whirlwind.tmp')
    temporary.write_text(json.dumps(after,ensure_ascii=False,separators=(',',':')),encoding='utf-8')
    temporary.replace(profile)
assert json.loads(profile.read_text('utf-8-sig'))==after
receipt={'changed_instances':changed,'backup':str(backup),'before_sha256':hashlib.sha256(backup.read_bytes()).hexdigest(),'after_sha256':hashlib.sha256(profile.read_bytes()).hexdigest(),'other_profile_fields_unchanged':True}
(proof/'profile-launch-migration.json').write_text(json.dumps(receipt,indent=2),encoding='utf-8')
print(json.dumps(receipt,ensure_ascii=False))
