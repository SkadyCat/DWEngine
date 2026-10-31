import fs from 'node:fs/promises';
import path from 'node:path';
import crypto from 'node:crypto';
import { pathToFileURL, fileURLToPath } from 'node:url';
import { createLootProject } from '../src/assets/loot-prototype/project.js';
import { validateDocument, walk } from '../src/core/model.js';
import { expandDocument } from '../src/core/prefabs.js';
import { computeLayout } from '../src/core/layout.js';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const source = process.env.RULE_STUDIO_ROOT || 'G:/DungeonWorldBeta/CustomTools/RuleStudio';
const screen = path.join(root, 'Screen/RuleStudio-Loot');
const api = process.env.UI_ALPHA_URL || 'http://127.0.0.1:5190';
const previousResponse = await fetch(`${api}/api/projects/opus-rulestudio-loot-20261025`), previous = previousResponse.ok ? await previousResponse.json() : null;
if (previous && !process.argv.includes('--replace-owned')) throw new Error('Prototype already exists; preserve editor changes. Use --replace-owned explicitly to regenerate this owned prototype.');
const reference = path.join(root, 'src/assets/loot-prototype/reference');
await fs.mkdir(reference, { recursive: true });
const manifest = [], visited = new Set();
async function snapshot(relative) {
  relative = relative.replaceAll('\\', '/'); if (visited.has(relative)) return; visited.add(relative);
  const original = await fs.readFile(path.join(source, relative), 'utf8');
  const dependencies = [...original.matchAll(/(?:from\s*|import\s*)['"]([^'"]+)['"]/g)].map(m => m[1]).filter(p => p.startsWith('.'));
  for (const dependency of dependencies) await snapshot(path.posix.normalize(path.posix.join(path.posix.dirname(relative), dependency)));
  const target = relative.replace(/\.mjs$/, '.js');
  let body = original.replace(/((?:from\s*|import\s*)['"][^'"]+)\.mjs(['"])/g, '$1.js$2');
  if(relative==='public/prefabs/loot-markers.mjs')body+='\n// Authoring-only access to the unchanged original trajectory.\nexport { seedOf, trajectory };\n';
  await fs.mkdir(path.dirname(path.join(reference, target)), { recursive: true });
  await fs.writeFile(path.join(reference, target), body);
  manifest.push({ source: relative, target, sha256: crypto.createHash('sha256').update(original).digest('hex'), snapshotSha256:crypto.createHash('sha256').update(body).digest('hex'), change: relative.endsWith('loot-markers.mjs')?'module extension + export existing seedOf / trajectory for SVG review; function bodies unchanged':'module extension only: .mjs → .js for the existing static server MIME contract' });
}
for (const relative of ['engine/index.mjs', 'public/ground-loot.mjs', 'public/prefabs/loot-style.mjs','public/prefabs/loot-markers.mjs','public/affix-text.mjs']) await snapshot(relative);
const { readRuleFiles } = await import(pathToFileURL(path.join(source, 'scripts/rule-files.mjs')));
const engine = await import(pathToFileURL(path.join(source, 'engine/index.mjs')));
const files = readRuleFiles(), checked = engine.checkRuleSet(files);
if (!checked.ok) throw new Error(checked.error);
await fs.writeFile(path.join(reference, 'rules.json'), JSON.stringify(files));
const rewards = new engine.FloorRewards(checked.collections, checked.library, checked.catalog, { seed: 7919, maxTier: 1 });
const candidates = new Map();
for (let i = 0; i < 1600 && candidates.size < 11; i++) {
  rewards.onKill({ x: 0, y: 0, boss: true });
  for (const drop of rewards.ground.slice(-2)) {
    const key = drop.kind === 'equipment' ? drop.rarity : drop.category === 'currency' ? `currency-${drop.currencyGrade || 'special'}` : drop.category;
    if (!candidates.has(key)) candidates.set(key, structuredClone(drop));
  }
}
const order = ['common', 'magic', 'rare', 'legendary', 'currency-1', 'currency-2', 'currency-3', 'currency-special', 'omen', 'material', 'consumable'];
const samples = order.filter(k => candidates.has(k)).map((k, i) => {
  const d = candidates.get(k); const col = i % 3, row = Math.floor(i / 3);
  return { ...d, x: -480 + col * 330 + (row % 2) * 60, y: 320 - row * 250, sourceX: 0, sourceY: 0, prototypeFamily: k };
});
const unidentified = structuredClone(samples.find(d => d.kind === 'equipment' && d.rarity === 'rare'));
if (unidentified) { unidentified.key += '_unidentified'; unidentified.id += '_unidentified'; unidentified.item.uid = unidentified.id; unidentified.item.identified = false; unidentified.x = 570; unidentified.y = -470; samples.push(unidentified); }
await fs.writeFile(path.join(reference, 'samples.json'), JSON.stringify(samples, null, 2));
await fs.writeFile(path.join(reference, 'sources.json'), JSON.stringify({ source, capturedAt: new Date().toISOString(), rules: files.length, modules: manifest.sort((a,b) => a.source.localeCompare(b.source)), scope: 'UICreator authoring prototype only; never shipped into the UE gameplay runtime' }, null, 2));

const project = createLootProject(), errors = validateDocument(project); if (errors.length) throw new Error(errors.join('\n'));
await fs.writeFile(path.join(screen, 'loot.uialpha.json'), JSON.stringify(project, null, 2));
await fs.mkdir(path.join(screen, 'Controls'), { recursive: true });
await fs.mkdir(path.join(root,'examples/loot-controls'), { recursive:true });
await fs.copyFile(path.join(screen,'controls.css'),path.join(root,'examples/loot-prototype.css'));
const cleanPreview=(await fs.readFile(path.join(screen,'screen.html'),'utf8')).replaceAll('../../src/','../src/').replace('href="controls.css"','href="./loot-prototype.css"');
await fs.writeFile(path.join(root,'examples/loot-structure.html'),cleanPreview);
for (const asset of Object.values(project.assets)) {
  const [,,w,h] = asset.viewBox;
  const html = `<!doctype html><html lang="zh-CN"><head><meta charset="utf-8"><link rel="stylesheet" href="../controls.css"></head><body data-control-id="${asset.id}" data-name="${asset.name}" data-width="${w}" data-height="${h}" style="width:${w}px;height:${h}px">${asset.source}</body></html>`;
  await fs.writeFile(path.join(screen, `Controls/${asset.id}.html`), html);
  await fs.writeFile(path.join(root,`examples/loot-controls/${asset.id}.html`),html.replace('../controls.css','../loot-prototype.css'));
}
const expanded = expandDocument(project).document;
const layout = computeLayout(expanded);
const composition = { canvas: {width:1600,height:900}, manual: true, textLayoutVersion: 1, backgroundPolicy: {review:'included',ue:'transparent'}, root: {id:'root',children:[]}, runtimeNodes:[] };
for (const entry of layout.entries.values()) {
  const n = entry.node, id = n.svgAsset || n.brushes?.normal;
  if (id) composition.root.children.push({id:n.id,type:id,x:entry.globalX,y:entry.globalY,width:entry.width,height:entry.height});
  if (n.type === 'TextBlock') composition.runtimeNodes.push({id:n.id,type:'text',x:entry.globalX,y:entry.globalY,width:entry.width,height:entry.height,safeRect:{x:entry.globalX,y:entry.globalY,width:entry.width,height:entry.height},fontSize:n.style.fontSize,align:n.style.textAlign,maxChars:n.source?.maxChars || 48,overflow:n.style.textOverflow || 'shrink'});
}
await fs.writeFile(path.join(screen, 'composition.json'), JSON.stringify(composition,null,2));
if (previous) { await fs.mkdir(path.join(root,'work/loot-prototype'),{recursive:true}); await fs.writeFile(path.join(root,`work/loot-prototype/before-r${previous.revision}.json`),JSON.stringify(previous)); }
const saved = await fetch(`${api}/api/projects/${project.id}`,{method:'PUT',headers:{'Content-Type':'application/json','If-Match':`"${previous?.revision || 0}"`},body:JSON.stringify(project)});
const result=await saved.json();if(!saved.ok)throw new Error(JSON.stringify(result));
let nodes=0;walk(project.root,()=>nodes++);
console.log(JSON.stringify({id:project.id,revision:result.revision,assets:Object.keys(project.assets).length,prefabs:Object.keys(project.prefabs).length,nodes,rules:files.length,sourceModules:manifest.length,sampleFamilies:samples.map(s=>s.prototypeFamily)}));
