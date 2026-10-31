import test from 'node:test';
import assert from 'node:assert/strict';
import {UIRuntime} from '../src/runtime/runtime.js';

test('runtime keeps dynamic accessible names through subsequent countdown paints',()=>{
  const fake={nodes:new Map([['offer',{}],['timer',{}]]),patches:new Map(),render(){this.paints=(this.paints||0)+1;}};
  UIRuntime.prototype.setMany.call(fake,[['offer',{name:'购买 地图 T1',action:{op:'replace'},children:[] }]]);
  assert.deepEqual(fake.patches.get('offer'),{name:'购买 地图 T1'});
  UIRuntime.prototype.setMany.call(fake,[['timer',{text:'下次刷新 29:59'}]]);
  assert.equal(fake.patches.get('offer').name,'购买 地图 T1');assert.equal(fake.paints,2);
});
