import { UIRuntime } from '../src/runtime/runtime.js';
import { SvgRenderer, measureText } from '../src/render/svg-renderer.js';
import { expandDocument } from '../src/core/prefabs.js';
import { computeLayout } from '../src/core/layout.js';
import { ENTITY_STATUSES } from '../src/assets/world-overlays/entity-overlays.js';
const record = await fetch('/api/projects/opus-world-overlays-20261025').then(r => r.json()), project = record.document;
const root = document.querySelector('#lab'), ns = root.namespaceURI, views = [];
function label(value, x, y) { const text = document.createElementNS(ns, 'text'); text.setAttribute('x', x); text.setAttribute('y', y); text.setAttribute('fill', '#e3ebf4'); text.setAttribute('font-size', 14); text.textContent = value; root.append(text); }
function mount(template, x, y) {
  const source = structuredClone(project), node = source.root.children.find(n => n.id === template); source.root = node; node.slot.offsets.left = node.slot.offsets.top = 0; source.viewport = { width: node.layout.width, height: node.layout.height, background: 'transparent' };
  const svg = document.createElementNS(ns, 'svg'); svg.setAttribute('x', x); svg.setAttribute('y', y); svg.style.overflow = 'visible'; root.append(svg);
  const runtime = new UIRuntime(svg, source, { width: source.viewport.width, height: source.viewport.height, motion: false, sound: false }); views.push(runtime); return runtime;
}
const hero = mount('hero', 150, 170), monster = mount('monster', 620, 170);
label('玩家 · 绿色气血 / 金色受伤残影 / 护盾', 70, 80); label('怪物 · 红色气血 / 金色受伤残影', 540, 80);
const badges = [];
const overhead = [];
for (const [bar, x, ids] of [[hero, 150, ['chill', 'poison', 'slow']], [monster, 620, ['chill', 'freeze', 'shock']]]) {
  for (const [index, id] of ids.entries()) {
    const view = mount('status-' + id, x, 0);
    for (const n of view.nodes.values()) if (n.binding === 'world-status-seconds') view.set(n.id, { text: '6' }); else if (n.binding === 'world-status-stacks') view.set(n.id, { text: id === 'poison' ? '100' : id === 'slow' ? '3' : '' });
    overhead.push({ bar, x, index, count: ids.length, view });
  }
}
for (const [index, s] of ENTITY_STATUSES.entries()) {
  label(s.name, 55 + index * 126, 340);
  const runtime = mount('status-' + s.id, 65 + index * 126, 365); badges.push(runtime);
  for (const n of runtime.nodes.values()) if (n.binding === 'world-status-seconds') runtime.set(n.id, { text: String(index + 1) }); else if (n.binding === 'world-status-stacks') runtime.set(n.id, { text: index === 6 ? '100' : index === 5 ? '3' : '' });
}
let active = true;
function refresh() {
  const ratio = Number(document.querySelector('#health').value) / 100, shield = document.querySelector('#shield').checked;
  for (const [runtime, prefix] of [[hero, 'hero'], [monster, 'monster']]) {
    for (const n of runtime.nodes.values()) if (n.binding === 'world-shield' || n.binding === 'world-shield-track') runtime.set(n.id, { visibility: shield ? 'visible' : 'collapsed' });
    for (const [id, entry] of runtime.layout.entries) if (entry.node.binding === 'world-health' || entry.node.binding === 'world-lag' || entry.node.binding === 'world-shield') {
      const group = runtime.renderer.groups.get(id).visual; const width = entry.node.binding === 'world-shield' ? .55 : entry.node.binding === 'world-lag' ? Math.min(1, ratio + .18) : ratio;
      group.style.clipPath = `inset(0 ${(1 - width) * 100}% 0 0)`;
    }
  }
  for (const { bar, x, index, count, view } of overhead) {
    const anchor = [...bar.layout.entries.values()].find(e => e.node.binding === (shield ? 'world-status-shield' : 'world-status-base'));
    view.svg.setAttribute('x', x + 48 + (index - (count - 1) / 2) * 21 - 14);
    view.svg.setAttribute('y', 170 + anchor.y - 36);
    view.svg.style.opacity = active ? '1' : '0';
  }
  badges.forEach(r => r.svg.style.opacity = active ? '1' : '0');
  document.querySelector('#status').textContent = `UICreator revision ${record.revision} · 68×8 玩家条 / 48×6 怪物条 · 9 种状态 · 19×19 徽标 · 21 px 排列步长\n状态颜色、文字安全区、血条渐变和锚点均来自可编辑的 UICreator 节点。`;
}
document.querySelector('#health').oninput = refresh; document.querySelector('#shield').onchange = refresh;
document.querySelector('#clear').onclick = () => { active = !active; refresh(); };
refresh();
document.querySelector('#export').onclick = async () => {
  const expanded = expandDocument(project), layout = computeLayout(expanded.document, { width: project.viewport.width, height: project.viewport.height, measureText });
  const svg = document.createElementNS(ns, 'svg'); document.querySelector('#source').append(svg); const renderer = new SvgRenderer(svg); renderer.render(expanded.document, layout);
  const job = 'wbp-' + crypto.randomUUID();
  const response = await fetch('/api/native-wbp/' + job + '/source', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ project, svg: renderer.exportSvg(), context: { width: layout.width, height: layout.height, name: project.name, entries: [...layout.entries], expandedRoot: expanded.document.root } }) });
  const saved = await response.json(); if (!response.ok) throw new Error(saved.error);
  document.querySelector('#status').textContent = '已冻结源：' + job;
  const link = document.createElement('a'); link.href = saved.url; link.textContent = '生成逐控件 UE 导入包'; document.querySelector('#status').append(' · ', link);
};
