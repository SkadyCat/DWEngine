import test from 'node:test';
import assert from 'node:assert/strict';
import { BattleSuiteState } from '../src/runtime/battle-suite-state.js';
import { GEAR } from '../src/assets/rulestudio-hud/suite-data.js';
test('character uses nine slots and equip replaces only the matching slot',()=>{
 assert.equal(GEAR.length,9);assert.equal(new Set(GEAR.map(v=>v[0])).size,9);
 const s=new BattleSuiteState();s.selected='gear-6';assert.ok(s.equip());assert.equal(s.items.find(i=>i.id==='gear-0').equipped,false);assert.equal(s.items.find(i=>i.id==='gear-6').equipped,true);assert.equal(s.items.find(i=>i.id==='gear-3').equipped,true);
 s.equip();assert.equal(s.item.equipped,false);s.selected='gear-7';s.item.unidentified=true;assert.equal(s.equip(),false);
});
test('search matches names and empty query restores the full inventory',()=>{
 const s=new BattleSuiteState();s.setField('characterQuery','霜棘');assert.equal(s.characterItems.length,1);s.setField('characterQuery','');assert.equal(s.characterItems.length,21);s.gearFilter='boots';assert.ok(s.characterItems.every(i=>i.slot==='boots'));
 s.setField('bagQuery','不存在的物品');assert.equal(s.bagItems.length,0);s.setField('bagQuery','');assert.equal(s.bagItems.length,s.items.length);
});
test('stack split preserves count and rejects zero, fractions and the whole stack',()=>{
 const s=new BattleSuiteState();s.selected='stone-low';const total=s.item.count,before=s.items.length;
 for(const n of ['0','-1','1.5',String(total)]){s.setField('splitAmount',n);assert.equal(s.split(),false);}
 s.setField('splitAmount','2');assert.ok(s.split());assert.equal(s.item.count+s.items.at(-1).count,total);assert.equal(s.items.length,before+1);
});
test('Hunter selected-only mode and currency filters have independent state',()=>{
 const s=new BattleSuiteState();s.setField('hunterQuery','火焰');assert.ok(s.hunterRows.every(v=>v.includes('火焰')));s.act('hunter-affix','火焰伤害 +n%');s.act('hunter-only');assert.deepEqual(s.hunterRows,['火焰伤害 +n%']);s.act('hunter-currency','stone-low');s.act('hunter-clear');assert.equal(s.affixes.size,0);assert.equal(s.currencies.size,1);
});
test('preview trades are single-use and retain configured prices',()=>{
 const s=new BattleSuiteState(),initial=s.items.length;s.act('buy','gear-0');s.act('buy','gear-0');assert.equal(s.wallet,972);assert.equal(s.items.length,initial+1);assert.equal(new BattleSuiteState().wallet,996);
 s.setField('price:gear-1','12');s.act('pricing-save');s.act('buy','gear-1');assert.equal(s.wallet,960);
});
test('craft selection limit and material exhaustion are enforced',()=>{
 const s=new BattleSuiteState();for(const i of [0,1,2,3])s.act('item-affix',i);assert.equal(s.targets.size,3);assert.ok(!s.targets.has(3));
 const item=s.item,before=item.affixes[0];s.act('item-affix',0);assert.notEqual(item.affixes[0],before);assert.equal(s.rolls,1);
 s.items.find(i=>i.id==='stone-low').count=0;const current=item.affixes[0];s.act('item-affix',0);assert.equal(item.affixes[0],current);
 s.act('craft-select','gear-7');s.item.unidentified=true;assert.equal(s.targets.size,0);s.act('craft-run');assert.equal(s.item.unidentified,false);
});
