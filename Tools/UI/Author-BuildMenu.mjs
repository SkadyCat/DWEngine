// Presentation-only build-time authoring. Runtime selection and application are AS/native.
import fs from 'node:fs/promises';
import path from 'node:path';
import crypto from 'node:crypto';
import {createDocument,createNode,validateDocument} from '../../CustomTools/UICreatorAlpha/src/core/model.js';
import {ProjectRepository} from '../../CustomTools/UICreatorAlpha/server/project-repository.mjs';
import {groupBuildPresets} from '../../CustomTools/RuleStudio/public/build-preset-menu.mjs';
const root=path.resolve(import.meta.dirname,'../..');
const indexText=await fs.readFile(path.join(root,'Content/Rules/Presets/index.json'),'utf8'),index=JSON.parse(indexText),groups=groupBuildPresets(index);
const doc=createDocument('Opus · Studio3D 可用 BD');doc.id='opus-studio3d-build-menu';doc.viewport={width:1200,height:900,background:'transparent'};doc.root.id='build-menu-authoring';
doc.presentation={independentCaches:true};doc.root.source={reference:'RuleStudio/public/battle3d.mjs .b3-bd-panel',catalogHash:crypto.createHash('sha256').update(indexText).digest('hex'),families:groups.length,presets:index.length};
const rect=(w,h,fill,stroke='transparent',r=0)=>`<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 ${w} ${h}" width="${w}" height="${h}"><rect x=".5" y=".5" width="${w-1}" height="${h-1}" rx="${r}" fill="${fill}" stroke="${stroke}"/></svg>`;
function skin(id,w,h,normal,hover=normal,stroke='transparent',radius=0){for(const [state,fill]of [['normal',normal],['hovered',hover],['pressed',hover],['disabled',normal]])doc.assets[id+'-'+state]={id:id+'-'+state,name:id+' '+state,source:rect(w,h,fill,stroke,radius),viewBox:[0,0,w,h],scaleMode:'stretch'};return Object.fromEntries(['normal','hovered','pressed','disabled'].map(s=>[s,id+'-'+s]));}
const clear=skin('row',264,54,'transparent','#ff974a17'),small=skin('subhead',36,20,'#29303b','#3b4657','transparent',3),arrow=skin('arrow',25,54,'transparent','#ff974a17'),guide=skin('guide',274,30,'#192635','#26384d','#ffffff0a',8);
const n=(type,id,x,y,w,h,p={})=>createNode(type,{id,name:id,binding:id,slot:{offsets:{left:x,top:y,right:w,bottom:h}},layout:{width:w,height:h},...p});
const t=(id,x,y,w,h,text='',size=12,color='#e6ebf3')=>n('TextBlock',id,x,y,w,h,{text,style:{fontSize:size,color,wrap:false,verticalAlign:'center'}});
const c=(id,x,y,w,h,children=[])=>n('CanvasPanel',id,x,y,w,h,{children});
function b(id,x,y,w,h,text,op,brush=clear){return n('Button',id,x,y,w,h,{name:text||id,brushes:brush,action:{type:'build-menu',op},children:[c(id+'-content',0,0,w,h,text?[t(id+'-text',0,0,w,h,text)]:[])]});}
const panel=n('Border','bd-menu',16,16,300,506,{style:{fill:'#0a0d13b8',stroke:'#ffffff14',strokeWidth:1,radius:12},children:[c('bd-menu-content',0,0,300,506)]});
panel.children[0].children.push(t('bd-title',23,11,180,33,'可用 BD',12.5,'#c9d3e2'),t('bd-count',75,11,90,33,''+groups.length,10,'#91a0b5'),b('bd-close',258,11,28,33,'×','close'),n('Border','bd-divider',13,44,274,1,{style:{fill:'#ffffff14'}}),n('ScrollBox','bd-rows',13,45,274,420,{style:{rowGap:0},layout:{width:274,height:420}}),b('bd-guide',13,465,274,30,'构筑原理与词条追求  ▤ ›','guide',guide));
Object.assign(panel.children[0].children.at(-1).children[0].children[0].style,{fontSize:10,color:'#bdcdb0',textAlign:'center'});
doc.root.children.push(panel);
const row=b('bd-family-row',350,16,264,54,'','family');
row.children[0].children.push(n('Border','bd-family-icon-bg',10,11,29,32,{style:{fill:'#40271e',stroke:'#855139',strokeWidth:1,radius:5}}),t('bd-family-icon',10,11,29,32,'余',17,'#ffb777'),t('bd-family-name',47,7,198,21,'余烬回响',12),t('bd-family-meta',47,29,198,18,'5 档强度',10,'#91a0b5'),t('bd-family-state',209,17,47,20,'›',9,'#edb781'));
row.children[0].children[1].style.textAlign='center';row.children[0].children.at(-1).style.textAlign='right';doc.root.children.push(row);
const gradeRow=c('bd-grade-row',350,90,264,54),apply=b('bd-apply',0,0,239,54,'','apply');apply.children[0].children.push(n('Border','bd-grade-icon-bg',10,11,29,32,{style:{fill:'#40271e',stroke:'#855139',strokeWidth:1,radius:5}}),t('bd-grade-icon',10,11,29,32,'余',17,'#ffb777'),t('bd-grade-name',47,7,135,21,'Ⅰ阶',12),t('bd-grade-meta',47,29,135,18,'回蓝循环',10,'#91a0b5'),t('bd-grade-state',181,17,50,20,'一键应用',9,'#edb781'));apply.children[0].children[1].style.textAlign='center';apply.children[0].children.at(-1).style.textAlign='right';gradeRow.children.push(apply,b('bd-details',239,0,25,54,'+','details',arrow));gradeRow.children[1].children[0].children[0].style.textAlign='center';doc.root.children.push(gradeRow);
doc.root.children.push(c('bd-subhead',350,164,264,36,[b('bd-back',9,7,36,22,'‹ 返回','back',small),t('bd-family-title',53,7,202,22,'余烬回响',11,'#e3cb95')]));
const detail=c('bd-detail',650,16,264,330,[t('bd-description',10,0,244,160,'配装说明',10,'#a0acbd'),t('bd-provenance',10,166,244,48,'',10,'#a0acbd')]);detail.children[0].style.wrap=detail.children[1].style.wrap=true;
for(let i=0;i<10;i++)detail.children.push(t('bd-equipment-'+i,10,220+i*20,195,20,'',9,'#c0cad8'),t('bd-owned-'+i,205,220+i*20,49,20,'',9,'#7f8999'));
doc.root.children.push(detail);
const errors=validateDocument(doc);if(errors.length)throw Error(errors.join('\n'));
await fs.mkdir(path.join(root,'Saved/UIAuthoringBackups/BuildMenu'),{recursive:true});
await fs.writeFile(path.join(root,'UIAuthoring/BuildMenu.uialpha.json'),JSON.stringify(doc,null,2));
const repo=new ProjectRepository(path.join(root,'CustomTools/UICreatorAlpha/data/projects'));const pointer=await repo.pointer(doc.id);await repo.write(doc,pointer?.revision||0);
const slotNames={weapon:'武器',helmet:'头盔',chest:'胸甲',gloves:'手套',boots:'鞋子',offhand:'副器',belt:'腰带',ring:'戒指',amulet:'项链'};
const q=JSON.stringify;let as='// Generated from the same catalogue and grouping function as Studio3D.\nnamespace DWOBuildCatalogue\n{\n TArray<FDWOBuildMenuRecord> Read()\n {\n  TArray<FDWOBuildMenuRecord> Out;\n';
for(const entry of index){const loadout=JSON.parse(await fs.readFile(path.join(root,'Content/Rules/Presets',entry.id+'.json'),'utf8'));const skill=loadout.skills.find(Boolean)||'';let skillName=skill;try{skillName=JSON.parse(await fs.readFile(path.join(root,'Content/Rules/Skills',skill+'.json'),'utf8')).name||skill;}catch{}
as+='  {FDWOBuildMenuRecord R;';for(const [key,value]of Object.entries({Id:entry.id,Name:entry.name,GroupId:entry.group||entry.id,GroupName:entry.groupName||entry.name,GradeName:entry.gradeName||entry.name,Mechanism:entry.mechanism||skillName,Description:entry.description||'',Skill:skill}))as+=`R.${key}=${q(value)};`;as+=`R.Grade=${entry.grade||1};`;
for(const id of loadout.skills.filter(Boolean))as+=`R.Skills.Add(${q(id)});`;
for(const [slot,id]of Object.entries(loadout.equipment)){let item=loadout.instances?.[id];if(!item){try{item=JSON.parse(await fs.readFile(path.join(root,'Content/Rules/Equipment',id+'.json'),'utf8'));}catch{throw Error('Missing preset equipment '+id);}}as+=`R.EquipmentIds.Add(${q(id)});R.EquipmentNames.Add(${q((slotNames[slot]||slot)+' · '+(loadout.instances?.[id]?'T'+item.tier+' · ':'')+item.name)});`;}
if(loadout.provenance)as+=`R.Provenance=${q('来源记录：'+loadout.provenance.attempts+' 轮 · '+loadout.provenance.kills+' 次击杀 · '+loadout.provenance.crafts+' 次付费洗炼。此按钮预览记录中的装备。')};`;
as+='Out.Add(R);}\n';}
const css=await fs.readFile(path.join(root,'CustomTools/RuleStudio/public/play.css'),'utf8'),blur=Number(css.match(/\.b3-card\s*\{[^}]*backdrop-filter:\s*blur\((\d+)px\)/)?.[1]);if(!Number.isFinite(blur))throw Error('Missing original BD backdrop blur');
as+=`  return Out;\n }\n float BackdropBlur(){return ${blur};}\n}\n`;await fs.writeFile(path.join(root,'Script/Opus/BuildMenuCatalogue.as'),as);
const ex=path.join(root,'CustomTools/UICreatorAlpha/examples');
await fs.writeFile(path.join(ex,'build-menu-export.html'),'<!doctype html><html lang="zh"><meta charset="utf-8"><title>Studio3D · BD 菜单</title><style>body{background:#090d13;color:#d4dfef;font:14px sans-serif}button{padding:10px}svg{width:1200px;height:900px;display:block}</style><button id="freeze">冻结 BD 菜单并导出 UE</button><svg id="scene"></svg><script type="module" src="build-menu-export.js"></script></html>');
await fs.writeFile(path.join(ex,'build-menu-export.js'),`import {UIRuntime} from '../src/runtime/runtime.js';\nconst r=await fetch('/api/projects/${doc.id}').then(r=>r.json()),project=r.document||r;const ui=new UIRuntime(document.querySelector('#scene'),project,{width:1200,height:900,sound:false,motion:false});document.querySelector('#freeze').onclick=async()=>{await document.fonts.ready;const job='wbp-build-menu-'+crypto.randomUUID(),svg=new XMLSerializer().serializeToString(document.querySelector('#scene'));const r=await fetch('/api/native-wbp/'+job+'/source',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({project,svg,context:{width:1200,height:900,expandedRoot:ui.expanded.document.root}})});if(!r.ok)throw Error(await r.text());location.href=(await r.json()).url;};`);
console.log(JSON.stringify({project:doc.id,errors,families:groups.length,presets:index.length,sourceHash:doc.root.source.catalogHash}));
