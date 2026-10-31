import test from 'node:test';
import assert from 'node:assert/strict';
import { createNode, createDocument, validateDocument } from '../src/core/model.js';
import { expandDocument } from '../src/core/prefabs.js';
import { motionSpec, motionTarget, createMotionAsset, motionFrame, sampleMotion, motionClip, validateMotionAsset, materializeMotionAssets, motionAssetId } from '../src/core/motion.js';
import { computeLayout } from '../src/core/layout.js';
import { exportWbpContract, importTree } from '../src/io/adapters.js';

test('HUD feedback keeps layout data immutable and gives press and release distinct timing', () => {
  const node = createNode('Button', { binding: 'ability' }), before = JSON.stringify(node);
  const spec = motionSpec(node);
  assert.ok(spec.press < spec.enter && spec.enter < spec.release);
  assert.ok(spec.pressScale < 1 && spec.hoverScale > 1 && spec.hoverLift < 0);
  assert.notDeepEqual(motionTarget(spec, 'pressed'), motionTarget(spec, 'hovered'));
  assert.equal(JSON.stringify(node), before);
});

test('selected rim breathes independently and disabled buttons stop movement', () => {
  const rim = createNode('Svg', { id: 'instance::hud-ability-selected' });
  assert.equal(motionSpec(rim).kind, 'breathe');
  const button = createNode('Button', { binding: 'ability', interactable: false });
  assert.equal(motionSpec(button), null);
  assert.equal(motionAssetId(button), 'motion-lively'); // Disabled state retains its reusable reference for re-enable.
  rim.motion.preset = 'none'; assert.equal(motionSpec(rim), null);
});

test('prefab instances can override motion without changing their shared template or sibling', () => {
  const doc = createDocument(), template = createNode('Button', { id: 'button' });
  doc.prefabs.shared = { id: 'shared', root: template, width: 100, height: 50 };
  doc.root.children = [createNode('PrefabInstance', { id: 'quiet', prefabId: 'shared', overrides: { button: { motion: { preset: 'none' } } } }), createNode('PrefabInstance', { id: 'active', prefabId: 'shared' })];
  const expanded = expandDocument(doc).document.root;
  assert.equal(motionSpec(expanded.children[0].children[0]), null);
  assert.equal(motionSpec(expanded.children[1].children[0]).kind, 'button');
  assert.equal(template.motion.preset, 'auto');
});

test('motion presets survive the WBP layout contract roundtrip', () => {
  const doc = createDocument(); doc.root.children = [createNode('Button', { motion: { preset: 'lively' } })];
  const expanded = expandDocument(doc), contract = exportWbpContract(doc, expanded.document, computeLayout(expanded.document));
  assert.equal(importTree(contract).document.root.children[0].motion.preset, 'lively');
});

test('authored multi-keyframe channels, loops and interrupted transitions share deterministic sampling', () => {
  const asset = createMotionAsset('authored');
  asset.clips.hover = { duration: 400, easing: 'linear', loop: false, fromCurrent: false, keyframes: [motionFrame(0), motionFrame(.5, { x: 8, y: -10, rotation: -6, scaleX: 1.12, scaleY: .96, opacity: .8 }), motionFrame(1)] };
  assert.deepEqual(sampleMotion(asset.clips.hover, 200), { x: 8, y: -10, rotation: -6, scaleX: 1.12, scaleY: .96, opacity: .8 });
  assert.equal(sampleMotion({ ...asset.clips.hover, loop: true }, 600).rotation, -6);
  const current = { x: 4, y: 5, scaleX: .8, scaleY: .9, rotation: 12, opacity: .5 };
  assert.deepEqual(sampleMotion(asset.clips.release, 0, current), current);
  assert.equal(motionClip(asset, 'hovered', 'pressed'), asset.clips.release);
  assert.deepEqual(validateMotionAsset(asset), []);
});

test('motion asset validation rejects unordered frames, unsupported curves, broken events and missing references', () => {
  const asset = createMotionAsset('authored'); asset.clips.hover.keyframes[1].t = 0;
  assert.ok(validateMotionAsset(asset).length);
  asset.clips.hover.keyframes[1].t = 1; asset.bindings.hovered = 'missing'; assert.ok(validateMotionAsset(asset).length);
  const doc = createDocument(); doc.root.motion.assetId = 'missing'; assert.ok(validateDocument(doc).some(e => e.includes('缺少动效资产')));
});

test('shared authored assets and folder identity survive export and import without per-effect code', () => {
  const doc = createDocument(); doc.motions.authored = createMotionAsset('authored'); doc.root.children = [createNode('Button', { motion: { assetId: 'authored' } })];
  const expanded = expandDocument(doc), contract = exportWbpContract(doc, expanded.document, computeLayout(expanded.document));
  assert.deepEqual(importTree(contract).document.motions.authored, doc.motions.authored);
  assert.equal(importTree(contract).document.root.children[0].motion.assetId, 'authored');
  const before = JSON.stringify(doc.motions.authored); materializeMotionAssets(doc); assert.equal(JSON.stringify(doc.motions.authored), before);
});
