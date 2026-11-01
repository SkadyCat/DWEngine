"""Analyze real runtime samples; keep candidate pose anomalies separate from defects."""
import collections
import csv
import hashlib
import json
import math
import pathlib
import statistics

ROOT=pathlib.Path(__file__).resolve().parents[1]
source=json.loads((ROOT/'Docs/source-audit.json').read_text(encoding='utf8'))
poses=json.loads((ROOT/'Docs/pose-audit.json').read_text(encoding='utf8'))
compression=json.loads((ROOT/'Docs/compression-audit.json').read_text(encoding='utf8'))
def percentile(values,q):
    v=sorted(values)
    return v[min(len(v)-1,int((len(v)-1)*q))] if v else None
def inspect(mode):
    rows=list(csv.DictReader((ROOT/'Saved/Diagnostics'/f'{mode}.csv').open(encoding='utf8')))
    ids={int(r['id']):int(r['team']) for r in rows}
    assert len(ids)==100,(mode,'unit count',len(ids))
    assert dict(collections.Counter(ids.values()))=={0:34,1:33,2:33},(mode,'factions')
    assert len({r['model'] for r in rows})==15,(mode,'model coverage')
    states=collections.Counter(r['state'] for r in rows)
    assert all(s in states for s in ('idle','run','attack1','hit','death')),(mode,'state coverage',states)
    assert max(float(r['time']) for r in rows)>=39,(mode,'run duration')
    assert all(int(r['feet_valid'])==1 for r in rows),(mode,'missing foot bones')
    active=[r for r in rows if float(r['time'])>2.5 and float(r['health'])>0]
    attack=[r for r in active if r['state'] in ('attack1','attack2') and float(r['anim_time'])>.15]
    moving=[r for r in attack if float(r['speed'])>25]
    tally={key:sum(max(int(r[key]) for r in rows if int(r['id'])==i) for i in ids) for key in ('attacks','hits','deaths')}
    return {'samples':len(rows),'units':100,'factions':[34,33,33],'models':15,'duration':max(float(r['time']) for r in rows),'states':dict(states),'events':tally,'attackSamples':len(attack),'movingAttackSamples':len(moving),'movingAttackPercent':100*len(moving)/max(1,len(attack)),'turnOver720Samples':sum(float(r['turn_deg_s'])>720 for r in active),'rootOffsetP99':percentile([float(r['root_offset']) for r in rows],.99)},rows

comparisons={};raw={}
for mode in ('opus','controlled'):
    comparisons[mode],raw[mode]=inspect(mode)
anatomy={'head','neck_01','neck_02','spine_01','spine_02','spine_03','upperarm_l','upperarm_r','lowerarm_l','lowerarm_r','thigh_l','thigh_r','calf_l','calf_r','foot_l','foot_r'}
models=[]
for s,p,c in zip(source,poses,compression):
    assert s['id']==p['id']==c['id']
    outliers=[dict(state=state,**x) for state,data in p['clips'].items() for x in data['boneLengthOutliers'] if x['bone'] in anatomy]
    maximum=max(x['distance'] for values in c['clips'].values() for x in values)
    models.append({'id':s['id'],'sameSkeleton':all(x['sameSkeleton'] for x in s['clips'].values()),'boneScaleMin':min(x['scaleMin'] for x in p['clips'].values()),'boneScaleMax':max(x['scaleMax'] for x in p['clips'].values()),'rawCompressedMaxCm':maximum,'anatomicalTranslationCandidates':outliers,'worldPositionOffset':[m['worldPositionOffset'] for m in p['materials']]})
assert all(m['sameSkeleton'] for m in models)
assert max(m['rawCompressedMaxCm'] for m in models)<.1
manifest=json.loads((ROOT/'Docs/asset-manifest.json').read_text(encoding='utf8'))
mismatches=[p['file'] for p in manifest if hashlib.sha256((ROOT/p['file']).read_bytes()).hexdigest()!=p['sha256']]
assert not mismatches,('Copied source asset changed',mismatches)
result={'verification':'PASS','units':100,'factions':[34,33,33],'models':models,'comparison':comparisons,'sourceAssetsVerified':len(manifest),'poseClips':sum(len(p['clips']) for p in poses),'poseSamplesPerClip':9,'notes':['Training damage only triggers visual states; no Opus rules are changed.','IK, weapon, book and interaction bones are excluded from anatomical length candidates.','Foot joint height is not a mesh-sole penetration measurement.','Raw/compressed audit covers pelvis, head and both feet, five phases, five clips per model.']}
(ROOT/'Docs/analysis.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf8')
print(json.dumps({'result':'PASS','comparison':comparisons,'models':len(models),'sourceAssetsVerified':len(manifest)},ensure_ascii=False))
