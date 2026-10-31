import fs from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { ProjectRepository } from '../../CustomTools/UICreatorAlpha/server/project-repository.mjs';
import { validateDocument } from '../../CustomTools/UICreatorAlpha/src/core/model.js';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const tool = path.join(root, 'CustomTools/UICreatorAlpha');
const source = JSON.parse(await fs.readFile(path.join(root, 'UIAuthoring/MapCoreSelector.uialpha.json'), 'utf8'));
const catalog = [];
for (const file of await fs.readdir(path.join(root, 'Content/Rules/Equipment'))) {
  if (!/^map_core_equipment(?:_t\d+)?\.json$/.test(file)) continue;
  const doc = JSON.parse(await fs.readFile(path.join(root, 'Content/Rules/Equipment', file), 'utf8'));
  const value = id => doc.traits.find(trait => trait.id === id)?.params?.n ?? 0;
  catalog.push({ id: doc.id, name: doc.name, tier: doc.tier, packs: value('map_monster_packs'), life: value('map_monster_life'), damage: value('map_monster_damage') });
}
catalog.sort((a, b) => a.tier - b.tier);
if (catalog.length !== 10) throw new Error('Expected the ten authored Opus map cores');
const controls = { assets: source.assets, equipped: source.root.children.find(node => node.id === 'map-core-equipped'), picker: source.root.children.find(node => node.id === 'map-core-selector') };
await fs.writeFile(path.join(tool, 'src/assets/rulestudio-hud/map-core-controls.js'), `// Generated from UIAuthoring/MapCoreSelector.uialpha.json and Content/Rules/Equipment.\nexport const MAP_CORE_CONTROLS = ${JSON.stringify(controls)};\nexport const MAP_CORE_CATALOG = ${JSON.stringify(catalog)};\n`);
const { installMapCoreSelector } = await import(pathToFileURL(path.join(tool, 'src/assets/rulestudio-hud/map-core-selector.js')));
const primary = new ProjectRepository(path.join(tool, 'data/projects'));
const id = 'hud-darksteel-20261024', previous = await primary.read(id);
if (!previous) throw new Error('Existing full RuleStudio3D HUD not found');
const backup = path.join(root, 'Saved/UIAuthoringBackups'); await fs.mkdir(backup, { recursive: true });
const backupFile = path.join(backup, `hud-before-map-cores-revision-${previous.revision}.json`);
try { await fs.writeFile(backupFile, JSON.stringify(previous), { flag: 'wx' }); } catch (error) { if (error.code !== 'EEXIST') throw error; }
const updated = structuredClone(previous.document);
updated.presentation.mapCores = catalog;
const document = installMapCoreSelector(updated);
const errors = validateDocument(document); if (errors.length) throw new Error(errors.join('\n'));
if ((await primary.pointer(id)).revision !== previous.revision) throw new Error('The HUD was edited during sync; retry with its latest revision');
const saved = await primary.write(document, previous.revision);
const mirror = new ProjectRepository(path.join(root, 'UIAuthoring/projects'));
const pointer = await mirror.pointer(id); await mirror.write(document, pointer?.revision || 0);
await fs.writeFile(path.join(root, 'UIAuthoring/RuleStudio3D.uialpha.json'), JSON.stringify(document, null, 2));
console.log(JSON.stringify({ project: id, revision: saved.revision, mapCores: catalog.length, backup: backupFile, validationErrors: errors.length }));
