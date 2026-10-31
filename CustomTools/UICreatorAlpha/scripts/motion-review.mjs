import fs from 'node:fs/promises';
import { materializeMotionAssets, motionAssetId, createMotionAsset, motionFrame } from '../src/core/motion.js';
import { walk, validateDocument } from '../src/core/model.js';
import { expandDocument } from '../src/core/prefabs.js';

const base = 'exports/wbp-hud-native-20261025', id = 'hud-motion-assets-20261025', output = `${base}/motion/generic`;
if (process.argv[2] === 'seed') {
  const exists = await fetch(`http://127.0.0.1:5190/api/projects/${id}`);
  if (exists.ok) { console.log('Review project already exists; preserved.'); process.exit(0); }
  const { document: doc } = JSON.parse(await fs.readFile(`${base}/project-record.json`, 'utf8'));
  doc.id = id; doc.name = 'HUD · 可复用动效资产'; materializeMotionAssets(doc);
  const custom = createMotionAsset('motion-review-custom', '自定义 · 旋转弹跳');
  custom.clips.hover = { duration: 400, loop: false, fromCurrent: false, easing: 'linear', keyframes: [motionFrame(0), motionFrame(.5, { x: 8, y: -10, scaleX: 1.12, scaleY: .96, rotation: -6, opacity: .8 }), motionFrame(1, { x: 0, y: -3, scaleX: 1.045, scaleY: 1.045 })] };
  doc.motions[custom.id] = custom;
  let bound = 0;
  for (const prefab of Object.values(doc.prefabs)) walk(prefab.root, node => { if (node.type === 'PrefabInstance' && node.id === 'hud-ability-instance-2') { node.overrides['hud-ability-button'] ||= {}; node.overrides['hud-ability-button'].motion = { preset: 'auto', assetId: custom.id }; bound++; } });
  if (bound !== 1) throw new Error(`Expected one second-slot prefab binding, found ${bound}`);
  const errors = validateDocument(doc); if (errors.length) throw new Error(errors.join('\n'));
  const response = await fetch(`http://127.0.0.1:5190/api/projects/${id}`, { method: 'PUT', headers: { 'Content-Type': 'application/json', 'If-Match': '"0"' }, body: JSON.stringify(doc) });
  if (!response.ok) throw new Error(await response.text());
  console.log(`http://127.0.0.1:5190/?project=${id}`);
} else if (process.argv[2] === 'bundle') {
  const response = await fetch(`http://127.0.0.1:5190/api/projects/${id}`); if (!response.ok) throw new Error('Missing review project');
  const record = await response.json(), bundle = JSON.parse(await fs.readFile(`${base}/bundle.json`, 'utf8'));
  bundle.project = materializeMotionAssets(record.document); const nodes = new Map(); walk(expandDocument(bundle.project).document.root, node => nodes.set(node.id, node));
  for (const node of bundle.nodes) { const source = nodes.get(node.id) || node; node.motion = { ...source.motion, assetId: motionAssetId(source) }; }
  await fs.mkdir(output, { recursive: true }); await fs.writeFile(`${output}/bundle.json`, JSON.stringify(bundle)); await fs.writeFile(`${output}/project-record.json`, JSON.stringify(record));
  console.log(JSON.stringify({ output, motions: Object.keys(bundle.project.motions).length, customNodes: bundle.nodes.filter(n => n.motion.assetId === 'motion-review-custom').map(n => n.id) }));
} else throw new Error('Use seed or bundle');
