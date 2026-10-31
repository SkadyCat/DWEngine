import { starterProject } from '/src/assets/starter.js';
import { parseSvg, makeSvgAsset, assetGraphic, svgElement } from '/src/assets/svg.js';
import { expandDocument } from '/src/core/prefabs.js';
import { computeLayout } from '/src/core/layout.js';
import { SvgRenderer, measureText } from '/src/render/svg-renderer.js';
import { UIRuntime } from '/src/runtime/runtime.js';
import { importHtml } from '/src/io/html-import.js';
import { portableHtml } from '/src/editor/dialogs.js';
import { createDocument, createNode } from '/src/core/model.js';

const results = [], output = document.querySelector('#results'), fixtures = document.querySelector('#fixtures');
const assert = (condition, message) => { if (!condition) throw new Error(message); };
async function test(name, action) {
  const start = performance.now();
  try { await action(); results.push({ name, status: 'PASS', ms: +(performance.now() - start).toFixed(1) }); }
  catch (e) { results.push({ name, status: 'FAIL', error: e.message }); }
  output.textContent = results.map(r => `${r.status}  ${r.name}  ${r.error || r.ms + ' ms'}`).join('\n');
}
const scene = () => { const svg = svgElement('svg'); fixtures.append(svg); return svg; };
await test('SVG CSS classes and inline styles become safe vector presentation attributes', () => {
  const parsed = parseSvg('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 40 40"><style>.paint{fill:#88cdaa;stroke:#234}</style><rect class="paint" style="opacity:.5" width="40" height="40"/></svg>');
  const fill = parsed.root.querySelector('rect').getAttribute('fill');
  assert(['#88cdaa', 'rgb(136, 205, 170)'].includes(fill), `class fill lost: ${fill} (${parsed.removed} removed)`); assert(parsed.root.querySelector('rect').getAttribute('opacity') === '0.5', 'inline opacity lost'); assert(!parsed.root.querySelector('style'), 'stylesheet leaked');
});
await test('SVG rejects executable markup and external references', () => {
  const parsed = parseSvg('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 40 40" onload="alert(1)"><script>alert(1)</script><foreignObject/><image href="https://example.com/a.png"/><rect width="40" height="40" fill="url(https://example.com/a.svg)"/></svg>');
  assert(!parsed.root.querySelector('script,foreignObject,[onload]'), 'unsafe markup retained'); assert(!parsed.root.querySelector('image').hasAttribute('href'), 'external href retained');
});
await test('9-slice produces nine clipped viewports and retains fixed corner dimensions', () => {
  const a = makeSvgAsset('test', '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 96 96"><rect width="96" height="96"/></svg>', { scaleMode: 'nine-slice' });
  const g = assetGraphic(a, 320, 150); assert(g.children.length === 9, 'slice count'); assert(g.children[0].getAttribute('width') === '12', 'corner stretched'); assert(g.children[4].getAttribute('width') === '296', 'middle not stretched');
});
await test('two renderers scope SVG asset and clip IDs independently', () => {
  const doc = starterProject(), expanded = expandDocument(doc).document, layout = computeLayout(expanded, { measureText });
  const a = new SvgRenderer(scene()), b = new SvgRenderer(scene()); a.render(expanded, layout); b.render(expanded, layout);
  assert(a.svg.querySelector('defs > g').id !== b.svg.querySelector('defs > g').id, 'asset IDs collide');
  const group = a.svg.querySelector('[data-node-type="Button"]'), old = group;
  a.render(expanded, layout, { forcedState: 'hovered' }); assert(a.svg.querySelector('[data-node-type="Button"]') === old, 'SVG group replaced during state change');
  const exported = a.exportSvg(); assert(!exported.includes('foreignObject') && !exported.includes('data-hit') && !exported.includes('data-editor-overlay'), 'export is polluted');
});
await test('starter text fits actual SVG glyph bounds at 1440×900 and 2048×780', () => {
  const doc = expandDocument(starterProject()).document;
  for (const [width, height] of [[1440, 900], [2048, 780]]) {
    const svg = scene(), layout = computeLayout(doc, { width, height, measureText }), renderer = new SvgRenderer(svg); renderer.render(doc, layout);
    for (const text of svg.querySelectorAll('[data-node-id] > g > text')) {
      const id = text.closest('[data-node-id]').getAttribute('data-node-id'), entry = layout.entries.get(id), box = text.getBBox();
      assert(box.x >= -1 && box.y >= -1 && box.x + box.width <= entry.width + 1 && box.y + box.height <= entry.height + 1, `glyph outside ${entry.node.name} at ${width}×${height}`);
    }
    assert(!renderer.textWarnings.length, 'text warning in starter');
  }
});
await test('runtime renders four states, activates once, and does not mutate source project', () => {
  const project = starterProject(), before = JSON.stringify(project), svg = scene(), runtime = new UIRuntime(svg, project, { prefabId: 'prefab-action' });
  const button = svg.querySelector('[data-node-type="Button"]'), normal = button.querySelector('use').getAttribute('href'); let events = 0; runtime.addEventListener('activate', () => events++);
  button.dispatchEvent(new PointerEvent('pointerover', { bubbles: true })); const hover = button.querySelector('use').getAttribute('href');
  button.dispatchEvent(new PointerEvent('pointerdown', { bubbles: true })); const pressed = button.querySelector('use').getAttribute('href');
  button.dispatchEvent(new PointerEvent('pointerup', { bubbles: true })); assert(events === 1, 'activation count');
  runtime.set(button.dataset.nodeId, { interactable: false }); const disabled = button.querySelector('use').getAttribute('href'); assert(new Set([normal, hover, pressed, disabled]).size === 4, 'state asset did not change');
  assert(JSON.stringify(project) === before, 'runtime changed project source'); runtime.destroy();
});
await test('HTML import preserves hierarchy, editable text and inline SVG assets', async () => {
  const html = await (await fetch('/examples/import.html')).text(), result = await importHtml(html), expanded = expandDocument(result.document).document, layout = computeLayout(expanded, { measureText }), renderer = new SvgRenderer(scene()); renderer.render(expanded, layout);
  assert(layout.entries.size > 15, 'HTML flattened into an image'); assert(Object.keys(result.document.assets).length === 3, 'SVG assets missing'); assert(renderer.svg.textContent.includes('探索，从这里开始。'), 'editable text missing');
  assert(!renderer.exportSvg().includes('foreignObject'), 'HTML wrapped as foreignObject');
});
await test('portable HTML is self-contained and switches button states without the server', async () => {
  const project = starterProject(), expanded = expandDocument(project).document, layout = computeLayout(expanded, { measureText }), renderer = new SvgRenderer(scene()); renderer.render(expanded, layout);
  const html = portableHtml({ store: { document: project }, renderer, layout });
  const frame = document.createElement('iframe'); frame.width = '1440'; frame.height = '900';
  const loaded = new Promise(resolve => { frame.onload = resolve; }); frame.srcdoc = html; fixtures.append(frame); await loaded;
  const inner = frame.contentDocument, button = inner.querySelector('g[role="button"]');
  assert(!inner.querySelector('script[src],link[href]'), 'offline HTML has external dependencies'); assert(inner.querySelectorAll('foreignObject').length === 0, 'offline HTML contains foreignObject');
  button.dispatchEvent(new frame.contentWindow.PointerEvent('pointerenter'));
  assert(button.querySelector('[data-state="hovered"]').style.display === '', 'offline hover did not switch SVG');
  button.dispatchEvent(new frame.contentWindow.PointerEvent('pointerdown', { bubbles: true }));
  assert(button.querySelector('[data-state="pressed"]').style.display === '', 'offline press did not switch SVG');
  assert(inner.querySelector('#uicreator-source').textContent.includes('uicreator-alpha'), 'project source missing');
  frame.remove();
});
await test('runtime can update and reveal an initially collapsed node without changing its template', () => {
  const project = createDocument(), label = createNode('TextBlock', { text: 'template', visibility: 'collapsed' }); project.root.children = [label];
  const runtime = new UIRuntime(scene(), project); assert(!runtime.layout.entries.has(label.id), 'collapsed node should not reserve layout');
  runtime.setText(label.id, 'revealed'); runtime.set(label.id, { visibility: 'visible' });
  assert(runtime.layout.entries.get(label.id)?.visible, 'collapsed node cannot be revealed'); assert(runtime.svg.textContent.includes('revealed'), 'runtime text patch missing');
  assert(label.text === 'template' && label.visibility === 'collapsed', 'runtime changed source'); runtime.destroy();
});
const failed = results.filter(r => r.status === 'FAIL');
output.textContent += `\n\n${results.length - failed.length}/${results.length} passed`;
output.dataset.result = failed.length ? 'failed' : 'passed'; output.classList.toggle('fail', !!failed.length);
fixtures.replaceChildren();
