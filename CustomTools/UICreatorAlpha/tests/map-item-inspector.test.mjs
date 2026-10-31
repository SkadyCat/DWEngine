import test from 'node:test';
import assert from 'node:assert/strict';
import { itemTooltipView } from '../src/assets/rulestudio-hud/item-tooltip.js';
import { walk } from '../src/core/model.js';

test('地图物品复用 UICreator 详情模板，显示真实词缀和使用动作', () => {
 const item={id:'map_item_7',kind:'item',category:'map',rarity:'rare',tier:16,count:1,name:'地图 T16 · 稀有',icon:'scroll',description:'使用消耗 1 张，进入第 16 层。',affixes:['怪物群 +24%','地图召唤物气血 +38%','地图召唤物伤害 +15%']};
 for(const pinned of [false,true]){
  const view=itemTooltipView({prefabs:{},assets:{}},item,pinned),nodes=new Map();walk(view.root,n=>nodes.set(n.id,n));
  assert.equal(nodes.get('battle-item-hover-actions').visibility,pinned?'visible':'collapsed');
  assert.equal(nodes.get('battle-item-hover-equip').action.op,'hover-use-map');
  assert.equal(nodes.get('battle-item-hover-equip').action.value,item.id);
  assert.equal(nodes.get('battle-item-hover-equip-label').text,'使用地图');
  assert.equal(nodes.get('battle-item-hover-crafting').visibility,'collapsed');
  assert.match(nodes.get('battle-item-hover-meta').text,/地图物品 · T16/);
  assert.equal(nodes.get('battle-item-hover-map-affix-1').text,item.affixes[1]);
 }
});

test('地图装配提示显示真实属性与图标，固定查看不进入加工预览事务',()=>{
 const item={id:'equipment:owned-7',kind:'equipment',rarity:'rare',tier:10,count:1,name:'测试护手',iconSrc:'blob:owned-icon',description:'装备说明',baseLines:['基础护甲 +93'],fixedLines:['命中恢复气血 +4'],affixes:[{text:'攻击速度 +18%',tier:8}]};
 const view=itemTooltipView({prefabs:{},assets:{}},item,true,undefined,{readOnly:true}),nodes=new Map();walk(view.root,n=>nodes.set(n.id,n));
 assert.equal(nodes.get('battle-item-hover-icon').type,'Image');assert.equal(nodes.get('battle-item-hover-icon').src,item.iconSrc);
 assert.equal(nodes.get('battle-item-hover-base-0').text,'基础护甲 +93');assert.equal(nodes.get('battle-item-hover-fixed-0').text,'命中恢复气血 +4');assert.equal(nodes.get('battle-item-hover-affix-0-text').text,'攻击速度 +18%');assert.equal(nodes.get('battle-item-hover-affix-0-tier').text,'T8');
 assert.equal(nodes.get('battle-item-hover-crafting').visibility,'collapsed');assert.equal(nodes.get('battle-item-hover-actions').visibility,'collapsed');assert.equal(nodes.get('battle-item-hover-close').visibility,'visible');
});
