// Offline authoring: combine the saved Studio controls without rebuilding their artwork.
import fs from 'node:fs/promises';
import path from 'node:path';
import {isDeepStrictEqual} from 'node:util';
import {createDocument,createNode,walk,validateDocument} from '../../CustomTools/UICreatorAlpha/src/core/model.js';
import {ProjectRepository} from '../../CustomTools/UICreatorAlpha/server/project-repository.mjs';
import {merchantCardBrushes} from '../../CustomTools/UICreatorAlpha/src/assets/rulestudio-hud/merchant-selection.js';
import {contextMenuColor} from '../../CustomTools/UICreatorAlpha/src/assets/rulestudio-hud/context-menu-style.js';
const root=path.resolve(import.meta.dirname,'../..');
const repo=new ProjectRepository(path.join(root,'CustomTools/UICreatorAlpha/data/projects'));
const doc=createDocument('Studio 同步 · 商人 / 特质装备 / 地牢地图 / 传送阵');
doc.id='opus-studio-sync-20261026';doc.viewport={width:2400,height:2250,background:'transparent'};doc.root.id='studio-sync-authoring';doc.root.children=[];doc.motions={};
const evidence=[];
async function source(id){const record=await repo.read(id);if(!record)throw Error('Missing saved source '+id);evidence.push({id,revision:record.revision});return structuredClone(record.document);}
function merge(d){Object.assign(doc.motions,d.motions||{});for(const [id,a] of Object.entries(d.assets))doc.assets[id]={...a};for(const [id,p]of Object.entries(d.prefabs))doc.prefabs[id]=p;}
function template(d,node,id,x,y,w,h){const n=structuredClone(node);walk(n,v=>{if(!v.binding)v.binding=v.id;v.id=id+'-'+v.id;});n.id=id;Object.assign(n.slot.offsets,{left:x,top:y,right:w,bottom:h});Object.assign(n.layout,{width:w,height:h});doc.root.children.push(n);return n;}
const merchant=await source('opus-outpost-merchant');merge(merchant);
// Freeze the approved purchase menu in each quality, including its independent text.
for(const [i,quality]of ['common','normal','magic','rare','legendary','unique'].entries()){
 const menu=structuredClone(merchant.prefabs['merchant-purchase-menu'].root);
 walk(menu,n=>{if(n.id==='merchant-purchase-background')n.svgAsset='merchant-purchase-frame-'+(quality==='normal'?'common':quality);if(n.id==='merchant-purchase-item-name')n.style.color=contextMenuColor(quality==='normal'?'common':quality);});
 template(merchant,menu,'merchant-purchase-'+quality,1660+i%2*250,520+Math.floor(i/2)*118,240,108);
}
// Drag presentation is the item's icon, with no card, name, count or border.
doc.root.children.push(createNode('CanvasPanel',{id:'inventory-drag-icon',layout:{width:52,height:52},slot:{offsets:{left:1660,top:920,right:52,bottom:52}},children:[createNode('Image',{id:'inventory-drag-picture',binding:'inventory-drag-picture',layout:{width:52,height:52},slot:{offsets:{right:52,bottom:52}},style:{objectFit:'contain'}})]}));
// Studio's BattleWindow shell also belongs to the saved UE source.
doc.assets['studio-window-body']={id:'studio-window-body',name:'拓荒商人 · 窗体',viewBox:[0,0,956,620],scaleMode:'stretch',source:'<svg xmlns="http://www.w3.org/2000/svg" width="956" height="620"><rect x=".5" y=".5" width="955" height="619" rx="10" fill="#101722" fill-opacity=".98" stroke="#86704a"/></svg>'};
doc.assets['studio-window-head']={id:'studio-window-head',name:'拓荒商人 · 标题栏',viewBox:[0,0,954,46],scaleMode:'stretch',source:'<svg xmlns="http://www.w3.org/2000/svg" width="954" height="46"><defs><linearGradient id="g"><stop stop-color="#353025"/><stop offset="1" stop-color="#151f2d"/></linearGradient></defs><path d="M9 0H945Q954 0 954 9V46H0V9Q0 0 9 0Z" fill="url(#g)"/><path d="M0 45.5H954" stroke="#4a4438"/></svg>'};
const shellNode=(type,id,x,y,w,h,props={})=>createNode(type,{id,slot:{offsets:{left:x,top:y,right:w,bottom:h}},layout:{width:w,height:h},...props});
const inner=structuredClone(merchant.root);inner.id='merchant-stock';inner.slot.offsets={...inner.slot.offsets,left:18,top:52,right:920,bottom:552};
const window=shellNode('CanvasPanel','merchant-window',0,0,956,620,{children:[shellNode('Svg','merchant-window-body',0,0,956,620,{svgAsset:'studio-window-body',locked:true}),shellNode('Svg','merchant-window-head',1,1,954,46,{svgAsset:'studio-window-head',locked:true}),shellNode('TextBlock','merchant-window-title',15,10,830,26,{text:'拓荒商人',locked:true,style:{fontSize:15,fontWeight:600,color:'#edcf97',textAlign:'left',wrap:false}}),shellNode('Button','merchant-window-close',914,10,26,26,{text:'×',style:{fill:'#0c141f',stroke:'#776244',strokeWidth:1,radius:4,color:'#d8c39e',fontSize:20,textAlign:'center'},action:{type:'ui',op:'close'}}),inner]});
template(merchant,window,'merchant-shop',10,10,956,620);
template(merchant,merchant.prefabs['merchant-launcher'].root,'merchant-launcher',1800,10,226,60);
const trait=await source('studio-trait-equipment');merge(trait);template(trait,trait.root,'trait-equipment',980,10,640,350);
const map=await source('opus-dungeon-map');merge(map);
template(map,map.root,'dungeon-map',10,690,1000,680);
template(map,map.prefabs['dungeon-minimap'].root,'dungeon-minimap',1080,420,176,222);
const glyphs=[[[[0,-1],[.72,.72],[0,.42],[-.72,.72],[0,-1]]],[[[0,-1],[1,0],[0,1],[-1,0],[0,-1]]],[[[-1,-.65],[-.5,-.32],[0,-1],[.5,-.32],[1,-.65],[.83,.7],[-.83,.7],[-1,-.65]],[[-.5,1],[.5,1]]],[[[0,-1],[.75,0],[0,1],[-.75,0],[0,-1]]]];
walk(doc.root,n=>{if(/dmap-(mini|panel)-viewport$/.test(n.id)){n.source={...n.source,plotSurface:{bounds:{x:0,y:0,width:1,height:1},fogColor:'#0a141d',fogOpacity:0,fontSize:11,labelFill:'#0a141d',glyphs,circle:n.id.includes('mini')}};n.children=[];}});
const portal=await source('opus-dungeon-exit');merge(portal);
for(const [i,mode]of ['normal','ready','travel'].entries()){
 const d=structuredClone(portal);for(const [id,patch]of d.presentation.portalStates[mode])walk(d.root,n=>{if(n.id===id)Object.assign(n,patch);});
 const n=template(d,d.root,'portal-'+mode,1800,100+i*90,220,64);walk(n,v=>{v.binding=v.binding||v.id;v.id='portal-'+mode+'-'+v.id;});n.id='portal-'+mode;
}
// The persistent selected state is the exact saved jade / warm gold card artwork.
for(const [i,tone]of ['jade','gold'].entries())for(const [j,selected]of [false,true].entries()){
 const node=structuredClone(merchant.root.children.find(n=>n.id==='stock-'+(i?3:0)));node.children=[];node.brushes=merchantCardBrushes(i===1,selected);node.binding='merchant-brush';
 template(merchant,node,`merchant-${tone}-${selected?'selected':'normal'}`,10+i*320,1400+j*130,280,106);
}
// An authored rail entry exposes the Studio catalogue without changing the main HUD contract.
const main=await source('hud-darksteel-20261024');merge({...main,prefabs:{'rs-prefab-function':main.prefabs['rs-prefab-function']}});
const rail=createNode('PrefabInstance',{id:'studio-trait-launcher',prefabId:'rs-prefab-function',slot:{offsets:{left:1320,top:420,right:36,bottom:36}},layout:{width:36,height:36},overrides:{'rs-function-button':{binding:'studio-trait-open',action:{type:'studio-equipment',op:'open'},name:'特质装备'},'rs-function-button/rs-function-content/rs-function-key':{text:'特质',style:{fontSize:9}},'rs-function-button/rs-function-content/rs-function-icon':{svgAsset:'rs-svg-icon-equipment'}}});doc.root.children.push(rail);
doc.folders=structuredClone(main.folders);
for(const a of Object.values(doc.assets))a.folderId='';for(const p of Object.values(doc.prefabs))p.folderId='';
// Exact published Studio tracker, including full/compact and authored live colors.
const quest=JSON.parse(await fs.readFile(path.join(root,'CustomTools/RuleStudio/public/ui/quest-tracker.uialpha.json'),'utf8'));merge(quest);
const questModes=['expanded-0','expanded-1','expanded-2','expanded-3','expanded-reward','compact-active','compact-reward','empty'];
for(const [i,mode]of questModes.entries()){
 const compact=mode.startsWith('compact')||mode==='empty',reward=mode.endsWith('reward'),steps=reward?3:Number(mode.slice(-1))||0;
 const n=structuredClone(compact?quest.prefabs['quest-collapsed'].root:quest.root);
 walk(n,v=>{if(v.binding==='quest-state')v.style.color=reward?'#ecd086':'#93a3b0';if(v.binding==='quest-hint')v.style.color=reward?'#e4c783':'#92a1ae';if(v.binding==='quest-wave-mark'||v.binding==='quest-boss-mark'||v.binding==='quest-boss-state')v.style.color=reward||(v.binding==='quest-wave-mark'&&steps===3)?'#98cfb8':v.binding==='quest-wave-mark'?'#a59a7f':'#8e9eac';for(let step=0;step<3;step++)if(v.binding==='quest-step-'+step)v.style.fill=step<steps?'#cdb67ed9':'#b7a16a33';});
 template(quest,n,'studio-quest-'+mode,10+i%4*300,1650+Math.floor(i/4)*225,288,compact?46:214);
}
evidence.push({id:quest.id,source:'RuleStudio/public/ui/quest-tracker.uialpha.json'});
const errors=validateDocument(doc);if(errors.length)throw Error(errors.join('\n'));
const previous=await repo.read(doc.id);if(previous){const backup=path.join(root,'Saved/UIAuthoringBackups',doc.id+'-'+previous.revision+'.json');await fs.mkdir(path.dirname(backup),{recursive:true});await fs.writeFile(backup,JSON.stringify(previous));}
const saved=await repo.write(doc,previous?.revision||0),readback=await repo.read(doc.id);if(!isDeepStrictEqual(doc,readback.document))throw Error('Saved source differs');
await fs.writeFile(path.join(root,'UIAuthoring/StudioSync.uialpha.json'),JSON.stringify(doc));
await fs.mkdir(path.join(root,'Saved/Verification/StudioSync'),{recursive:true});await fs.writeFile(path.join(root,'Saved/Verification/StudioSync/sources.json'),JSON.stringify({project:doc.id,revision:saved.revision,sources:evidence},null,2));
console.log(JSON.stringify({project:doc.id,revision:saved.revision,sources:evidence,templates:doc.root.children.map(n=>n.id)}));
