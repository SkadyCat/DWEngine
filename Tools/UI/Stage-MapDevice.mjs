import fs from 'node:fs/promises';import path from 'node:path';
const root=path.resolve(import.meta.dirname,'../..'),job=process.argv[2];if(!/^wbp-[\w-]{1,90}$/.test(job||''))throw Error('Invalid job');
const bundle=JSON.parse(await fs.readFile(path.join(root,'CustomTools/UICreatorAlpha/exports',job,'bundle.json'),'utf8'));
if(bundle.project.id!=='opus-map-device'||bundle.project.provenance.businessRuntime!=='AngelScript')throw Error('Wrong map source');
const contract=structuredClone(bundle);for(const r of Object.values(contract.rasters)){delete r.png;delete r.svg;}
const target=path.join(root,'Content/UICreatorMaps');await fs.mkdir(target,{recursive:true});
await fs.writeFile(path.join(target,'contract.json'),JSON.stringify(contract));await fs.writeFile(path.join(target,'source.uialpha.json'),JSON.stringify(bundle.project));
await fs.writeFile(path.join(root,'Saved/Verification/MapDevice/source-contract.json'),JSON.stringify({job,sourceHash:bundle.sourceHash,source:bundle.project.provenance,nodes:bundle.nodes.map(n=>({id:n.id,type:n.type,parentId:n.parentId,binding:n.binding})),states:Object.keys(bundle.states),rasters:Object.keys(bundle.rasters).length},null,2));
console.log(JSON.stringify({job,sourceHash:bundle.sourceHash,nodes:bundle.nodes.length,rasters:Object.keys(bundle.rasters).length,target}));
