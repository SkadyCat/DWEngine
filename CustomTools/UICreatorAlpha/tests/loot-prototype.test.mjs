import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import path from 'node:path';
import crypto from 'node:crypto';
import { pathToFileURL } from 'node:url';
import { LootPrototypeSession, visibleDrops, packLoot, lootStyle } from '../src/runtime/loot-prototype.js';
import { FloorRewards, emptyHunter, hunterAllows } from '../src/assets/loot-prototype/reference/engine/index.js';
import { createLootProject } from '../src/assets/loot-prototype/project.js';
import { validateDocument, walk } from '../src/core/model.js';
const reference=new URL('../src/assets/loot-prototype/reference/',import.meta.url);
const files=JSON.parse(await fs.readFile(new URL('rules.json',reference),'utf8'));
const samples=JSON.parse(await fs.readFile(new URL('samples.json',reference),'utf8'));
const create=options=>new LootPrototypeSession(files,samples,options);
const base=create();

test('original reward stream is unaffected by Hunter, and boss guarantees its two original loot types',()=>{
 const a=create(),b=create();a.hunter.enabled=true;a.hunter.targets=['never-match'];
 for(let i=0;i<20;i++){a.kill(i%4===0);b.kill(i%4===0);void a.visible;}
 assert.deepEqual(a.rewards.ground,b.rewards.ground);
 const start=a.remaining.length;assert.equal(a.kill(true),2);assert.equal(a.remaining.length,start+2);
 assert.deepEqual(new Set(a.remaining.slice(-2).map(d=>d.kind)),new Set(['equipment','item']));
});

test('pickup validates exact 200 cm radius, dead players, quantity caps and duplicate claims before mutation',()=>{
 const s=create(),drop=s.remaining.find(d=>d.kind==='item'&&d.count>1);
 const before=JSON.stringify(s.state);s.hero={x:drop.x+200.01,y:drop.y,dead:false};assert.ok(s.pickup(drop.key).error);assert.equal(JSON.stringify(s.state),before);
 s.hero={x:drop.x+200,y:drop.y,dead:true};assert.ok(s.pickup(drop.key).error);assert.equal(JSON.stringify(s.state),before);
 s.hero.dead=false;s.state.owned.item[drop.id]=9999;const cap=JSON.stringify(s.state);assert.ok(s.pickup(drop.key).error);assert.equal(JSON.stringify(s.state),cap);
 s.state.owned.item[drop.id]=0;assert.ok(s.pickup(drop.key).drop);assert.equal(s.state.owned.item[drop.id],drop.count);
 const after=JSON.stringify(s.state);assert.ok(s.pickup(drop.key).error);assert.equal(JSON.stringify(s.state),after);
});

test('click approaches and picks once; a movement key cancels the approach',()=>{
 const s=create(),d=s.remaining[0];s.hero={x:d.x+1000,y:d.y,dead:false};assert.equal(s.requestPickup(d.key),true);
 s.step(.05,{x:0,y:1});assert.equal(s.target,'');assert.equal(d.claimed,false);
 s.requestPickup(d.key);for(let i=0;i<300&&s.target;i++)s.step(1/60);
 assert.equal(d.claimed,true);assert.equal(s.claims.length,1);assert.ok(s.state.instances[d.id]);
});

test('Hunter retains original enabled default, recursive AND matching and selected currency semantics',()=>{
 assert.deepEqual(emptyHunter(),{visible:false,enabled:true,targets:[],currencies:[]});assert.equal(create({reviewAll:false}).visible.length,0);
 const d={kind:'equipment',item:{identified:true,bases:[{id:'base',traits:[{id:'outer',with:[{id:'inner'}]}]}],affixes:[{id:'cold'}]}};
 assert.equal(hunterAllows(d,{enabled:true,targets:['base','inner','cold']}),true);
 assert.equal(hunterAllows(d,{enabled:true,targets:['base','missing']}),false);
 assert.equal(hunterAllows({...d,item:{...d.item,identified:false}},{enabled:true,targets:['base']}),false);
 assert.equal(hunterAllows({kind:'item',id:'stone',category:'currency'},{enabled:true,currencies:['stone']}),true);
 assert.equal(hunterAllows({kind:'item',id:'potion',category:'consumable'},{enabled:true,currencies:['potion']}),false);
 assert.equal(visibleDrops([{key:'own',kind:'item',origin:'inventory',x:0,y:0}],{x:0,y:0},{enabled:true}).length,1);
});

test('nearest 96 and packed rectangles stay inside the viewport without overlap',()=>{
 const drops=Array.from({length:180},(_,i)=>({...samples[i%samples.length],key:'stress-'+i,x:i,y:0}));
 const visible=visibleDrops(drops,{x:0,y:0},{enabled:false});assert.equal(visible.length,96);assert.equal(visible.at(-1).key,'stress-95');
 const packed=packLoot(visible,()=>({x:800,y:450}),t=>t.length*8);assert.ok(packed.length>30);
 for(let i=0;i<packed.length;i++){const a=packed[i];assert.ok(a.x>=4&&a.y>=4&&a.x+a.w<=1596&&a.y+a.h<=896);for(const b of packed.slice(0,i))assert.ok(!(a.x<b.x+b.w+3&&a.x+a.w+3>b.x&&a.y<b.y+b.h+3&&a.y+a.h+3>b.y));}
});

test('packing agrees with the original ground-loot overlay for the same measured controls',async()=>{
 const {GroundLootOverlay}=await import('../src/assets/loot-prototype/reference/public/ground-loot.js');
 const previous=globalThis.document;
 globalThis.document={createElement(){return{dataset:{},style:{setProperty(){}},classList:{add(){},toggle(){}},setAttribute(){},addEventListener(){},remove(){},get offsetWidth(){return Math.min(260,Math.max(70,this.textContent.length*8+(this.dataset.lootFamily?46:20)));},offsetHeight:26};}};
 try{
  const root={append(){}},overlay=new GroundLootOverlay(root,()=>{}),rows=Array.from({length:120},(_,i)=>({...samples[i%samples.length],key:'pack-'+i,x:i%7*5,y:i%5*7})),hero={x:30,y:30};
  overlay.update({width:1600,height:900,lootAnchor:key=>rows.find(d=>d.key===key),project:()=>({x:800,y:450})},rows,hero);
  const packed=packLoot(visibleDrops(rows,hero,{enabled:false}),()=>({x:800,y:450}),t=>t.length*8);
  const original=[...overlay.labels].filter(([,label])=>!label.hidden);assert.equal(packed.length,original.length);
  for(const r of packed){const actual=overlay.labels.get(r.key);assert.ok(actual&&!actual.hidden);assert.equal(actual.style.transform,`translate3d(${r.x.toFixed(1)}px,${r.y.toFixed(1)}px,0)`);}
 }finally{globalThis.document=previous;}
});

test('grade marks and original landing trajectory are used before showing a new label',()=>{
 const s=create();for(const grade of[1,2,3]){const d=s.remaining.find(d=>d.currencyGrade===grade);assert.equal(lootStyle(d).mark,['','Ⅰ','Ⅱ','Ⅲ'][grade]);}
 s.kill(true);const drop=s.remaining.at(-1);assert.equal(s.presentation(drop).landed,false);
 for(let i=0;i<80;i++)s.step(1/60);assert.equal(s.presentation(drop).landed,true);
 const at=s.presentation(drop);assert.ok(Math.abs(at.x-drop.x*.01)<1e-9&&Math.abs(at.z+drop.y*.01)<1e-9);
});

test('UICreator owns editable SVG controls, separate text, states and native action bindings',()=>{
 const p=createLootProject();assert.deepEqual(validateDocument(p),[]);assert.equal(Object.keys(p.prefabs).length,13);
 for(const a of Object.values(p.assets))assert.doesNotMatch(a.source,/<(?:image|foreignObject|text)\b/i);
 for(const f of['common','magic','rare','legendary','currency-1','currency-2','currency-3','currency-special','omen']){let button;walk(p.prefabs['loot-prefab-'+f].root,n=>{if(n.binding==='loot:pick')button=n;});assert.equal(button.action.op,'pickup');assert.ok(button.brushes.hovered&&button.brushes.pressed);}
});

const manifest=JSON.parse(await fs.readFile(new URL('sources.json',reference),'utf8'));
const sourceExists=await fs.access(manifest.source).then(()=>true,()=>false);
test('snapshot reward output matches current RuleStudio source for identical seeds and kill sequences',{skip:!sourceExists},async()=>{
 const {FloorRewards:Original}=await import(pathToFileURL(path.join(manifest.source,'engine/floor-rewards.mjs')));
 for(const seed of[1,7,7919,0x12345678]){
  const a=new Original(base.collections,base.library,base.catalog,{seed,maxTier:1}),b=new FloorRewards(base.collections,base.library,base.catalog,{seed,maxTier:1});
  for(let i=0;i<80;i++){const mon={x:i*47,y:-i*31,boss:i%7===0};a.onKill(mon);b.onKill(mon);assert.deepEqual(a.ground,b.ground);}
 }
});
test('captured modules are intact and all five drop-related modules still match RuleStudio',{skip:!sourceExists},async()=>{
 const current=new Set(['engine/floor-rewards.mjs','engine/loot-hunter.mjs','public/ground-loot.mjs','public/prefabs/loot-style.mjs','public/prefabs/loot-markers.mjs']);
 for(const file of manifest.modules){const snapshot=await fs.readFile(new URL(file.target,reference),'utf8');assert.equal(crypto.createHash('sha256').update(snapshot).digest('hex'),file.snapshotSha256,file.target);if(current.has(file.source)){const source=await fs.readFile(path.join(manifest.source,file.source),'utf8');assert.equal(crypto.createHash('sha256').update(source).digest('hex'),file.sha256,file.source);let expected=source.replace(/((?:from\s*|import\s*)['"][^'"]+)\.mjs(['"])/g,'$1.js$2');if(file.source.endsWith('loot-markers.mjs'))expected+='\n// Authoring-only access to the unchanged original trajectory.\nexport { seedOf, trajectory };\n';assert.equal(snapshot,expected);}}
});
