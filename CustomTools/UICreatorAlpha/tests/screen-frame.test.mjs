import test from 'node:test';
import assert from 'node:assert/strict';
import { screenFromPoints, resizeScreen } from '../src/editor/screen-frame.js';
import { createDocument, createNode } from '../src/core/model.js';
import { computeLayout } from '../src/core/layout.js';

test('screen rectangle has the same dimensions in either drag direction', () => {
  const rect = { x: 120, y: 60, width: 1280, height: 720 };
  assert.deepEqual(screenFromPoints({ x: 120, y: 60 }, { x: 1400, y: 780 }), rect);
  assert.deepEqual(screenFromPoints({ x: 1400, y: 780 }, { x: 120, y: 60 }), rect);
  assert.equal(screenFromPoints({ x: 0, y: 0 }, { x: 0, y: 0 }).width, 16);
});

test('north and west screen resizing keeps the opposite corner fixed, including limits', () => {
  const rect = { x: 100, y: 80, width: 1280, height: 720 };
  for (const [dx, dy] of [[200, 100], [5000, 5000], [-50000, -50000]]) {
    const next = resizeScreen(rect, 'nw', dx, dy);
    assert.equal(next.x + next.width, rect.x + rect.width);
    assert.equal(next.y + next.height, rect.y + rect.height);
    assert.ok(next.width >= 16 && next.width <= 16384);
    assert.ok(next.height >= 16 && next.height <= 16384);
  }
  assert.deepEqual(rect, { x: 100, y: 80, width: 1280, height: 720 });
});

test('changing the screen repositions right and bottom anchors without scaling controls or changing authored slots', () => {
  const doc = createDocument();
  const right = createNode('Image', { id: 'right', slot: { anchors: { minX: 1, maxX: 1, minY: 0, maxY: 0 }, alignment: { x: 1, y: 0 }, offsets: { left: -14, top: 14, right: 200, bottom: 100 } } });
  const bottom = createNode('Image', { id: 'bottom', slot: { anchors: { minX: .5, maxX: .5, minY: 1, maxY: 1 }, alignment: { x: .5, y: 1 }, offsets: { left: 0, top: -20, right: 640, bottom: 80 } } });
  doc.root.children = [right, bottom];
  const authored = JSON.stringify(doc);
  for (const [width, height] of [[1280, 720], [1920, 1080], [1024, 768], [2560, 1080]]) {
    const screen = screenFromPoints({ x: 90, y: 60 }, { x: 90 + width, y: 60 + height });
    const result = computeLayout(doc, screen);
    const r = result.entries.get('right'), b = result.entries.get('bottom');
    assert.equal(width - r.globalX - r.globalWidth, 14);
    assert.equal(b.globalX + b.globalWidth / 2, width / 2);
    assert.equal(height - b.globalY - b.globalHeight, 20);
    assert.equal(r.globalWidth, 200);
    assert.equal(b.globalWidth, 640);
  }
  assert.equal(JSON.stringify(doc), authored);
});
