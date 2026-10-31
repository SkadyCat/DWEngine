// Real Studio combat, map traits and pointer pickup in a separate browser profile.
import fs from 'node:fs/promises';
import path from 'node:path';
import { spawn } from 'node:child_process';
const proof = path.resolve(import.meta.dirname, '../../Saved/Verification/StudioMapEventWorld');
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
    await new Promise((resolve,reject)=>{const link=document.createElement('link');link.rel='stylesheet';link.href='/battle-map-events.css';link.onload=resolve;link.onerror=reject;document.head.append(link);});
    const {reloadStore,store}=await import('/editors.mjs');await reloadStore();if(store.error)throw Error(store.error);
    const {createAtlas,atlasBattleOptions}=await import('/atlas-state.mjs');
    const {AtlasSession,ATLAS_STORAGE_KEY}=await import('/atlas-session.mjs');
    const {mapEventPlan}=await import('/map-event-locations.mjs');
    let seed;
    for(let n=1;n<1000;n++){const state=createAtlas(n),options=atlasBattleOptions(state,'0:0'),seedAtFloor=(options.seed+Math.imul(options.floor,0x9e3779b9))>>>0,kinds=mapEventPlan(seedAtFloor,options.floor).map(e=>e.kind);if(kinds.includes('altar')&&kinds.includes('fusion')){seed=n;break;}}
    if(!seed)throw Error('No real altar/fusion seed found');
    localStorage.removeItem(ATLAS_STORAGE_KEY);new AtlasSession(localStorage,{seed});
    const {mountBattle}=await import('/battle3d.mjs');const host=document.createElement('div');host.id='event-world-qa';host.style='position:fixed;inset:0';document.body.replaceChildren(host);
    const api=mountBattle(host,{rendering:{maxPixelRatio:1,shadowSize:256}});api.show();const vm=host._vnode.component.proxy;vm.closeAtlas();vm.paused=true;api.battle.hero.god=true;
    globalThis.qa={api,vm};
  })()`);
  async function click(selector) {
    const point=await evaluate(`(()=>{const el=document.querySelector(${JSON.stringify(selector)}),r=el.getBoundingClientRect();return {x:r.x+r.width/2,y:r.y+r.height/2}})()`);
    await call('Input.dispatchMouseEvent',{type:'mouseMoved',...point});
    await call('Input.dispatchMouseEvent',{type:'mousePressed',...point,button:'left',buttons:1,clickCount:1});
    await call('Input.dispatchMouseEvent',{type:'mouseReleased',...point,button:'left',buttons:0,clickCount:1});
  }
  async function visit(kind){
    return evaluate(`(()=>{const b=qa.api.battle,event=b.mapEvents.find(e=>e.kind===${JSON.stringify(kind)});if(!event)throw Error('Expected actual generated event');const candidates=b.dungeon.tiles.map(t=>b.dungeon.tileCenter(t.x,t.y)).filter(p=>{const d=Math.hypot(p.x-event.x,p.y-event.y);return d>180&&d<450&&b.dungeon.sweep(p.x,p.y,event.x,event.y,45)<0;}).sort((a,c)=>a.y-c.y);if(!candidates.length)throw Error('No reachable approach');Object.assign(b.hero,candidates[0]);qa.vm.syncHud();return event;})()`);
  }
  const altar=await visit('altar');samples.push({kind:'altar',event:altar});
  await until(`!!document.querySelector('[data-world-event="${altar.id}"] [role="button"]')`);
  await sleep(1000);await shot('altar-world');
  check(await evaluate(`qa.api.battle.mapEvents.some(e=>e.id===${JSON.stringify(altar.id)}&&e.x===${altar.x}&&e.y===${altar.y})`),'world interaction uses a real generated altar');
  check(await evaluate(`qa.vm.hud.dungeon.events.some(e=>e.id===${JSON.stringify(altar.id)}&&e.x===${altar.x}&&e.y===${altar.y})`),'local map and world altar share coordinates');
  await click(`[data-world-event="${altar.id}"] [role="button"]`);
  await until("document.querySelector('.b3-event-layer [data-node-id=event-title]')?.textContent.includes('百炼法坛')");
  await until("document.querySelector('.b3-event-layer').getBoundingClientRect().height===innerHeight");
  check(await evaluate('qa.vm.paused'),'clicking altar opens its real crafting panel and pauses combat');
  await shot('altar-panel');
  await click('.b3-event-layer [data-node-id=event-close]');await until("!document.querySelector('.b3-event-layer')");
  const fusion=await visit('fusion');samples.push({kind:'fusion',event:fusion});
  await until(`!!document.querySelector('[data-world-event="${fusion.id}"] [role="button"]')`);
  await sleep(1000);await shot('fusion-world');
  await click(`[data-world-event="${fusion.id}"] [role="button"]`);
  await until("document.querySelector('.b3-event-layer [data-node-id=event-title]')?.textContent.includes('万相合成祭坛')");
  check(true,'second real station opens fusion instead of the previous altar kind');
  await click('.b3-event-layer [data-node-id=event-close]');await until("!document.querySelector('.b3-event-layer')");
  await evaluate("qa.api.battle.mapEvents.find(e=>e.kind==='fusion').completed=true;qa.vm.syncHud()");
  await until(`document.querySelector('[data-world-event="${fusion.id}"]')?.textContent.includes('已完成')`);
  check(true,'completed station keeps its interaction and shows completed state');
  await evaluate('qa.api.battle.mapEvents=[];qa.vm.syncHud()');
  await until("document.querySelectorAll('[data-world-event]').length===0");
  check(true,'removed map events leave no stale interaction labels');
  await call('Page.navigate',{url:'http://127.0.0.1:5291/?view=battle'});await until("!!document.querySelector('.b3-stage')");
  const release=JSON.parse(await fs.readFile(path.resolve(proof,'../../RuleStudioWeb/current.json'),'utf8')).id;
  check(await evaluate(`Array.from(document.scripts).some(s=>s.src.includes('/_release/${release}/'))`),'5291 loads the published release');
  check(errors.length===0,'no browser script or console errors');
  await fs.writeFile(path.join(proof,'browser-receipt.json'),JSON.stringify({ok:true,release,checks,samples,errors,isolatedBrowserProfile:true},null,2));
  console.log(JSON.stringify({ok:true,release,checks}));
} catch(error) {await fs.writeFile(path.join(proof,'browser-failure.json'),JSON.stringify({ok:false,error:error.stack,checks,samples,errors},null,2));throw error;}
finally {try{await call('Browser.close');}catch{}ws?.close();if(browser.exitCode===null)browser.kill();}
