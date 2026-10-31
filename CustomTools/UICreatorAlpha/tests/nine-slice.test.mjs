import test from 'node:test';
import assert from 'node:assert/strict';
import { nineSliceCuts } from '../src/assets/svg.js';

test('93x23 map button shares one integer cut when corner bands consume the height', () => {
  assert.deepEqual(nineSliceCuts(23, 11.5, 11.5), [0, 12, 12, 23]);
});
test('cuts follow the final pixel grid through fractional positions and ScaleBoxes', () => {
  for (const scale of [.5, .72, 1, 1.25, 2]) for (const origin of [0, .25, 17.5, -3.4]) {
    const cuts = nineSliceCuts(93, 11.5, 11.5, origin, scale);
    assert.equal(cuts[0], 0); assert.equal(cuts[3], 93);
    for (const cut of cuts.slice(1, 3)) assert.ok(Math.abs(origin + cut * scale - Math.round(origin + cut * scale)) < 1e-9);
    assert.ok(cuts.every((v, i) => !i || v >= cuts[i - 1]));
  }
});
test('very small controls remain covered without negative or overlapping bands', () => {
  for (const length of [0, .25, 1, 5, 23, 24, 150]) {
    const corner = Math.min(12, length / 2), cuts = nineSliceCuts(length, corner, corner, .25, .5);
    assert.equal(cuts[0], 0); assert.equal(cuts[3], length);
    assert.ok(cuts.every((v, i) => !i || v >= cuts[i - 1]));
    assert.equal(cuts.slice(1).reduce((sum, end, i) => sum + end - cuts[i], 0), length);
  }
});
