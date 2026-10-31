import { SvgRenderer, measureText } from '../src/render/svg-renderer.js';
import { computeLayout } from '../src/core/layout.js';
import { expandDocument } from '../src/core/prefabs.js';
import { UIRuntime } from '../src/runtime/runtime.js';
import { BattleHudController } from '../src/runtime/battle-hud.js';

const svgNS = 'http://www.w3.org/2000/svg', report = document.querySelector('#report');
const record = await fetch('/api/projects/hud-darksteel-20261024').then(r => r.json());
const project = record.document, results = [];
async function raster(source) {
  const url = URL.createObjectURL(new Blob([source], { type: 'image/svg+xml' })), image = new Image();
  try {
    await new Promise((resolve, reject) => { image.onload = resolve; image.onerror = reject; image.src = url; });
    const canvas = document.createElement('canvas'); canvas.width = 1600; canvas.height = 900;
    canvas.getContext('2d').drawImage(image, 0, 0); return canvas;
  } finally { URL.revokeObjectURL(url); }
}
function authored(legacy = false) {
  const source = structuredClone(project); source.viewport.background = 'transparent';
  if (legacy) source.prefabs['hud-prefab-ability'].root.children[0].children.find(n => n.id === 'hud-ability-flame').style.clip = false;
  const expanded = expandDocument(source).document, layout = computeLayout(expanded, { width: 1600, height: 900, measureText });
  const svg = document.createElementNS(svgNS, 'svg'), renderer = new SvgRenderer(svg);
  renderer.render(expanded, layout);
  return { renderer, layout };
}
const before = authored(true), after = authored(), icon = [...after.layout.entries.values()].find(e => e.node.id.endsWith('hud-ability-flame') && e.visible);
const x = Math.round(icon.globalX), y = Math.round(icon.globalY);
const legacyPng = await raster(before.renderer.exportSvg()), currentPng = await raster(after.renderer.exportSvg());
const sample = (canvas, dx, dy) => [...canvas.getContext('2d').getImageData(x + dx, y + dy, 1, 1).data];
const key = e => e.node.id.endsWith('hud-ability-flame');
const runtimeSvg = document.createElementNS(svgNS, 'svg'); document.querySelector('#source').append(runtimeSvg);
const runtime = new UIRuntime(runtimeSvg, project, { width: 1600, height: 900, motion: false, sound: false });
const controller = new BattleHudController(runtime, { keyboard: false });
for (let slot = 0; slot < 4; slot++) {
  controller.state.select(slot, true); controller.state.assign('poison_flask'); controller.refresh();
  const entry = [...runtime.layout.entries.values()].find(e => key(e) && e.visible);
  const group = runtime.renderer.groups.get(entry.node.id).group;
  const selected = [...runtime.layout.entries.values()].find(e => e.node.id.endsWith('hud-ability-selected') && e.visible);
  results.push({ slot, clip: entry.node.style.clip, radius: entry.node.style.radius, width: entry.width, height: entry.height,
    pass: entry.node.style.clip === true && entry.node.style.radius === 11.5 && entry.width === 72 && entry.height === 72 && group.hasAttribute('clip-path') && !runtime.renderer.groups.get(selected.node.id).group.hasAttribute('clip-path') });
}
controller.state.setAvailability(controller.state.catalog.map(s => s.id));
controller.state.select(0, true); controller.state.assign('frost_lance'); controller.refresh();
const glyph = [...runtime.layout.entries.values()].find(e => key(e) && e.visible);
results.push({ glyph: 'frost_lance', clip: glyph.node.style.clip, pass: glyph.node.style.clip === false });
const cornerBefore = sample(legacyPng, 0, 70), cornerAfter = sample(currentPng, 0, 70);
// The slot frame may retain a faint stroke/glow; the bright tile must be absent.
results.push({ cornerBefore, cornerAfter, pass: cornerBefore[3] > 200 && cornerAfter[3] < 64 });
const review = document.createElement('canvas'); review.width = 880; review.height = 390;
const ctx = review.getContext('2d'); ctx.fillStyle = '#8091a8'; ctx.fillRect(0, 0, 880, 390); ctx.imageSmoothingEnabled = false; ctx.font = '18px sans-serif';
for (const [i, [canvas, title]] of [[legacyPng, 'Before: unmasked tile'], [currentPng, 'Fixed: clipped artwork / intact glow']].entries()) {
  ctx.fillStyle = '#111820'; ctx.fillText(title, 18 + i * 440, 28); ctx.drawImage(canvas, x - 10, y - 10, 94, 94, 25 + i * 440, 48, 320, 320);
}
document.querySelector('#images').append(review);
const passed = results.filter(r => r.pass).length;
report.textContent = JSON.stringify({ revision: record.revision, passed, total: results.length, results }, null, 2);
if (passed !== results.length) throw new Error('Skill icon clipping regression failed');
const job = 'wbp-skill-clip-' + crypto.randomUUID();
const sourceHash = [...new Uint8Array(await crypto.subtle.digest('SHA-256', new TextEncoder().encode(JSON.stringify(results))))].map(n => n.toString(16).padStart(2, '0')).join('');
const bundle = { schema: 'uicreator-alpha/native-wbp', version: 1, sourceHash, project: { assets: {} }, nodes: [], states: { canvas: { nodes: [] } }, rasters: {}, references: { comparison: { png: review.toDataURL('image/png').split(',')[1] } }, pixelRegression: { passed, total: results.length, results } };
const saved = await fetch('/api/native-wbp/' + job, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(bundle) });
report.textContent += '\n' + JSON.stringify({ job, saved: saved.ok });
controller.destroy(); runtime.destroy();
