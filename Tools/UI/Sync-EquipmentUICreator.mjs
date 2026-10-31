import fs from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { ProjectRepository } from '../../CustomTools/UICreatorAlpha/server/project-repository.mjs';
import { validateDocument } from '../../CustomTools/UICreatorAlpha/src/core/model.js';
import { installEquipmentStyle } from '../../CustomTools/UICreatorAlpha/src/assets/rulestudio-hud/equipment-style.js';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'../..');
const primary=new ProjectRepository(path.join(root,'CustomTools/UICreatorAlpha/data/projects'));
const id='hud-darksteel-20261024',previous=await primary.read(id);
if(!previous)throw Error('Existing full HUD is missing');
const backup=path.join(root,`Saved/UIAuthoringBackups/hud-before-equipment-revision-${previous.revision}.json`);
await fs.mkdir(path.dirname(backup),{recursive:true});try{await fs.writeFile(backup,JSON.stringify(previous),{flag:'wx'});}catch(error){if(error.code!=='EEXIST')throw error;}
const document=installEquipmentStyle(structuredClone(previous.document));
const errors=validateDocument(document);if(errors.length)throw Error(errors.join('\n'));
if((await primary.pointer(id)).revision!==previous.revision)throw Error('Concurrent edit; retry with latest HUD');
const saved=await primary.write(document,previous.revision);
const mirror=new ProjectRepository(path.join(root,'UIAuthoring/projects'));await mirror.write(document,(await mirror.pointer(id))?.revision||0);
await fs.writeFile(path.join(root,'UIAuthoring/RuleStudio3D.uialpha.json'),JSON.stringify(document,null,2));
console.log(JSON.stringify({project:id,revision:saved.revision,backup,errors:errors.length,style:document.presentation.equipmentStyle}));
