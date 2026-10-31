import { UIRuntime } from '../src/runtime/runtime.js';
import { BattleHudController } from '../src/runtime/battle-hud.js';
import { walk, indexTree, createNode } from '../src/core/model.js';
import { computeLayout } from '../src/core/layout.js';
import { SvgRenderer, measureText } from '../src/render/svg-renderer.js';
import { materializeMotionAssets, motionAssetId } from '../src/core/motion.js';
import { materializeSounds } from '../src/assets/ui-sounds.js';
import { defaultAudio } from '../src/core/audio.js';

const $ = s => document.querySelector(s), job = new URLSearchParams(location.search).get('job') || 'wbp-hud-20261025';
const base = `/exports/${job}`;
const [record, context, sourceSvg] = await Promise.all([fetch(`${base}/project-record.json`).then(r => r.json()), fetch(`${base}/canvas-context.json`).then(r => r.json()), fetch(`${base}/current-canvas.svg`).then(r => r.text())]);
const project = materializeSounds(materializeMotionAssets(record.document)), width = context.width, height = context.height;
const clean = source => {
  const doc = new DOMParser().parseFromString(source, 'image/svg+xml');
  if (doc.querySelector('parsererror')) throw new Error('Frozen canvas SVG failed to parse');
  doc.querySelectorAll('[data-editor-overlay],[data-hit]').forEach(e => e.remove()); return doc.documentElement;
};
const frozen = clean(sourceSvg); frozen.id = 'scene'; $('#preview').append(document.importNode(frozen, true));
$('#size').textContent = `${width} × ${height} · ${record.revision == null ? '当前网页快照' : `revision ${record.revision}`}`;
$('#status').textContent = '使用当前画布冻结快照；保留逐控件层级、源 SVG 和按钮四态。';
const runtime = new UIRuntime($('#runtime'), project, { width, height, motion: false, sound: false });
// Reuse only caches with the identical content-and-crop SHA; references are
// always rendered again from the current frozen source.
const cacheJob = new URLSearchParams(location.search).get('cacheJob');
if (cacheJob && !/^wbp-[a-zA-Z0-9_-]{1,90}$/.test(cacheJob)) throw Error('Invalid raster cache job');
const previousRasters = cacheJob ? (await fetch(`/exports/${cacheJob}/bundle.json`).then(r=>{if(!r.ok)throw Error('Missing raster cache job');return r.json();})).rasters : {};
const serialize = node => new XMLSerializer().serializeToString(node);
const normalized = source => source.replace(/r[a-f0-9]{32}_/g, 'native_');
const sha = async text => [...new Uint8Array(await crypto.subtle.digest('SHA-256', new TextEncoder().encode(text)))].map(n => n.toString(16).padStart(2, '0')).join('');
const canvas = document.createElement('canvas'), cropCanvas = document.createElement('canvas');
// Frozen PNG atlas resources must survive SVG Blob rasterization.
const imageResources = new Map();
async function inlineFrozenImages(root) {
 const ns='http://www.w3.org/2000/svg', defs=document.createElementNS(ns,'defs'), symbols=new Map();
 for(const img of [...root.querySelectorAll('image')]) {
  const href=img.getAttribute('href')||img.getAttribute('xlink:href')||'';
  if(!href.startsWith('/exports/'))continue;
  if(!/^\/exports\/wbp-[a-zA-Z0-9_-]{1,90}\/source-assets\/[a-zA-Z0-9_-]+\.png$/.test(href))throw Error('Invalid frozen image resource');
  if(!imageResources.has(href))imageResources.set(href,fetch(href).then(async r=>{if(!r.ok)throw Error('Missing frozen PNG '+href);const b=await r.blob();return new Promise(resolve=>{const f=new FileReader();f.onload=()=>resolve(f.result);f.readAsDataURL(b);});}));
  const data=await imageResources.get(href),w=img.getAttribute('width')||1,h=img.getAttribute('height')||1,key=href+':'+w+':'+h;
  if(!symbols.has(key)){const id='frozen-png-'+symbols.size,sym=document.createElementNS(ns,'symbol'),image=img.cloneNode(true);sym.id=id;sym.setAttribute('viewBox',`0 0 ${w} ${h}`);sym.setAttribute('preserveAspectRatio','none');image.setAttribute('href',data);image.removeAttribute('xlink:href');image.setAttribute('x',0);image.setAttribute('y',0);sym.append(image);defs.append(sym);symbols.set(key,id);}
  const use=document.createElementNS(ns,'use');for(const a of img.attributes)if(!['href','xlink:href'].includes(a.name))use.setAttribute(a.name,a.value);use.setAttribute('href','#'+symbols.get(key));img.replaceWith(use);
 }
 if(symbols.size)root.prepend(defs);
}
async function raster(svg, w, h, x = 0, y = 0, trim = true) {
  const source = svg.cloneNode(true); source.removeAttribute('id'); source.setAttribute('width', w); source.setAttribute('height', h); source.setAttribute('viewBox', `${x} ${y} ${w} ${h}`); source.setAttribute('xmlns', 'http://www.w3.org/2000/svg');
  await inlineFrozenImages(source);
  const url = URL.createObjectURL(new Blob([serialize(source)], { type: 'image/svg+xml' })), image = new Image();
  try {
    await new Promise((resolve, reject) => { image.onload = resolve; image.onerror = () => reject(new Error('SVG rasterization failed')); image.src = url; });
    canvas.width = w; canvas.height = h; const ctx = canvas.getContext('2d', { willReadFrequently: true }); ctx.clearRect(0, 0, w, h); ctx.drawImage(image, 0, 0, w, h);
    let l = 0, t = 0, r = w - 1, b = h - 1;
    if (trim) {
      l = w; t = h; r = -1; b = -1; const pixels = ctx.getImageData(0, 0, w, h).data;
      for (let yy = 0; yy < h; yy++) for (let xx = 0; xx < w; xx++) if (pixels[(yy * w + xx) * 4 + 3]) { l = Math.min(l, xx); r = Math.max(r, xx); t = Math.min(t, yy); b = Math.max(b, yy); }
      if (r < l) return null;
    }
    cropCanvas.width = r - l + 1; cropCanvas.height = b - t + 1; cropCanvas.getContext('2d').drawImage(canvas, l, t, r - l + 1, b - t + 1, 0, 0, r - l + 1, b - t + 1);
    return { x: x + l, y: y + t, width: cropCanvas.width, height: cropCanvas.height, png: cropCanvas.toDataURL('image/png').split(',')[1] };
  } finally { URL.revokeObjectURL(url); }
}
function refs(element) {
  const result = new Set();
  for (const e of [element, ...element.querySelectorAll('*')]) for (const a of e.attributes) {
    if (['href', 'xlink:href'].includes(a.name) && a.value.startsWith('#')) result.add(a.value.slice(1));
    for (const m of a.value.matchAll(/url\(\s*["']?#([^\s)'" ]+)["']?\s*\)/g)) result.add(m[1]);
  }
  return result;
}
const exportIdMaps = new WeakMap();
function layerSvg(root, node, replacement = null, extraDefs = null) {
  const output = root.cloneNode(false); output.removeAttribute('id'); const defs = document.createElementNS(root.namespaceURI, 'defs'); output.append(defs);
  let content = replacement ? replacement.cloneNode(true) : node.firstElementChild.cloneNode(true);
  for (let parent = node; parent?.matches?.('[data-node-id]'); parent = parent.parentElement.closest('[data-node-id]')) {
    const wrapper = document.createElementNS(root.namespaceURI, 'g');
    for (const key of ['transform', 'opacity', ...(project.presentation?.independentCaches ? [] : ['clip-path'])]) if (parent.hasAttribute(key)) wrapper.setAttribute(key, parent.getAttribute(key));
    wrapper.append(content); content = wrapper;
  }
  let ids=exportIdMaps.get(root);if(!ids){ids=new Map([...root.querySelectorAll('[id]')].map(e=>[e.id,e]));exportIdMaps.set(root,ids);}if(extraDefs)ids=new Map([...ids,...[...extraDefs.querySelectorAll('[id]')].map(e=>[e.id,e])]);
  const included = [], pending = [...refs(content)], seen = new Set();
  while (pending.length) {
    const id = pending.pop(); if (seen.has(id)) continue; seen.add(id); const element = ids.get(id); if (!element) throw new Error(`Unresolved SVG resource: ${id}`);
    if (!included.some(e => e.contains(element))) { included.push(element); for (const dep of refs(element)) pending.push(dep); }
  }
  for (const e of included) if (!included.some(other => other !== e && other.contains(e))) defs.append(e.cloneNode(true));
  output.append(content); return output;
}

$('#export').onclick = async () => {
  $('#export').disabled = true; const started = performance.now();
  try {
    await document.fonts.ready;
    const bundle = { schema: 'uicreator-alpha/native-wbp', version: 1, project, viewport: { width, height }, sourceSvg: serialize(frozen), sourceHash: await sha(normalized(serialize(frozen))), nodes: [], rasters: {}, states: {}, references: {} };
    const expanded = context.expandedRoot ? { document: { root: context.expandedRoot } } : runtime.expanded;
    const sourceNodes = new Map(), rasterKeys = new Map();
    function register(root) {
      const parents = indexTree(root).parents;
      walk(root, node => { if (!sourceNodes.has(node.id)) { const record = { ...node, audio: node.audio || defaultAudio(node.type, node.binding), motion: { ...node.motion, assetId: motionAssetId(node) }, children: undefined, parentId: parents.get(node.id)?.id || '', brushRasters: {} }; bundle.nodes.push(record); sourceNodes.set(node.id, record); } });
    }
    register(expanded.document.root);
    let count = 0;
    async function getRaster(root, nodeElement, entry, replacement = null, extraDefs = null) {
      if (!replacement && !nodeElement.firstElementChild?.children.length) return '';
      const source = layerSvg(root, nodeElement, replacement, extraDefs), svg = normalized(serialize(source));
      const x = Math.max(0, Math.floor(entry.globalX) - 3), y = Math.max(0, Math.floor(entry.globalY) - 3), w = Math.max(1, Math.min(width - x, Math.ceil(entry.globalX + entry.globalWidth) + 3 - x)), h = Math.max(1, Math.min(height - y, Math.ceil(entry.globalY + entry.globalHeight) + 3 - y));
      if (entry.globalX >= width || entry.globalY >= height || entry.globalX + entry.globalWidth < 0 || entry.globalY + entry.globalHeight < 0) return '';
      const key = await sha(`${x},${y},${w},${h}:${svg}`); if (rasterKeys.has(key)) return rasterKeys.get(key);
      if (previousRasters[key]?.svg === svg) { bundle.rasters[key] = previousRasters[key]; rasterKeys.set(key, key); return key; }
      const image = await raster(source, w, h, x, y); rasterKeys.set(key, image ? key : '');
      if (image) bundle.rasters[key] = { ...image, svg };
      if (++count % 12 === 0) { $('#status').textContent = `正在转换 SVG → PNG：${count} 个控件缓存…`; await new Promise(requestAnimationFrame); }
      return image ? key : '';
    }
    async function capture(name, root, layout, variants = true, reference = true) {
      const state = { nodes: [] }, elementMap = new Map([...root.querySelectorAll('[data-node-id]')].map(e => [e.getAttribute('data-node-id'), e]));
      // Renderer-owned scrollbars paint above the source children. Keep each as
      // an independent overlay node rather than dropping it or baking the page.
      for (const [id, entry] of layout.entries) {
        const element = elementMap.get(id);
        if (!element || entry.node.type !== 'ScrollBox' || ![...element.children].some(child => child.hasAttribute('data-scrollbar'))) continue;
        const overlayId = `${id}::native-scrollbar`;
        if (!sourceNodes.has(overlayId)) {
          const overlay = { ...createNode('Svg', { id: overlayId, name: `${entry.node.name || 'ScrollBox'} · 滚动条`, locked: true, source: { kind: 'renderer-scrollbar' } }), parentId: id, children: undefined, brushRasters: {} };
          bundle.nodes.push(overlay); sourceNodes.set(overlayId, overlay);
        }
      }
      for (const node of bundle.nodes) {
        if (node.source?.kind === 'renderer-scrollbar') {
          const owner = layout.entries.get(node.parentId), element = elementMap.get(node.parentId);
          const bar = element && [...element.children].find(child => child.hasAttribute('data-scrollbar'));
          const visible = !!(owner?.visible && bar && bar.getAttribute('display') !== 'none');
          state.nodes.push({ id: node.id, parentId: node.parentId, layoutScale: owner?.scale || 1, x: owner?.globalX || 0, y: owner?.globalY || 0, width: owner?.globalWidth || 0, height: owner?.globalHeight || 0, visibility: visible ? 'visible' : 'collapsed', localVisibility: visible ? 'visible' : 'collapsed', raster: visible ? await getRaster(root, element, owner, bar) : '', text: '', interactable: false, brushRasters: {}, paintOrder: (owner?.paintChildren?.length || 0) + 1 });
          continue;
        }
        const e = layout.entries.get(node.id), parent = e ? layout.entries.get(e.parentId) : null, element = elementMap.get(node.id);
        const model = name === 'canvas' ? node : name === 'authoring' ? e?.node || node : runtime.nodes.get(node.id) || e?.node || node;
        const result = { id: node.id, parentId: node.parentId, layoutScale: e?.scale || 1, x: e?.globalX || 0, y: e?.globalY || 0, width: e?.globalWidth || 0, height: e?.globalHeight || 0, visibility: e?.visible ? 'visible' : model.visibility === 'hidden' ? 'hidden' : 'collapsed', localVisibility: e ? model.visibility || 'visible' : 'collapsed', raster: '', text: model.text ?? node.text, interactable: model.interactable !== false, brushRasters: {}, paintOrder: parent ? parent.paintChildren.indexOf(node.id) : 0 };
        if (e?.visible && element) result.raster = await getRaster(root, element, e);
        if (variants && e?.visible && element && node.type === 'Button') for (const state of ['normal', 'hovered', 'pressed', 'disabled']) {
          const visual = document.createElementNS(root.namespaceURI, 'g');
          // A disabled default must still export the enabled normal/hover/press skins.
          const model = { ...e.node, interactable: state !== 'disabled' };
          visual.append(...runtime.renderer.paintNode(model, e.width, e.height, project.assets, state, { x: e.globalX, y: e.globalY, scale: e.scale || 1 }));
          result.brushRasters[state] = await getRaster(root, element, e, visual, runtime.renderer.defs);
          if (name === 'canvas') node.brushRasters[state] = result.brushRasters[state];
        }
        // Authored dynamic brush families are exported at each live node's exact
        // geometry, so native data changes use UICreator artwork for every rarity.
        if(e?.visible && element && model.source?.dynamicBrushes && !node.dynamicBrushRasters) {
          node.dynamicBrushOrigin={x:e.globalX,y:e.globalY};node.dynamicBrushRasters={};
          for(const [key,brushes] of Object.entries(model.source.dynamicBrushes)) {
            const cached=node.dynamicBrushRasters[key]={};
            for(const state of node.type==='Button'?['normal','hovered','pressed','disabled']:['normal']) {
              const variant={...e.node,interactable:true,brushes,svgAsset:brushes.normal};
              const visual=document.createElementNS(root.namespaceURI,'g');
              visual.append(...runtime.renderer.paintNode(variant,e.width,e.height,project.assets,state,{x:e.globalX,y:e.globalY,scale:e.scale||1}));
              cached[state]=await getRaster(root,element,e,visual,runtime.renderer.defs);
            }
          }
        }
        state.nodes.push(result);
      }
      // Expected responsive geometry comes from the authoring layout engine,
      // independently of native Slate. Freeze every exported interface at each size.
      if (reference) {
        bundle.responsiveLayouts ||= {};
        bundle.responsiveLayouts[name] = [[1280,720],[1024,768],[1600,900],[2258,1080],[2560,1080]].map(([width,height]) => {
          const root = name === 'canvas' ? expanded.document.root : runtime.expanded.document.root;
          const measured = computeLayout({ ...project, root }, { width, height, measureText });
          return { width, height, nodes: [...measured.entries].filter(([,e]) => e.visible).map(([id,e]) => ({ id, x:e.globalX, y:e.globalY, width:e.globalWidth, height:e.globalHeight })) };
        });
      }
      // Canvas background is its own source layer, distinct from all interface controls.
      const bg = root.querySelector('[data-background]');
      if (bg) {
        const background = root.cloneNode(false); background.append(bg.cloneNode(true)); const svg = normalized(serialize(background)), key = await sha(svg), image = await raster(background, width, height, 0, 0);
        if (image) { bundle.rasters[key] = { ...image, svg }; state.nodes.find(n => !n.parentId).raster = key; }
      }
      if (reference) { const image = await raster(root, width, height, 0, 0, false); bundle.references[name] = { png: image.png, svg: serialize(root) }; bundle.states[name] = state; }
      else bundle.authoringNodes = state.nodes;
    }
    await capture('canvas', frozen, context.entries ? { entries: new Map(context.entries) } : runtime.layout, true);
    if ($('#states').checked && project.presentation?.controller === 'rulestudio-battle-hud') {
      const controller = new BattleHudController(runtime, { keyboard: false });
      for (let i = 0; i < 4; i++) {
        controller.state.select(i, false); controller.refresh(); register(runtime.expanded.document.root); await capture(`selected-${i + 1}`, clean(runtime.renderer.exportSvg()), runtime.layout);
        controller.state.select(i, true); controller.refresh(); register(runtime.expanded.document.root); await capture(`picker-${i + 1}`, clean(runtime.renderer.exportSvg()), runtime.layout);
      }
      // The interface catalog grows independently of the original HUD panels.
      // Capture every authored page, including pages mounted only when opened.
      const panels = new Set(['inventory', 'character', 'equipment', 'loadout', 'belt', 'hunter', 'settings', 'shop', 'craft', 'tooltip', 'completion', 'death']);
      for (const page of project.interfaceCatalog || []) if (!['normal', 'boss', 'skills'].includes(page.id)) panels.add(page.id);
      controller.state.select(project.presentation.selected ?? 0, false);
      for (const panel of panels) { controller.state.close(); if (!controller.state.open(panel)) throw new Error(`Cannot export HUD interface: ${panel}`); controller.refresh(); register(runtime.expanded.document.root); await capture(panel, clean(runtime.renderer.exportSvg()), runtime.layout); }
      controller.state.close(); controller.state.phase = 'boss'; controller.refresh(); await capture('boss', clean(runtime.renderer.exportSvg()), runtime.layout);
      controller.state.phase = 'waves'; controller.state.bd = true; controller.refresh(); await capture('bd', clean(runtime.renderer.exportSvg()), runtime.layout);
      controller.state.bd = false;
      for (const tab of ['equipment', 'lineage']) { controller.state.mapTab = tab; controller.refresh(); await capture(`map-${tab}`, clean(runtime.renderer.exportSvg()), runtime.layout); }
      if (project.presentation.mapCoreSelectorVersion) { controller.state.activate({ type: 'map-core', op: 'open' }); controller.refresh(); register(runtime.expanded.document.root); await capture('map-core-picker', clean(runtime.renderer.exportSvg()), runtime.layout); }
      controller.destroy();
    }
    // Hidden source widgets still need independent, editable images in Designer.
    // This supplies authoring geometry/caches only; it is never an acceptance image.
    const authoring = structuredClone({ ...project, root: expanded.document.root }); walk(authoring.root, n => { n.visibility = 'visible'; });
    const authoringSvg = document.createElementNS('http://www.w3.org/2000/svg', 'svg'); $('#source').append(authoringSvg);
    const authoringRenderer = new SvgRenderer(authoringSvg), authoringLayout = computeLayout(authoring, { width, height, measureText });
    authoringRenderer.render(authoring, authoringLayout); await capture('authoring', clean(authoringRenderer.exportSvg()), authoringLayout, true, false); authoringSvg.remove();
    for (const state of Object.values(bundle.states)) {
      const ids = new Set(state.nodes.map(n => n.id));
      for (const node of bundle.nodes) if (!ids.has(node.id)) state.nodes.push({ id: node.id, parentId: node.parentId, x: 0, y: 0, width: 0, height: 0, visibility: 'collapsed', localVisibility: 'collapsed', raster: '', text: node.text || '', paintOrder: 0, brushRasters: {}, interactable: false });
    }
    const payload = new Blob([JSON.stringify(bundle)], { type: 'application/json' });
    $('#status').textContent = `正在压缩完整导入包：${(payload.size / 1024 / 1024).toFixed(1)} MB…`;
    const compressed = await new Response(payload.stream().pipeThrough(new CompressionStream('gzip'))).blob();
    $('#status').textContent = `正在保存完整导入包：${(payload.size / 1024 / 1024).toFixed(1)} MB → ${(compressed.size / 1024 / 1024).toFixed(1)} MB…`;
    const response = await fetch(`/api/native-wbp/${job}`, { method: 'POST', headers: { 'Content-Type': 'application/json', 'Content-Encoding': 'gzip' }, body: compressed });
    const result = await response.json(); if (!response.ok) throw new Error(result.error);
    $('#status').dataset.result = 'passed'; $('#status').textContent = `${JSON.stringify(result, null, 2)}\n导出耗时 ${((performance.now() - started) / 1000).toFixed(1)} 秒`;
    const review = document.createElement('a'); review.href = `/examples/native-wbp-review.html?job=${job}`; review.textContent = '查看透明 PNG 与 UE 渲染对比'; $('#status').append(document.createElement('br'), review);
  } catch (error) { $('#status').dataset.result = 'failed'; $('#status').textContent = error.stack; console.error(error); }
  finally { $('#export').disabled = false; }
};
