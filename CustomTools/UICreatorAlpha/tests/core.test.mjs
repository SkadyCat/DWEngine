import test from 'node:test';
import assert from 'node:assert/strict';
import { createDocument, createNode as node, validateDocument, clone, insets, walk } from '../src/core/model.js';
import { computeLayout, resolveCanvasSlot, slotForRect } from '../src/core/layout.js';
import { expandDocument, detachInstance, createPrefabFromNode, applyInstanceOverrides } from '../src/core/prefabs.js';
import { createFolder, deleteEmptyFolder, folderPath, folderOptions } from '../src/core/folders.js';
import { EditorStore } from '../src/core/store.js';
import { starterProject } from '../src/assets/starter.js';
import { importTree, exportWbpContract } from '../src/io/adapters.js';

test('Canvas fixed anchors resolve position, alignment, AutoSize and exact inverse', () => {
  const s = node().slot; s.anchors = { minX: .5, maxX: .5, minY: 1, maxY: 1 }; s.alignment = { x: .5, y: 1 }; s.offsets = { left: 10, top: -20, right: 200, bottom: 60 };
  assert.deepEqual(resolveCanvasSlot(s, 1000, 700), { x: 410, y: 620, width: 200, height: 60 });
  assert.deepEqual(slotForRect(s, resolveCanvasSlot(s, 1000, 700), 1000, 700), s.offsets);
  s.autoSize = true; assert.equal(resolveCanvasSlot(s, 1000, 700, { width: 88, height: 24 }).width, 88);
});
test('stretched Canvas offsets remain margins at a non-16:9 viewport', () => {
  const s = node().slot; s.anchors = { minX: 0, minY: 0, maxX: 1, maxY: 1 }; s.offsets = { left: 24, top: 60, right: 32, bottom: 40 };
  for (const [w, h] of [[1600, 900], [2048, 780], [1024, 768]]) {
    const r = resolveCanvasSlot(s, w, h); assert.equal(r.width, w - 56); assert.equal(r.height, h - 100); assert.equal(r.x + r.width, w - 32); assert.equal(r.y + r.height, h - 40);
    assert.deepEqual(slotForRect(s, r, w, h), s.offsets);
  }
});
test('mixed stretch and fixed axes preserve inverse geometry including negative offsets', () => {
  const s = node().slot; s.anchors = { minX: .1, maxX: .9, minY: .5, maxY: .5 }; s.alignment.y = .5;
  const rect = { x: 70, y: 230, width: 560, height: 100 }; s.offsets = slotForRect(s, rect, 800, 600);
  assert.deepEqual(resolveCanvasSlot(s, 800, 600), rect);
});
test('linear panel distributes Fill after Auto, gap and padding; collapsed nodes reserve no space', () => {
  const doc = createDocument(); doc.viewport.width = 1000; doc.viewport.height = 200;
  const a = node('SizeBox', { layout: { widthOverride: 180, heightOverride: 50 } }), b = node('Border'), c = node('Border'), gone = node('Border', { visibility: 'collapsed' });
  b.slot.sizeRule = c.slot.sizeRule = 'fill'; b.slot.fill = 1; c.slot.fill = 3;
  doc.root.type = 'HorizontalBox'; doc.root.layout.gap = 20; doc.root.layout.padding = insets(10); doc.root.children = [a, b, gone, c];
  const { entries } = computeLayout(doc);
  assert.equal(entries.get(a.id).width, 180); assert.equal(entries.get(b.id).width, 190); assert.equal(entries.get(c.id).width, 570); assert.equal(entries.has(gone.id), false); assert.equal(entries.get(c.id).x + entries.get(c.id).width, 990);
});
test('ScaleBox propagates scale to nested world geometry', () => {
  const doc = createDocument(); doc.viewport.width = 200; doc.viewport.height = 100;
  const child = node('SizeBox', { layout: { widthOverride: 400, heightOverride: 200 }, children: [node('CanvasPanel', { children: [node('Image', { slot: { offsets: { left: 40, top: 20, right: 80, bottom: 40 } } })] })] });
  const scale = node('ScaleBox', { children: [child], slot: { anchors: { maxX: 1, maxY: 1 }, offsets: { right: 0, bottom: 0 } } }); doc.root.children = [scale];
  const result = computeLayout(doc), leaf = child.children[0].children[0], e = result.entries.get(leaf.id);
  assert.equal(e.scale, .5); assert.equal(e.globalX, 20); assert.equal(e.globalY, 10); assert.equal(e.globalWidth, 40);
});
test('starter project validates, nested instances keep unique paths and per-instance overrides', () => {
  const project = starterProject(); assert.deepEqual(validateDocument(project), []);
  const before = JSON.stringify(project), expanded = expandDocument(project), ids = [];
  walk(expanded.document.root, n => ids.push(n.id)); assert.equal(new Set(ids).size, ids.length); assert.equal(JSON.stringify(project), before);
  const labels = []; walk(expanded.document.root, n => { if (n.id.endsWith('::item-name')) labels.push(n.text); });
  assert.deepEqual(labels, ['星图罗盘', '余烬提灯', '回响晶石']);
  const tags = []; walk(expanded.document.root, n => { if (n.id.endsWith('::tag-label')) tags.push(n.text); }); assert.deepEqual(tags, ['探索工具', '探索工具', '探索工具']);
});
test('outer instance can override a node in a nested prefab without affecting siblings', () => {
  const doc = createDocument(); const leaf = node('TextBlock', { id: 'leaf', text: 'base' });
  doc.prefabs.inner = { id: 'inner', name: 'Inner', width: 100, height: 30, root: leaf };
  const nested = node('PrefabInstance', { id: 'nested', prefabId: 'inner', overrides: { leaf: { text: 'nested' } } });
  const outerRoot = node('CanvasPanel', { id: 'outer-root', children: [nested] });
  doc.prefabs.outer = { id: 'outer', name: 'Outer', width: 200, height: 100, root: outerRoot };
  doc.root.children = [node('PrefabInstance', { id: 'first', prefabId: 'outer', overrides: { 'outer-root/nested/leaf': { text: 'override' } } }), node('PrefabInstance', { id: 'second', prefabId: 'outer' })];
  assert.deepEqual(validateDocument(doc), []);
  const { document: expanded, origins } = expandDocument(doc);
  const labels = []; walk(expanded.root, n => { if (n.id.endsWith('::leaf')) labels.push(n.text); }); assert.deepEqual(labels, ['override', 'nested']);
  assert.equal(origins.get('first::outer-root')?.instanceId, 'first');
  assert.equal(origins.get('first::nested::leaf')?.path, 'outer-root/nested/leaf');
});
test('cycles and missing assets / prefabs fail validation', () => {
  const doc = createDocument(); doc.prefabs.a = { id: 'a', name: 'A', width: 100, height: 100, root: node('PrefabInstance', { prefabId: 'a' }) };
  assert.ok(validateDocument(doc).some(e => e.includes('循环')));
  doc.prefabs = {}; doc.root.children = [node('PrefabInstance', { prefabId: 'missing' }), node('Svg', { svgAsset: 'missing-svg' })];
  const errors = validateDocument(doc); assert.ok(errors.some(e => e.includes('丢失预制体'))); assert.ok(errors.some(e => e.includes('丢失 SVG')));
});
test('detaching nested prefab preserves effective text and removes references', () => {
  const doc = starterProject(), item = Object.values(doc.prefabs).find(p => p.name === 'ItemCard');
  const source = node('PrefabInstance', { prefabId: item.id }); const detached = detachInstance(source, doc); let instances = 0, tags = 0;
  walk(detached, n => { if (n.type === 'PrefabInstance') instances++; if (n.text === '探索工具') tags++; }); assert.equal(instances, 0); assert.equal(tags, 1); assert.equal(detached.id, source.id);
});
test('history groups an entire pointer gesture into one reversible change', () => {
  const doc = createDocument(), child = node(); doc.root.children = [child]; const store = new EditorStore(doc);
  store.beginGesture('drag'); for (let i = 1; i <= 50; i++) store.preview(() => { child.slot.offsets.left = i; }); store.endGesture();
  assert.equal(store.undoStack.length, 1); store.undo(); assert.equal(store.nodes.get(child.id).slot.offsets.left, 0); store.redo(); assert.equal(store.nodes.get(child.id).slot.offsets.left, 50);
});
test('failed invalid transactions roll back atomically', () => {
  const store = new EditorStore(createDocument()); assert.throws(() => store.transaction('bad', doc => { doc.root.slot.anchors.maxX = -1; })); assert.equal(store.document.root.slot.anchors.maxX, 0); assert.equal(store.undoStack.length, 0);
});
test('Frame importer retains anchor margin semantics; WBP export retains raw Slot', () => {
  const { document } = importTree({ name: 'Legacy', canvas: { width: 1600, height: 900 }, nodes: [{ type: 'container', name: 'Panel', x: 20, y: 30, width: 40, height: 50, layout: { anchors: { minX: 0, minY: 0, maxX: 1, maxY: 1 } }, children: [] }] });
  assert.deepEqual(validateDocument(document), []); const child = document.root.children[0], layout = computeLayout(document, { width: 2048, height: 780 });
  assert.equal(layout.entries.get(child.id).width, 1988); assert.equal(layout.entries.get(child.id).height, 700);
  const exported = exportWbpContract(document, document, layout); assert.equal(exported.root.children[0].slot.offsets.right, 40);
});
test('5,000-node expansion and layout complete with one unique entry per node', () => {
  const doc = createDocument(); for (let i = 0; i < 5000; i++) doc.root.children.push(node('Image', { name: `Image${i}` }));
  const start = performance.now(), expanded = expandDocument(doc), layout = computeLayout(expanded.document);
  assert.equal(layout.entries.size, 5001); assert.ok(performance.now() - start < 2500, 'layout should remain bounded for 5k nodes');
});
test('folders support nested paths and moves without changing asset or prefab identities', () => {
  const doc = starterProject(), top = createFolder(doc, 'Custom', 'svg'), child = createFolder(doc, 'Buttons', 'svg', top.id);
  assert.equal(folderPath(doc, child.id), 'Custom / Buttons');
  const asset = Object.values(doc.assets)[0], id = asset.id; asset.folderId = child.id;
  top.name = 'Renamed'; assert.equal(asset.id, id); assert.equal(folderPath(doc, asset.folderId), 'Renamed / Buttons'); assert.deepEqual(validateDocument(doc), []);
  assert.throws(() => deleteEmptyFolder(doc, child.id));
  assert.ok(!folderOptions(doc, 'svg', top.id).some(([id]) => id === child.id));
  top.parentId = child.id; assert.ok(validateDocument(doc).some(e => e.includes('文件夹循环')));
});
test('folder names are unique among siblings and type boundaries are enforced', () => {
  const doc = createDocument(), folder = createFolder(doc, 'Controls', 'prefab');
  assert.throws(() => createFolder(doc, 'Controls', 'prefab')); assert.throws(() => createFolder(doc, '../oops', 'svg'));
  const other = createFolder(doc, 'Icons', 'svg'); other.parentId = folder.id; assert.ok(validateDocument(doc).some(e => e.includes('父级无效')));
});
test('applying outer overrides retains nested instance ownership', () => {
  const doc = createDocument(), leaf = node('TextBlock', { id: 'leaf-source', text: 'base' });
  doc.prefabs.inner = { id: 'inner', name: 'Inner', width: 100, height: 30, root: leaf };
  const nested = node('PrefabInstance', { id: 'nested-source', prefabId: 'inner' }), root = node('CanvasPanel', { id: 'outer-source', children: [nested] });
  doc.prefabs.outer = { id: 'outer', name: 'Outer', width: 200, height: 100, root };
  const instance = node('PrefabInstance', { prefabId: 'outer', overrides: { 'outer-source/nested-source/leaf-source': { text: 'outer-specific' } } });
  doc.root.children = [instance, node('PrefabInstance', { prefabId: 'inner' })];
  applyInstanceOverrides(doc, instance);
  assert.equal(doc.prefabs.inner.root.text, 'base'); assert.deepEqual(root.children[0].overrides['leaf-source'], { text: 'outer-specific' }); assert.deepEqual(instance.overrides, {});
  const texts = []; walk(expandDocument(doc).document.root, n => { if (n.type === 'TextBlock') texts.push(n.text); }); assert.deepEqual(texts, ['outer-specific', 'base']);
});
test('demo fills available width while retaining margins at wide aspect ratios', () => {
  const doc = starterProject(), expanded = expandDocument(doc).document;
  for (const [width, height] of [[1440, 900], [2048, 780]]) {
    const l = computeLayout(expanded, { width, height }), entries = [...l.entries.values()], body = entries.find(e => e.node.name === 'MainPanel'), content = entries.find(e => e.node.name === 'EquipmentCanvas'), row = entries.find(e => e.node.name === 'EquipmentRow');
    assert.equal(body.globalX + body.globalWidth, width - 48); assert.equal(body.globalY + body.globalHeight, height - 132); assert.equal(row.width, content.width); assert.equal(content.globalX + content.width, body.globalX + body.width - 32);
  }
});
test('WBP layout contract can be re-opened without losing vector assets or button states', () => {
  const project = starterProject(), expanded = expandDocument(project).document, layout = computeLayout(expanded);
  const contract = exportWbpContract(project, expanded, layout), imported = importTree(contract).document;
  assert.deepEqual(validateDocument(imported), []); assert.equal(Object.keys(imported.assets).length, 13);
  let buttons = 0; walk(imported.root, n => { if (n.type === 'Button') { buttons++; assert.ok(n.brushes.normal); } }); assert.equal(buttons, 5);
});
