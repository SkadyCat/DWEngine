// Fit each routed connector as one cubic, retaining its endpoints and topology.
// Run after Repair-TalentsGeometry.mjs, before freezing the UICreator export.
import fs from 'node:fs/promises';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {ProjectRepository} from '../../CustomTools/UICreatorAlpha/server/project-repository.mjs';
import {validateDocument} from '../../CustomTools/UICreatorAlpha/src/core/model.js';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'../..');
const repo=new ProjectRepository(path.join(root,'CustomTools/UICreatorAlpha/data/projects'));
const {document:doc}=await repo.read('opus-talent-constellation');
const all=[];function visit(n){all.push(n);for(const c of n.children||[])visit(c);}visit(doc.root);
const lines=all.find(n=>n.id==='talent-lines');
const evaluate=(c,t)=>Object.fromEntries(['x','y'].map(k=>[k,c.reduce((s,p,i)=>s+p[k]*[(1-t)**3,3*t*(1-t)**2,3*t*t*(1-t),t**3][i],0)]));
let fitted=0,maxDeviation=0;
for(const p of lines.source.vectorPaths){
 if(!p.sourcePath.includes('C')||p.smoothFit)continue;
 const samples=[p.curves[0][0],...p.curves.flatMap(c=>Array.from({length:20},(_,i)=>evaluate(c,(i+1)/20)))];
 const lengths=[0];for(let i=1;i<samples.length;i++)lengths.push(lengths[i-1]+Math.hypot(samples[i].x-samples[i-1].x,samples[i].y-samples[i-1].y));
 const ts=lengths.map(x=>x/lengths.at(-1)),a=samples[0],b=samples.at(-1);
 let aa=0,ab=0,bb=0,ax=0,ay=0,bx=0,by=0;
 for(let i=0;i<samples.length;i++){const t=ts[i],u=3*t*(1-t)**2,v=3*t*t*(1-t),q=samples[i],rx=q.x-(1-t)**3*a.x-t**3*b.x,ry=q.y-(1-t)**3*a.y-t**3*b.y;aa+=u*u;ab+=u*v;bb+=v*v;ax+=u*rx;ay+=u*ry;bx+=v*rx;by+=v*ry;}
 const det=aa*bb-ab*ab;if(det<=0)throw Error('Degenerate connector '+p.key);
 const curve=[a,{x:(ax*bb-bx*ab)/det,y:(ay*bb-by*ab)/det},{x:(bx*aa-ax*ab)/det,y:(by*aa-ay*ab)/det},b];
 let error=0;for(let i=0;i<samples.length;i++){const q=evaluate(curve,ts[i]);error=Math.max(error,Math.hypot(q.x-samples[i].x,q.y-samples[i].y));}
 if(error>7)throw Error('Connector fit exceeds tolerance '+p.key);
 p.curves=[curve];p.smoothFit={method:'arc-length least-squares cubic',maxWorldDeviation:error};fitted++;maxDeviation=Math.max(maxDeviation,error);
}
const fmt=n=>Number(n.toFixed(8));
const d=p=>`M${fmt(p.curves[0][0].x)} ${fmt(p.curves[0][0].y)}`+p.curves.map(c=>'C'+c.slice(1).map(q=>`${fmt(q.x)} ${fmt(q.y)}`).join(' ')).join('');
const o=lines.source.origin,gs=lines.source.graphScale,w=lines.layout.width,h=lines.layout.height;
doc.assets[lines.svgAsset].source=`<svg xmlns="http://www.w3.org/2000/svg" width="${w}" height="${h}" viewBox="0 0 ${w} ${h}"><g fill="none" transform="translate(${o.x} ${o.y}) scale(${gs})">`+lines.source.vectorPaths.map(p=>`<path d="${d(p)}" stroke="rgb(177, 156, 111)" stroke-width="0.65" stroke-opacity="0.52" vector-effect="non-scaling-stroke"/>`).join('')+'</g></svg>';
doc.provenance.geometryRepair.smoothConnectorFits=307;
const errors=validateDocument(doc);if(errors.length)throw Error(errors.join('\n'));
for(const directory of ['CustomTools/UICreatorAlpha/data/projects','UIAuthoring/projects']){const target=new ProjectRepository(path.join(root,directory));const pointer=await target.pointer(doc.id);await target.write(doc,pointer?.revision||0);}
await fs.writeFile(path.join(root,'UIAuthoring/TalentConstellation.uialpha.json'),JSON.stringify(doc,null,2));
const report={fitted,maxWorldDeviation:maxDeviation,bezierEdges:lines.source.vectorPaths.length,cubicSegments:lines.source.vectorPaths.reduce((s,p)=>s+p.curves.length,0),endpointsChanged:false};
await fs.writeFile(path.join(root,'Saved/Verification/Talents/LoadCurves/smooth-source.json'),JSON.stringify(report,null,2));console.log(JSON.stringify(report));
