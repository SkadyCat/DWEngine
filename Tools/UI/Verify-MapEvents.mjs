import fs from 'node:fs/promises';
import path from 'node:path';
import assert from 'node:assert/strict';
import crypto from 'node:crypto';
import {validateDocument,walk} from '../../CustomTools/UICreatorAlpha/src/core/model.js';
const root=path.resolve(import.meta.dirname,'../..');
const job=process.argv[2]||'wbp-events-590cb159-7df5-42f3-821e-1e4b22a60245';
assert.match(job,/^wbp-[a-zA-Z0-9_-]{1,90}$/);
const source=JSON.parse(await fs.readFile(path.join(root,'UIAuthoring/MapEvents.uialpha.json'),'utf8'));
assert.deepEqual(validateDocument(source),[]);
const directory=path.join(root,'CustomTools/UICreatorAlpha/exports',job);
const bundle=JSON.parse(await fs.readFile(path.join(directory,'bundle.json'),'utf8'));
const deployed=JSON.parse(await fs.readFile(path.join(root,'Content/UICreatorEvents/contract.json'),'utf8'));
assert.equal(bundle.sourceHash,deployed.sourceHash);const visual=value=>Array.isArray(value)?value.map(visual):value&&typeof value==='object'?Object.fromEntries(Object.keys(value).sort().filter(k=>!['motion','motions','audio','sounds'].includes(k)).map(k=>[k,visual(value[k])])):value;
const digest=value=>crypto.createHash('sha256').update(JSON.stringify(value)).digest('hex');
assert.equal(digest(visual(bundle.project)),digest(visual(source)),"export visual contract must match UICreator source");
const svg=bundle.sourceSvg;
assert.equal(crypto.createHash('sha256').update(svg.replace(/r[a-f0-9]{32}_/g,'native_')).digest('hex'),bundle.sourceHash);
const nodes=new Map();walk(source.root,n=>nodes.set(n.id,n));
for(const id of ['event-panel','event-label','event-item-row','event-line-row','event-craft-panel','event-craft-drag','event-craft-scrim'])assert.ok(nodes.has(id));
for(let i=0;i<3;i++){
 const body=nodes.get('event-rows-'+i),button=nodes.get('event-select-'+i);
 assert.ok(body.slot.offsets.top+body.layout.height<=button.slot.offsets.top,'scroll body must not overlap its selection button');
}
let buttons=0;
for(const n of bundle.nodes)if(n.type==='Button'){
 buttons++;
 const normal=bundle.states.canvas.nodes.find(v=>v.id===n.id);
 const live=normal.visibility==='visible'?normal:bundle.authoringNodes.find(v=>v.id===n.id);
 for(const state of ['normal','hovered','pressed','disabled'])assert.ok(bundle.rasters[state==='normal'?live.raster:live.brushRasters[state]],`missing ${state} brush on ${n.id}`);
}
for(const [key,r] of Object.entries(bundle.rasters)){
 const png=await fs.readFile(path.join(root,'Content/UICreatorEvents/rasters',key+'.png'));
 assert.equal(png.subarray(0,8).toString('hex'),'89504e470d0a1a0a');
 assert.equal(png.readUInt32BE(16),r.width);assert.equal(png.readUInt32BE(20),r.height);
}
const proof={job,sourceHash:bundle.sourceHash,sourceValidation:true,templates:7,buttons,fourStateBrushes:true,rasters:Object.keys(bundle.rasters).length,scrollBodiesDoNotOverlapActions:true,deployedContractMatchesSource:true};
await fs.mkdir(path.join(root,'Saved/Verification/MapEvents'),{recursive:true});
await fs.writeFile(path.join(root,'Saved/Verification/MapEvents/authoring.json'),JSON.stringify(proof,null,2));
console.log(JSON.stringify(proof));
