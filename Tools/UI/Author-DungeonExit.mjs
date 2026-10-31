// UICreator owns the portal artwork; the existing Studio exit flow stays in its controller.
import fs from 'node:fs/promises';
import path from 'node:path';
import {createDocument,createNode,validateDocument} from '../../CustomTools/UICreatorAlpha/src/core/model.js';
import {createMotionAsset} from '../../CustomTools/UICreatorAlpha/src/core/motion.js';
import {ProjectRepository} from '../../CustomTools/UICreatorAlpha/server/project-repository.mjs';
const root=path.resolve(import.meta.dirname,'../..');
const proof=path.join(root,'Saved/Verification/DungeonExit');await fs.mkdir(proof,{recursive:true});
const file=path.join(root,'CustomTools/RuleStudio/public/ui/dungeon-exit.uialpha.json');
if(!(await fs.stat(path.join(proof,'before.uialpha.json')).catch(()=>null)))await fs.copyFile(file,path.join(proof,'before.uialpha.json'));
const d=createDocument('Studio3D · 地牢出口传送阵');
d.id='opus-dungeon-exit';d.viewport={width:220,height:64,background:'transparent'};
d.motions={};d.sounds=Object.fromEntries(Object.entries(d.sounds).filter(([id])=>['ui-hover','ui-click'].includes(id)));
const n=(type,id,x,y,w,h,more={})=>createNode(type,{id,name:id,slot:{offsets:{left:x,top:y,right:w,bottom:h}},layout:{width:w,height:h},...more});
const asset=(id,name,w,h,body)=>{d.assets[id]={id,name,viewBox:[0,0,w,h],scaleMode:'stretch',source:`<svg xmlns="http://www.w3.org/2000/svg" width="${w}" height="${h}" viewBox="0 0 ${w} ${h}" fill="none" stroke="none">${body}</svg>`};return id;};
const palette={normal:['#14212a','#0b151d','#3e615e','#c5ab79','#64b5a8'],hovered:['#1d343b','#102229','#8cd8c3','#f3d698','#a4f0d3'],pressed:['#0d2028','#09151d','#6ebca9','#cfb780','#79d8bc'],disabled:['#131b22','#0d141b','#2b3a40','#75817f','#4e706d'],ready:['#172c31','#0c1d23','#69bca7','#e9cb90','#8be5c5'],travel:['#1a292e','#0b1a23','#617d76','#cfbc8a','#8cbbb0']};
for(const [state,[top,bottom,border,gold,mint]]of Object.entries(palette)){
 asset('portal-plate-'+state,'世界地图入口 · '+state,220,64,`
 <defs><linearGradient id="panel" x2="0" y2="1"><stop stop-color="${top}"/><stop offset="1" stop-color="${bottom}"/></linearGradient><linearGradient id="cap"><stop stop-color="${gold}" stop-opacity="0"/><stop offset=".48" stop-color="${gold}" stop-opacity=".55"/><stop offset="1" stop-color="${gold}" stop-opacity="0"/></linearGradient></defs>
 <path d="M13 7h194q9 0 9 9v27q0 9-9 9h-88l-9 8-9-8H13q-9 0-9-9V16q0-9 9-9" fill="#000000" opacity=".24"/>
 <path d="M13 4.5h194q8.5 0 8.5 8.5v27q0 8.5-8.5 8.5h-89l-8 7-8-7H13q-8.5 0-8.5-8.5V13q0-8.5 8.5-8.5Z" fill="url(#panel)" stroke="${border}" stroke-width="1"/>
 <path d="M14 6h192" stroke="url(#cap)" stroke-width="1"/><path d="M5 17v20" stroke="${mint}" stroke-width="2" opacity=".65"/>
 <path d="m198 23 4 4-4 4" stroke="${gold}" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/>`);
}
for(const [state,color]of [['normal','#83c8ba'],['ready','#b5f3d9'],['travel','#b7cbb2']])asset('portal-icon-'+state,'世界地图 · '+state,32,32,`
 <path d="m4 8 8-3 8 3 8-3v20l-8 3-8-3-8 3Z" fill="${color}" fill-opacity=".08" stroke="${color}" stroke-width="1.4" stroke-linejoin="round"/>
 <path d="M12 5v20M20 8v20" stroke="${color}" stroke-width="1.1" stroke-opacity=".6"/>
 <path d="m7 18 5-5 5 4 7-7" stroke="${color}" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round"/>
 <circle cx="24" cy="10" r="2" fill="${color}"/>`);
const motion=createMotionAsset('motion-portal-feedback','传送阵 · 悬停抬起与按下');
for(const clip of ['hover','release'])Object.assign(motion.clips[clip].keyframes[1],{y:-1,scaleX:1,scaleY:1});
Object.assign(motion.clips.press.keyframes[1],{y:1,scaleX:1,scaleY:1});
d.motions[motion.id]=motion;
const text=(id,b,x,y,w,h,value,size,color,weight=400)=>n('TextBlock',id,x,y,w,h,{binding:b,text:value,style:{fontSize:size,fontWeight:weight,color,wrap:false,textOverflow:'ellipsis'}});
d.root=n('CanvasPanel','portal-root',0,0,220,64,{name:'地牢出口 · 世界空间浮牌'});
d.root.children=[n('Button','portal-open',0,0,220,64,{name:'前往世界地图',binding:'portal:activate',action:{type:'ui',op:'world-map'},motion:{assetId:motion.id},brushes:{normal:'portal-plate-normal',hovered:'portal-plate-hovered',pressed:'portal-plate-pressed',disabled:'portal-plate-disabled'},children:[n('CanvasPanel','portal-content',0,0,220,64,{children:[
 n('Image','portal-icon',17,11,30,30,{name:'世界地图',binding:'portal:icon',svgAsset:'portal-icon-normal'}),
 text('portal-title','portal:title',58,7,134,23,'前往世界地图',14,'#efd7a4',600),
 text('portal-note','portal:note',58,29,134,17,'自动前往 · 到达后打开',10,'#96b4b8'),
 ]})]})];
d.presentation={portalStates:Object.fromEntries(['normal','ready','travel'].map(mode=>[mode,[
 ['portal-title',{text:mode==='travel'?'正在前往…':'前往世界地图'}],
 ['portal-note',{text:mode==='travel'?'到达后自动打开地图':mode==='ready'?'选择下一处冒险':'自动前往 · 到达后打开'}],
 ['portal-icon',{svgAsset:'portal-icon-'+mode}],
 ['portal-open',{brushes:{normal:'portal-plate-'+mode}}],
 ]]))};
const errors=validateDocument(d);if(errors.length)throw Error(errors.join('\n'));
const repo=new ProjectRepository(path.join(root,'CustomTools/UICreatorAlpha/data/projects'));const previous=await repo.pointer(d.id);
await repo.write(d,previous?.revision||0);
await fs.writeFile(file,JSON.stringify(d,null,2)+'\n');
await fs.writeFile(path.join(root,'UIAuthoring/DungeonExit.uialpha.json'),JSON.stringify(d,null,2)+'\n');
console.log(JSON.stringify({project:d.id,viewport:d.viewport,assets:Object.keys(d.assets).length,motions:Object.keys(d.motions).length,previousRevision:previous?.revision,errors}));
