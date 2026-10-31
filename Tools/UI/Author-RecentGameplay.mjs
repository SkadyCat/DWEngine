// UICreator source documents; no runtime gameplay is implemented in JavaScript.
import fs from 'node:fs/promises';
import path from 'node:path';
import {createDocument,createNode,validateDocument} from '../../CustomTools/UICreatorAlpha/src/core/model.js';
import {ProjectRepository} from '../../CustomTools/UICreatorAlpha/server/project-repository.mjs';
import {createEdgeMotionAsset} from '../../CustomTools/UICreatorAlpha/src/core/motion.js';
const root=path.resolve(import.meta.dirname,'../..');
const d=createDocument('玩法接入 · 光环 / 任务 / 藏身处');
d.id='opus-recent-gameplay';d.viewport={width:2400,height:1800,background:'#090d13'};d.root.id='recent-authoring';
const activeMotion=createEdgeMotionAsset('motion-aura-active','光环 · 激活流光');
activeMotion.edge={width:2,radius:7,color:'#8fe8d9'};
activeMotion.clips.orbit.duration=2200;
activeMotion.bindings={normal:'orbit',hovered:'orbit',pressed:'orbit',disabled:'off'};
d.motions[activeMotion.id]=activeMotion;
for(const [id,fill,stroke] of [['normal','#15202c','#394c60'],['hover','#223346','#b49a62'],['pressed','#28413e','#7bc4ae'],['disabled','#101720','#273340']])
 d.assets[id]={id,name:id,viewBox:[0,0,300,54],source:`<svg xmlns="http://www.w3.org/2000/svg" width="300" height="54"><rect x=".5" y=".5" width="299" height="53" rx="5" fill="${fill}" stroke="${stroke}"/></svg>`,scaleMode:'stretch',slice:{top:5,right:5,bottom:5,left:5}};
d.assets.clear={id:'clear',name:'透明交互区域',viewBox:[0,0,290,110],source:'<svg xmlns="http://www.w3.org/2000/svg" width="290" height="110"><rect width="290" height="110" fill="transparent"/></svg>',scaleMode:'stretch'};
function n(type,id,x,y,w,h,more={}){return createNode(type,{id:id.replaceAll(':','-'),name:id,slot:{offsets:{left:x,top:y,right:w,bottom:h}},layout:{width:w,height:h},...more});}
const text=(id,x,y,w,h,value='',size=14,color='#c9d6e5')=>n('TextBlock',id,x,y,w,h,{binding:id,text:value,style:{fontSize:size,color,wrap:true}});
function button(id,x,y,w,h,value,op=''){
 return n('Button',id,x,y,w,h,{binding:id,brushes:{normal:'normal',hovered:'hover',pressed:'pressed',disabled:'disabled'},action:{type:'recent-gameplay',op},children:[n('CanvasPanel',id+'-body',0,0,w,h,{children:[text(id+':text',12,7,w-24,h-10,value,14,'#e6d4a5')]})]});
}
function panel(id,x,y,w,h,title){
 const body=n('CanvasPanel',id+'-content',0,0,w,h,{children:[text(id+':title',24,18,w-100,34,title,23,'#ebd39a'),button(id+':close',w-64,18,40,32,'×','close')]});
 const p=n('Border',id,x,y,w,h,{style:{fill:'#0c131d',stroke:'#9c8453',strokeWidth:1,radius:8}});p.children.push(body);d.root.children.push(p);return body;
}
const aura=panel('aura-panel',20,20,940,630,'光环装配');
aura.children.push(text('aura:help',24,62,892,42,'装备非武器物品以获得光环。选择 Q / E / F / G 槽，再选择光环；按相应按键开启或关闭。',13,'#94a8bd'));
for(let i=0;i<4;i++){
 const x=24+i*226;
 aura.children.push(button('aura:slot-'+i,x,118,214,58,['Q','E','F','G'][i]+' · 空槽','slot-'+i),button('aura:toggle-'+i,x,184,104,34,'开启','toggle-'+i),button('aura:clear-'+i,x+110,184,104,34,'卸下','clear-'+i));
}
aura.children.push(text('aura:reservation',24,232,892,30,'魔力封印 0% · 可用上限 100',15,'#aab6ed'),n('ScrollBox','aura:rows',24,274,892,278,{binding:'aura:rows',layout:{width:892,height:278,gap:8,orientation:'vertical'}}),text('aura:status',24,568,892,42,'每个光环默认封印50%总魔力；关闭不会立即回复魔力。',13,'#8fdbbc'));
const quest=panel('quest-panel',990,20,1020,630,'任务 · 初入地牢');
quest.children.push(text('quest:description',24,64,972,44,'使用霜棘法杖进入地牢。完成第一层三波战斗并击败首领后，从三件不同的 T1 武器中选择一件。',14,'#94a8bd'),text('quest:progress',24,114,972,32,'第一层 · 等待挑战',16,'#8fdbbc'));
for(let i=0;i<3;i++){
 const x=24+i*328;
 const body=n('CanvasPanel','quest:card-body-'+i,0,0,316,370,{children:[n('Image','quest:icon-'+i,14,14,72,72,{binding:'quest:icon-'+i}),text('quest:name-'+i,100,14,200,34,'待解锁',18,'#e6d4a5'),text('quest:meta-'+i,100,50,200,34,'T1 武器',13,'#94a8bd'),text('quest:traits-'+i,14,104,288,188,'完成挑战后生成真实随机装备。',14),button('quest:claim-'+i,12,314,292,40,'选择这件武器','claim-'+i)]});
 quest.children.push(n('Border','quest:card-'+i,x,160,316,370,{style:{fill:'#101b27',stroke:'#304155',strokeWidth:1,radius:5},children:[body]}));
}
quest.children.push(text('quest:status',24,550,972,54,'奖励候选生成后保存，关闭或重新打开界面不会刷新。',14,'#8fdbbc'));
const hideout=panel('hideout-panel',20,690,1060,700,'藏身处');
hideout.children.push(button('hideout:stash',24,66,180,38,'仓库','stash'),button('hideout:shop',214,66,180,38,'商店 / 摊位','shop'),text('hideout:help',414,70,620,34,'点击操作，支持背包与仓库 / 摊位之间拖放。',13,'#94a8bd'),text('hideout:bag-title',24,120,482,30,'背包',17,'#e6d4a5'),text('hideout:other-title',548,120,488,30,'仓库',17,'#e6d4a5'),n('ScrollBox','hideout:bag',24,160,488,440,{binding:'hideout:bag',layout:{width:488,height:440,orientation:'vertical',gap:8}}),n('ScrollBox','hideout:other',548,160,488,440,{binding:'hideout:other',layout:{width:488,height:440,orientation:'vertical',gap:8}}),text('hideout:status',24,616,1012,58,'物品保留 UID、数量、品质及全部词条。',14,'#8fdbbc'));
const row=button('aura-choice-row',1120,700,620,72,'','choose-aura');row.children[0].children=[text('row:name',14,8,594,25,'光环名称',16,'#e6d4a5'),text('row:description',14,38,594,25,'效果与魔力封印比例',12,'#91a8c0')];d.root.children.push(row);
const item=button('recent-item-row',1120,800,480,72,'','move-item');item.children[0].children=[n('Image','recent-item-icon',8,8,54,54,{binding:'row:icon'}),text('row:item-name',74,8,390,28,'物品名称',15,'#e6d4a5'),text('row:item-meta',74,40,390,24,'数量 · 品质 · T阶',12,'#91a8c0')];d.root.children.push(item);
const tracker=panel('quest-tracker',1120,910,290,110,'初入地牢');tracker.children[0].style.fontSize=15;tracker.children[1].visibility='collapsed';tracker.children.push(text('quest:tracker-text',14,56,262,40,'第一层 · 等待挑战',13,'#91a8c0'));
const trackerOpen=button('quest:open',0,0,290,110,'','quests');trackerOpen.brushes={normal:'clear',hovered:'clear',pressed:'clear',disabled:'clear'};tracker.children.push(trackerOpen);
const launch=n('CanvasPanel','recent-launcher',1120,1080,460,42,{children:[button('recent:auras',0,0,144,40,'光环 [O]','auras'),button('recent:quests',154,0,144,40,'任务 [J]','quests'),button('recent:hideout',308,0,144,40,'藏身处 [H]','hideout')]});d.root.children.push(launch);
const auraSlot=button('aura-hud-slot',1820,700,64,76,'','toggle-slot');auraSlot.children[0].children=[n('Image','aura-slot-icon',5,5,54,54,{binding:'aura-slot:icon'}),text('aura-slot:key',4,0,20,18,'Q',11,'#b4c4da'),text('aura-slot:state',4,60,56,14,'关闭',10,'#82d9c5')];
// A separate authored layer keeps the icon, key label and native hit region stable.
auraSlot.children[0].children.push(n('Image','aura-slot-active',0,0,64,64,{binding:'aura-slot:active',locked:true,motion:{preset:'auto',assetId:activeMotion.id,enabled:false}}));
d.root.children.push(auraSlot);
const grid=n('CanvasPanel','stored-grid-row',1820,820,480,60);
for(let i=0;i<8;i++){
 const cell=button('stored:cell-'+i,i*60,0,56,56,'','storage-cell');cell.children[0].children=[n('Image','stored-cell-icon-'+i,4,4,48,48,{binding:'cell:icon-'+i}),text('cell:count-'+i,26,36,26,17,'',12,'#e7dfbd')];grid.children.push(cell);
}
d.root.children.push(grid);
for(const side of ['bag','other']){
 const x=side==='bag'?24:548;
 hideout.children.push(button('hideout:'+side+'-prev',x,572,90,30,'上一页',side+'-prev'),text('hideout:'+side+'-page',x+100,576,276,24,'1 / 1',12,'#9bacc1'),button('hideout:'+side+'-next',x+392,572,90,30,'下一页',side+'-next'));
}
const errors=validateDocument(d);if(errors.length)throw Error(errors.join('\n'));
await fs.mkdir(path.join(root,'UIAuthoring'),{recursive:true});await fs.writeFile(path.join(root,'UIAuthoring/RecentGameplay.uialpha.json'),JSON.stringify(d,null,2));
const repo=new ProjectRepository(path.join(root,'CustomTools/UICreatorAlpha/data/projects'));const old=await repo.pointer(d.id);await repo.write(d,old?.revision||0);
await fs.writeFile(path.join(root,'CustomTools/UICreatorAlpha/examples/recent-gameplay-export.html'),`<!doctype html><html lang="zh"><meta charset="utf-8"><title>UICreator · 玩法接入</title><style>body{margin:20px;background:#090d13;color:#d7c69f;font:14px sans-serif}button{padding:12px;background:#203344;color:#eed49d;border:1px solid #8d7951}svg{display:block;width:100%;height:auto;max-width:2400px}</style><button id="freeze">冻结玩法 UI 并导出 UE</button><svg id="scene"></svg><script type="module" src="recent-gameplay-export.js"></script></html>`);
await fs.writeFile(path.join(root,'CustomTools/UICreatorAlpha/examples/recent-gameplay-export.js'),`import {UIRuntime} from '../src/runtime/runtime.js';
const record=await fetch('/api/projects/opus-recent-gameplay').then(r=>r.json()),project=record.document||record;
const runtime=new UIRuntime(document.querySelector('#scene'),project,{width:2400,height:1800,motion:false,sound:false});
document.querySelector('#freeze').onclick=async()=>{await document.fonts.ready;const job='wbp-recent-'+crypto.randomUUID(),svg=new XMLSerializer().serializeToString(document.querySelector('#scene'));const response=await fetch('/api/native-wbp/'+job+'/source',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({project,svg,context:{width:2400,height:1800,expandedRoot:runtime.expanded.document.root}})});const result=await response.json();if(!response.ok)throw Error(JSON.stringify(result));location.href=result.url;};`);
console.log(JSON.stringify({project:d.id,templates:d.root.children.length,errors}));
