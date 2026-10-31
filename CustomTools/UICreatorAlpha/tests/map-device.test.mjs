import test from 'node:test';
import assert from 'node:assert/strict';
import { MapDeviceState } from '../src/runtime/map-device-state.js';
import { MAP_DEVICE_SLOTS, buildMapDevicePage } from '../src/assets/rulestudio-hud/map-device.js';
import { walk } from '../src/core/model.js';
const items=[{key:'map',category:'map',count:1},{key:'stack',mapDeviceRole:'modifier',count:3},{key:'gear',kind:'equipment',count:1}];
test('one map and five modifier sockets, no fabricated rewards',()=>{
  assert.equal(MAP_DEVICE_SLOTS.filter(s=>s.role==='map').length,1);assert.equal(MAP_DEVICE_SLOTS.filter(s=>s.role==='modifier').length,5);
  const root=buildMapDevicePage(),sockets=[];walk(root,n=>{if(n.binding.startsWith('map-device-slot:'))sockets.push(n);});assert.equal(sockets.length,6);assert.equal(root.source.mapDevice.consumeOnPlace,false);
});
test('map device preserves the real engraved atlas instead of the old card fixture',()=>{
  const page=buildMapDevicePage(),nodes=[];walk(page,n=>nodes.push(n));
  const plot=nodes.find(n=>n.binding==='atlas-plot');
  assert.ok(plot.source.plotSurface.glyphs.length===4);
  assert.equal(nodes.some(n=>n.id.includes('battle-atlas-node-')),false);
  assert.equal(nodes.find(n=>n.binding==='atlas-viewport').layout.width,838);
  assert.equal(page.source.backdrop,'#03070d73');
  const detail=nodes.find(n=>n.id.endsWith('map-device-sockets'));
  const bag=nodes.find(n=>n.id.endsWith('map-device-backpack'));
  assert.equal(detail.slot.offsets.left,852);assert.equal(bag.slot.offsets.left,1132);assert.equal(bag.slot.offsets.top+bag.layout.height,page.layout.height);
});
test('drag reservations preserve owned quantities; return, replacement and invalid drops',()=>{
  const input=structuredClone(items),s=new MapDeviceState(input);
  assert.equal(s.place('gear','map'),false);assert.equal(s.place('map','modifier-0'),false);
  assert.equal(s.place('map','map'),true);assert.equal(s.available('map'),0);assert.equal(s.place('map','map'),false);
  assert.equal(s.place('stack','modifier-0'),true);assert.equal(s.place('stack','modifier-1'),true);assert.equal(s.place('stack','modifier-2'),true);assert.equal(s.place('stack','modifier-3'),false);
  assert.equal(s.place('stack','modifier-4','modifier-0'),true);assert.equal(s.available('stack'),0);assert.equal(s.item('modifier-0'),undefined);
  assert.equal(s.remove('map'),true);assert.equal(s.available('map'),1);assert.deepEqual(input,items);
});
test('stale inventory and forged source cannot duplicate a reserved item',()=>{
  const s=new MapDeviceState(items);assert.equal(s.place('map','map','modifier-2'),false);s.place('stack','modifier-0');s.place('stack','modifier-1');s.update([{...items[1],count:1}]);assert.equal(Object.keys(s.slots).length,1);assert.equal(s.available('stack'),0);
});
