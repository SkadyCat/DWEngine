// Merge actual frozen UICreator exports, preserving each state's rendered reference.
import fs from 'node:fs/promises';
import path from 'node:path';
import crypto from 'node:crypto';
import {saveNativeWbpBundle,freezeNativeWbpSource} from '../../CustomTools/UICreatorAlpha/server/native-wbp-export.mjs';
const root=path.resolve(import.meta.dirname,'../..'),tool=path.join(root,'CustomTools/UICreatorAlpha');
const jobs=process.argv.slice(2);if(jobs.length!==3||jobs.some(j=>!/^wbp-portal-(normal|ready|travel)-[\w-]+$/.test(j)))throw Error('Expected normal, ready and travel frozen jobs');
const bundles=await Promise.all(jobs.map(async j=>JSON.parse(await fs.readFile(path.join(tool,'exports',j,'bundle.json'),'utf8'))));
const merged=structuredClone(bundles[0]);merged.references={};merged.variantSources={};
const ids=b=>b.nodes.map(n=>n.id).sort().join('|');
for(const [index,mode]of ['normal','ready','travel'].entries()){
 const b=bundles[index],folder=path.join(tool,'exports',jobs[index]);
 if(ids(b)!==ids(merged)||b.viewport.width!==merged.viewport.width||b.viewport.height!==merged.viewport.height)throw Error('Variant layout differs');
 merged.variantSources[mode]={job:jobs[index],sourceHash:b.sourceHash};
 Object.assign(merged.rasters,b.rasters);
 for(const [state,record]of Object.entries(b.states)){
  const name=mode==='normal'?state:state==='canvas'?mode:mode+'-'+state;
  if(mode!=='normal'&&state==='authoring')continue;
  merged.states[name]=record;
  if(b.references[state])merged.references[name]={png:(await fs.readFile(path.join(folder,b.references[state]))).toString('base64')};
 }
}
const job='wbp-portal-complete-'+crypto.randomUUID();
const context=JSON.parse(await fs.readFile(path.join(tool,'exports',jobs[0],'canvas-context.json'),'utf8'));
await freezeNativeWbpSource(tool,job,{project:merged.project,svg:merged.sourceSvg,context});
const result=await saveNativeWbpBundle(tool,job,merged);
const proof=path.join(root,'Saved/Verification/DungeonExit');await fs.mkdir(proof,{recursive:true});
await fs.writeFile(path.join(proof,'export.json'),JSON.stringify({...result,variantSources:merged.variantSources},null,2));
console.log(JSON.stringify({...result,variantSources:merged.variantSources}));
