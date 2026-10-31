import fs from 'node:fs/promises';
import { installBattleSuite } from '../src/assets/rulestudio-hud/suite.js';
import { validateDocument, walk } from '../src/core/model.js';
import { builtinSounds } from '../src/assets/ui-sounds.js';
import { expandDocument } from '../src/core/prefabs.js';
import { computeLayout } from '../src/core/layout.js';
const response=await fetch('http://127.0.0.1:5190/api/projects/hud-darksteel-20261024');if(!response.ok)throw new Error('HUD project unavailable');
const previous=await response.json(),generated=installBattleSuite(structuredClone(previous.document));
// A map revision must preserve edits to other screens and shared skins.
let document=process.argv.includes('--atlas-only')?{...structuredClone(previous.document),assets:{...previous.document.assets,...Object.fromEntries(Object.entries(generated.assets).filter(([id])=>id.startsWith('battle-svg-atlas-')||id==='battle-svg-region'))},prefabs:{...previous.document.prefabs,'battle-prefab-atlas':generated.prefabs['battle-prefab-atlas']}}:generated;
if(process.argv.includes('--item-tooltip-only'))document={...structuredClone(previous.document),prefabs:{...previous.document.prefabs,'battle-prefab-item-hover':generated.prefabs['battle-prefab-item-hover']}};
const defaults=builtinSounds(),resolveAudio=node=>{for(const id of Object.values(node.audio?.events||{}))if(id&&!document.sounds?.[id]&&defaults[id]){document.sounds||={};document.sounds[id]=defaults[id];}};
walk(document.root,resolveAudio);for(const prefab of Object.values(document.prefabs))walk(prefab.root,resolveAudio);
const errors=validateDocument(document);if(errors.length)throw new Error(errors.join('\n'));
const backup=new URL('../work/rulestudio-hud/',import.meta.url);await fs.mkdir(backup,{recursive:true});
await fs.writeFile(new URL(`before-suite-r${previous.revision}.json`,backup),JSON.stringify(previous,null,2),{flag:'wx'}).catch(e=>{if(e.code!=='EEXIST')throw e;});
await fs.writeFile(new URL('suite-draft.json',backup),JSON.stringify(document,null,2));
const dir=new URL('../src/assets/rulestudio-hud/svg/suite/',import.meta.url);await fs.mkdir(dir,{recursive:true});for(const asset of Object.values(document.assets).filter(a=>a.id.startsWith('battle-svg-')))await fs.writeFile(new URL(`${asset.id.slice(11)}.svg`,dir),asset.source);
console.log({previous:previous.revision,interfaces:document.interfaceCatalog.length,assets:Object.keys(document.assets).length,prefabs:Object.keys(document.prefabs).length});
if(process.argv.includes('--save')){
 const saved=await fetch(`http://127.0.0.1:5190/api/projects/${document.id}`,{method:'PUT',headers:{'Content-Type':'application/json','If-Match':`"${previous.revision}"`},body:JSON.stringify(document)});const result=await saved.json();if(!saved.ok)throw new Error(JSON.stringify(result));
 await fs.writeFile(new URL('../Screen/RuleStudio-HUD/hud.uialpha.json',import.meta.url),JSON.stringify(document,null,2));
 await fs.writeFile(new URL('../Screen/RuleStudio-HUD/interfaces.json',import.meta.url),JSON.stringify(document.interfaceCatalog,null,2));
 const expanded=expandDocument(document),layout=computeLayout(expanded.document);
 await fs.writeFile(new URL('../Screen/RuleStudio-HUD/composition.json',import.meta.url),JSON.stringify({schema:'uicreator-alpha/svg-composition',version:1,projectId:document.id,source:document.presentation.source,viewport:document.viewport,assets:Object.keys(document.assets),prefabs:Object.keys(document.prefabs),interfaces:document.interfaceCatalog,presentation:document.presentation,currentNodes:layout.entries.size,availableNodes:expanded.count,revision:result.revision},null,2));
 console.log({saved:result.revision});
}
