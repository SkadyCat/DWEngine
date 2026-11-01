"""Compare actual ground-phase joint motion in matched straight-line trials."""
import collections
import csv
import json
import math
import pathlib
import statistics

ROOT=pathlib.Path(__file__).resolve().parents[1]
cal={m['id']:m['clips'] for m in json.loads((ROOT/'Docs/gait-calibration.json').read_text(encoding='utf8'))}
groups=collections.defaultdict(list)
for row in csv.DictReader((ROOT/'Saved/Diagnostics/gait-probe.csv').open(encoding='utf8')):
    groups[(row['model'],row['state'],float(row['fraction']),int(row['synced']))].append(row)

def inspect(values):
    clip=cal[values[0]['model']][values[0]['state']]
    slips=[];ratios=[];speed=float(values[0]['body_speed'])
    for a,b in zip(values,values[1:]):
        dt=float(b['time'])-float(a['time'])
        if float(a['time'])<.35 or dt<=0 or float(b['anim_time'])<float(a['anim_time']):continue
        measured=(float(b['x'])-float(a['x']))/dt
        assert abs(measured-speed)<.1, ('probe movement', measured,speed)
        for index,foot in enumerate(('left','right')):
            z=float(b[foot+'_z']);previous_z=float(a[foot+'_z'])
            vx=(float(b[foot+'_x'])-float(a[foot+'_x']))/dt
            vy=(float(b[foot+'_y'])-float(a[foot+'_y']))/dt
            vz=(z-previous_z)/dt
            if max(z,previous_z)<=clip['minimumFootZ'][index]+7 and abs(vz)<50 and vx-speed < -15:
                slips.append(math.hypot(vx,vy));ratios.append(abs(vx)/speed)
    assert len(slips)>=5, ('not enough ground-phase samples',values[0],len(slips))
    return {'samples':len(slips),'meanJointMotionCmS':statistics.mean(slips),
            'medianJointMotionCmS':statistics.median(slips),'meanForwardSlipRatio':statistics.mean(ratios)}

results=[]
assert len(groups)==50, ('Expected 25 variants in both modes',len(groups))
for model,state,fraction in sorted({key[:3] for key in groups}):
    old,new=inspect(groups[(model,state,fraction,0)]),inspect(groups[(model,state,fraction,1)])
    results.append({'model':model,'state':state,'fraction':fraction,'legacy':old,'synchronized':new})
aggregate={}
for label,subset in [('walk',[r for r in results if r['state']=='walk']),
                     ('slowRun',[r for r in results if r['state']=='run' and r['fraction']<.3]),
                     ('run',[r for r in results if r['state']=='run' and r['fraction']>.3])]:
    before=statistics.mean(r['legacy']['meanJointMotionCmS'] for r in subset)
    after=statistics.mean(r['synchronized']['meanJointMotionCmS'] for r in subset)
    aggregate[label]={'variants':len(subset),'beforeCmS':before,'afterCmS':after,'reductionPercent':100*(1-after/before)}
report={'verification':'PENDING','models':14,'variants':25,'cases':50,'aggregate':aggregate,'results':results,
        'boundary':'Low foot-joint horizontal motion is a comparison proxy, not a complete shoe-sole contact/IK measurement. Mage source has no stride and is excluded.'}
out=ROOT/'Docs/gait-analysis.json'
out.write_text(json.dumps(report,indent=2),encoding='utf8')
assert aggregate['walk']['afterCmS']<aggregate['walk']['beforeCmS']*.5,aggregate
assert aggregate['slowRun']['afterCmS']<aggregate['slowRun']['beforeCmS']*.5,aggregate
assert aggregate['run']['afterCmS']<=aggregate['run']['beforeCmS']*1.15,aggregate
report['verification']='PASS';out.write_text(json.dumps(report,indent=2),encoding='utf8')
print(json.dumps({'verification':'PASS','aggregate':aggregate},indent=2))
