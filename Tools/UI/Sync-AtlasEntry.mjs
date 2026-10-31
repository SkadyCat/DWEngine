// Save the shared UICreator control and update only the map-device prefab family.
import fs from 'node:fs/promises';import path from 'node:path';
import {ProjectRepository} from '../../CustomTools/UICreatorAlpha/server/project-repository.mjs';
import {validateDocument} from '../../CustomTools/UICreatorAlpha/src/core/model.js';
import {installMapDevice} from '../../CustomTools/UICreatorAlpha/src/assets/rulestudio-hud/map-device.js';
import {atlasEntryDocument} from '../../CustomTools/UICreatorAlpha/src/assets/rulestudio-hud/atlas-entry.js';
const root=path.resolve(import.meta.dirname,'../..'),repo=new ProjectRepository(path.join(root,'CustomTools/UICreatorAlpha/data/projects'));
const record=await repo.read('hud-darksteel-20261024'),doc=structuredClone(record.document);
const proof=path.join(root,'Saved/Verification/MapEntry');await fs.mkdir(proof,{recursive:true});
const backup=path.join(proof,`before-hud-revision-${record.revision}.json`);try{await fs.writeFile(backup,JSON.stringify(record),{flag:'wx'});}catch(error){if(error.code!=='EEXIST')throw error;}
installMapDevice(doc);
for(const candidate of [doc,atlasEntryDocument]){const errors=validateDocument(candidate);if(errors.length)throw Error(errors.join('\n'));}
await repo.write(doc,record.revision);
await repo.write(atlasEntryDocument,(await repo.pointer(atlasEntryDocument.id))?.revision||0);
await fs.writeFile(path.join(root,'UIAuthoring/AtlasEntry.uialpha.json'),JSON.stringify(atlasEntryDocument,null,2)+'\n');
const saved=await repo.read(doc.id);await fs.writeFile(path.join(proof,'authoring.json'),JSON.stringify({project:doc.id,revision:saved.revision,control:atlasEntryDocument.id,removed:['region-status','region-neighbors','atlas-route'],motion:'motion-atlas-entry-ready'},null,2));
console.log(JSON.stringify({project:doc.id,revision:saved.revision,control:atlasEntryDocument.id}));
