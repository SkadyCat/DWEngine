// New skill icon authored in UICreator; the existing aura panel/slots remain the UI contract.
import fs from 'node:fs/promises';
import path from 'node:path';
import {createDocument,createNode,validateDocument} from '../../CustomTools/UICreatorAlpha/src/core/model.js';
import {ProjectRepository} from '../../CustomTools/UICreatorAlpha/server/project-repository.mjs';
const root=path.resolve(import.meta.dirname,'../..');
const source=`<svg xmlns="http://www.w3.org/2000/svg" width="128" height="128" viewBox="0 0 128 128">
<defs><radialGradient id="jade"><stop stop-color="#225b62"/><stop offset=".67" stop-color="#102e38"/><stop offset="1" stop-color="#07151e"/></radialGradient><linearGradient id="silver" x2="1" y2="1"><stop stop-color="#efffff"/><stop offset=".5" stop-color="#84e9dc"/><stop offset="1" stop-color="#2b849b"/></linearGradient></defs>
<circle cx="64" cy="64" r="59" fill="url(#jade)" stroke="#94b9ae" stroke-width="2"/>
<circle cx="64" cy="64" r="52" fill="none" stroke="#457d81" stroke-width="1" opacity=".6"/>
<g fill="none" stroke="url(#silver)" stroke-linecap="round" stroke-linejoin="round">
<path d="M24 67C35 31 79 24 98 44C111 59 96 75 78 70C65 66 69 49 81 52" stroke-width="6"/>
<path d="M103 67C92 103 49 109 30 88C17 74 32 57 50 62C63 66 59 83 47 80" stroke-width="6"/>
<path d="M42 39C62 19 95 25 107 47M86 89C66 109 33 103 21 81" stroke-width="2.5"/>
<path d="M54 54L64 44L74 54M54 74L64 84L74 74" stroke-width="3"/>
</g><circle cx="64" cy="64" r="4" fill="#ddfff5"/>
</svg>`;
const doc=createDocument('风之息 · 光环图标');doc.id='opus-wind-breath';doc.viewport={width:320,height:240,background:'#090d13'};doc.root.id='wind-breath-authoring';
doc.assets['wind-breath']={id:'wind-breath',name:'风之息',source,viewBox:[0,0,128,128],scaleMode:'contain'};
doc.root.children.push(createNode('Image',{id:'wind-breath-icon',name:'风之息',svgAsset:'wind-breath',slot:{offsets:{left:96,top:22,right:128,bottom:128}},layout:{width:128,height:128}}));
doc.root.children.push(createNode('TextBlock',{id:'wind-breath-label',name:'名称',text:'风之息 · 光环',slot:{offsets:{left:72,top:170,right:220,bottom:32}},layout:{width:220,height:32},style:{fontSize:22,color:'#b4ebdf'}}));
const errors=validateDocument(doc);if(errors.length)throw Error(JSON.stringify(errors));
await fs.mkdir(path.join(root,'UIAuthoring/Skills'),{recursive:true});await fs.writeFile(path.join(root,'UIAuthoring/Skills/wind_breath.svg'),source);await fs.writeFile(path.join(root,'UIAuthoring/WindBreath.uialpha.json'),JSON.stringify(doc,null,2));
const repo=new ProjectRepository(path.join(root,'CustomTools/UICreatorAlpha/data/projects'));const pointer=await repo.pointer(doc.id);const saved=await repo.write(doc,pointer?.revision||0);
const file=path.join(root,'CustomTools/RuleStudio/public/icon-catalog.mjs');let catalogue=await fs.readFile(file,'utf8');
for(const [kind,id,name]of [['skill','wind_breath','风之息'],['equipment','wind_breath_ring','风息指环'],['equipment','wind_breath_amulet','风息护符']])if(!catalogue.includes(`"id":"${id}"`))catalogue=catalogue.replace(/\n\];\s*$/,`,\n  ${JSON.stringify({kind,id,name,sheet:null,row:null,column:null,src:'/assets/icons/warcraft-v1/'+kind+'/'+id+'.png'})}\n];\n`);
await fs.writeFile(file,catalogue);console.log(JSON.stringify({project:doc.id,...saved,errors}));
