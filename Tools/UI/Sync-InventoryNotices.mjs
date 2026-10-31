import fs from 'node:fs/promises';
import path from 'node:path';
import { ProjectRepository } from '../../CustomTools/UICreatorAlpha/server/project-repository.mjs';
import { createNode,validateDocument } from '../../CustomTools/UICreatorAlpha/src/core/model.js';
import { installInventoryNotices } from '../../CustomTools/UICreatorAlpha/src/assets/rulestudio-hud/inventory-notices.js';
import { qualityColor } from '../../CustomTools/UICreatorAlpha/src/assets/rulestudio-hud/equipment-style.js';
const root=path.resolve(import.meta.dirname,'../..'),tool=path.join(root,'CustomTools/UICreatorAlpha');
const repo=new ProjectRepository(path.join(tool,'data/projects')),record=await repo.read('hud-darksteel-20261024');
if(!record)throw Error('Existing HUD is missing');
const backup=path.join(root,'Saved/UIAuthoringBackups/inventory-notices-'+Date.now());await fs.mkdir(backup,{recursive:true});await fs.writeFile(path.join(backup,'project.json'),JSON.stringify(record));
const doc=installInventoryNotices(structuredClone(record.document));
let errors=validateDocument(doc);if(errors.length)throw Error(errors.join('\n'));
if((await repo.pointer(doc.id)).revision!==record.revision)throw Error('Concurrent edit; retry');
const saved=await repo.write(doc,record.revision),mirror=new ProjectRepository(path.join(root,'UIAuthoring/projects'));
await mirror.write(doc,(await mirror.pointer(doc.id))?.revision||0);
for(const file of ['UIAuthoring/RuleStudio3D.uialpha.json','CustomTools/UICreatorAlpha/Screen/RuleStudio-HUD/hud.uialpha.json'])await fs.writeFile(path.join(root,file),JSON.stringify(doc,null,2));
const palette=structuredClone(doc);palette.id='opus-inventory-notices';palette.name='通用动效 · 新物品边缘流光';palette.viewport={width:720,height:240,background:'#111923'};
palette.root=createNode('CanvasPanel',{id:'inventory-notice-review',layout:{width:720,height:240},slot:{offsets:{right:720,bottom:240}}});palette.interfaceCatalog=[];palette.presentation={};
for(const [i,rarity] of ['common','magic','rare','legendary'].entries())palette.root.children.push(createNode('PrefabInstance',{
 id:'notice-'+rarity,prefabId:'battle-prefab-item-cell',name:rarity,slot:{offsets:{left:55+i*160,top:72,right:64,bottom:64}},overrides:{
  'battle-cell':{brushes:Object.fromEntries(['normal','hovered','pressed','disabled'].map(state=>[state,'battle-svg-equipment-cell-'+rarity+'-'+state]))},
  'battle-cell/battle-cell-content/battle-cell-new-edge':{motion:{enabled:true,tint:qualityColor(rarity)}},
 },
}));
errors=validateDocument(palette);if(errors.length)throw Error(errors.join('\n'));
await repo.write(palette,(await repo.pointer(palette.id))?.revision||0);await mirror.write(palette,(await mirror.pointer(palette.id))?.revision||0);
await fs.writeFile(path.join(root,'UIAuthoring/InventoryNotices.uialpha.json'),JSON.stringify(palette,null,2));
console.log(JSON.stringify({project:doc.id,revision:saved.revision,example:palette.id,backup}));
