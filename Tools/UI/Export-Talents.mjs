// Authoring only: UICreator owns every visible control, AS owns native behavior.
import fs from 'node:fs/promises';
import path from 'node:path';
import crypto from 'node:crypto';
import {fileURLToPath} from 'node:url';
import {createDocument,createNode,validateDocument,walk} from '../../CustomTools/UICreatorAlpha/src/core/model.js';
import {ProjectRepository} from '../../CustomTools/UICreatorAlpha/server/project-repository.mjs';
import {LEGENDARY_ICONS} from '../../CustomTools/RuleStudio/public/talents/legendary-icons.mjs';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'../..');
const tool=path.join(root,'CustomTools/UICreatorAlpha');
const tree=JSON.parse(await fs.readFile(path.join(root,'Content/Rules/Talents/constellation.json'),'utf8'));
const job=process.argv[2]||`wbp-opus-talents-${Date.now()}`;
if(!/^wbp-[a-zA-Z0-9_-]{1,90}$/.test(job))throw Error('Invalid job');
const frozen=path.join(tool,'exports',job);if(await fs.stat(path.join(frozen,'project-record.json')).catch(()=>null))throw Error('Snapshot already frozen; choose a new job');await fs.mkdir(path.join(frozen,'source-assets'),{recursive:true});
const doc=createDocument('Opus · 天赋星盘');doc.id='opus-talent-constellation';doc.viewport={width:1600,height:900,background:'#080c12'};doc.root.id='talent-screen';
const node=(type,id,x,y,w,h,props={})=>createNode(type,{id,name:id,binding:id,slot:{offsets:{left:x,top:y,right:w,bottom:h}},layout:{width:w,height:h},motion:{preset:'none'},...props});
const text=(id,value,x,y,w,h=24,size=14,color='#b8c6d4')=>node('TextBlock',id,x,y,w,h,{text:value,style:{fontSize:size,color,wrap:true}});
const box=(id,x,y,w,h,children=[],fill='#111820',stroke='#60543b')=>node('Border',id,x,y,w,h,{style:{fill,stroke,strokeWidth:1,radius:6},children:[node('CanvasPanel',id+'-content',0,0,w,h,{children})]});
const button=(id,label,x,y,w,h,op,value='',extra={})=>node('Button',id,x,y,w,h,{action:{type:'talent',op,value},style:{fill:'#15202b',stroke:'#45515d',strokeWidth:1,radius:4},children:label?[text(id+'-label',label,0,0,w,h,14,'#e4d3a4')]:[],...extra});
const svg=(w,h,body)=>`<svg xmlns="http://www.w3.org/2000/svg" width="${w}" height="${h}" viewBox="0 0 ${w} ${h}">${body}</svg>`;
const asset=(id,w,h,source)=>doc.assets[id]={id,name:id,source,viewBox:[0,0,w,h],scaleMode:'stretch',slice:{top:0,right:0,bottom:0,left:0},folderId:''};
const sourceAssets=[];
async function png(file,name){const bytes=await fs.readFile(file);await fs.writeFile(path.join(frozen,'source-assets',name),bytes);sourceAssets.push({name,sha256:crypto.createHash('sha256').update(bytes).digest('hex')});return `/exports/${job}/source-assets/${name}`;}
for(const a of tree.attributes){const href=await png(path.join(root,'CustomTools/RuleStudio/public/talents/icons',a.id+'.png'),a.id+'.png');asset('attr-'+a.id,1,1,svg(1,1,`<image width="1" height="1" href="${href}"/>`));}
const sheets=new Map();for(const info of Object.values(LEGENDARY_ICONS)){const name=path.basename(info.sheet);if(!sheets.has(name))sheets.set(name,await png(path.join(root,'CustomTools/RuleStudio/public',info.sheet),name));}
for(const [id,info]of Object.entries(LEGENDARY_ICONS)){const href=sheets.get(path.basename(info.sheet));asset('legend-'+id,1,1,svg(1,1,`<svg viewBox="${info.column} ${info.row} 1 1" width="1" height="1"><image width="3" height="3" href="${href}"/></svg>`));}
asset('dot',12,12,svg(12,12,'<circle cx="6" cy="6" r="4.8" fill="#101923" stroke="#aa965f" stroke-width="1.2"/><circle cx="6" cy="6" r="1" fill="#d7c385"/>'));
asset('origin',20,20,svg(20,20,'<circle cx="10" cy="10" r="9" fill="#242b2d" stroke="#f3d991"/><path d="m10 2 2 6 6 2-6 2-2 6-2-6-6-2 6-2z" fill="#f3d991"/>'));
asset('legend-ring',24,24,svg(24,24,'<circle cx="12" cy="12" r="11" fill="#111820" stroke="#ad8949"/><circle cx="12" cy="12" r="9.3" fill="none" stroke="#514b3b" stroke-width=".5"/>'));
const scale=.2,X=x=>580+x*scale,Y=y=>370+y*scale,byId=new Map(tree.nodes.map(n=>[n.id,n]));
let lines='';for(const [a,b]of tree.edges){const key=[a,b].sort().join(':'),route=tree.layout.edgeRoutes[key];const points=route?.points||[byId.get(a),byId.get(b)];lines+=`<polyline points="${points.map(p=>`${X(p.x).toFixed(2)},${Y(p.y).toFixed(2)}`).join(' ')}"/>`;}
asset('graph-lines',1160,740,svg(1160,740,`<g fill="none" stroke="#5b605a" stroke-opacity=".65" stroke-width=".55">${lines}</g>`));
const graph=node('CanvasPanel','talent-graph',0,0,1160,740);graph.children.push(node('Svg','talent-lines',0,0,1160,740,{svgAsset:'graph-lines'}));
for(const n of tree.nodes){const w=n.kind==='origin'?14:n.kind==='legendary'?12:6,id='point-'+n.id;const content=node('CanvasPanel',id+'-content',0,0,w,w);content.children.push(node('Svg','ring-'+n.id,0,0,w,w,{svgAsset:n.kind==='origin'?'origin':n.kind==='legendary'?'legend-ring':'dot'}));if(n.kind==='legendary')content.children.push(node('Svg','icon-'+n.id,1.5,1.5,w-3,w-3,{svgAsset:'legend-'+n.id}));else if(n.kind==='attribute')content.children.push(node('Svg','icon-'+n.id,0,0,w,w,{svgAsset:'attr-intelligence',visibility:'hidden'}));graph.children.push(button(id,'',X(n.x)-w/2,Y(n.y)-w/2,w,w,'node',n.id,{style:{fill:'transparent',strokeWidth:0,radius:w/2},children:[content]}));}
doc.root.children.push(text('talent-title','天赋星盘',32,22,600,36,24,'#ecd49a'),text('talent-subtitle','1000 星点 · 100 传奇 · 自由属性 +10',32,61,800,24,13,'#8897a6'),button('talent-close','关闭  Esc',1450,26,118,36,'close'));
doc.root.children.push(box('talent-viewport',24,100,1176,752,[graph],'#0b1118','#2c3b46'));
doc.root.children.push(box('talent-summary',1220,100,352,752,[text('talent-budget','已用 0 / 120',18,16,300,34,24,'#ecd49a'),text('talent-budget-note','规划点数',18,55,190,22,12),button('talent-budget-minus','−10',208,51,52,28,'budget','-10'),button('talent-budget-plus','+10',270,51,52,28,'budget','10'),text('talent-attribute-summary','力量 +0\n敏捷 +0\n智力 +0\n体质 +0',20,105,300,140,18),text('talent-legend-count','传奇  0 / 100',20,265,300,28,18,'#e0c487'),text('talent-selected-legends','尚未点亮传奇',20,306,300,240,13),text('talent-help','点击相邻星点分配天赋。\n属性点可随时更换属性。\n滚轮缩放 · 右键拖动\nP / Esc 关闭',20,562,305,105,13,'#8897a6'),button('talent-clear','重置方案',20,686,144,38,'clear'),button('talent-overview','总览 / 起点',178,686,154,38,'overview')],'#10171f','#655a41'));
doc.root.children.push(text('talent-footer','P 天赋星盘    滚轮缩放    右键拖动    点击星点查看 / 分配',32,863,1200,24,12,'#7e8e9f'));
const menu=box('talent-attribute-menu',480,350,258,112,[text('talent-menu-title','选择属性',12,5,200,22,12,'#dcc58d'),...tree.attributes.map((a,i)=>button('talent-choose-'+a.id,'',10+i*60,32,54,44,'attribute',a.id,{name:a.name+' +10',children:[node('Svg','sample-'+a.id,11,6,32,32,{svgAsset:'attr-'+a.id})]})),button('talent-refund','↶ 退还',164,83,82,22,'refund')]);menu.visibility='collapsed';doc.root.children.push(menu);
const card=box('talent-card',570,270,362,300,[node('Svg','talent-card-icon',16,16,40,40,{svgAsset:'legend-embers_echo'}),text('talent-card-title','传奇特性',68,14,278,45,18,'#e6c981'),text('talent-card-description','传奇效果',16,73,330,116,14),text('talent-card-requirement','',16,191,330,39,12,'#91a0ad'),button('talent-card-allocate','点亮 · 1 点',16,244,204,38,'allocate'),button('talent-card-refund','退还',232,244,114,38,'refund')]);card.visibility='collapsed';doc.root.children.push(card);
const tip=box('talent-tip',600,380,160,42,[text('talent-tip-text','无法到达',10,8,140,26,14,'#edd198')]);tip.visibility='collapsed';doc.root.children.push(tip);
// Native zoom uses these high-resolution caches of the same authored SVG/PNG assets.
const library=node('CanvasPanel','talent-icon-library',1240,120,96,96,{visibility:'collapsed'});
for(const [id,info]of Object.entries(LEGENDARY_ICONS))library.children.push(node('Svg','legend-sample-'+id,0,0,96,96,{svgAsset:'legend-'+id}));
for(const a of tree.attributes)library.children.push(node('Svg','attr-sample-'+a.id,0,0,96,96,{svgAsset:'attr-'+a.id}));
for(const id of ['dot','origin','legend-ring'])library.children.push(node('Svg','shape-sample-'+id,0,0,96,96,{svgAsset:id}));
doc.root.children.push(library);
doc.provenance={source:'RuleStudio constellation revision '+tree.revision,treeId:tree.id,layout:tree.layout.kind||'organic-network-v5',job,sourceAssets,nodeCount:1000,legendaryCount:100,businessRuntime:'AngelScript',contract:'UICreator native WBP; allocation and rule host sources in AS'};
const errors=validateDocument(doc);if(errors.length)throw Error(errors.slice(0,12).join('\n'));
for(const dir of ['UIAuthoring/projects','CustomTools/UICreatorAlpha/data/projects']){const repo=new ProjectRepository(path.join(root,dir));const previous=await repo.pointer(doc.id);await repo.write(doc,previous?.revision||0);}
await fs.mkdir(path.join(root,'UIAuthoring'),{recursive:true});await fs.writeFile(path.join(root,'UIAuthoring/TalentConstellation.uialpha.json'),JSON.stringify(doc,null,2));
await fs.writeFile(path.join(frozen,'authoring-project.json'),JSON.stringify(doc));
let count=0;walk(doc.root,()=>count++);console.log(JSON.stringify({job,project:doc.id,nodes:count,assets:Object.keys(doc.assets).length,sourceAssets:sourceAssets.length}));
