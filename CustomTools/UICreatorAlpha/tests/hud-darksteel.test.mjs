import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { HUD_ASSETS, HUD_PREFABS, createHudProject } from '../src/assets/hud-darksteel/project.js';
import { validateDocument, walk } from '../src/core/model.js';
import { expandDocument } from '../src/core/prefabs.js';
import { computeLayout } from '../src/core/layout.js';

const sources = Object.fromEntries(await Promise.all(HUD_ASSETS.map(async a => [a.file, await fs.readFile(new URL(`../src/assets/hud-darksteel/svg/${a.file}`, import.meta.url), 'utf8')])));
test('HUD contains reusable vector-only assets, six valid nested prefabs and eight independent hit regions', () => {
  const project = createHudProject(sources); assert.deepEqual(validateDocument(project), []);
  assert.equal(Object.keys(project.assets).length, 20); assert.equal(Object.keys(project.prefabs).length, 6);
  const expanded = expandDocument({ ...project, root: project.prefabs[HUD_PREFABS.bar].root, viewport: { width: 688, height: 170 } }).document;
  const layout = computeLayout(expanded), buttons = [...layout.entries.values()].filter(e => e.node.type === 'Button');
  assert.equal(buttons.length, 8);
  const expected = [[42, 75, 74, 74], [122, 75, 74, 74], [202, 75, 74, 74], [282, 75, 74, 74], [374, 83, 64, 64], [444, 83, 64, 64], [514, 83, 64, 64], [584, 83, 64, 64]];
  assert.deepEqual(buttons.map(e => [e.globalX, e.globalY, e.globalWidth, e.globalHeight]), expected);
  for (let i = 0; i < buttons.length; i++) for (let j = i + 1; j < buttons.length; j++) {
    const a = buttons[i], b = buttons[j]; assert.ok(a.globalX + a.globalWidth <= b.globalX || b.globalX + b.globalWidth <= a.globalX || a.globalY + a.globalHeight <= b.globalY || b.globalY + b.globalHeight <= a.globalY);
  }
  for (const asset of Object.values(project.assets)) assert.ok(!/<(?:image|foreignObject|text)\b/i.test(asset.source), `${asset.name} should not bake runtime text or raster art`);
  for (const e of buttons) assert.equal(new Set(Object.values(e.node.brushes)).size, 4);
});
test('HUD reproduces one equipped ability, three empty abilities and the fourth selection independently', () => {
  const project = createHudProject(sources), source = { ...project, root: project.prefabs[HUD_PREFABS.bar].root, viewport: { width: 688, height: 170 } }, expanded = expandDocument(source).document;
  const l = computeLayout(expanded), visible = [...l.entries.values()].filter(e => e.visible);
  assert.equal(visible.filter(e => e.node.type === 'TextBlock' && e.node.text === '烈焰爆发').length, 1);
  assert.equal(visible.filter(e => e.node.type === 'TextBlock' && e.node.text === '空槽').length, 3);
  const selected = visible.filter(e => e.node.id.endsWith('hud-ability-selected')); assert.equal(selected.length, 1); assert.ok(selected[0].node.id.includes('hud-ability-instance-4::'));
  assert.equal(visible.filter(e => e.node.svgAsset === 'hud-svg-plus').length, 3);
  assert.equal(visible.filter(e => e.node.svgAsset === 'hud-svg-flame-glyph').length, 1);
});
test('HUD bottom center anchoring keeps its native aspect ratio across multiple viewports', () => {
  const project = createHudProject(sources), expanded = expandDocument(project).document;
  for (const [width, height] of [[1600, 900], [2048, 780], [1280, 720]]) {
    const layout = computeLayout(expanded, { width, height }), bar = [...layout.entries.values()].find(e => e.node.id.endsWith('hud-bar-root'));
    assert.ok(Math.abs(bar.globalX + bar.globalWidth / 2 - width / 2) < .01);
    assert.ok(Math.abs(bar.globalY + bar.globalHeight - height + 20) < .01);
    assert.ok(Math.abs(bar.globalWidth / bar.globalHeight - 688 / 170) < .0001);
  }
});
