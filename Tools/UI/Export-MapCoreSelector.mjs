// Offline UICreator authoring/export only; the website loads the exported SVGs.
import fs from 'node:fs/promises';
import path from 'node:path';
import crypto from 'node:crypto';
import { fileURLToPath } from 'node:url';
import { createDocument, createNode, validateDocument } from '../../CustomTools/UICreatorAlpha/src/core/model.js';
import { ProjectRepository } from '../../CustomTools/UICreatorAlpha/server/project-repository.mjs';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const directory = path.join(root, 'CustomTools/RuleStudio/public/uicreator/map-cores');
const document = createDocument('RuleStudio3D · 地图核心装配');
document.id = 'opus-map-core-selector';
document.viewport = { width: 640, height: 640, background: '#090c12' };
document.root.id = 'map-core-authoring';
const svg = (w, h, body) => `<svg xmlns="http://www.w3.org/2000/svg" width="${w}" height="${h}" viewBox="0 0 ${w} ${h}" preserveAspectRatio="none">${body}</svg>`;
const card = (fill, border) => svg(272, 44, `<rect x=".5" y=".5" width="271" height="43" rx="4" fill="${fill}" stroke="${border}"/>`);
const sources = {
  'card-normal': [272, 44, card('#141d29', '#354459')],
  'card-hover': [272, 44, card('#202b39', '#968054')],
  'card-selected': [272, 44, card('#20312e', '#658d73')],
  core: [32, 32, svg(32, 32, '<defs><linearGradient id="gold" x2="0" y2="1"><stop stop-color="#ffe5a4"/><stop offset="1" stop-color="#b49350"/></linearGradient></defs><rect x=".5" y=".5" width="31" height="31" rx="5" fill="#0d151e" stroke="#756441"/><path d="m16 4 9 6v12l-9 6-9-6V10z" fill="#3a3021" stroke="url(#gold)"/><path d="m16 8 6 8-6 8-6-8z" fill="url(#gold)"/><path d="m16 8-1 8-5 0m5 0 1 8 6-8z" fill="#fff0bf" fill-opacity=".3"/>')],
};
for (const [id, [w, h, source]] of Object.entries(sources)) document.assets[id] = { id, name: id, viewBox: [0, 0, w, h], source, scaleMode: 'stretch', slice: { top: 0, right: 0, bottom: 0, left: 0 }, folderId: '' };
function node(type, id, x, y, w, h, props = {}) {
  return createNode(type, { id, name: id, slot: { offsets: { left: x, top: y, right: w, bottom: h } }, layout: { width: w, height: h }, ...props });
}
function label(id, x, y, w, h, binding, text = '', color = '#dce4ef', size = 11) {
  return node('TextBlock', id, x, y, w, h, { binding, text, style: { fontSize: size, color, wrap: false } });
}
function row(id, x, y, w = 272) {
  const content = node('CanvasPanel', `${id}-content`, 0, 0, w, 44);
  content.children.push(node('Svg', `${id}-icon`, 7, 6, 32, 32, { svgAsset: 'core' }),
    label(`${id}-tier`, 23, 29, 17, 12, `${id}:tier`, '', '#ffe3a0', 9),
    label(`${id}-name`, 47, 5, w - 100, 17, `${id}:name`, '', '#e3d4ae'),
    label(`${id}-packs`, 47, 23, w - 100, 15, `${id}:packs`, '', '#98aabd', 10),
    label(`${id}-selected`, w - 48, 13, 41, 18, `${id}:selected`, '', '#85d6b7', 10));
  return node('Button', id, x, y, w, 44, { binding: id, style: { fill: 'transparent' }, brushes: { normal: 'card-normal', hovered: 'card-hover', pressed: 'card-selected', disabled: 'card-normal' }, children: [content] });
}
const picker = node('Border', 'map-core-selector', 80, 34, 292, 566, { style: { fill: '#0c111a', stroke: '#665b40', strokeWidth: 1, radius: 7 } });
const content = node('CanvasPanel', 'map-core-selector-content', 0, 0, 292, 566);
content.children.push(label('map-core-picker-heading', 12, 8, 240, 24, '', '装配地图核心', '#e4cf98', 12),
  label('map-core-picker-note', 12, 40, 268, 20, '', '选择后直接装配，后续波次使用新规模。', '#8d9fb3', 10));
content.children.push(node('Button', 'map-core-picker-close', 256, 8, 24, 24, { style: { fill: 'transparent' }, action: { type: 'map-core', op: 'close' }, children: [label('map-core-picker-close-label', 0, 0, 24, 24, '', '×', '#98aabd', 19)] }));
for (let index = 0; index < 10; index++) content.children.push(row(`map-core-choice-${index}`, 10, 66 + index * 48));
picker.children.push(content);
const equipped = row('map-core-equipped', 392, 100, 188);
equipped.action = { type: 'map-core', op: 'open' };
equipped.children[0].children.push(label('map-core-equipped-arrow', 168, 8, 16, 28, '', '‹', '#c4aa71', 20));
document.root.children.push(picker, equipped);
const errors = validateDocument(document); if (errors.length) throw new Error(errors.join('\n'));
await fs.mkdir(directory, { recursive: true });
const assets = [];
for (const [id, [width, height, source]] of Object.entries(sources)) {
  await fs.writeFile(path.join(directory, `${id}.svg`), source);
  assets.push({ id, file: `${id}.svg`, width, height, sha256: crypto.createHash('sha256').update(source).digest('hex') });
}
await fs.writeFile(path.join(directory, 'manifest.json'), JSON.stringify({ projectId: document.id, format: 'uicreator-svg-controls/v1', dataFree: true, assets }, null, 2));
await fs.writeFile(path.join(root, 'UIAuthoring/MapCoreSelector.uialpha.json'), JSON.stringify(document, null, 2));
const repository = new ProjectRepository(path.join(root, 'UIAuthoring/projects'));
const previous = await repository.pointer(document.id);
const record = await repository.write(document, previous?.revision || 0);
console.log(JSON.stringify({ project: document.id, revision: record.revision, controls: assets.length, nodeValidationErrors: errors.length }));
