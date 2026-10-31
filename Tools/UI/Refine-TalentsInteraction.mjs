// User-requested refinement of the saved UICreator project; topology and Imagegen originals stay intact.
import fs from 'node:fs/promises';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {ProjectRepository} from '../../CustomTools/UICreatorAlpha/server/project-repository.mjs';
import {createNode,validateDocument} from '../../CustomTools/UICreatorAlpha/src/core/model.js';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'../..');
const repo=new ProjectRepository(path.join(root,'CustomTools/UICreatorAlpha/data/projects'));
const saved=await repo.read('opus-talent-constellation');if(!saved)throw Error('Saved talent project missing');
const doc=saved.document,tree=JSON.parse(await fs.readFile(path.join(root,'Content/Rules/Talents/constellation.json'),'utf8'));
const all=[];function visit(n){all.push(n);for(const c of n.children||[])visit(c);}visit(doc.root);
const byid=new Map(all.map(n=>[n.id,n])),svg=(w,h,b)=>`<svg xmlns="http://www.w3.org/2000/svg" width="${w}" height="${h}" viewBox="0 0 ${w} ${h}">${b}</svg>`;
function asset(id,w,h,source){doc.assets[id]={id,name:id,source,viewBox:[0,0,w,h],scaleMode:'stretch',slice:{top:0,right:0,bottom:0,left:0},folderId:''};return id;}
function node(type,id,x,y,w,h,extra={}){return createNode(type,{id,name:id,binding:id,motion:{preset:'none'},slot:{offsets:{left:x,top:y,right:w,bottom:h}},layout:{width:w,height:h},style:{fill:'transparent',strokeWidth:0,radius:0,...extra.style},...extra});}
// Ear clipping yields the exact concave SVG polygon, not a bitmap or a convex hull.
function triangulate(p){const cross=(a,b,c)=>(b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);const area=p.reduce((s,a,i)=>s+a.x*p[(i+1)%p.length].y-p[(i+1)%p.length].x*a.y,0);let rem=p.map((_,i)=>i);if(area<0)rem.reverse();const out=[];while(rem.length>3){let ear=false;for(let i=0;i<rem.length;i++){const a=rem[(i+rem.length-1)%rem.length],b=rem[i],c=rem[(i+1)%rem.length];if(cross(p[a],p[b],p[c])<=1e-10)continue;if(rem.some(k=>k!==a&&k!==b&&k!==c&&cross(p[a],p[b],p[k])>=-1e-10&&cross(p[b],p[c],p[k])>=-1e-10&&cross(p[c],p[a],p[k])>=-1e-10))continue;out.push(a,b,c);rem.splice(i,1);ear=true;break;}if(!ear)throw Error('Invalid simple polygon');}out.push(...rem);const triangles=out.reduce((s,_,i)=>i%3?s:s+Math.abs(cross(p[out[i]],p[out[i+1]],p[out[i+2]])),0);if(Math.abs(triangles-Math.abs(area))>1e-8)throw Error('Polygon triangulation area mismatch');return out;}
function polygon(id,coords,w,h,fill,stroke='none',width=0){const points=coords.map(([x,y])=>({x:x/w,y:y/h}));byid.get(id).source={...(byid.get(id).source||{}),vectorPolygons:[{points,indices:triangulate(points),fill,stroke,width:width/w}],origin:'authored SVG path, normalized to its original viewBox'};}
polygon('star-origin',[[19,0],[25,13],[38,19],[25,25],[19,38],[13,25],[0,19],[13,13]],38,38,'#cdb37b','#ead9af',1);
for(const n of all)if(n.id.startsWith('crown-'))polygon(n.id,[[0,5],[4,0],[8,5],[4,10]],8,10,'#c5a875');
for(const n of all)if(n.type==='TextBlock'&&typeof n.text==='string')n.text=n.text.replaceAll('+10','+5');
for(const a of tree.attributes){const n=byid.get('talent-choose-'+a.id),w=n.layout.width,h=n.layout.height;const shape=(bg)=>svg(w,h,`<rect x=".5" y=".5" width="${w-1}" height="${h-1}" rx="4" fill="${bg}" stroke="${a.color}"/>`);n.brushes.hovered=asset('attribute-hover-'+a.id,w,h,shape('#b5a17416'));n.brushes.pressed=asset('attribute-pressed-'+a.id,w,h,shape('#b5a17420'));n.source={...n.source,attributeId:a.id,tooltip:a.name+' +'+a.value};}
if(!byid.has('talent-choice-tip')){const tip=node('CanvasPanel','talent-choice-tip',700,565,116,30,{visibility:'collapsed',interactable:false});tip.children=[node('Svg','talent-choice-tip-background',0,0,116,30,{svgAsset:asset('attribute-choice-tooltip',116,30,svg(116,30,'<rect x=".5" y=".5" width="115" height="29" rx="4" fill="#191d20f7" stroke="#91764b"/>'))}),node('TextBlock','talent-choice-tip-text',8,5,100,20,{text:'敏捷 +5',style:{fill:'transparent',color:'#e3cb9c',fontSize:12,fontFamily:'Segoe UI, Microsoft YaHei, sans-serif',fontWeight:500,textAlign:'center',verticalAlign:'center',wrap:false}})];doc.root.children.push(tip);}
doc.provenance={...doc.provenance,refinement:{request:'Remove star/crown mosaic, authored attribute hover and numeric tooltip, all four attributes +5',attributeValue:5,vectorPolygons:101,topologyChanged:false,originalImagegenPNGsChanged:false}};
const errors=validateDocument(doc);if(errors.length)throw Error(errors.join('\n'));
for(const directory of ['CustomTools/UICreatorAlpha/data/projects','UIAuthoring/projects']){const target=new ProjectRepository(path.join(root,directory));const pointer=await target.pointer(doc.id);await target.write(doc,pointer?.revision||0);}
await fs.writeFile(path.join(root,'UIAuthoring/TalentConstellation.uialpha.json'),JSON.stringify(doc,null,2));
console.log(JSON.stringify({id:doc.id,vectorPolygons:101,attributeValue:5,authoredHoverButtons:4,tooltip:'talent-choice-tip',nodes:all.length+(byid.has('talent-choice-tip')?0:3)}));
