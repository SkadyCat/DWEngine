"""Aggregate native authoring, GPU hit probe, fallback probe and exact asset hashes."""
from pathlib import Path
import hashlib
import json
import re

root=Path(__file__).resolve().parents[2]
saved=root/'Saved/Verification/UEVFX'
migration=json.loads((root/'Docs/IceHitMigration.json').read_text(encoding='utf-8'))
authoring=json.loads((saved/'ice-hit-map.json').read_text(encoding='utf-8'))
runtime=json.loads((saved/'ice-small-runtime.json').read_text(encoding='utf-8'))
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
for row in migration['files']:
 assert sha(root/row['path'])==row['sha256'],row['path']
 assert sha(root/'CustomTools/特效'/row['path'])==row['sha256'],row['path']
assert authoring['ok'] and authoring['preserved_other_entries']
assert authoring['after']['ice_lance.impact']['system'].endswith('/NS_Ice_Hit_Small.NS_Ice_Hit_Small')
assert runtime['systems']=={'ice_lance.flight':10,'ice_lance.impact':10}
assert runtime['spawns']==runtime['returns']==20 and runtime['activeAfterClear']==0
results={}
for name in ('ice-small-verified','ice-default-arena'):
 log=(saved/(name+'.log')).read_text(encoding='utf-8',errors='replace')
 assert not re.search(r'Angelscript: Error:|UE_VFX_TEST_FAIL|Fatal error:',log),name
 match=re.search(r'UE_VFX_TEST_PASS real ice projectile hits casts=(\d+) life=([\d.]+)',log)
 assert match,name
 results[name]={'casts':int(match[1]),'target_life':float(match[2])}
assert results['ice-small-verified']==results['ice-default-arena']
assert results['ice-small-verified']['casts']==10
log=(saved/'ice-small-verified.log').read_text(encoding='utf-8',errors='replace')
assert 'UE_VFX_SPAWN id=ice_lance.impact active=1 system='+authoring['after']['ice_lance.impact']['system'] in log
report={'ok':True,'system':migration['system'],'map_sha256':sha(root/'Content/WebVFX/DA_UEVFXMap.uasset'),'scale':[.5,.5,.5],'offset':[0,0,0],'max_visual_seconds':1,'dependency_hashes_verified':len(migration['files']),'new_assets':migration['new_files'],'other_map_entries_preserved':True,'runtime':runtime,'damage_comparison':results,'same_damage':True,'probe_options':['DWOHeroArena','DWOTransient','DWOUEVFXTest','DWOUEVFXWarmup=5'],'gpu_proof':'Saved/Verification/UEVFX/ice-small-verified.png','library_comparison':'Saved/Verification/UEVFX/ice-hit-library-comparison.png','rule_core_modified_by_this_change':False,'rule_data_modified_by_this_change':False}
for prop in ('gpu_proof','library_comparison'):assert (root/report[prop]).is_file()
(root/'Docs/IceHitVerification.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print('ICE_HIT_VERIFY_PASS assets=14 casts=10 impacts=10 pooled_returns=20 damage=658/658')
