import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { createHudProject, HUD_ASSETS } from '../src/assets/hud-darksteel/project.js';
import { installBattleHud } from '../src/assets/rulestudio-hud/project.js';
import { installMapCoreSelector, MAP_CORE_CATALOG, MAP_CORE_PICKER_PREFAB } from '../src/assets/rulestudio-hud/map-core-selector.js';
import { battleHudSources } from '../scripts/rulestudio-hud-art.mjs';
import { validateDocument, walk } from '../src/core/model.js';
import { expandDocument } from '../src/core/prefabs.js';
import { computeLayout } from '../src/core/layout.js';
import { BattleHudState } from '../src/runtime/battle-hud.js';
import { projectMapCores } from '../src/runtime/map-core-selector.js';

const art = Object.fromEntries(await Promise.all(HUD_ASSETS.map(async a => [a.file, await fs.readFile(new URL(`../src/assets/hud-darksteel/svg/${a.file}`, import.meta.url), 'utf8')])));
const source = installBattleHud(createHudProject(art), battleHudSources());

test('full HUD contains the equipped item and ten distinct core choices with independent text', () => {
  assert.deepEqual(validateDocument(source), []);
  const expanded = expandDocument(source).document;
  const nodes = []; walk(expanded.root, node => nodes.push(node));
  assert.equal(nodes.filter(node => node.binding === 'map-core-equipped').length, 1);
  assert.equal(nodes.filter(node => /^map-core-choice-\d+$/.test(node.binding)).length, 10);
  assert.equal(nodes.filter(node => node.id.endsWith('::rs-map-core')).length, 0, 'remove the obsolete plain-text core label');
  assert.deepEqual(MAP_CORE_CATALOG.map(core => core.packs), Array.from({ length: 10 }, (_, i) => i * 100));
  assert.ok(source.prefabs[MAP_CORE_PICKER_PREFAB]);
});

test('core preview opens, equips by item ID, refreshes its values, and retains the choice on reset', () => {
  const state = new BattleHudState(source.presentation);
  assert.equal(state.mapCore.tier, 1); assert.equal(state.mapPicker, false);
  state.activate({ type: 'map-core', op: 'open' }); assert.equal(state.mapPicker, true);
  const id = MAP_CORE_CATALOG[9].id;
  assert.equal(state.activate({ type: 'map-core', op: 'equip', value: id }), true);
  assert.equal(state.mapPicker, false); assert.equal(state.mapCore.tier, 10);
  const patches = new Map(); projectMapCores({ project: source, height: 900 }, state, (key, value) => patches.set(key, value));
  assert.equal(patches.get('map-core-equipped:name').text, '地图核心 T10');
  assert.equal(patches.get('map-core-affix:0').text, '怪物群 +900%');
  assert.equal(patches.get('map-core-choice-9:selected').text, '已装配');
  assert.equal(patches.get('map-core-choice-0:selected').text, '');
  state.activate({ type: 'restart' }); assert.equal(state.mapCoreId, id);
  state.activate({ type: 'map-core', op: 'open' }); assert.equal(state.key('Escape'), true); assert.equal(state.mapPicker, false);
  assert.equal(state.activate({ type: 'map-core', op: 'equip', value: 'missing' }), false);
  assert.equal(state.mapCoreId, id);
});

test('the left popover stays inside all supported desktop viewport sizes', () => {
  for (const [width, height] of [[1024,720], [1280,720], [1600,900], [2048,780]]) {
    const project = structuredClone(source), state = new BattleHudState(project.presentation);
    state.mapPicker = true;
    const patches = new Map(); projectMapCores({ project, height }, state, (key, value) => patches.set(key, value));
    const popup = project.prefabs['hud-prefab-screen'].root.children.find(node => node.binding === 'map-core-picker');
    popup.visibility = 'visible'; popup.slot.offsets.top = patches.get('map-core-picker').slot.offsets.top;
    const layout = computeLayout(expandDocument(project).document, { width, height });
    const entries = [...layout.entries.values()], p = entries.find(entry => entry.node.binding === 'map-core-picker');
    const equipped = entries.find(entry => entry.node.binding === 'map-core-equipped');
    assert.ok(p.globalX >= 0 && p.globalY >= 0);
    assert.ok(p.globalX + p.globalWidth < equipped.globalX);
    assert.ok(p.globalX + p.globalWidth <= width && p.globalY + p.globalHeight <= height);
    assert.equal(entries.filter(entry => entry.visible && /^map-core-choice-\d+$/.test(entry.node.binding)).length, 10);
  }
});

test('repeated synchronization preserves unrelated HUD edits and cannot duplicate the popup', () => {
  const project = structuredClone(source);
  project.prefabs['rs-prefab-dps'].name = 'User-authored DPS name';
  installMapCoreSelector(project); installMapCoreSelector(project);
  assert.equal(project.prefabs['rs-prefab-dps'].name, 'User-authored DPS name');
  assert.equal(project.prefabs['hud-prefab-screen'].root.children.filter(node => node.binding === 'map-core-picker').length, 1);
  assert.deepEqual(validateDocument(project), []);
});
