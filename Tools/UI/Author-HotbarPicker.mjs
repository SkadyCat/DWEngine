import fs from 'node:fs/promises';
import path from 'node:path';
import {createDocument,createNode,validateDocument} from '../../CustomTools/UICreatorAlpha/src/core/model.js';
import {ProjectRepository} from '../../CustomTools/UICreatorAlpha/server/project-repository.mjs';
const root=path.resolve(import.meta.dirname,'../..'),doc=createDocument('Studio HUD · 技能与光环图标选择栏');
doc.id='opus-hotbar-picker-20261028';doc.viewport={width:1400,height:650,background:'transparent'};doc.root.id='hotbar-authoring';doc.root.children=[];
const n=(type,id,x,y,w,h,more={})=>createNode(type,{id, binding:id,slot:{offsets:{left:x,top:y,right:w,bottom:h}},layout:{width:w,height:h},...more});
const text=(id,x,y,w,h,value='',size=11,color='#c9d6e5')=>n('TextBlock',id,x,y,w,h,{text:value,style:{fontSize:size,color,wrap:true}});
for(const [state,fill,stroke]of [['normal','#0a1018','#374b5a'],['hovered','#14232e','#a6d8e7'],['pressed','#142b33','#79c9db'],['disabled','#0a121b','#263847']]){
 const id='picker-cell-'+state;doc.assets[id]={id,name:'图标格 · '+state,viewBox:[0,0,48,48],scaleMode:'stretch',source:`<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 48 48"><rect x=".5" y=".5" width="47" height="47" rx="6" fill="${fill}" stroke="${stroke}"/></svg>`};
}
const brushes=Object.fromEntries(['normal','hovered','pressed','disabled'].map(s=>[s,'picker-cell-'+s]));
const toolBrushes={};for(const state of ['normal','hovered','pressed','disabled']){const id='picker-tool-'+state;doc.assets[id]={id,name:'选择栏操作 · '+state,viewBox:[0,0,26,24],scaleMode:'stretch',source:`<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 26 24"><rect x=".5" y=".5" width="25" height="23" rx="4" fill="${state==='hovered'?'#273744':state==='pressed'?'#142b33':'none'}" stroke="${state==='hovered'?'#53758b':'none'}"/></svg>`};toolBrushes[state]=id;}
function button(id,x,y,w,h,label,op,value=''){
 const labelNode=text(id+'-text',0,0,w,h,label,13);labelNode.style.textAlign='center';return n('Button',id,x,y,w,h,{action:{type:'hotbar',op,value},brushes:/^r\d+-(prev|next|clear|close)$/.test(id)?toolBrushes:brushes,children:[labelNode]});
}
for(let rows=1;rows<=4;rows++){
 const p=n('Border','hotbar-picker-'+rows,(rows-1)*330,8,294,42+rows*56,{style:{fill:'#0e1720fa',stroke:'#405466',strokeWidth:1,radius:8}});
 p.children.push(button('r'+rows+'-prev',8,5,24,24,'‹','page','-1'),button('r'+rows+'-next',36,5,24,24,'›','page','1'),text('r'+rows+'-page',66,8,130,18,'1 / 1',10,'#8095a8'),button('r'+rows+'-clear',230,5,26,24,'⌫','clear'),button('r'+rows+'-close',260,5,26,24,'×','close'));
 p.children[0].binding='picker-prev';p.children[1].binding='picker-next';p.children[2].binding='picker-page';p.children[3].binding='picker-clear';
 for(let i=0;i<rows*5;i++){
  const id='r'+rows+'-cell-'+i,b=button(id,10+i%5*56,38+Math.floor(i/5)*56,48,48,'','choose',''+i);b.binding='picker-cell-'+i;
  b.children=[n('Image',id+'-icon',3,3,42,42,{binding:'picker-icon-'+i}),text(id+'-check',31,30,16,16,'✓',12,'#a4ebf5')];b.children[1].binding='picker-check-'+i;
  p.children.push(b);
 }
 doc.root.children.push(p);
}
const tip=n('Border','hotbar-tip',8,360,264,160,{style:{fill:'#101a23fa',stroke:'#536c7e',strokeWidth:1,radius:6},children:[n('Image','tip-icon',12,12,32,32),text('tip-name',54,12,198,32,'技能名称',13,'#d7eaf6'),text('tip-detail',12,54,240,94,'装备来源与效果说明',11,'#afbfcd')]});doc.root.children.push(tip);
const quick=button('hotbar-quick-slot',330,360,64,64,'','quick');
quick.children=[n('Image','quick-icon',3,3,58,58),text('quick-key',3,0,20,16,'Q',9,'#b4c4da'),text('quick-name',2,38,60,14,'空槽',10),text('quick-state',2,51,60,12,'',9,'#82d9c5'),text('quick-plus',0,14,64,26,'＋',20,'#8298ae')];
for(const child of quick.children)if(['quick-name','quick-state','quick-plus'].includes(child.id))child.style.textAlign='center';
const recent=JSON.parse(await fs.readFile(path.join(root,'UIAuthoring/RecentGameplay.uialpha.json'),'utf8'));
doc.motions=structuredClone(recent.motions);quick.children.push(n('Image','aura-slot-active',0,0,64,64,{locked:true,motion:{preset:'auto',assetId:'motion-aura-active',enabled:false}}));doc.root.children.push(quick);
function wrap(node){for(const child of node.children||[])wrap(child);if(['Border','Button'].includes(node.type)&&node.children.length>1)node.children=[n('CanvasPanel',node.id+'-content',0,0,node.layout.width,node.layout.height,{children:node.children})];}wrap(doc.root);
const errors=validateDocument(doc);if(errors.length)throw Error(errors.join('\n'));
await fs.writeFile(path.join(root,'UIAuthoring/HotbarPicker.uialpha.json'),JSON.stringify(doc,null,2));
const repo=new ProjectRepository(path.join(root,'CustomTools/UICreatorAlpha/data/projects')),old=await repo.pointer(doc.id);await repo.write(doc,old?.revision||0);
console.log(JSON.stringify({project:doc.id,templates:doc.root.children.length,errors}));
