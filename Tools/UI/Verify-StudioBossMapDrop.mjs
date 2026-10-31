// Real Studio combat, map traits and pointer pickup in a separate browser profile.
import fs from 'node:fs/promises';
import path from 'node:path';
import { spawn } from 'node:child_process';
const proof = path.resolve(import.meta.dirname, '../../Saved/Verification/StudioBossMapDrop');
await fs.mkdir(proof, { recursive: true });
const profile = path.join(proof, 'browser-' + Date.now());
const browser = spawn('C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe', ['--headless=new', '--no-first-run', '--no-default-browser-check', '--remote-debugging-port=0', '--remote-debugging-address=127.0.0.1', '--user-data-dir=' + profile, 'about:blank'], { windowsHide: true, stdio: 'ignore' });
const sleep = ms => new Promise(resolve => setTimeout(resolve, ms));
let ws, seq = 0; const pending = new Map(), errors = [], checks = [], samples = [];
function check(value, name) { if (!value) throw Error(name); checks.push(name); }
async function bounded(work) { let timer; try { return await Promise.race([work, new Promise((_, reject) => { timer = setTimeout(() => reject(Error('Browser operation timed out')), 15000); })]); } finally { clearTimeout(timer); } }
async function call(method, params = {}) { const id = ++seq; return bounded(new Promise((resolve, reject) => { pending.set(id, { resolve, reject }); ws.send(JSON.stringify({ id, method, params })); })); }
async function evaluate(expression) { const r = await call('Runtime.evaluate', { expression, returnByValue: true, awaitPromise: true }); if (r.exceptionDetails) throw Error(r.exceptionDetails.exception?.description || r.exceptionDetails.text); return r.result.value; }
async function until(expression) { const end = Date.now() + 30000; while (Date.now() < end) { if (await evaluate(expression)) return; await sleep(100); } throw Error('Timed out waiting for ' + expression); }
async function shot(name) { const r = await call('Page.captureScreenshot', { format: 'png', captureBeyondViewport: false }); await fs.writeFile(path.join(proof, name + '.png'), Buffer.from(r.data, 'base64')); }
try {
  let port;
  for (let i = 0; i < 80; i++) { try { port = Number((await fs.readFile(path.join(profile, 'DevToolsActivePort'), 'utf8')).split('\n')[0]); break; } catch { await sleep(100); } }
  if (!port) throw Error('Owned browser did not expose its loopback debugger');
  const pages = await fetch(`http://127.0.0.1:${port}/json/list`).then(r => r.json());
  ws = new WebSocket(pages.find(p => p.type === 'page').webSocketDebuggerUrl);
  await bounded(new Promise((resolve, reject) => { ws.onopen = resolve; ws.onerror = reject; }));
  ws.onmessage = event => { const m = JSON.parse(event.data); if (m.id) { const p = pending.get(m.id); pending.delete(m.id); if (m.error) p?.reject(Error(m.error.message)); else p?.resolve(m.result); } else if (m.method === 'Runtime.exceptionThrown') errors.push(m.params.exceptionDetails); else if (m.method === 'Runtime.consoleAPICalled' && m.params.type === 'error') errors.push({ console: m.params.args.map(a => a.description || a.value).join(' ') }); };
  await call('Page.enable'); await call('Runtime.enable');
  await call('Emulation.setDeviceMetricsOverride', { width: 1440, height: 900, deviceScaleFactor: 1, mobile: false });
  await call('Page.navigate', { url: 'http://127.0.0.1:5291/dev/stun-overlay.html' });
  await until("!!document.querySelector('#samples .hb-crust svg')");
  await evaluate(`(async()=>{
    const {reloadStore,store}=await import('/editors.mjs'); await reloadStore(); if(store.error)throw Error(store.error);
    const {mountBattle}=await import('/battle3d.mjs');
    const host=document.createElement('div');host.id='boss-map-qa';host.style='position:fixed;inset:0';document.body.replaceChildren(host);
    const api=mountBattle(host,{rendering:{maxPixelRatio:1,shadowSize:256}});api.show();
    const vm=host._vnode.component.proxy;vm.closeAtlas();vm.paused=true;
    globalThis.qa={api,vm};
    const b=api.battle;b.hero.god=true;b.setLoadout({skills:[]});b.rewards.mapRandom=()=>.599999;b.step(1/60);
    for(const monster of b.monsters)if(!monster.boss)monster.health=0;
    b.step(1/60);
    if(b.rewards.ground.some(d=>d.category==='map'))throw Error('Ordinary monster produced a boss map');
    Object.assign(b.boss,{x:b.hero.x,y:b.hero.y,health:0});
    globalThis.beforeMapCount=vm.player.owned.item['map_t01_common']||0;
    b.step(1/60);vm.syncHud();vm.paused=false;
  })()`);
  const state = await evaluate(`(()=>{const b=qa.api.battle;return {phase:b.phase,kills:b.kills,chance:b.rewards.bossMapDropChance,tier:b.mapTier,hunterEnabled:qa.vm.hunter.enabled,drops:b.rewards.ground.filter(d=>d.category==='map')}})()`);
  samples.push(state);
  check(state.phase === 'cleared' && state.kills === 20, 'real boss defeat uses normal kill settlement');
  check(state.chance === .6, 'actual equipped map trait supplies 60 percent');
  check(state.drops.length === 1 && state.drops[0].id === 'map_t01_common' && state.drops[0].tier === 1 && state.drops[0].count === 1, 'one current-tier map is scattered at boss death');
  check(state.hunterEnabled, 'default Hunter remains enabled');
  await until("!!document.querySelector('.b3-loot-label[aria-label=\"拾取地图 T1 · 普通\"]:not([hidden])')");
  await sleep(800); await shot('boss-map-ground');
  const at = await evaluate(`(()=>{const el=document.querySelector('.b3-loot-label[aria-label="拾取地图 T1 · 普通"]'),r=el.getBoundingClientRect();return {x:r.x+r.width/2,y:r.y+r.height/2,hit:document.elementFromPoint(r.x+r.width/2,r.y+r.height/2)?.className}})()`);
  check(at.hit?.includes('b3-loot-label'), 'map label remains visible and receives pointer input under default filtering');
  await call('Input.dispatchMouseEvent', { type: 'mouseMoved', x: at.x, y: at.y });
  await call('Input.dispatchMouseEvent', { type: 'mousePressed', x: at.x, y: at.y, button: 'left', buttons: 1, clickCount: 1 });
  await call('Input.dispatchMouseEvent', { type: 'mouseReleased', x: at.x, y: at.y, button: 'left', buttons: 0, clickCount: 1 });
  await until("qa.api.battle.rewards.ground.find(d=>d.category==='map').claimed");
  check(await evaluate("qa.vm.player.owned.item['map_t01_common']===beforeMapCount+1"), 'real map label click grants one usable map ticket to inventory');
  await sleep(700);
  check(await evaluate("qa.api.battle.rewards.ground.filter(d=>d.category==='map').length===1 && qa.vm.player.owned.item['map_t01_common']===beforeMapCount+1"), 'later frames do not duplicate boss map or pickup');
  await shot('boss-map-collected');
  await call('Page.navigate', { url: 'http://127.0.0.1:5291/?view=battle' });
  await until("!!document.querySelector('.b3-stage')");
  const release = JSON.parse(await fs.readFile(path.resolve(proof, '../../RuleStudioWeb/current.json'), 'utf8')).id;
  check(await evaluate(`Array.from(document.scripts).some(s=>s.src.includes('/_release/${release}/'))`), '5291 loads the current published release');
  check(errors.length === 0, 'no browser script or console errors');
  await fs.writeFile(path.join(proof, 'browser-receipt.json'), JSON.stringify({ ok: true, release, checks, samples, errors, isolatedBrowserProfile: true, deterministicSuccessfulRoll: .599999 }, null, 2));
  console.log(JSON.stringify({ ok: true, release, checks }));
} catch (error) {
  await fs.writeFile(path.join(proof, 'browser-failure.json'), JSON.stringify({ ok: false, error: error.stack, checks, samples, errors }, null, 2)); throw error;
} finally { try { await call('Browser.close'); } catch {} ws?.close(); if (browser.exitCode === null) browser.kill(); }
