import fs from 'node:fs/promises';
import path from 'node:path';
const roots = ['G:/DungeonWorldBeta', 'G:/DungeonWorldOpus'];
const css = `
/* Compact RuleStudio3D HUD: direct actions stay on the right rail. */
.b3-stage .b3-top { flex-wrap: nowrap; gap: 8px; }
.b3-top .b3-stats { flex: 0 1 auto; min-width: 0; min-height: 40px; align-items: center; }
.b3-top .b3-dps { display: flex; align-items: center; gap: 14px; width: auto; min-width: 128px; min-height: 40px; padding: 7px 12px; }
.b3-top .b3-dps header small, .b3-top .b3-dps footer { display: none; }
.b3-top .b3-dps output { margin: 0 0 0 auto; font-size: 18px; white-space: nowrap; }
.b3-inventory .b3-bag-wallet { display: flex; flex: none; align-items: center; gap: 7px; margin-right: 8px; color: #9ff5d2; white-space: nowrap; }
.b3-bag-wallet b { font: 600 14px var(--mono); }.b3-bag-wallet small { color: var(--muted); }
.b3-stage > .b3-bd-panel { position: absolute; z-index: 6; top: 64px; right: calc(var(--hud-right-inset) + var(--hud-map-width) + var(--hud-gap)); width: 300px; max-width: calc(100% - var(--hud-rail-width) - 3 * var(--hud-edge)); max-height: calc(100% - 180px); }
@container battle-hud (max-width: 760px) { .b3-stage .b3-top { flex-wrap: wrap; }.b3-stage > .b3-bd-panel { right: var(--hud-right-inset); top: var(--hud-edge); } }
`;
const writes = [];
function replace(source, from, to) {
  if (!source.includes(from)) throw Error('Missing HUD migration marker: ' + from.slice(0, 100));
  return source.replace(from, to);
}
for (const root of roots) {
  const dir = path.join(root, 'CustomTools/RuleStudio/public');
  for (const file of ['battle3d.mjs', 'battle-functions.mjs', 'play.css']) {
    const target = path.join(dir, file), original = await fs.readFile(target, 'utf8');
    let next = original.replace(/\r\n/g, '\n');
    if (file === 'battle3d.mjs' && !next.includes('b3-bag-wallet')) {
      const wallet = next.match(/^\s*<div class="b3-card wallet">.*<\/div>\r?\n/m)?.[0];
      if (!wallet) throw Error('HUD wallet missing: ' + root);
      next = next.replace(wallet, '');
      next = replace(next, '<header class="b3-panel-head">', '<header class="b3-panel-head">\n        <span class="b3-bag-wallet" aria-label="灵石余额"><i class="stone-ico" aria-hidden="true"></i><b>{{ player.stones }}</b><small>灵石</small></span>');
      next = next.replace(/^\s*<button type="button" class="b3-atlas-launch".*<\/button>\r?\n/m, '');
      next = next.replace(/^\s*<button type="button" class="btn sm b3-hud-reset".*<\/button>\r?\n/m, '');
      const start = next.indexOf('      <section class="b3-bd-panel b3-card"');
      const end = next.indexOf('      </section>', start) + '      </section>'.length;
      if (start < 0 || end < start) throw Error('Available builds panel missing');
      const popup = next.slice(start, end).replace('<section class=', '<section v-if="bdExpanded" class=').replace("bdExpanded ? '▴' : '▾'", "bdExpanded ? '×' : '▾'");
      next = next.slice(0, start) + next.slice(end);
      const rail = next.match(/^.*<BattleFunctions .*\/>/m)?.[0];
      if (!rail) throw Error('Right function rail missing');
      next = replace(next, rail, rail.replace('<BattleFunctions ', '<BattleFunctions :ready="ready" ') + '\n' + popup);
      next = replace(next, '    <BattleDps :stats="dpsHud" />', '');
      next = replace(next, '    </div>\n    <div id="top_left"', '      <BattleDps :stats="dpsHud" />\n    </div>\n    <div id="top_left"');
      next = replace(next, "const activeHUDFunction = computed(() => ", "const activeHUDFunction = computed(() => atlasOpen.value ? 'atlas' : bdExpanded.value ? 'bd' : ");
      next = replace(next, "        if (id === 'hunter')", "        if (id === 'bd') { const wasOpen = bdExpanded.value; closeHUDPanel(); bdExpanded.value = !wasOpen; return; }\n        bdExpanded.value = false;\n        if (id === 'atlas') { openAtlas(); return; }\n        if (id === 'restart') { closeHUDPanel(); restart(); return; }\n        if (id === 'hunter')");
      next = replace(next, "        if (id === 'bd') { openBuildGuide(); return; }", '');
      next = replace(next, "        if (splitKey.value)", "        if (e.code === 'Escape' && bdExpanded.value) { e.preventDefault(); bdExpanded.value = false; return; }\n        if (splitKey.value)");
    }
    if (file === 'battle3d.mjs') {
      for (const signature of ['function closeHUDPanel() {', 'function openAtlas() {', "function openBuildGuide(id = 'synergy_rime_thunder') {"]) {
        if (!next.includes(signature + '\n        bdExpanded.value = false;'))
          next = replace(next, signature, signature + '\n        bdExpanded.value = false;');
      }
    }
    if (file === 'battle-functions.mjs' && !next.includes("id: 'restart'")) {
      next = replace(next, "    { id: 'settings'", "    { id: 'atlas', name: '异界星图', key: 'M', path: 'M12 2 15 9l7 3-7 3-3 7-3-7-7-3 7-3 3-7Z' },\n    { id: 'settings'");
      next = replace(next, "  { id: 'tools'", "  { id: 'tools'");
      next = replace(next, "\n  ] },\n];", "\n    { id: 'restart', name: '重置当前层', path: 'M4 10a8 8 0 1 1 1.8 7.2M4 4v6h6' },\n  ] },\n];");
      next = replace(next, 'props: { mapReady: Boolean,', 'props: { ready: { type: Boolean, default: true }, mapReady: Boolean,');
      next = replace(next, ':disabled="!mapReady &&', ':disabled="item.id === \'restart\' && !ready || !mapReady &&');
    }
    if (file === 'play.css' && !next.includes('Compact RuleStudio3D HUD')) {
      next = next.replace('.b3-top-left .b3-bd-panel {', '.b3-bd-panel {') + css;
    }
    if (original.includes('\r\n')) next = next.replace(/\n/g, '\r\n');
    if (next !== original) writes.push({ target, original, next, root, file });
  }
}
const backup = 'G:/DungeonWorldOpus/Saved/UIAuthoringBackups/RuleStudioHUD-' + Date.now();
for (const w of writes) {
  if (await fs.readFile(w.target, 'utf8') !== w.original) throw Error('Concurrent RuleStudio edit: ' + w.target);
  const saved = path.join(backup, path.basename(w.root), w.file);
  await fs.mkdir(path.dirname(saved), { recursive: true }); await fs.writeFile(saved, w.original);
  await fs.writeFile(w.target, w.next);
}
console.log(JSON.stringify({ files: writes.map(w => w.target), backup }));
