import fs from 'node:fs/promises';
import path from 'node:path';
import { ProjectRepository } from '../../CustomTools/UICreatorAlpha/server/project-repository.mjs';
import { validateDocument } from '../../CustomTools/UICreatorAlpha/src/core/model.js';
import { installPlayerEffects, PLAYER_EFFECT_FRAME, PLAYER_EFFECT_ICONS } from '../../CustomTools/UICreatorAlpha/src/assets/rulestudio-hud/player-effects.js';
const root = path.resolve(import.meta.dirname,'../..');
const backup = path.join(root,'Saved/UIAuthoringBackups/PlayerEffects-'+Date.now());
const css = `
/* Player temporary effects: UICreator rs-prefab-player-effect. */
.b3-stage .b3-top { flex-wrap:wrap; align-items:flex-start; }
.b3-player-effects { position:static; flex:0 0 100%; max-height:112px; overflow:auto; pointer-events:auto; scrollbar-width:thin; cursor:default; }
.b3-player-effects ul { display:flex; flex-wrap:wrap; align-items:flex-start; gap:6px; margin:0; padding:2px; list-style:none; }
.b3-player-effects li { flex:none; width:48px; height:48px; }
.b3-player-effects .b3-effect-icon { position:relative; display:block; width:48px; height:48px; min-width:0; padding:0; border:0; border-radius:8px; background:url('/hud-player-effect.svg') center/100% 100% no-repeat; cursor:help; transition:filter .15s,box-shadow .15s; }
.b3-effect-icon > img { position:absolute; inset:6px; width:36px; height:36px; object-fit:cover; border-radius:4px; pointer-events:none; }
.b3-player-effects .b3-effect-icon.hovered,.b3-player-effects .b3-effect-icon:focus-visible { outline:1px solid #f1d795; outline-offset:1px; box-shadow:0 0 12px color-mix(in srgb,var(--effect-color) 35%,transparent); filter:brightness(1.15); }
.b3-effect-countdown { position:absolute; inset:0; width:48px; height:48px; fill:none; stroke-width:2; pointer-events:none; }
.b3-effect-countdown .track { stroke:#3d4858; }.b3-effect-countdown .remaining { stroke:var(--effect-color); stroke-linecap:round; transition:stroke-dashoffset .1s linear; }
.b3-effect-stack { position:absolute; right:3px; bottom:2px; min-width:12px; padding:1px 3px; border-radius:3px; background:#080d15e8; color:#fff; text-align:right; font:700 13px/16px var(--mono); text-shadow:0 1px 2px #000; pointer-events:none; }
.b3-effect-tooltip { position:fixed; z-index:1600; box-sizing:border-box; width:min(280px,calc(100vw - 24px)); max-height:calc(100vh - 24px); overflow:hidden; padding:14px; border:1px solid color-mix(in srgb,var(--effect-color) 65%,#354356); border-radius:8px; background:linear-gradient(145deg,#152131fc,#0d1522fc); box-shadow:0 12px 30px #0009,inset 0 1px #ffffff08; color:#d8e2ee; font:12px/1.6 var(--sans); pointer-events:none; animation:b3-effect-tip-in .12s ease-out; }
.b3-effect-tooltip header { display:flex; align-items:center; gap:10px; padding-bottom:12px; border-bottom:1px solid #354356; }.b3-effect-tooltip header img { flex:none; width:38px; height:38px; border:1px solid #43546b; border-radius:5px; object-fit:cover; }.b3-effect-tooltip header div { min-width:0; display:flex; flex-direction:column; gap:3px; }.b3-effect-tooltip header b { font-size:13px; color:var(--effect-color); }.b3-effect-tooltip header small { color:#96a7bd; font-size:10px; }
.b3-effect-tooltip dl { display:flex; flex-direction:column; gap:6px; margin:12px 0 0; }.b3-effect-tooltip dl > div { display:flex; justify-content:space-between; gap:12px; }.b3-effect-tooltip dt { color:#96a7bd; }.b3-effect-tooltip dd { margin:0; font-family:var(--mono); color:#edcf97; }.b3-effect-tooltip dd small { color:#96a7bd; }.b3-effect-tooltip p { margin:12px 0 0; padding-top:10px; border-top:1px solid #354356; color:#afbed0; white-space:pre-line; overflow-wrap:anywhere; }
@keyframes b3-effect-tip-in { from { opacity:0; transform:translateY(-3px); } to { opacity:1; transform:translateY(0); } }
@media(prefers-reduced-motion:reduce) { .b3-effect-countdown .remaining,.b3-player-effects .b3-effect-icon { transition:none; }.b3-effect-tooltip { animation:none; } }
/* End player temporary effects. */
`;
const source = await fs.readFile(path.join(root,'CustomTools/RuleStudio/public/battle-player-effects.mjs'),'utf8');
await fs.mkdir(backup,{recursive:true});
for (const project of ['G:/DungeonWorldBeta',root]) {
  const dir=path.join(project,'CustomTools/RuleStudio/public');
  await fs.writeFile(path.join(dir,'battle-player-effects.mjs'),source);
  await fs.writeFile(path.join(dir,'hud-player-effect.svg'),PLAYER_EFFECT_FRAME);
  await fs.mkdir(path.join(dir,'assets/player-effects'),{recursive:true});
  for(const [id,svg] of Object.entries(PLAYER_EFFECT_ICONS))await fs.writeFile(path.join(dir,'assets/player-effects',id+'.svg'),svg);
  for (const file of ['battle3d.mjs','play.css']) {
    const target=path.join(dir,file),original=await fs.readFile(target,'utf8');
    let next=original.replace(/\r\n/g,'\n');
    if(file==='battle3d.mjs'&&!next.includes("from '/battle-player-effects.mjs'")) {
      const replacements=[
        ["import { BattleDps }", "import { BattlePlayerEffects, playerEffects } from '/battle-player-effects.mjs';\nimport { BattleDps }"],
        ['BattleLootHunter, BattleDps,','BattleLootHunter, BattlePlayerEffects, BattleDps,'],
        ['    <div id="top_left"','    <BattlePlayerEffects :effects="hud.effects" @release="releaseBattleInput" />\n    <div id="top_left"'],
        ['buffs: [], earned:', 'buffs: [], effects: [], earned:'],
        ['buffs: battle.buffs(), earned:', 'buffs: battle.buffs(), effects: playerEffects(battle), earned:'],
      ];
      for(const [from,to] of replacements) { if(!next.includes(from))throw Error('Missing effect UI marker: '+from);next=next.replace(from,to); }
    }
    if(file==='battle3d.mjs') {
      next=next.replace(/^.*<BattlePlayerEffects :effects="hud.effects".*\n/m,'');
      next=next.replace('      <BattleDps :stats="dpsHud" />','      <BattleDps :stats="dpsHud" />\n      <BattlePlayerEffects :effects="hud.effects" @release="releaseBattleInput" />');
      next=next.replace(/<span v-for="entry in hud\.maintained"[^\n]*?<\/span>/g,'');
    }
    if(file==='play.css') {
      next=next.replace(/\n\/\* Player temporary effects:[\s\S]*?(?:\/\* End player temporary effects\. \*\/|\.b3-effect-stack small \{ font-size:9px; font-weight:400; \})\n/,'');
      next+=css;
    }
    if(original.includes('\r\n'))next=next.replace(/\n/g,'\r\n');
    if(original!==next){const dest=path.join(backup,path.basename(project),file);await fs.mkdir(path.dirname(dest),{recursive:true});await fs.writeFile(dest,original);await fs.writeFile(target,next);}
  }
}
const primary=new ProjectRepository(path.join(root,'CustomTools/UICreatorAlpha/data/projects'));
const id='hud-darksteel-20261024',previous=await primary.read(id);
await fs.writeFile(path.join(backup,'uicreator.json'),JSON.stringify(previous));
const document=installPlayerEffects(structuredClone(previous.document));
const errors=validateDocument(document);if(errors.length)throw Error(errors.join('\n'));
if((await primary.pointer(id)).revision!==previous.revision)throw Error('Concurrent UICreator edit');
const saved=await primary.write(document,previous.revision);
const mirror=new ProjectRepository(path.join(root,'UIAuthoring/projects'));await mirror.write(document,(await mirror.pointer(id))?.revision||0);
for(const file of ['UIAuthoring/RuleStudio3D.uialpha.json','CustomTools/UICreatorAlpha/Screen/RuleStudio-HUD/hud.uialpha.json'])await fs.writeFile(path.join(root,file),JSON.stringify(document,null,2));
console.log(JSON.stringify({revision:saved.revision,backup,prefab:'rs-prefab-player-effect',sites:[5290,5291]}));
