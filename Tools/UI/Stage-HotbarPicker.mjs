import fs from 'node:fs/promises';import path from 'node:path';
const root=path.resolve(import.meta.dirname,'../..'),job=process.argv[2];if(!/^wbp-hotbar-[\w-]+$/.test(job||''))throw Error('Invalid job');
const bundle=JSON.parse(await fs.readFile(path.join(root,'CustomTools/UICreatorAlpha/exports',job,'bundle.json'),'utf8'));
if(bundle.project.id!=='opus-hotbar-picker-20261028')throw Error('Wrong hotbar source');
const out=path.join(root,'Content/UICreatorHotbar');await fs.mkdir(out,{recursive:true});const contract=structuredClone(bundle);for(const r of Object.values(contract.rasters)){delete r.png;delete r.svg;}
await fs.writeFile(path.join(out,'contract.json'),JSON.stringify(contract));await fs.writeFile(path.join(out,'source.uialpha.json'),JSON.stringify(bundle.project));
console.log(JSON.stringify({job,sourceHash:bundle.sourceHash,nodes:bundle.nodes.length,rasters:Object.keys(bundle.rasters).length}));
