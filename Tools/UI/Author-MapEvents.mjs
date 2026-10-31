// UICreator authoring only. Gameplay and item transactions live in AS/native rules.
import fs from 'node:fs/promises';
import path from 'node:path';
import { createDocument, createNode, validateDocument } from '../../CustomTools/UICreatorAlpha/src/core/model.js';
import { ProjectRepository } from '../../CustomTools/UICreatorAlpha/server/project-repository.mjs';
import {installMapEventCraftUI} from './MapEventCraftUI.mjs';
const root=path.resolve(import.meta.dirname,'../..');
const d=createDocument('地图奇遇 · 宝库 / 百炼 / 万相');
d.id='opus-map-events';d.viewport={width:1600,height:1000,background:'#090d13'};d.root.id='events-authoring';
const svg=(w,h,fill,stroke)=>`<svg xmlns="http://www.w3.org/2000/svg" width="${w}" height="${h}" viewBox="0 0 ${w} ${h}"><rect x=".5" y=".5" width="${w-1}" height="${h-1}" rx="5" fill="${fill}" stroke="${stroke}"/></svg>`;
for(const [id,fill,stroke] of [['normal','#15202c','#394c60'],['hover','#223346','#b49a62'],['pressed','#28413e','#7bc4ae'],['disabled','#101720','#273340']])d.assets[id]={id,name:id,viewBox:[0,0,300,54],source:svg(300,54,fill,stroke),scaleMode:'stretch',slice:{top:5,right:5,bottom:5,left:5}};
function n(type,id,x,y,w,h,more={}){return createNode(type,{id:id.replaceAll(':','-'),name:id,slot:{offsets:{left:x,top:y,right:w,bottom:h}},layout:{width:w,height:h},...more});}
function t(id,x,y,w,h,text='',size=14,color='#c9d6e5'){return n('TextBlock',id,x,y,w,h,{binding:id,text,style:{fontSize:size,color,wrap:true}});}
function b(id,x,y,w,h,text,op=''){const label=t(id+':text',12,0,w-24,h,text,14,'#e6d4a5');label.style.textAlign='center';return n('Button',id,x,y,w,h,{binding:id,brushes:{normal:'normal',hovered:'hover',pressed:'pressed',disabled:'disabled'},action:{type:'map-event',op},children:[n('CanvasPanel',id+'-button-content',0,0,w,h,{children:[label]})]});}
const panel=n('Border','event-panel',24,24,1000,680,{style:{fill:'#0c131d',stroke:'#9c8453',strokeWidth:1,radius:8}});
const c=n('CanvasPanel','event-content',0,0,1000,680);panel.children.push(c);
c.children.push(t('event:title',24,18,860,32,'地图奇遇',24,'#ebd39a'),b('event:close',934,16,42,32,'×','close'),t('event:description',24,58,952,48,'',13,'#8fa4bb'));
for(let i=0;i<3;i++){
 const x=24+i*320;
 c.children.push(n('Border','event:column-'+i,x,116,312,418,{style:{fill:'#101b27',stroke:'#304155',strokeWidth:1,radius:5},children:[n('CanvasPanel','event:col-content-'+i,0,0,312,418,{children:[n('Image','event-icon-'+i,12,10,42,42,{binding:'event:icon-'+i}),t('event:heading-'+i,62,10,238,38,'',15,'#e3c88f'),t('event:meta-'+i,12,54,288,28,'',12,'#8ab8cc'),n('CanvasPanel','event:rows-'+i,8,88,296,280,{binding:'event:rows-'+i}),b('event:select-'+i,8,376,296,34,'选择这件','reward-'+i)]})]}));
}
c.children.push(t('event:status',24,548,952,44,'',14,'#8fdbbc'),t('event:warning',24,596,480,58,'',12,'#bdad8a'),b('event:primary',526,610,260,44,'确认','primary'),b('event:secondary',798,610,178,44,'收取装备','collect'));
d.root.children.push(panel);
const prompt=b('event-label',1060,24,250,54,'奇遇 · 靠近后按 V','open');prompt.children[0].children[0].binding='event:label';d.root.children.push(prompt);
const item=b('event-item-row',1060,110,288,66,'','bag');item.children=[];
item.children.push(n('Image','event-item-icon',8,8,48,48,{binding:'row:icon'}),t('row:name',66,7,212,26,'',14,'#dfcfaa'),t('row:meta',66,36,212,22,'',11,'#8fa9c1'));d.root.children.push(item);
const line=b('event-line-row',1060,210,288,116,'','affix');line.children=[];line.children.push(t('row:text',10,8,268,32,'',13,'#cbd7e8'),t('row:description',10,42,268,66,'',12,'#92a9c0'));d.root.children.push(line);
for(const row of [item,line]){const children=row.children;row.children=[n('CanvasPanel',row.id+'-content',0,0,row.layout.width,row.layout.height,{children})];}
installMapEventCraftUI(d,JSON.parse(await fs.readFile(path.join(root,'CustomTools/RuleStudio/public/ui/map-events.uialpha.json'),'utf8')));
const errors=validateDocument(d);if(errors.length)throw Error(errors.join('\n'));
await fs.writeFile(path.join(root,'UIAuthoring/MapEvents.uialpha.json'),JSON.stringify(d,null,2));
const repo=new ProjectRepository(path.join(root,'CustomTools/UICreatorAlpha/data/projects'));const old=await repo.pointer(d.id);await repo.write(d,old?.revision||0);
// The page freezes the actual SvgRenderer output, then the existing exporter
// rasterizes individual skins and four button states in the browser.
await fs.writeFile(path.join(root,'CustomTools/UICreatorAlpha/examples/map-events-export.html'),`<!doctype html><html lang="zh"><meta charset="utf-8"><title>UICreator 地图奇遇</title><style>body{margin:20px;background:#090d13;color:#d7c69f;font:14px sans-serif}button{padding:12px;background:#203344;color:#eed49d;border:1px solid #8d7951}svg{display:block;width:100%;max-width:1600px}a{color:#80d5b7}</style><button id="freeze">冻结奇遇 UI 并导出 UE</button><span id="status"></span><svg id="scene"></svg><script type="module" src="map-events-export.js"></script></html>`);
await fs.writeFile(path.join(root,'CustomTools/UICreatorAlpha/examples/map-events-export.js'),`import {UIRuntime} from '../src/runtime/runtime.js';
const record=await fetch('/api/projects/opus-map-events').then(r=>r.json());
const project=record.document||record;
const runtime=new UIRuntime(document.querySelector('#scene'),project,{width:project.viewport.width,height:project.viewport.height,motion:false,sound:false});
document.querySelector('#freeze').onclick=async()=>{await document.fonts.ready;const job='wbp-events-'+crypto.randomUUID();const svg=new XMLSerializer().serializeToString(document.querySelector('#scene'));const response=await fetch('/api/native-wbp/'+job+'/source',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({project,svg,context:{width:project.viewport.width,height:project.viewport.height,expandedRoot:runtime.expanded.document.root}})});const result=await response.json();if(!response.ok)throw Error(JSON.stringify(result));location.href=result.url;};`);
console.log(JSON.stringify({project:d.id,templates:d.root.children.length,errors}));
