import test from 'node:test';
import assert from 'node:assert/strict';
import { createDocument,createNode,validateDocument } from '../src/core/model.js';
import { createEdgeMotionAsset,validateMotionAsset,sampleMotion,motionDefinition } from '../src/core/motion.js';
import { expandDocument } from '../src/core/prefabs.js';
import { computeLayout } from '../src/core/layout.js';
import { exportWbpContract,importTree } from '../src/io/adapters.js';

test('edge overlay has a reusable timed phase and explicit stop; it preserves slot geometry and roundtrips settings',()=>{
 const asset=createEdgeMotionAsset();assert.deepEqual(validateMotionAsset(asset),[]);
 assert.equal(sampleMotion(asset.clips.orbit,650).rotation,90);assert.equal(sampleMotion(asset.clips.orbit,3250).rotation,90);
 assert.equal(sampleMotion(asset.clips.off,1).opacity,0);
 const doc=createDocument();doc.motions[asset.id]=asset;
 const button=createNode('Button',{id:'item',slot:{offsets:{left:80,top:50,right:52,bottom:52}},children:[createNode('CanvasPanel',{id:'content',children:[createNode('CanvasPanel',{id:'edge',binding:'row:new',motion:{assetId:asset.id,enabled:true,dismissOnHover:true,tint:'#7ca8e9'},slot:{anchors:{minX:0,minY:0,maxX:1,maxY:1},offsets:{left:0,top:0,right:0,bottom:0}}})]})]});
 doc.root.children=[button];assert.deepEqual(validateDocument(doc),[]);
 const expanded=expandDocument(doc),layout=computeLayout(expanded.document),before=structuredClone(button.slot);
 const out=importTree(exportWbpContract(doc,expanded.document,layout)).document;
 assert.deepEqual(out.motions[asset.id],asset);assert.deepEqual(out.root.children[0].children[0].children[0].motion,button.children[0].children[0].motion);
 assert.deepEqual(button.slot,before);
 assert.equal(motionDefinition({...button.children[0].children[0],motion:{assetId:asset.id,enabled:false}},doc.motions),null);
});
test('edge authoring rejects unusable geometry and colors rather than silently changing the effect',()=>{
 for(const patch of [{width:0},{width:17},{radius:-1},{radius:257},{color:'red'}]){
  const asset=createEdgeMotionAsset();Object.assign(asset.edge,patch);assert.ok(validateMotionAsset(asset).length,JSON.stringify(patch));
 }
});
