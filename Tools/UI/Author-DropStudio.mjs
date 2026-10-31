import fs from 'node:fs/promises';
import path from 'node:path';
import {validateDocument} from '../../CustomTools/UICreatorAlpha/src/core/model.js';
import {createDropStudioProject} from '../../CustomTools/UICreatorAlpha/src/assets/rulestudio-hud/drop-studio.js';
import {ProjectRepository} from '../../CustomTools/UICreatorAlpha/server/project-repository.mjs';
import {designCsv,validateDesign} from '../../CustomTools/RuleStudio/public/drop-studio-model.mjs';
const root=path.resolve(import.meta.dirname,'../..'),project=createDropStudioProject();
const errors=validateDocument(project);if(errors.length)throw Error(errors.join('\n'));
const dataFile=path.join(root,'CustomTools/RuleStudio/public/assets/data/drop-design.v1.json');
const data=validateDesign(JSON.parse(await fs.readFile(dataFile,'utf8')));
// Resolve authored item labels from the existing catalogue; never invent asset IDs.
for(const item of data.items){const doc=JSON.parse(await fs.readFile(path.join(root,'Content/Rules/Items',item.id+'.json'),'utf8'));item.name=doc.name;}
await fs.writeFile(dataFile,JSON.stringify(data,null,2)+'\n');
for(const boss of data.draft.bossPools)for(const id of boss.equipmentIds)await fs.access(path.join(root,'Content/Rules/Equipment',id+'.json'));
await fs.writeFile(path.join(root,'UIAuthoring/DropStudio.uialpha.json'),JSON.stringify(project,null,2)+'\n');
await fs.writeFile(path.join(root,'CustomTools/RuleStudio/public/assets/data/drop-design.v1.csv'),designCsv(data));
const repo=new ProjectRepository(path.join(root,'CustomTools/UICreatorAlpha/data/projects'));
const old=await repo.pointer(project.id);const saved=await repo.write(project,old?.revision||0);
console.log(JSON.stringify({project:project.id,revision:saved.revision,items:data.items.length,csv:'public/assets/data/drop-design.v1.csv',gameplayApplied:false}));
