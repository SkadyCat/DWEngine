import fs from 'node:fs/promises';
import path from 'node:path';
import assert from 'node:assert/strict';
import {createMapEventMarkerProject} from '../../CustomTools/UICreatorAlpha/src/assets/rulestudio-hud/map-event-markers.js';
import {validateDocument} from '../../CustomTools/UICreatorAlpha/src/core/model.js';
import {ProjectRepository} from '../../CustomTools/UICreatorAlpha/server/project-repository.mjs';
const root = path.resolve(import.meta.dirname, '../..');
const config = JSON.parse(await fs.readFile(path.join(root, 'Content/MapEvents/events.json'), 'utf8'));
await fs.writeFile(path.join(root, 'CustomTools/RuleStudio/public/data/map-events-config.mjs'),
  '// Generated from Content/MapEvents/events.json by Tools/UI/Author-MapEventMarkers.mjs.\nexport const MAP_EVENT_CONFIG = ' + JSON.stringify(config, null, 2) + ';\n');
const doc = createMapEventMarkerProject(), errors = validateDocument(doc);
if (errors.length) throw Error(errors.join('\n'));
await fs.writeFile(path.join(root, 'UIAuthoring/MapEventMarkers.uialpha.json'), JSON.stringify(doc, null, 2) + '\n');
const repo = new ProjectRepository(path.join(root, 'CustomTools/UICreatorAlpha/data/projects'));
const previous = await repo.pointer(doc.id), saved = await repo.write(doc, previous?.revision || 0);
const cold = await repo.read(doc.id);
assert.deepEqual(cold.document, doc, 'UICreator cold read mismatch');
console.log(JSON.stringify({id: doc.id, revision: saved.revision, kinds: doc.root.source.kinds, coldRead: true}));
