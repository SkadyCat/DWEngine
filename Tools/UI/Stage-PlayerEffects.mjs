import fs from 'node:fs/promises';import path from 'node:path';import crypto from 'node:crypto';
const root=path.resolve(import.meta.dirname,'../..'),job=process.argv[2];if(!/^wbp-[\w-]{1,90}$/.test(job||''))throw Error('Invalid job');
const frozen=path.join(root,'CustomTools/UICreatorAlpha/exports',job),bundle=JSON.parse(await fs.readFile(path.join(frozen,'bundle.json'),'utf8'));
const target=path.join(root,'Content/UICreatorEffects');await fs.mkdir(target,{recursive:true});
const contract=structuredClone(bundle);for(const r of Object.values(contract.rasters)){delete r.png;delete r.svg;}
await fs.writeFile(path.join(target,'contract.json'),JSON.stringify(contract));await fs.writeFile(path.join(target,'source.uialpha.json'),JSON.stringify(bundle.project));
const icons=path.join(root,'Content/OpusIcons/status');await fs.mkdir(icons,{recursive:true});const proof=[];
for(const n of bundle.states.canvas.nodes.filter(n=>n.id.startsWith('effect-symbol-'))){const id=n.id.slice(14),file=path.join(frozen,'rasters',n.raster+'.png');const data=await fs.readFile(file);await fs.writeFile(path.join(icons,id+'.png'),data);proof.push({id,sha256:crypto.createHash('sha256').update(data).digest('hex')});}
if(proof.length!==10)throw Error('Missing authored status icons');
await fs.writeFile(path.join(root,'Saved/Verification/FullHUD/status-icons.json'),JSON.stringify({sourceHash:bundle.sourceHash,icons:proof},null,2));console.log(JSON.stringify({job,sourceHash:bundle.sourceHash,icons:proof.length}));
