// Register only the new authored assets. Preserve the historical A-G catalogue.
import fs from 'node:fs/promises';
import path from 'node:path';
import {fileURLToPath,pathToFileURL} from 'node:url';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'../..');
const pub=path.join(root,'CustomTools/RuleStudio/public');
const proof=path.join(root,'Saved/Verification/IconCompletion');
const file=path.join(pub,'icon-catalog.mjs');
await fs.access(path.join(pub,'icon-completion-20261026.mjs'));
let source=await fs.readFile(file,'utf8');
const marker='// Dedicated art completion 2026-10-26';
if(!source.includes(marker)){
 await fs.copyFile(file,path.join(proof,'icon-catalog-before.mjs'));
 source="import {COMPLETION_SHEETS,COMPLETION_ENTRIES} from './icon-completion-20261026.mjs';\n"+source;
 source+='\n'+marker+'\nfor(const entry of COMPLETION_ENTRIES){const at=ICON_ENTRIES.findIndex(icon=>icon.kind===entry.kind&&icon.id===entry.id);if(at>=0)ICON_ENTRIES[at]=entry;else ICON_ENTRIES.push(entry);}\nICON_SHEETS.push(...COMPLETION_SHEETS);\n';
 await fs.writeFile(file,source);
}
const {ICON_ENTRIES,ICON_SHEETS}=await import(pathToFileURL(file));
const manifest=path.join(pub,'assets/icons/warcraft-v1/manifest.json');
if(!(await fs.stat(path.join(proof,'manifest-before.json')).catch(()=>null)))await fs.copyFile(manifest,path.join(proof,'manifest-before.json'));
const old=JSON.parse(await fs.readFile(manifest,'utf8'));
await fs.writeFile(manifest,JSON.stringify({...old,sheets:ICON_SHEETS,icons:ICON_ENTRIES},null,2)+'\n');
console.log(JSON.stringify({catalogue:ICON_ENTRIES.length,sheets:ICON_SHEETS.length,newArt:32,mapVariants:48}));
