import fs from 'node:fs/promises';
import path from 'node:path';
import {ProjectRepository} from '../../CustomTools/UICreatorAlpha/server/project-repository.mjs';
import {validateDocument} from '../../CustomTools/UICreatorAlpha/src/core/model.js';
const root=path.resolve(import.meta.dirname,'../..'),repo=new ProjectRepository(path.join(root,'CustomTools/UICreatorAlpha/data/projects'));
const record=await repo.read('hud-darksteel-20261024'),doc=structuredClone(record.document);
// Add palette aliases only. All artwork comes from the approved selected assets.
let aliases=0;function visit(v){if(!v||typeof v!=='object')return;if(v.dynamicBrushes){for(const [quality,states]of Object.entries(v.dynamicBrushes)){if(quality==='empty'||quality.endsWith('-selected'))continue;const asset=states.normal?.replace(/-normal$/,'-selected');if(!asset||!doc.assets[asset])throw Error('Missing approved selected brush '+asset);v.dynamicBrushes[quality+'-selected']=Object.fromEntries(['normal','hovered','pressed','disabled'].map(s=>[s,asset]));aliases++;}}for(const c of Object.values(v))if(c&&typeof c==='object')visit(c);}
visit(doc.root);for(const p of Object.values(doc.prefabs))visit(p.root);
doc.id='opus-studio-iteration-hud-20261027';doc.name='Opus · Studio 最近迭代';doc.provenance={...doc.provenance,source:record.document.id,revision:record.revision,nativeProjection:'selected palette aliases; source artwork and layout preserved'};
const errors=validateDocument(doc);if(errors.length)throw Error(errors.join('\n'));
const old=await repo.read(doc.id),saved=await repo.write(doc,old?.revision||0);
await fs.writeFile(path.join(root,'Saved/Verification/StudioToUE-20261027/hud-source.json'),JSON.stringify({source:record.document.id,sourceRevision:record.revision,project:doc.id,revision:saved.revision,aliases},null,2));
console.log(JSON.stringify({id:doc.id,revision:saved.revision,sourceRevision:record.revision,aliases}));
