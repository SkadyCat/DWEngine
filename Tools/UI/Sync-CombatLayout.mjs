import fs from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { ProjectRepository } from '../../CustomTools/UICreatorAlpha/server/project-repository.mjs';
import { validateDocument } from '../../CustomTools/UICreatorAlpha/src/core/model.js';
import { installCombatLayout } from '../../CustomTools/UICreatorAlpha/src/assets/rulestudio-hud/combat-layout.js';
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const primary = new ProjectRepository(path.join(root, 'CustomTools/UICreatorAlpha/data/projects'));
const id = 'hud-darksteel-20261024', previous = await primary.read(id);
if (!previous) throw Error('Existing full HUD is missing');
const backup = path.join(root, `Saved/UIAuthoringBackups/hud-before-combat-layout-revision-${previous.revision}.json`);
await fs.mkdir(path.dirname(backup), { recursive: true });
try { await fs.writeFile(backup, JSON.stringify(previous), { flag: 'wx' }); } catch (e) { if (e.code !== 'EEXIST') throw e; }
const document = installCombatLayout(structuredClone(previous.document));
const errors = validateDocument(document); if (errors.length) throw Error(errors.join('\n'));
if ((await primary.pointer(id)).revision !== previous.revision) throw Error('Concurrent edit; retry with latest HUD');
const saved = await primary.write(document, previous.revision);
const mirror = new ProjectRepository(path.join(root, 'UIAuthoring/projects'));
await mirror.write(document, (await mirror.pointer(id))?.revision || 0);
for (const target of ['UIAuthoring/RuleStudio3D.uialpha.json', 'CustomTools/UICreatorAlpha/Screen/RuleStudio-HUD/hud.uialpha.json'])
  await fs.writeFile(path.join(root, target), JSON.stringify(document, null, 2));
console.log(JSON.stringify({ project: id, revision: saved.revision, backup, layout: document.presentation.combatLayout }));
