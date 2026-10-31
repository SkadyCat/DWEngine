import fs from 'node:fs/promises';
import { materializeSounds } from '../src/assets/ui-sounds.js';
import { defaultAudio } from '../src/core/audio.js';
import { materializeMotionAssets } from '../src/core/motion.js';
import { walk } from '../src/core/model.js';
import { expandDocument } from '../src/core/prefabs.js';

const args = process.argv.slice(2), input = args.find(arg => !arg.startsWith('--'));
const base = 'exports/wbp-hud-native-20261025', out = `${base}/${input ? 'audio-current' : 'audio'}`, source = JSON.parse(await fs.readFile(input || `${base}/motion/generic/bundle.json`, 'utf8'));
materializeSounds(source.project); materializeMotionAssets(source.project);
for (const root of [source.project.root, ...Object.values(source.project.prefabs).map(p => p.root)]) walk(root, node => { if (node.binding === 'picker') node.audio.events = { opened: 'ui-open', closed: 'ui-close' }; });
for (const node of source.nodes) node.audio ||= defaultAudio(node.type, node.binding);
// Authoring defaults for the independent skill-picker window; source pixels stay frozen.
for (const node of source.nodes) if (node.binding === 'picker') node.audio.events = { opened: 'ui-open', closed: 'ui-close' };
const audioProject = args.find(arg => arg.startsWith('--audio-project='))?.split('=')[1];
if (audioProject) {
  const response = await fetch(`http://127.0.0.1:5190/api/projects/${encodeURIComponent(audioProject)}`); if (!response.ok) throw new Error('Audio project could not be loaded');
  const { document: authored } = await response.json(); source.project.sounds = structuredClone(authored.sounds);
  for (const [id, folder] of Object.entries(authored.folders || {})) if (folder.kind === 'sound') source.project.folders[id] = structuredClone(folder);
  const copy = (target, from) => {
    if (!target || !from) return; const originals = new Map(); walk(from, node => originals.set(node.id, node));
    walk(target, node => { const original = originals.get(node.id); if (!original) return; if (original.audio) node.audio = structuredClone(original.audio); for (const [key, patch] of Object.entries(original.overrides || {})) if (patch.audio) { node.overrides ||= {}; node.overrides[key] ||= {}; node.overrides[key].audio = structuredClone(patch.audio); } });
  };
  copy(source.project.root, authored.root); for (const [id, prefab] of Object.entries(source.project.prefabs)) copy(prefab.root, authored.prefabs[id]?.root);
  const expanded = new Map(); walk(expandDocument(source.project).document.root, node => expanded.set(node.id, node));
  for (const node of source.nodes) if (expanded.has(node.id)) node.audio = structuredClone(expanded.get(node.id).audio);
}
await fs.mkdir(out, { recursive: true }); await fs.writeFile(`${out}/bundle.json`, JSON.stringify(source));
for (const asset of Object.values(source.project.sounds)) await fs.writeFile(`${out}/${asset.id}.wav`, Buffer.from(asset.source.split(',')[1], 'base64'));
console.log(JSON.stringify({ output: out, sounds: Object.keys(source.project.sounds).length, bindings: source.nodes.reduce((n, node) => n + Object.keys(node.audio.events).length, 0) }));
if (process.argv.includes('--seed')) {
  const id = 'hud-ui-audio-20261025', response = await fetch(`http://127.0.0.1:5190/api/projects/${id}`);
  if (!response.ok) { source.project.id = id; source.project.name = 'HUD · UI 音效'; const saved = await fetch(`http://127.0.0.1:5190/api/projects/${id}`, { method: 'PUT', headers: { 'Content-Type': 'application/json', 'If-Match': '"0"' }, body: JSON.stringify(source.project) }); if (!saved.ok) throw new Error(await saved.text()); }
  console.log(`http://127.0.0.1:5190/?project=${id}`);
}
