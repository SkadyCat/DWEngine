// UICreator source for Studio3D's equipment context menu. Gameplay lives in AS.
import fs from 'node:fs/promises';
import path from 'node:path';
import {createDocument,createNode,validateDocument} from '../../CustomTools/UICreatorAlpha/src/core/model.js';
import {ProjectRepository} from '../../CustomTools/UICreatorAlpha/server/project-repository.mjs';
import {CONTEXT_MENU_COLORS,contextMenuFrame,CONTEXT_MENU_ACTIONS,CONTEXT_MENU_STONE} from '../../CustomTools/UICreatorAlpha/src/assets/rulestudio-hud/context-menu-style.js';
const root=path.resolve(import.meta.dirname,'../..'),d=createDocument('物品 · 右键操作菜单');
d.id='opus-item-context';d.viewport={width:1120,height:540,background:'#090d13'};d.root.id='item-context-authoring';
const colors=CONTEXT_MENU_COLORS;
const n=(type,id,x,y,w,h,more={})=>createNode(type,{id,name:id,slot:{offsets:{left:x,top:y,right:w,bottom:h}},layout:{width:w,height:h},...more});
const text=(id,b,x,y,w,h,value,size=12,color='#d4deed')=>n('TextBlock',id,x,y,w,h,{binding:b,text:value,style:{fontSize:size,color,wrap:false,textOverflow:'shrink'}});
function asset(id,w,h,body){d.assets[id]={id,name:id,viewBox:[0,0,w,h],source:`<svg xmlns="http://www.w3.org/2000/svg" width="${w}" height="${h}" viewBox="0 0 ${w} ${h}">${body}</svg>`,scaleMode:'stretch'};return id;}
for(const [id,[fill,stroke]] of Object.entries(CONTEXT_MENU_ACTIONS))asset(id,228,36,`<rect x=".5" y=".5" width="227" height="35" rx="5" fill="${fill}" stroke="${stroke}"/>`);
const icons={equip:'<path d="M14 4h6v16h-6M3 12h12m-4-4 4 4-4 4"/>',unequip:'<path d="M14 4h6v16h-6M3 12h12m-8-4-4 4 4 4"/>',identify:'<path d="m12 3 2.4 6.6L21 12l-6.6 2.4L12 21l-2.4-6.6L3 12l6.6-2.4ZM19 3v4m-2-2h4"/>',sell:'<ellipse cx="9" cy="6" rx="6" ry="3"/><path d="M3 6v5c0 1.7 2.7 3 6 3m0-5v5m6-7v2m-9 4v3c0 1.7 2.7 3 6 3"/><ellipse cx="16" cy="13" rx="5" ry="2.5"/><path d="M11 13v5c0 1.4 2.2 2.5 5 2.5s5-1.1 5-2.5v-5"/>',drop:'<path d="M12 3v12m-5-5 5 5 5-5M4 17v4h16v-4"/>'};
for(const [id,body] of Object.entries(icons))asset('icon-'+id,24,24,`<g stroke="#8495ad" stroke-width="1.6" fill="none" stroke-linejoin="round" stroke-linecap="round">${body}</g>`);
asset('stone',14,14,CONTEXT_MENU_STONE);
function menu(quality,index,item=false){
 const id='context-'+quality,H=item?108:184,color=colors[quality]||colors.common;
 const bg=asset(id+'-bg',240,H,contextMenuFrame(quality,H));
 const p=n('CanvasPanel',id,24+(index%3)*356,24+Math.floor(index/3)*254,240,H);
 p.children.push(n('Image',id+'-background',0,0,240,H,{svgAsset:bg}),n('Border',id+'-icon-frame',12,11,36,36,{style:{fill:'#080e17',stroke:color,strokeWidth:1,radius:5}}),n('Image',id+'-icon',14,13,32,32,{binding:'context:icon'}),text(id+'-name','context:name',58,11,168,19,'风暴指环',12,color),text(id+'-meta','context:meta',58,32,168,16,quality==='rare'?'稀有 · T1':'T1',10,'#8e9cb0'));
 const row=(op,y,label,icon)=>{
  const children=[n('Image',id+'-'+op+'-icon',9,10,16,16,{svgAsset:'icon-'+icon,...(op==='primary'?{binding:'context:equip'}:{})}),text(id+'-'+op+'-text','context:'+op+'-text',34,0,184,36,label)];
  if(op==='primary')for(const key of ['unequip','identify'])children.push(n('Image',id+'-primary-'+key,9,10,16,16,{binding:'context:'+key,svgAsset:'icon-'+key,visibility:'collapsed'}));
  const b=n('Button',id+'-'+op,6,y,228,36,{binding:'context:'+op,action:{type:'item-context',op},brushes:{normal:'normal',hovered:'hovered',pressed:'pressed',disabled:'disabled'},children:[n('CanvasPanel',id+'-'+op+'-body',0,0,228,36,{children})]});
  p.children.push(b);return b.children[0];
 };
 if(!item){row('primary',63,'装备','equip');const sale=row('sell',101,'出售','sell');sale.children.push(n('Border',id+'-price-bg',150,7,70,22,{binding:'context:price-box',style:{fill:'#102c2c',stroke:'#1d4945',strokeWidth:1,radius:3}}),n('Image',id+'-stone',156,11,14,14,{binding:'context:stone',svgAsset:'stone'}),text(id+'-price','context:price',171,7,20,22,'+4',12,'#8fe2c1'),text(id+'-stone-name','context:money',193,7,25,22,'灵石',10,'#8e9cb0'));}
 row('drop',item?63:139,'丢到地面','drop');d.root.children.push(p);
}
Object.keys(colors).forEach((q,i)=>menu(q,i));menu('item',5,true);
const errors=validateDocument(d);if(errors.length)throw Error(errors.join('\n'));
const repo=new ProjectRepository(path.join(root,'CustomTools/UICreatorAlpha/data/projects'));const old=await repo.pointer(d.id);await repo.write(d,old?.revision||0);
await fs.writeFile(path.join(root,'UIAuthoring/ItemContext.uialpha.json'),JSON.stringify(d,null,2));
await fs.writeFile(path.join(root,'CustomTools/UICreatorAlpha/examples/item-context-export.html'),'<!doctype html><meta charset="utf-8"><title>UICreator 物品右键菜单</title><style>body{background:#090d13;color:#ddd;font:14px sans-serif}button{padding:12px}svg{display:block;width:1120px}</style><button id="freeze">冻结右键菜单并导出 UE</button><svg id="scene"></svg><script type="module" src="item-context-export.js"></script>');
await fs.writeFile(path.join(root,'CustomTools/UICreatorAlpha/examples/item-context-export.js'),`import {UIRuntime} from '../src/runtime/runtime.js';const record=await fetch('/api/projects/opus-item-context').then(r=>r.json());const project=record.document||record;const runtime=new UIRuntime(document.querySelector('#scene'),project,{width:project.viewport.width,height:project.viewport.height,motion:false,sound:false});document.querySelector('#freeze').onclick=async()=>{await document.fonts.ready;const job='wbp-context-'+crypto.randomUUID();const svg=new XMLSerializer().serializeToString(document.querySelector('#scene'));const response=await fetch('/api/native-wbp/'+job+'/source',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({project,svg,context:{width:project.viewport.width,height:project.viewport.height,expandedRoot:runtime.expanded.document.root}})});const result=await response.json();if(!response.ok)throw Error(JSON.stringify(result));location.href=result.url;};`);
console.log(JSON.stringify({project:d.id,templates:d.root.children.length,errors}));
