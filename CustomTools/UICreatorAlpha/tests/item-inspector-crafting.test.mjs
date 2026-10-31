import test from 'node:test';
import assert from 'node:assert/strict';
import { InspectorCraftingPreview } from '../src/runtime/inspector-crafting.js';
import { itemTooltipTemplate } from '../src/assets/rulestudio-hud/item-tooltip.js';
import { walk } from '../src/core/model.js';
test('the authored inspector removes page-navigation actions and owns inline controls',()=>{
 const actions=[];walk(itemTooltipTemplate().root,n=>{if(n.action)actions.push(n.action.op);});
 for(const op of ['hover-currency','hover-omen','hover-affix','hover-whole','hover-identify','hover-equip'])assert.ok(actions.includes(op));
 assert.ok(!actions.includes('hover-details'));assert.ok(!actions.includes('hover-craft'));
});
test('the authoring interaction accepts materials in place and never spends on activation',()=>{
 const item={affixes:['A','B','C']},low={id:'stone-low',name:'下品灵石',count:3},mid={id:'stone-mid',name:'中品灵石',count:2},high={id:'stone-high',name:'上品灵石',count:1},omen={id:'omen_fire',name:'元素预兆',count:1};
 const p=new InspectorCraftingPreview({state:{items:[item,low,mid,high,omen]}});p.activate({op:'hover-currency',value:low.id},item);assert.equal(low.count,3);assert.deepEqual(p.view(item).eligible,[0,1,2]);
 p.activate({op:'hover-affix',value:'2'},item);assert.equal(low.count,2);assert.deepEqual(item.reforgeTargets,[2]);assert.deepEqual(p.view(item).eligible,[2]);
 p.activate({op:'hover-currency',value:mid.id},item);assert.deepEqual(p.view(item).eligible,[0,1]);p.activate({op:'hover-affix',value:'0'},item);assert.deepEqual(item.reforgeTargets,[2,0]);
 p.activate({op:'hover-currency',value:high.id},item);p.activate({op:'hover-affix',value:'1'},item);assert.deepEqual(item.reforgeTargets,[2,0,1]);assert.equal(high.count,0);assert.equal(p.view(item).currencyId,'');
 p.activate({op:'hover-omen',value:omen.id},item);assert.equal(omen.count,1);p.activate({op:'hover-omen',value:omen.id},item);assert.equal(p.omenId,'');
});
