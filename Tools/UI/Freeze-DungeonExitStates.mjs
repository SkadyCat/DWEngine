// Freeze the three production UICreator canvases captured from the preview DOM.
import fs from 'node:fs/promises';
import path from 'node:path';
import crypto from 'node:crypto';
import {freezeNativeWbpSource} from '../../CustomTools/UICreatorAlpha/server/native-wbp-export.mjs';
const root=path.resolve(import.meta.dirname,'../..');
const proof=path.join(root,'Saved/Verification/DungeonExit');
const project=JSON.parse(await fs.readFile(path.join(root,'UIAuthoring/DungeonExit.uialpha.json'),'utf8'));
const rendered=JSON.parse(await fs.readFile(path.join(proof,'action-rendered-states.json'),'utf8'));
const canvases=rendered.filter(svg=>/^<svg\b[^>]*aria-label="地牢出口传送阵"/.test(svg));
if(canvases.length!==3)throw Error('Expected the three actual preview root SVGs');
const jobs=[];
for(const [index,mode] of ['normal','ready','travel'].entries()){
 const document=structuredClone(project),nodes=new Map();
 const visit=node=>{nodes.set(node.id,node);for(const child of node.children||[])visit(child);};visit(document.root);
 for(const [id,patch] of document.presentation.portalStates[mode]){
  const node=nodes.get(id);if(!node)throw Error('Missing authored node '+id);
  const {style,brushes,...rest}=patch;Object.assign(node,rest);
  if(style)Object.assign(node.style,style);if(brushes)Object.assign(node.brushes,brushes);
 }
 const svg=canvases[index];
 for(const id of ['portal-title','portal-note'])if(!svg.includes(nodes.get(id).text))throw Error('Rendered text differs for '+mode);
 if(svg.includes('data-node-id="portal-state"'))throw Error('Stale status node in canvas');
 const job='wbp-portal-'+mode+'-'+crypto.randomUUID();
 await freezeNativeWbpSource(path.join(root,'CustomTools/UICreatorAlpha'),job,{project:document,svg,context:{width:document.viewport.width,height:document.viewport.height,expandedRoot:document.root}});
 jobs.push(job);
}
await fs.writeFile(path.join(proof,'action-jobs.json'),JSON.stringify(jobs,null,2));
console.log(JSON.stringify(jobs));
