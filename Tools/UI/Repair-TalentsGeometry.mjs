// Repair the saved UICreator geometry; keep node IDs, rules, graph topology and Imagegen originals.
import fs from 'node:fs/promises';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {ProjectRepository} from '../../CustomTools/UICreatorAlpha/server/project-repository.mjs';
import {validateDocument} from '../../CustomTools/UICreatorAlpha/src/core/model.js';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'../..');
const repo=new ProjectRepository(path.join(root,'CustomTools/UICreatorAlpha/data/projects'));
const saved=await repo.read('opus-talent-constellation');const doc=saved.document;
const tree=JSON.parse(await fs.readFile(path.join(root,'Content/Rules/Talents/constellation.json'),'utf8'));
const all=[];function visit(n){all.push(n);for(const c of n.children||[])visit(c);}visit(doc.root);
const byid=new Map(all.map(n=>[n.id,n])),world=new Map(tree.nodes.map(n=>[n.id,n]));
const lines=byid.get('talent-lines'),gs=lines.source.graphScale,origin=lines.source.origin;
const place=(n,x,y,w,h)=>{Object.assign(n.slot.offsets,{left:x,top:y,right:w,bottom:h});Object.assign(n.layout,{width:w,height:h});};
const svg=(w,h,body)=>`<svg xmlns="http://www.w3.org/2000/svg" width="${w}" height="${h}" viewBox="0 0 ${w} ${h}">${body}</svg>`;
let corrected=0;
for(const n of tree.nodes){
 const button=byid.get('point-'+n.id),content=byid.get('point-'+n.id+'-content'),ring=byid.get('ring-'+n.id);
 // getBoundingClientRect of an SVG group included the hidden 3x3 atlas extents.
 // The canonical box contains the actual circle/marker, not the uncropped <image>.
 const r=n.kind==='origin'?31:n.kind==='attribute'?9.5:n.sourceBuild?26:22;
 const before=ring.source.circleStates[0].circles[0];
 const oldCenter={x:button.slot.offsets.left+before.cx*ring.layout.width,y:button.slot.offsets.top+before.cy*ring.layout.height};
 const center={x:origin.x+n.x*gs,y:origin.y+n.y*gs};
 if(Math.hypot(oldCenter.x-center.x,oldCenter.y-center.y)>.01)corrected++;
 const half=r+6,x=-half,y=n.kind==='legendary'?-36:-half,w=half*2,h=half-y;
 const oldWidth=ring.layout.width/gs;
 const states=ring.source.circleStates.map(state=>({...state,circles:state.circles.map(c=>({...c,cx:-x/w,cy:-y/h,r:c.r*oldWidth/w}))}));
 ring.source.circleStates=states;
 place(button,center.x+x*gs,center.y+y*gs,w*gs,h*gs);place(content,0,0,w*gs,h*gs);place(ring,0,0,w*gs,h*gs);
 button.source.canonicalWorldCenter={x:n.x,y:n.y};button.source.skinBounds={x,y,width:w,height:h};
 const color=ring.source.nodeColor;
 const mask=n.kind==='origin'?1:0;
 doc.assets[ring.svgAsset].source=svg(w*gs,h*gs,states[mask].circles.map(c=>`<circle cx="${c.cx*w*gs}" cy="${c.cy*h*gs}" r="${c.r*w*gs}" fill="${c.fill==='node'?color:c.fill}" fill-opacity="${c.fillOpacity}" stroke="${c.stroke==='node'?color:c.stroke}" stroke-opacity="${c.strokeOpacity}" stroke-width="${c.width}"/>`).join(''));
 const icon=byid.get('icon-'+n.id);
 if(icon){const size=n.kind==='legendary'?34:15;place(icon,(-x-size/2)*gs,(-y-size/2)*gs,size*gs,size*gs);icon.source={...(icon.source||{}),runtimeRasterFrom:n.kind==='legendary'?'legend-sample-'+n.id:'attr-sample-intelligence'};}
 if(n.kind==='legendary')place(byid.get('crown-'+n.id),(-x-4)*gs,(-y-36)*gs,8*gs,10*gs);
 if(n.kind==='origin')place(byid.get('star-origin'),(-x-19)*gs,(-y-19)*gs,38*gs,38*gs);
}
const P=(x,y)=>({x,y}),add=(a,b,k=1)=>P(a.x+b.x*k,a.y+b.y*k),sub=(a,b)=>P(a.x-b.x,a.y-b.y),mix=(a,b,t)=>add(a,sub(b,a),t);
function arc(start,rx,ry,rotation,large,sweep,end){
 rx=Math.abs(rx);ry=Math.abs(ry);if(!rx||!ry)return [[start,mix(start,end,1/3),mix(start,end,2/3),end]];
 const phi=rotation*Math.PI/180,cos=Math.cos(phi),sin=Math.sin(phi),dx=(start.x-end.x)/2,dy=(start.y-end.y)/2;
 const xp=cos*dx+sin*dy,yp=-sin*dx+cos*dy,lambda=xp*xp/(rx*rx)+yp*yp/(ry*ry);
 if(lambda>1){rx*=Math.sqrt(lambda);ry*=Math.sqrt(lambda);}
 const numerator=rx*rx*ry*ry-rx*rx*yp*yp-ry*ry*xp*xp,den=rx*rx*yp*yp+ry*ry*xp*xp;
 const f=(large===sweep?-1:1)*Math.sqrt(Math.max(0,numerator/den)),cxp=f*rx*yp/ry,cyp=-f*ry*xp/rx;
 const cx=cos*cxp-sin*cyp+(start.x+end.x)/2,cy=sin*cxp+cos*cyp+(start.y+end.y)/2;
 const u=P((xp-cxp)/rx,(yp-cyp)/ry),v=P((-xp-cxp)/rx,(-yp-cyp)/ry);
 const theta=Math.atan2(u.y,u.x);let delta=Math.atan2(u.x*v.y-u.y*v.x,u.x*v.x+u.y*v.y);
 if(!sweep&&delta>0)delta-=Math.PI*2;if(sweep&&delta<0)delta+=Math.PI*2;
 const at=t=>P(cx+rx*cos*Math.cos(t)-ry*sin*Math.sin(t),cy+rx*sin*Math.cos(t)+ry*cos*Math.sin(t));
 const tangent=t=>P(-rx*cos*Math.sin(t)-ry*sin*Math.cos(t),-rx*sin*Math.sin(t)+ry*cos*Math.cos(t));
 const count=Math.ceil(Math.abs(delta)/(Math.PI/4)),out=[];let p0=start;
 for(let i=0;i<count;i++){const a=theta+delta*i/count,b=theta+delta*(i+1)/count,k=4/3*Math.tan((b-a)/4),p3=i===count-1?end:at(b);out.push([p0,add(p0,tangent(a),k),add(p3,tangent(b),-k),p3]);p0=p3;}
 return out;
}
function cubicPath(d){const ts=d.match(/[A-Za-z]|[-+]?(?:\d*\.\d+|\d+\.?\d*)(?:[eE][-+]?\d+)?/g)||[];let i=0,p=P(0,0),curves=[];
 const num=()=>{const n=Number(ts[i++]);if(!Number.isFinite(n))throw Error('Invalid SVG number: '+d);return n;};
 while(i<ts.length){const cmd=ts[i++];if(cmd==='M')p=P(num(),num());else if(cmd==='L'){const end=P(num(),num());curves.push([p,mix(p,end,1/3),mix(p,end,2/3),end]);p=end;}else if(cmd==='C'){const a=P(num(),num()),b=P(num(),num()),end=P(num(),num());curves.push([p,a,b,end]);p=end;}else if(cmd==='A'){const rx=num(),ry=num(),angle=num(),large=num(),sweep=num(),end=P(num(),num());curves.push(...arc(p,rx,ry,angle,large,sweep,end));p=end;}else throw Error('Unsupported path command '+cmd);}
 return curves;
}
lines.source.vectorPaths=tree.edges.map(e=>{const key=[...e].sort().join(':'),route=tree.layout.edgeRoutes[key];const curves=cubicPath(route.path);const first=curves[0][0],last=curves.at(-1)[3],a=world.get(route.from),b=world.get(route.to);if(Math.hypot(first.x-a.x,first.y-a.y)>.001||Math.hypot(last.x-b.x,last.y-b.y)>.001)throw Error('Curve endpoint mismatch '+key);return {key,from:route.from,to:route.to,curves,sourcePath:route.path};});
let maxCenterError=0;for(const n of tree.nodes){const b=byid.get('point-'+n.id),r=byid.get('ring-'+n.id),c=r.source.circleStates[0].circles[0];maxCenterError=Math.max(maxCenterError,Math.hypot(b.slot.offsets.left+c.cx*r.layout.width-origin.x-n.x*gs,b.slot.offsets.top+c.cy*r.layout.height-origin.y-n.y*gs));}
if(maxCenterError>1e-6)throw Error('Node/curve centers differ');
doc.provenance={...doc.provenance,geometryRepair:{cause:'SVG atlas image overflow contaminated measured group bounds',canonicalNodes:1000,previouslyMisalignedLegendary:corrected,bezierEdges:1113,maxNodeCurveCenterError:maxCenterError,topologyChanged:false}};
const errors=validateDocument(doc);if(errors.length)throw Error(errors.join('\n'));
for(const directory of ['CustomTools/UICreatorAlpha/data/projects','UIAuthoring/projects']){const target=new ProjectRepository(path.join(root,directory));const pointer=await target.pointer(doc.id);await target.write(doc,pointer?.revision||0);}
await fs.writeFile(path.join(root,'UIAuthoring/TalentConstellation.uialpha.json'),JSON.stringify(doc,null,2));
const report={corrected,canonicalNodes:1000,bezierEdges:lines.source.vectorPaths.length,cubicSegments:lines.source.vectorPaths.reduce((s,p)=>s+p.curves.length,0),maxCenterError};await fs.writeFile(path.join(root,'Saved/Verification/Talents/LoadCurves/source-geometry.json'),JSON.stringify(report,null,2));console.log(JSON.stringify(report));
