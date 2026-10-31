// Offline UICreator authoring only. Production item/crafting logic is Lua/AS/native.
import fs from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { createDocument, clone, walk, validateDocument } from '../../CustomTools/UICreatorAlpha/src/core/model.js';
import { itemTooltipTemplate, inspectorDetailTemplate, itemColor } from '../../CustomTools/UICreatorAlpha/src/assets/rulestudio-hud/item-tooltip.js';
import { ProjectRepository } from '../../CustomTools/UICreatorAlpha/server/project-repository.mjs';

const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'../..'),tool=path.join(root,'CustomTools/UICreatorAlpha');
const stamp=new Date().toISOString().replace(/[:.]/g,'-'),backup=path.join(root,'Saved/Verification/ItemHover','before-'+stamp);
await fs.mkdir(backup,{recursive:true});
const mainPath=path.join(root,'UIAuthoring/RuleStudio3D.uialpha.json');
const main=JSON.parse(await fs.readFile(mainPath,'utf8')),prefab=itemTooltipTemplate();main.prefabs[prefab.id]=prefab;
const repository=new ProjectRepository(path.join(tool,'data/projects'));
const existing=await repository.read(main.id);
if(existing){const document=clone(existing.document);document.prefabs[prefab.id]=clone(prefab);await fs.writeFile(path.join(backup,'editor-project.json'),JSON.stringify(existing));await repository.write(document,existing.revision);}
for(const relative of ['UIAuthoring/RuleStudio3D.uialpha.json','CustomTools/UICreatorAlpha/Screen/RuleStudio-HUD/hud.uialpha.json']){
 const file=path.join(root,relative);let document;try{document=JSON.parse(await fs.readFile(file,'utf8'));await fs.copyFile(file,path.join(backup,path.basename(file)));}catch(e){if(e.code==='ENOENT')continue;throw e;}
 document.prefabs[prefab.id]=clone(prefab);await fs.writeFile(file,JSON.stringify(document,null,2));
}
const source=createDocument('RuleStudio3D · 物品浮层与原位加工控件');source.folders=clone(main.folders);source.id='opus-item-inspector-3d-20261025';source.viewport={width:2700,height:860,background:'transparent'};source.root.id='item-inspector-authoring';
const qualities=['common','normal','magic','rare','legendary','unique','skill'];
for(const [index,quality]of qualities.entries()){
 const card=clone(prefab.root);walk(card,n=>{n.id=`hover-${quality}-${n.id}`;});card.id=`hover-${quality}`;card.slot.offsets={left:12+index*382,top:12,right:360,bottom:760};
 walk(card,n=>{if(n.id.endsWith('battle-item-hover-frame'))n.style.stroke=itemColor({rarity:quality});if(n.id.endsWith('battle-item-hover-name'))n.style.color=itemColor({rarity:quality});});source.root.children.push(card);
}
const detail=inspectorDetailTemplate();detail.slot.offsets={left:12,top:790,right:300,bottom:250};source.root.children.push(detail);source.viewport.height=1060;
walk(source.root,n=>{for(const id of [n.svgAsset,...Object.values(n.brushes||{})])if(id&&main.assets[id])source.assets[id]=clone(main.assets[id]);});
const errors=validateDocument(source);if(errors.length)throw new Error(errors.join('\n'));
const previous=await repository.read(source.id);await repository.write(source,previous?.revision||0);
const sourcePath=path.join(root,'UIAuthoring/HoverSource.uialpha.json');await fs.copyFile(sourcePath,path.join(backup,'HoverSource.uialpha.json'));await fs.writeFile(sourcePath,JSON.stringify(source,null,2));
await fs.copyFile(path.join(root,'Content/UICreatorHover/contract.json'),path.join(backup,'contract.json'));
console.log(JSON.stringify({project:source.id,editor:`http://127.0.0.1:5199/?project=${source.id}`,main:main.id,templates:qualities.length+1,backup,errors}));
