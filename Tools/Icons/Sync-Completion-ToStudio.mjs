// Sync this authored icon batch to another local Studio without replacing its catalogue.
import fs from 'node:fs/promises';
import path from 'node:path';
import {fileURLToPath,pathToFileURL} from 'node:url';
import {createHash} from 'node:crypto';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'../..');
const target=path.resolve(process.argv[2]||'G:/DungeonWorldBeta');
const relative='CustomTools/RuleStudio';
const source=path.join(root,relative), destination=path.join(target,relative);
await fs.access(path.join(destination,'server.mjs'));
const proof=path.join(root,'Saved/Verification/IconCompletion/StudioIntegration');
const backup=path.join(proof,'previous',path.basename(target));
await fs.mkdir(proof,{recursive:true});
async function write(relativePath,bytes){
 const file=path.join(destination,relativePath);
 const old=await fs.readFile(file).catch(()=>null);
 if(old?.equals(Buffer.from(bytes)))return;
 if(old){const saved=path.join(backup,relativePath);await fs.mkdir(path.dirname(saved),{recursive:true});
  if(!(await fs.stat(saved).catch(()=>null)))await fs.writeFile(saved,old);}
 await fs.mkdir(path.dirname(file),{recursive:true});await fs.writeFile(file,bytes);
}
const {COMPLETION_ENTRIES,COMPLETION_SHEETS}=await import(pathToFileURL(path.join(source,'public/icon-completion-20261026.mjs')));
const files=new Set(['public/icon-completion-20261026.mjs','docs/game-icons-completion-20261026.json']);
for(const icon of COMPLETION_ENTRIES){
 files.add('public'+icon.src);
 if(icon.sourceImage)files.add('public/assets/icons/warcraft-v1/'+icon.sourceImage);
}
files.add('public/assets/icons/warcraft-v1/skill/taunt.png');
for(const sheet of COMPLETION_SHEETS)files.add('public/assets/icons/warcraft-v1/sheets/'+sheet.id+'.png');
for(const file of files)await write(file,await fs.readFile(path.join(source,file)));
let catalog=await fs.readFile(path.join(destination,'public/icon-catalog.mjs'),'utf8');
if(!catalog.includes("from './icon-completion-20261026.mjs'")){
 catalog="import {COMPLETION_SHEETS,COMPLETION_ENTRIES} from './icon-completion-20261026.mjs';\n"+catalog;
 catalog+='\n// Dedicated art completion 2026-10-26\nfor(const entry of COMPLETION_ENTRIES){const at=ICON_ENTRIES.findIndex(icon=>icon.kind===entry.kind&&icon.id===entry.id);if(at>=0)ICON_ENTRIES[at]=entry;else ICON_ENTRIES.push(entry);}\nICON_SHEETS.push(...COMPLETION_SHEETS);\n';
 await write('public/icon-catalog.mjs',catalog);
}
let lookup=await fs.readFile(path.join(destination,'public/game-icons.mjs'),'utf8');
if(!lookup.includes("doc.category === 'map'"))lookup=lookup.replace('  if (direct) return direct;',"  if (direct) return direct;\n  if (kind === 'item' && doc.category === 'map') return icons.get('item:map');");
else lookup=lookup.replace("if (kind === 'item' && doc.category === 'map') return icons.get('item:scroll_frost_lance');","if (kind === 'item' && doc.category === 'map') return icons.get('item:map');");
await write('public/game-icons.mjs',lookup);
const {ICON_ENTRIES,ICON_SHEETS}=await import(pathToFileURL(path.join(destination,'public/icon-catalog.mjs')));
const manifestPath='public/assets/icons/warcraft-v1/manifest.json';
const manifest=JSON.parse(await fs.readFile(path.join(destination,manifestPath),'utf8'));
await write(manifestPath,JSON.stringify({...manifest,sheets:ICON_SHEETS,icons:ICON_ENTRIES},null,2)+'\n');
// These stones now carry their own painted colours; keep grade labels, skip legacy recolouring.
let css=await fs.readFile(path.join(destination,'public/app.css'),'utf8');
if(!css.includes('Dedicated spirit-stone art')){
 css+='\n/* Dedicated spirit-stone art already includes the grade colour. */\n.game-icon[data-currency-grade="2"][src$="/item/spirit_stone_mid.png"],\n.game-icon[data-currency-grade="3"][src$="/item/spirit_stone_high.png"] { filter: none; }\n';
 await write('public/app.css',css);
}
const results=[];
for(const entry of COMPLETION_ENTRIES){const file='public'+entry.src;const a=await fs.readFile(path.join(source,file)),b=await fs.readFile(path.join(destination,file));if(!a.equals(b))throw Error('Sync mismatch '+entry.id);results.push({id:entry.id,kind:entry.kind,sha256:createHash('sha256').update(b).digest('hex')});}
const receipt={source:root,target,verifiedAt:new Date().toISOString(),mapped:results.length,newArt:32,catalogue:ICON_ENTRIES.length,sheets:ICON_SHEETS.length,files:files.size,icons:results};
await fs.writeFile(path.join(proof,path.basename(target)+'-sync.json'),JSON.stringify(receipt,null,2)+'\n');
console.log(JSON.stringify({target,mapped:results.length,newArt:32,catalogue:ICON_ENTRIES.length,sheets:ICON_SHEETS.length,files:files.size}));
