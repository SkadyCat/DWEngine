import { HUD_ASSETS, HUD_PROJECT_ID, HUD_PREFABS, loadHudProject } from '/src/assets/hud-darksteel/project.js';
import { UIRuntime } from '/src/runtime/runtime.js';
import { validateDocument } from '/src/core/model.js';
import { svgElement, scopedSvgContent, assetGraphic, parseSvg } from '/src/assets/svg.js';
import { download } from '/src/io/project.js';
import { attachPresentation } from '/src/runtime/battle-hud.js';

const $ = selector => document.querySelector(selector);
const response = await fetch(`/api/projects/${HUD_PROJECT_ID}`);
const project = response.ok ? (await response.json()).document : await loadHudProject();
const errors = validateDocument(project); if (errors.length) throw new Error(errors.join('\n'));
const referenceProject = await loadHudProject();
const native = new UIRuntime($('#native-ui'), referenceProject, { prefabId: HUD_PREFABS.bar });
const screen = new UIRuntime($('#screen-ui'), project, { prefabId: HUD_PREFABS.screen, width: 1600, height: 900 });
$('#inventory').textContent = `${Object.keys(project.assets).length} SVG 资产 · ${Object.keys(project.prefabs).length} 预制体`;
$('#validation').textContent = `${screen.layout.entries.size} 节点 · ${native.renderer.textWarnings.length} 文字溢出`;
const activation = runtime => event => {
  const name = event.detail.node.name, match = name.match(/^HUD_Ability_(\d)$/);
  if (match) {
    for (const node of runtime.nodes.values()) if (node.id.endsWith('hud-ability-selected')) {
      const prefix = node.id.slice(0, -'hud-ability-selected'.length), button = runtime.nodes.get(prefix + 'hud-ability-button');
      runtime.set(node.id, { visibility: button?.name === name ? 'visible' : 'collapsed' });
    }
    $('#event-status').textContent = `当前选中 ${match[1]} 号技能槽 · 选中框与按钮四态独立`;
  } else $('#event-status').textContent = `触发快捷键 ${name.replace('HUD_Consumable_', '')} · 独立 SVG 按钮`;
};
native.addEventListener('activate', activation(native));
const presentation = attachPresentation(screen, { onStatus: text => $('#event-status').textContent = text });
if (!presentation) screen.addEventListener('activate', activation(screen));
$('#context-toggle').onchange = event => {
  for (const runtime of [native, screen]) for (const entry of runtime.layout.entries.values()) if (entry.node.id.endsWith('hud-context')) runtime.set(entry.node.id, { visibility: event.target.checked ? 'visible' : 'collapsed' });
};
$('#screen-size').onchange = event => { const [w, h] = event.target.value.split(',').map(Number); screen.render(w, h); };
$('#export-native').onclick = () => download('HUD-DarkSteel.svg', native.renderer.exportSvg(), 'image/svg+xml');
window.addEventListener('keydown', event => {
  if (event.repeat || event.ctrlKey || event.metaKey || event.altKey || ['INPUT', 'SELECT', 'TEXTAREA'].includes(event.target.tagName)) return;
  const key = event.key.toUpperCase(); if (!/^[1-4QEFG]$/.test(key)) return;
  const name = /[1-4]/.test(key) ? `HUD_Ability_${key}` : `HUD_Consumable_${key}`;
  for (const runtime of presentation ? [native] : [native, screen]) {
    const entry = [...runtime.layout.entries.values()].find(e => e.node.type === 'Button' && e.node.name === name); if (!entry) continue;
    runtime.states.set(entry.node.id, 'pressed'); runtime.paint(); activation(runtime)({ detail: { node: entry.node } });
    setTimeout(() => { runtime.states.delete(entry.node.id); runtime.paint(); }, 160);
  }
});
for (const definition of HUD_ASSETS) {
  const asset = project.assets[definition.id], card = document.createElement('article'); card.className = 'asset-tile';
  const demo = document.createElement('div'); demo.className = 'asset-demo';
  const svg = svgElement('svg', { viewBox: `0 0 ${asset.viewBox[2]} ${asset.viewBox[3]}`, width: asset.viewBox[2], height: asset.viewBox[3], preserveAspectRatio: 'xMidYMid meet', 'aria-label': asset.name });
  const namespace = `catalog_${asset.id}_`, defs = svgElement('defs'); defs.append(scopedSvgContent(asset, namespace).group); svg.append(defs, assetGraphic(asset, asset.viewBox[2], asset.viewBox[3], namespace)); demo.append(svg);
  const info = document.createElement('div'); info.className = 'asset-info'; const name = document.createElement('strong'); name.textContent = asset.name;
  const link = document.createElement('a'); link.href = `/src/assets/hud-darksteel/svg/${definition.file}`; link.textContent = definition.file; link.target = '_blank';
  const details = document.createElement('small'); details.textContent = `${asset.viewBox[2]} × ${asset.viewBox[3]} · ${asset.scaleMode === 'nine-slice' ? '9 Slice' : asset.scaleMode}`;
  info.append(name, link, details); card.append(demo, info); $('#assets').append(card);
}
$('#hierarchy').textContent = '⬡ HUD · 暗钢 / 1600 × 900\n  └─ 底部中央 · 等比适应\n      └─ ⬡ 暗钢快捷栏 / 688 × 170\n          ├─ 底板 / 折面 / 装饰（独立 SVG）\n          ├─ ⬡ 技能栏 1–4\n          │   └─ ⬡ 技能槽 × 4\n          │       ├─ 四态 SVG\n          │       ├─ 按键 / 名称 / 装备标记\n          │       └─ 技能图标 / 加号 / 选中描边\n          ├─ ⬡ 消耗品栏 Q E F G\n          │   └─ ⬡ 快捷键槽 × 4\n          └─ 操作提示（独立文字）';

if (new URLSearchParams(location.search).has('verify')) {
  await document.fonts.ready;
  const checks = [], assert = (name, condition) => { checks.push({ name, pass: !!condition }); };
  assert('参考快照保留 20 个独立 SVG 资产', Object.keys(referenceProject.assets).length === 20);
  assert('参考快照保留 6 个嵌套预制体', Object.keys(referenceProject.prefabs).length === 6);
  for (const asset of Object.values(project.assets)) { const parsed = parseSvg(asset.source); assert(`矢量有效: ${asset.name}`, !parsed.removed && !parsed.root.querySelector('image,foreignObject,text')); }
  const buttons = [...native.layout.entries.values()].filter(e => e.node.type === 'Button');
  assert('8 个独立按钮', buttons.length === 8);
  for (const e of buttons) {
    const n = e.node.name, key = n.at(-1), x = n.startsWith('HUD_Ability_') ? 42 + (Number(key) - 1) * 80 : 374 + ['Q', 'E', 'F', 'G'].indexOf(key) * 70;
    assert(`${n} 原图位置`, e.globalX === x && e.globalY === (n.startsWith('HUD_Ability_') ? 75 : 83));
    assert(`${n} 四态资产互异`, new Set(Object.values(e.node.brushes)).size === 4);
  }
  const glyphErrors = [];
  for (const text of $('#native-ui').querySelectorAll('[data-node-id] > g > text')) {
    const entry = native.layout.entries.get(text.closest('[data-node-id]').dataset.nodeId); if (!entry.visible) continue;
    const b = text.getBBox(); if (b.x < -.5 || b.y < -.5 || b.x + b.width > entry.width + .5 || b.y + b.height > entry.height + .5) glyphErrors.push(`${entry.node.name} (${b.x.toFixed(2)},${b.y.toFixed(2)},${b.width.toFixed(2)},${b.height.toFixed(2)}) / ${entry.width}×${entry.height}`);
  }
  assert(`可见文字真实字形处于安全区内${glyphErrors.length ? ': ' + glyphErrors.join('; ') : ''}`, glyphErrors.length === 0);
  for (const [w, h] of [[1600, 900], [2048, 780], [1280, 720]]) {
    screen.render(w, h); const bar = [...screen.layout.entries.values()].find(e => e.node.id.endsWith(presentation ? 'rs-hotbar' : 'hud-bar-root'));
    assert(`${w}×${h} 底部中央与等比`, Math.abs(bar.globalX + bar.globalWidth / 2 - w / 2) < .1 && Math.abs(bar.globalY + bar.globalHeight - (h - (presentation ? 28 : 20))) < .1 && Math.abs(bar.globalWidth / bar.globalHeight - (presentation ? 606 / 74 : 688 / 170)) < .001);
  }
  screen.render(1600, 900);
  const first = buttons.find(e => e.node.name === 'HUD_Ability_1'), group = [...$('#native-ui').querySelectorAll('[data-node-id]')].find(e => e.dataset.nodeId === first.node.id);
  const original = group.querySelector('use').getAttribute('href'); group.dispatchEvent(new PointerEvent('pointerover', { bubbles: true })); const hovered = group.querySelector('use').getAttribute('href'); group.dispatchEvent(new PointerEvent('pointerdown', { bubbles: true })); const pressed = group.querySelector('use').getAttribute('href');
  assert('技能按钮真实切换 Normal / Hover / Pressed', new Set([original, hovered, pressed]).size === 3);
  group.dispatchEvent(new PointerEvent('pointerup', { bubbles: true })); assert('点击技能槽切换独立选中描边', [...native.layout.entries.values()].filter(e => e.node.id.endsWith('hud-ability-selected') && e.visible).length === 1);
  activation(native)({ detail: { node: { name: 'HUD_Ability_4' } } });
  native.states.clear(); native.paint();
  assert('导出无位图与 foreignObject', !/<(?:image|foreignObject)\b/.test(native.renderer.exportSvg()));
  const failed = checks.filter(c => !c.pass); $('#qa-results').hidden = false; $('#qa-results').dataset.result = failed.length ? 'failed' : 'passed'; $('#qa-results').classList.toggle('fail', !!failed.length); $('#qa-results').textContent = checks.map(c => `${c.pass ? 'PASS' : 'FAIL'} ${c.name}`).join('\n') + `\n\n${checks.length - failed.length}/${checks.length} passed`;
  $('#validation').textContent = `${checks.length - failed.length}/${checks.length} 浏览器检查通过`;
}
