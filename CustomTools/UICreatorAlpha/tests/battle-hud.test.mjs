import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { createHudProject, HUD_ASSETS } from '../src/assets/hud-darksteel/project.js';
import { installBattleHud } from '../src/assets/rulestudio-hud/project.js';
import { battleHudSources } from '../scripts/rulestudio-hud-art.mjs';
import { validateDocument } from '../src/core/model.js';
import { expandDocument } from '../src/core/prefabs.js';
import { computeLayout } from '../src/core/layout.js';
import { BattleHudState } from '../src/runtime/battle-hud.js';
const original = Object.fromEntries(await Promise.all(HUD_ASSETS.map(async a => [a.file, await fs.readFile(new URL(`../src/assets/hud-darksteel/svg/${a.file}`, import.meta.url), 'utf8')])));
const project = installBattleHud(createHudProject(original), battleHudSources());
test('full battle HUD persists independent SVGs and editable nested windows', () => {
  const before = JSON.stringify(project);
  assert.deepEqual(validateDocument(project), []);
  assert.ok(Object.keys(project.assets).length >= 100);assert.ok(Object.keys(project.prefabs).length >= 50);
  for(const kind of ['character','inventory','loadout','hunter','item','unidentified','context','split','skill-detail','affix','material','affix-book','completion','completion-empty','death','timeout','atlas','shop','pricing','craft','builds','map-info','summons','loading'])assert.ok(project.interfaceCatalog.some(p=>p.id===kind),kind);
  for (const asset of Object.values(project.assets)) {
    assert.ok(!/<(?:foreignObject|text)\b/.test(asset.source), asset.name);
    // Masked equipment preserves the original 256px artwork; it is embedded
    // locally, while frames, labels and all other UI assets remain vector nodes.
    if (/<image\b/.test(asset.source)) {
      assert.ok(asset.name.startsWith('equipment-art-'),asset.name);
      assert.match(asset.source, /href="data:image\/png;base64,[A-Za-z0-9+/=]+"/);
      assert.ok(!/href="(?:https?:|file:|\/)/.test(asset.source),asset.name);
    }
  }
  const expanded = expandDocument(project); assert.ok(![...expanded.origins.values()].some(o=>o.prefabId?.startsWith('battle-prefab-')),'closed windows are not expanded into the HUD');
  const layout = computeLayout(expanded.document), visible = [...layout.entries.values()].filter(e => e.visible);
  for (const id of ['rs-resources', 'rs-objective', 'rs-summons', 'rs-statistics', 'rs-dps', 'rs-map', 'rs-rail', 'rs-hotbar']) assert.ok(visible.some(e => e.node.id.endsWith(id)), id);
  assert.ok(!visible.some(e => e.node.id.endsWith('hud-context')));
  assert.equal(visible.filter(e => e.node.id.endsWith('hud-ability-selected')).length, 1);
  assert.ok(visible.find(e => e.node.id.endsWith('hud-ability-selected')).node.id.includes('hud-ability-instance-1::'));
  assert.equal(JSON.stringify(project), before, 'prefab expansion must not mutate source action metadata');
  assert.deepEqual(visible.filter(e => e.node.binding === 'ability').map(e => e.node.action.value), [0, 1, 2, 3]);
});
test('HUD anchors keep resources, maps, statistics and hotbar inside desktop viewports', () => {
  const expanded = expandDocument(project).document;
  for (const [width, height] of [[1024, 720], [1280, 720], [1600, 900], [2048, 780]]) {
    const layout = computeLayout(expanded, { width, height }), entries = [...layout.entries.values()];
    const find = suffix => entries.find(e => e.node.id.endsWith(suffix));
    const hotbar = find('rs-hotbar'), rail = find('rs-rail'), resources = find('rs-resources'), stats = find('rs-statistics'), dps = find('rs-dps');
    assert.equal(hotbar.globalWidth, 606); assert.equal(hotbar.globalHeight, 74);
    assert.equal(hotbar.globalX + hotbar.globalWidth / 2, width / 2); assert.equal(hotbar.globalY + hotbar.globalHeight, height - 28);
    assert.equal(resources.globalX, 14); assert.equal(rail.globalX + rail.globalWidth, width - 14);
    assert.ok(stats.globalX >= resources.globalX + resources.globalWidth + 12); assert.ok(stats.globalX + stats.globalWidth <= dps.globalX - 12 + .001);
  }
});
test('click toggles the skill picker while digit keys only select a stable slot', () => {
  const s = new BattleHudState(); assert.equal(s.selected, 0);
  s.activate({ type: 'ability', value: 3 }); assert.equal(s.selected, 3); assert.equal(s.picker, 3); assert.equal(s.skills[3], null);
  s.activate({ type: 'ability', value: 3 }); assert.equal(s.picker, -1); assert.equal(s.selected, 3);
  s.key('Digit2'); assert.equal(s.selected, 1); assert.equal(s.picker, -1); assert.deepEqual(s.skills, ['flame_burst', null, null, null]);
});
test('assigning an existing skill moves it and clearing a slot does not shift other skills', () => {
  const s = new BattleHudState(); s.select(2, true); assert.ok(s.assign('flame_burst'));
  assert.deepEqual(s.skills, [null, null, 'flame_burst', null]); assert.equal(s.selected, 2);
  assert.equal(s.choices.find(c => c.id === 'flame_burst').granted, true);
  s.select(1, true); s.assign('frost_lance'); s.select(2, true); s.assign(null);
  assert.deepEqual(s.skills, [null, 'frost_lance', null, null]);
  s.select(0, true); assert.equal(s.assign('unknown'), false); assert.equal(s.picker, 0);
});
test('panels and map tabs are independent of current skill and empty belt slots consume nothing', () => {
  const s = new BattleHudState(); s.key('Digit4'); s.key('KeyB'); assert.equal(s.panel, 'inventory'); assert.equal(s.selected, 3);
  assert.ok(s.key('Escape')); assert.equal(s.panel, ''); assert.equal(s.selected, 3);
  s.open('lineage'); assert.equal(s.mapTab, 'lineage'); assert.equal(s.selected, 3);
  s.key('KeyQ'); assert.equal(s.beltCount, 12); assert.match(s.message, /为空/);
  s.activate({ type: 'equip-belt' }); s.key('KeyQ'); assert.equal(s.beltCount, 11);
});
test('picker only shows available skills and distinguishes current, moved and unassigned choices', () => {
  const s = new BattleHudState({ skills: project.presentation.skills, availableSkills: ['poison_flask'], loadout: ['poison_flask', null, null, null] });
  s.select(0, true); assert.equal(s.choiceRows().length, 1); assert.equal(s.choiceRows()[0].caption, '装备授予 · 当前槽位'); assert.equal(s.choiceRows()[0].current, true);
  s.select(2, true); assert.equal(s.choiceRows()[0].caption, '装备授予 · 从槽 1 移入'); assert.equal(s.choiceRows()[0].current, false);
  assert.equal(s.assign('frost_lance'), false, 'catalog entries do not imply ownership');
  s.assign('poison_flask'); assert.deepEqual(s.skills, [null, null, 'poison_flask', null]);
  s.select(2, true); assert.equal(s.choiceRows()[0].caption, '装备授予 · 当前槽位');
  s.assign(null); s.select(2, true); assert.equal(s.choiceRows()[0].caption, '装备授予'); assert.equal(s.assign(null), false);
  s.setAvailability([]); assert.deepEqual(s.choiceRows(), []); assert.equal(s.assign('poison_flask'), false);
});
