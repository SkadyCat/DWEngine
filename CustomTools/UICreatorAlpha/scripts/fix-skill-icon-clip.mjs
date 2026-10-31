import fs from 'node:fs/promises';
import { walk, validateDocument } from '../src/core/model.js';

const url = 'http://127.0.0.1:5190/api/projects/hud-darksteel-20261024';
const response = await fetch(url);
if (!response.ok) throw new Error('HUD project unavailable');
const previous = await response.json(), document = structuredClone(previous.document);
let changed = 0;
walk(document.prefabs['hud-prefab-ability'].root, node => {
  if (node.id !== 'hud-ability-flame') return;
  if (node.style.clip !== true || node.style.radius !== 11.5) {
    Object.assign(node.style, { clip: true, radius: 11.5 }); changed++;
  }
});
if (!changed) { console.log('Skill icon clipping is already current.'); process.exit(0); }
const errors = validateDocument(document);
if (errors.length) throw new Error(errors.join('\n'));
const backup = new URL('../work/skill-icon-clip/', import.meta.url);
await fs.mkdir(backup, { recursive: true });
await fs.writeFile(new URL(`before-r${previous.revision}.json`, backup), JSON.stringify(previous), { flag: 'wx' });
const saved = await fetch(url, { method: 'PUT', headers: { 'Content-Type': 'application/json', 'If-Match': `"${previous.revision}"` }, body: JSON.stringify(document) });
const result = await saved.json();
if (!saved.ok) throw new Error(JSON.stringify(result));
await fs.writeFile(new URL('../Screen/RuleStudio-HUD/hud.uialpha.json', import.meta.url), JSON.stringify(document, null, 2));
console.log({ previous: previous.revision, saved: result.revision, clippedIconTemplates: changed });
