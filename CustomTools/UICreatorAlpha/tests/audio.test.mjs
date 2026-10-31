import test from 'node:test';
import assert from 'node:assert/strict';
import { builtinSounds, materializeSounds } from '../src/assets/ui-sounds.js';
import { inspectWav, wavBytes, validateSoundAsset, resolveSound, validateAudioBinding } from '../src/core/audio.js';
import { createDocument, createNode, validateDocument } from '../src/core/model.js';
import { expandDocument } from '../src/core/prefabs.js';
import { computeLayout } from '../src/core/layout.js';
import { exportWbpContract, importTree } from '../src/io/adapters.js';
import { UiAudioPlayer } from '../src/runtime/audio.js';
import { EditorStore } from '../src/core/store.js';

test('built-in UI cues are distinct non-silent PCM WAV resources, with validated headers and metadata', () => {
  const sounds = builtinSounds(); assert.equal(Object.keys(sounds).length, 6); assert.equal(new Set(Object.values(sounds).map(s => s.source)).size, 6);
  for (const asset of Object.values(sounds)) { assert.deepEqual(validateSoundAsset(asset), []); const bytes = wavBytes(asset.source), info = inspectWav(bytes); assert.ok(info.duration > .02 && info.duration < .2); assert.equal(info.sampleRate, 24000); assert.ok(bytes.subarray(44).some(v => v !== 0)); }
  const broken = wavBytes(sounds['ui-click'].source).slice(0, -1); assert.throws(() => inspectWav(broken));
});
test('prefab sound overrides are isolated and source WAV plus bindings survive project/WBP contracts', () => {
  const doc = createDocument(), template = createNode('Button', { id: 'button' }); doc.prefabs.shared = { id: 'shared', width: 100, height: 60, root: template };
  doc.root.children = [createNode('PrefabInstance', { id: 'a', prefabId: 'shared', overrides: { button: { audio: { volume: .4, events: { clicked: 'ui-select' } } } } }), createNode('PrefabInstance', { id: 'b', prefabId: 'shared' })];
  const expanded = expandDocument(doc); assert.equal(expanded.document.root.children[0].children[0].audio.events.clicked, 'ui-select'); assert.equal(expanded.document.root.children[1].children[0].audio.events.clicked, 'ui-click'); assert.equal(template.audio.volume, 1);
  const output = exportWbpContract(doc, expanded.document, computeLayout(expanded.document)), restored = importTree(output).document;
  assert.deepEqual(restored.sounds, doc.sounds); assert.equal(restored.root.children[0].children[0].audio.events.clicked, 'ui-select'); assert.deepEqual(validateDocument(restored), []);
  assert.ok(validateAudioBinding({ ...template.audio, events: { clicked: 'constructor' } }, {}).length);
  const before = JSON.stringify(doc); materializeSounds(doc); assert.equal(JSON.stringify(doc), before);
  const store = new EditorStore(createDocument());
  store.transaction('remove unused sound', source => { source.root.children = []; delete source.sounds['ui-click']; });
  assert.equal(store.document.sounds['ui-click'], undefined);
  store.transaction('create button after cleanup', source => { source.root.children.push(createNode('Button')); });
  assert.ok(store.document.sounds['ui-click']); assert.deepEqual(validateDocument(store.document), []);
});
function fakeContext() {
  const context = { state: 'suspended', currentTime: 0, destination: {}, decoded: 0, created: [], async resume() { this.state = 'running'; }, async decodeAudioData(buffer) { this.decoded++; return { duration: inspectWav(new Uint8Array(buffer)).duration }; }, createGain() { return { gain: { value: 1 }, connect() {}, disconnect() {} }; }, createBufferSource() { const voice = { playbackRate: { value: 1 }, start() { this.started = true; }, stop() { this.stopped = true; this.onended?.(); }, connect() {}, disconnect() {} }; this.created.push(voice); return voice; } }; return context;
}
test('audio unlock, decoded-buffer reuse, gain/pitch, shared cooldown, mute and disabled guards', async () => {
  const context = fakeContext(), events = [], player = new UiAudioPlayer({ context, onPlay: value => events.push(value) }), sounds = builtinSounds(), node = createNode('Button', { audio: { volume: .5, pitch: 1.2 } }); player.volume = .8;
  assert.equal(await player.play(node, 'clicked', sounds), false); assert.equal(await player.unlock(), true);
  assert.equal(await player.play(node, 'clicked', sounds), true); assert.equal(events[0].volume, .65 * .5 * .8); assert.equal(events[0].pitch, 1.2); assert.equal(context.created[0].started, true);
  assert.equal(await player.play(createNode('Button'), 'clicked', sounds), false);
  context.currentTime = .1; assert.equal(await player.play(node, 'clicked', sounds), true); assert.equal(context.decoded, 1);
  node.interactable = false; assert.equal(resolveSound(node, 'clicked', sounds), null); node.interactable = true;
  player.setMuted(true); assert.equal(player.voices.length, 0); assert.equal(await player.play(node, 'clicked', sounds), false); player.destroy();
});
test('voice cap and teardown also suppress a pending decode', async () => {
  const context = fakeContext(), player = new UiAudioPlayer({ context }), sounds = builtinSounds(), node = createNode('Button'); await player.unlock(); sounds['ui-click'].cooldown = 0;
  for (let i = 0; i < 12; i++) await player.play(node, 'clicked', sounds);
  assert.equal(player.voices.length, 8); assert.equal(context.created.filter(v => v.stopped).length, 4); player.destroy(); assert.equal(player.voices.length, 0);
  let finish; const pendingContext = fakeContext(); pendingContext.decodeAudioData = () => new Promise(resolve => { finish = resolve; });
  const pending = new UiAudioPlayer({ context: pendingContext }); await pending.unlock(); const promise = pending.play(node, 'clicked', sounds); pending.destroy(); finish({ duration: .085 }); assert.equal(await promise, false); assert.equal(pendingContext.created.length, 0);
});
