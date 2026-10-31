"""Audit the frozen UICreator geometry, including sampled connector clearance."""
import hashlib,json,math,sys
from pathlib import Path
from shapely.geometry import LineString,Point
from shapely.strtree import STRtree

root=Path(__file__).resolve().parents[2]
job=sys.argv[1]
bundle=root/'CustomTools/UICreatorAlpha/exports'/job/'bundle.json'
doc=json.loads(bundle.read_text(encoding='utf-8'))
tree=json.loads((root/'Content/Rules/Talents/constellation.json').read_text(encoding='utf-8'))
world={n['id']:n for n in tree['nodes']};nodes={n['id']:n for n in doc['nodes']}
layout={n['id']:n for n in doc['states']['canvas']['nodes']}
source=nodes['talent-lines']['source'];scale=source['graphScale'];origin=source['origin']
paths=source['vectorPaths'];max_center_error=0
for point in tree['nodes']:
    p=layout['point-'+point['id']];ring=nodes['ring-'+point['id']]
    circle=ring['source']['circleStates'][0]['circles'][0]
    x=p['x']-layout['talent-graph']['x']+circle['cx']*p['width'];y=p['y']-layout['talent-graph']['y']+circle['cy']*p['height']
    max_center_error=max(max_center_error,math.hypot(x-origin['x']-point['x']*scale,y-origin['y']-point['y']*scale))
assert max_center_error<.001,max_center_error
assert len(paths)==len(tree['edges'])==1113
assert {p['key'] for p in paths}=={':'.join(sorted(e)) for e in tree['edges']}
def evaluate(c,t):
    basis=((1-t)**3,3*t*(1-t)**2,3*t*t*(1-t),t**3)
    return tuple(sum(c[i][k]*v for i,v in enumerate(basis)) for k in ('x','y'))
curves=[]
for p in paths:
    first=p['curves'][0][0];last=p['curves'][-1][3]
    for q,key in ((first,p['from']),(last,p['to'])):
        assert math.hypot(q['x']-world[key]['x'],q['y']-world[key]['y'])<.001,(p['key'],key)
    for a,b in zip(p['curves'],p['curves'][1:]):assert a[3]==b[0]
    # 32 steps per cubic; each circle arc spans at most 45 degrees.
    line=LineString([evaluate(p['curves'][0],0)]+[evaluate(c,j/32) for c in p['curves'] for j in range(1,33)])
    assert line.is_simple,p['key']
    curves.append(line)
index=STRtree(curves);crossings=[];clearance=[]
points={key:Point(n['x'],n['y']) for key,n in world.items()}
for i,line in enumerate(curves):
    for j in index.query(line):
        j=int(j)
        if j<=i or {paths[i]['from'],paths[i]['to']}&{paths[j]['from'],paths[j]['to']}:continue
        if line.intersects(curves[j]):crossings.append([paths[i]['key'],paths[j]['key']])
    for key,point in points.items():
        if key in (paths[i]['from'],paths[i]['to']):continue
        n=world[key];radius=31 if n['kind']=='origin' else 9.5 if n['kind']=='attribute' else 26 if n.get('sourceBuild') else 22
        if line.distance(point)<radius+2:clearance.append([paths[i]['key'],key])
assert not crossings,crossings
assert not clearance,clearance
assets=doc['project']['provenance']['sourceAssets']
for asset in assets:
    assert hashlib.sha256((bundle.parent/'source-assets'/asset['name']).read_bytes()).hexdigest()==asset['sha256']
assert len(assets)==16
report={'sourceHash':doc['sourceHash'],'nodesAudited':1000,'frozenNodeCenterMaxErrorPixels':max_center_error,'bezierPaths':1113,'cubicSegments':sum(len(p['curves']) for p in paths),'smoothSingleCubicConnectors':sum(bool(p.get('smoothFit')) for p in paths),'sampleStepsPerCubic':32,'nonincidentCrossings':crossings,'nonincidentNodeClearanceViolations':clearance,'originalImagegenPNGs':16}
(root/'Saved/Verification/Talents/LoadCurves/frozen-geometry.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps(report))
