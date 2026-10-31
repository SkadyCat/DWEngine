import fs from 'node:fs/promises';
import path from 'node:path';
import crypto from 'node:crypto';
import assert from 'node:assert/strict';
import {presets} from '../../CustomTools/RuleStudio/public/vfx/catalog.mjs';
const root=path.resolve(import.meta.dirname,'../..'),source=path.join(root,'Saved/WebVFX/Source');
const manifest=JSON.parse(await fs.readFile(path.join(source,'manifest.json'),'utf8'));
const hash=b=>crypto.createHash('sha256').update(b).digest('hex').slice(0,20);
assert.equal(manifest.poseStride,20);assert.equal(manifest.units,'centimeters');
const seen=new Set(),geometry=new Set(manifest.geometries.map(g=>g.id)),materials=new Set(manifest.materials.map(m=>m.id));
for(const fingerprint of manifest.sources)assert.equal(hash(await fs.readFile(path.join(root,fingerprint.file))),fingerprint.sha256,fingerprint.file+' changed after export');
let poses=0,bytes=0;
for(const clip of manifest.clips){
 assert.ok(!seen.has(clip.id));seen.add(clip.id);
 const b=await fs.readFile(path.join(source,'Clips',clip.file));assert.equal(hash(b),clip.sha256);bytes+=b.length;
 assert.equal(b.readUInt32LE(0),0x58465744);assert.equal(b.readUInt32LE(4),1);assert.equal(b.readUInt32LE(8),clip.frames);
 assert.ok(clip.frames>=2&&clip.duration>0&&clip.fps===60);let at=20;
 for(let frame=0;frame<clip.frames;frame++){
  assert.ok(at+4<=b.length);const n=b.readUInt32LE(at);at+=4;assert.ok(n<=clip.parts.length);
  for(let row=0;row<n;row++){
   const part=b.readUInt32LE(at),count=b.readUInt32LE(at+4);at+=8;assert.ok(part<clip.parts.length&&count<=16384);poses+=count;
   for(let i=0;i<count*20;i++){assert.ok(at+4<=b.length);assert.ok(Number.isFinite(b.readFloatLE(at)));at+=4;}
  }
 }
 assert.equal(at,b.length,clip.id+' binary boundary');
 for(const p of clip.parts){assert.ok(geometry.has(p.geometry));assert.ok(materials.has(p.material));}
}
assert.deepEqual([...seen].filter(id=>!manifest.clips.find(c=>c.id===id).internal).sort(),presets.map(p=>p.id).sort());
for(const id of ['whirlwind','ground-mirror','ground-shock','projectile-treasury-weapon','royal-treasury-gate'])assert.ok(seen.has(id));
for(const type of ['royal-treasury-gate','projectile-treasury-weapon'])for(let variant=0;variant<3;variant++)assert.ok(seen.has(type+'@style'+variant));
const proof={ok:true,publicPrefabs:presets.length,clips:manifest.clips.length,geometries:geometry.size,materials:materials.size,sourceFingerprints:manifest.sources.length,poses,bytes};
await fs.mkdir(path.join(root,'Saved/Verification/UEIntegration'),{recursive:true});await fs.writeFile(path.join(root,'Saved/Verification/UEIntegration/vfx-export-check.json'),JSON.stringify(proof,null,2));
console.log(JSON.stringify(proof));
