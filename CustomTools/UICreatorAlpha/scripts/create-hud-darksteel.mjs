import fs from 'node:fs/promises';
import path from 'node:path';
import crypto from 'node:crypto';
import { fileURLToPath } from 'node:url';
import { HUD_ASSETS, createHudProject } from '../src/assets/hud-darksteel/project.js';
import { hudVectorSources } from './hud-darksteel-art.mjs';
import { validateDocument, walk } from '../src/core/model.js';
import { expandDocument } from '../src/core/prefabs.js';
import { computeLayout } from '../src/core/layout.js';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const directory = path.join(root, 'src/assets/hud-darksteel'), svgDir = path.join(directory, 'svg');
const output = path.join(root, 'Screen/HUD-DarkSteel');
await fs.mkdir(svgDir, { recursive: true }); await fs.mkdir(output, { recursive: true });
if (process.argv.includes('--create-assets')) {
  const flame = await fs.readFile(path.join(svgDir, 'flame-glyph.svg'), 'utf8');
  for (const [file, source] of Object.entries(hudVectorSources(flame))) {
    try { await fs.writeFile(path.join(svgDir, file), source, { encoding: 'utf8', flag: 'wx' }); }
    catch (error) { if (error.code !== 'EEXIST') throw error; }
  }
}
const sources = Object.fromEntries(await Promise.all(HUD_ASSETS.map(async a => [a.file, await fs.readFile(path.join(svgDir, a.file), 'utf8')])));
const project = createHudProject(sources), errors = validateDocument(project);
if (errors.length) throw new Error(errors.join('\n'));
const reference = path.join(directory, 'reference.png');
try { project.provenance.referenceSha256 = crypto.createHash('sha256').update(await fs.readFile(reference)).digest('hex'); } catch {}
const expanded = expandDocument(project), layout = computeLayout(expanded.document);
const manifest = {
  schema: 'uicreator-alpha/svg-composition', version: 1, projectId: project.id,
  referenceSize: { width: 688, height: 170 }, designSize: project.viewport,
  backgroundPolicy: project.provenance.backgroundPolicy,
  textLayoutVersion: 1,
  assets: HUD_ASSETS.map(({ file, ...meta }) => ({ ...meta, sourceFile: `../../src/assets/hud-darksteel/svg/${file}`, sha256: crypto.createHash('sha256').update(sources[file]).digest('hex') })),
  prefabIds: Object.keys(project.prefabs),
  sourceRects: { filledSlot: [42, 75, 74, 74], emptySlots: [[122, 75, 74, 74], [202, 75, 74, 74], [282, 75, 74, 74]], selectedGlow: [274, 67, 90, 90], consumables: [[374, 83, 64, 64], [444, 83, 64, 64], [514, 83, 64, 64], [584, 83, 64, 64]], leftFacet: [0, 0, 184, 170], rightFacet: [544, 0, 144, 170] },
  textNodes: [],
};
for (const p of Object.values(project.prefabs)) walk(p.root, node => {
  if (node.type === 'TextBlock') manifest.textNodes.push({ prefabId: p.id, id: node.id, safeRect: { x: node.slot.offsets.left, y: node.slot.offsets.top, width: node.slot.offsets.right, height: node.slot.offsets.bottom }, fontSize: node.style.fontSize, align: node.style.textAlign, maxChars: node.id === 'hud-control-guide' ? 104 : node.id === 'hud-ability-name' ? 6 : 2, overflow: node.style.textOverflow });
});
await fs.writeFile(path.join(output, 'hud.uialpha.json'), JSON.stringify(project, null, 2), 'utf8');
await fs.writeFile(path.join(output, 'composition.json'), JSON.stringify(manifest, null, 2), 'utf8');
console.log(JSON.stringify({ project: project.name, assets: HUD_ASSETS.length, prefabs: Object.keys(project.prefabs).length, nodes: layout.entries.size, textSafeRegions: manifest.textNodes.length }));
if (process.argv.includes('--save')) {
  const base = process.env.UI_ALPHA_URL || 'http://127.0.0.1:5190';
  const stateFile = path.join(root, 'work/hud-darksteel/save-state.json'); let expected = 0;
  try { expected = JSON.parse(await fs.readFile(stateFile, 'utf8')).revision; } catch {}
  const response = await fetch(`${base}/api/projects/${project.id}`, { method: 'PUT', headers: { 'Content-Type': 'application/json', 'If-Match': `"${expected}"` }, body: JSON.stringify(project) });
  const result = await response.json(); if (!response.ok) throw new Error(`Save refused (${response.status}): ${result.error}`);
  await fs.mkdir(path.dirname(stateFile), { recursive: true }); await fs.writeFile(stateFile, JSON.stringify({ id: project.id, ...result }, null, 2));
  console.log(`Saved revision ${result.revision}. Editor: ${base}/?project=${project.id}`);
}
