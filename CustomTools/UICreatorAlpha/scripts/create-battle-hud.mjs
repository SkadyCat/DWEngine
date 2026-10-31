import fs from 'node:fs/promises';
import { HUD_ASSETS, HUD_PROJECT_ID, createHudProject } from '../src/assets/hud-darksteel/project.js';
import { installBattleHud } from '../src/assets/rulestudio-hud/project.js';
import { battleHudSources } from './rulestudio-hud-art.mjs';
import { validateDocument } from '../src/core/model.js';
import { expandDocument } from '../src/core/prefabs.js';
import { computeLayout } from '../src/core/layout.js';

const baseUrl = process.env.UI_ALPHA_URL || 'http://127.0.0.1:5190';
const directory = new URL('../src/assets/rulestudio-hud/svg/', import.meta.url), output = new URL('../Screen/RuleStudio-HUD/', import.meta.url), backup = new URL('../work/rulestudio-hud/', import.meta.url);
await Promise.all([fs.mkdir(directory, { recursive: true }), fs.mkdir(output, { recursive: true }), fs.mkdir(backup, { recursive: true })]);
const generated = battleHudSources(), sources = {};
for (const [file, source] of Object.entries(generated)) {
  try { await fs.writeFile(new URL(file, directory), source, { encoding: 'utf8', flag: 'wx' }); } catch (e) { if (e.code !== 'EEXIST') throw e; }
  sources[file] = await fs.readFile(new URL(file, directory), 'utf8');
}
const result = await fetch(`${baseUrl}/api/projects/${HUD_PROJECT_ID}`);
const previous = result.ok ? await result.json() : null;
const base = previous?.document || createHudProject(Object.fromEntries(await Promise.all(HUD_ASSETS.map(async a => [a.file, await fs.readFile(new URL(`../src/assets/hud-darksteel/svg/${a.file}`, import.meta.url), 'utf8')]))));
if (previous) await fs.writeFile(new URL(`before-r${previous.revision}.json`, backup), JSON.stringify(previous, null, 2), { flag: 'wx' }).catch(e => { if (e.code !== 'EEXIST') throw e; });
const project = installBattleHud(structuredClone(base), sources), errors = validateDocument(project);
if (errors.length) throw new Error(errors.join('\n'));
const expanded = expandDocument(project), layout = computeLayout(expanded.document);
await fs.writeFile(new URL('hud.uialpha.json', output), JSON.stringify(project, null, 2));
await fs.writeFile(new URL('composition.json', output), JSON.stringify({ schema: 'uicreator-alpha/svg-composition', version: 1, projectId: project.id, source: project.presentation.source, viewport: project.viewport, assets: Object.keys(project.assets), prefabs: Object.keys(project.prefabs), presentation: project.presentation, currentNodes: layout.entries.size, availableNodes: expanded.count }, null, 2));
console.log(JSON.stringify({ assets: Object.keys(project.assets).length, prefabs: Object.keys(project.prefabs).length, arranged: layout.entries.size, expanded: expanded.count, validation: errors }));
if (process.argv.includes('--save')) {
  if (previous?.document.presentation?.controller === 'rulestudio-battle-hud' && !process.argv.includes('--replace-owned')) throw new Error('完整 HUD 已安装；请在编辑器继续编辑，避免重建覆盖。');
  const save = await fetch(`${baseUrl}/api/projects/${project.id}`, { method: 'PUT', headers: { 'Content-Type': 'application/json', 'If-Match': `"${previous?.revision || 0}"` }, body: JSON.stringify(project) });
  const saved = await save.json(); if (!save.ok) throw new Error(`Save refused: ${save.status} ${saved.error}`);
  await fs.writeFile(new URL('save-state.json', backup), JSON.stringify({ id: project.id, ...saved }, null, 2));
  console.log(`Saved revision ${saved.revision}. ${baseUrl}/?project=${project.id}`);
}
