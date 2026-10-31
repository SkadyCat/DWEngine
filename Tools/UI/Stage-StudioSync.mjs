import fs from 'node:fs/promises';import path from 'node:path';
const root=path.resolve(import.meta.dirname,'../..'),job=process.argv[2];if(!/^wbp-[\w-]{1,90}$/.test(job||''))throw Error('Invalid job');
const bundle=JSON.parse(await fs.readFile(path.join(root,'CustomTools/UICreatorAlpha/exports',job,'bundle.json'),'utf8'));
if(bundle.project.id!=='opus-studio-sync-20261026'||bundle.nodes.filter(n=>n.source?.plotSurface).length!==2)throw Error('Wrong Studio sync contract');
const target=path.join(root,'Content/UICreatorStudio');await fs.mkdir(target,{recursive:true});
const contract=structuredClone(bundle);for(const r of Object.values(contract.rasters)){delete r.png;delete r.svg;}
await fs.writeFile(path.join(target,'contract.json'),JSON.stringify(contract));await fs.writeFile(path.join(target,'source.uialpha.json'),JSON.stringify(bundle.project));
const receipt={job,project:bundle.project.id,sourceHash:bundle.sourceHash,nodes:bundle.nodes.length,rasters:Object.keys(bundle.rasters).length,states:Object.keys(bundle.states)};
await fs.writeFile(path.join(root,'Saved/Verification/StudioSync/frozen-source.json'),JSON.stringify(receipt,null,2));console.log(JSON.stringify(receipt));
