import fs from 'node:fs/promises';
import { battleHudSources } from './rulestudio-hud-art.mjs';
import { SAMPLE_SKILLS, REFERENCE_SKILL_IDS } from '../src/assets/rulestudio-hud/definition.js';
import { skillChoicePrefab, skillPickerPrefab } from '../src/assets/rulestudio-hud/skill-picker.js';
import { validateDocument, walk, merge, insets } from '../src/core/model.js';
const response = await fetch('http://127.0.0.1:5190/api/projects/hud-darksteel-20261024'); if (!response.ok) throw new Error('HUD project unavailable');
const previous = await response.json(), document = structuredClone(previous.document);
const backup = new URL('../work/rulestudio-hud/', import.meta.url), directory = new URL('../src/assets/rulestudio-hud/svg/', import.meta.url);
await fs.mkdir(backup, { recursive: true }); await fs.writeFile(new URL(`before-picker-r${previous.revision}.json`, backup), JSON.stringify(previous, null, 2), { flag: 'wx' }).catch(e => { if (e.code !== 'EEXIST') throw e; });
const newFiles = ['poison.svg', 'picker-panel.svg', 'choice-normal.svg', 'choice-hovered.svg', 'choice-pressed.svg', 'choice-selected.svg', 'selection-filled.svg'];
const sources = battleHudSources();
for (const file of newFiles) {
  try { await fs.writeFile(new URL(file, directory), sources[file], { flag: 'wx', encoding: 'utf8' }); } catch (e) { if (e.code !== 'EEXIST') throw e; }
  const id = `rs-svg-${file.replace('.svg', '')}`;
  const size = file === 'selection-filled.svg' ? 90 : 96;
  document.assets[id] ||= { id, name: ({ 'poison.svg': '技能 · 毒瓶', 'picker-panel.svg': '技能装配 · 面板', 'choice-normal.svg': '技能条目 · Normal', 'choice-hovered.svg': '技能条目 · Hover', 'choice-pressed.svg': '技能条目 · Pressed', 'choice-selected.svg': '技能条目 · 当前已装配', 'selection-filled.svg': '选中 · 已装配技能实线边框' })[file], source: await fs.readFile(new URL(file, directory), 'utf8'), viewBox: [0, 0, size, size], scaleMode: ['poison.svg', 'selection-filled.svg'].includes(file) ? 'contain' : 'nine-slice', slice: insets(12), folderId: file === 'poison.svg' ? 'rs-svg-icons' : 'rs-svg-panels' };
}
const catalog = new Map(document.presentation.skills.map(s => [s.id, s])); for (const skill of SAMPLE_SKILLS) if (!catalog.has(skill.id)) catalog.set(skill.id, skill);
document.presentation.skills = [...catalog.values()]; document.presentation.version = 2;
if (process.argv.includes('--reference')) {
  document.presentation.availableSkills = REFERENCE_SKILL_IDS; document.presentation.loadout = ['poison_flask', null, null, null]; document.presentation.selected = 0;
  document.presentation.referenceFixture = '用户提供的毒瓶 / 装备授予 / 槽位 1 截图';
}
document.presentation.availableSkills ||= document.presentation.skills.map(s => s.id);
document.prefabs['rs-prefab-skill-choice'] = skillChoicePrefab();
document.prefabs['rs-prefab-picker'] = skillPickerPrefab(document.presentation.skills.filter(s => document.presentation.availableSkills.includes(s.id)));
walk(document.prefabs['hud-prefab-screen'].root, n => { if (n.binding === 'picker') n.slot.offsets.bottom = document.prefabs['rs-prefab-picker'].height; });
if (process.argv.includes('--reference')) {
  const row = document.prefabs['hud-prefab-ability-row'].root;
  row.children.forEach((instance, i) => { instance.overrides['hud-ability-button/hud-ability-content/hud-ability-selected'] = { visibility: i === 0 ? 'visible' : 'collapsed' }; });
  const patch = row.children[0].overrides;
  patch['hud-ability-button/hud-ability-content/hud-ability-selected'].svgAsset = 'rs-svg-selection-filled';
  merge(patch['hud-ability-button/hud-ability-content/hud-ability-flame'], { visibility: 'visible', svgAsset: 'rs-svg-poison', slot: { offsets: { left: 1, top: 1, right: 72, bottom: 72 } } });
  merge(patch['hud-ability-button/hud-ability-content/hud-ability-name'], { text: '毒瓶', slot: { zOrder: 20 }, style: { fill: '#060a11b8', radius: 3 } });
  for (const [id, extra] of [['hud-ability-key', { text: '1' }], ['hud-ability-equipped', { visibility: 'visible' }]]) patch[`hud-ability-button/hud-ability-content/${id}`] = merge(patch[`hud-ability-button/hud-ability-content/${id}`] || {}, { ...extra, slot: { zOrder: 20 }, style: { fill: '#060a11b8', radius: 3 } });
}
const errors = validateDocument(document); if (errors.length) throw new Error(errors.join('\n'));
console.log({ previous: previous.revision, assets: Object.keys(document.assets).length, prefabs: Object.keys(document.prefabs).length, choices: document.presentation.availableSkills });
if (process.argv.includes('--save')) {
  const saved = await fetch(`http://127.0.0.1:5190/api/projects/${document.id}`, { method: 'PUT', headers: { 'Content-Type': 'application/json', 'If-Match': `"${previous.revision}"` }, body: JSON.stringify(document) });
  const result = await saved.json(); if (!saved.ok) throw new Error(JSON.stringify(result));
  await fs.writeFile(new URL('save-state.json', backup), JSON.stringify({ id: document.id, ...result }, null, 2));
  await fs.writeFile(new URL('../Screen/RuleStudio-HUD/hud.uialpha.json', import.meta.url), JSON.stringify(document, null, 2));
  console.log({ saved: result.revision });
}
