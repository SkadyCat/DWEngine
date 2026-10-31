// Real Studio component and combat policy in an owned browser; no user profile is opened.
import fs from 'node:fs/promises';
import path from 'node:path';
import {spawn} from 'node:child_process';
const proof=path.resolve(import.meta.dirname,'../../Saved/Verification/StudioLootPickup');
await fs.mkdir(proof,{recursive:true});
const profile=path.join(proof,'browser-'+Date.now());
const browser=spawn('C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',['--headless=new','--no-first-run','--no-default-browser-check','--remote-debugging-port=0','--remote-debugging-address=127.0.0.1','--user-data-dir='+profile,'about:blank'],{windowsHide:true,stdio:'ignore'});
const sleep=ms=>new Promise(r=>setTimeout(r,ms));
let ws,seq=0;const pending=new Map(),errors=[],checks=[],samples=[];
function check(value,name){if(!value)throw Error(name);checks.push(name);}
async function bounded(work,ms=15000){let timer;try{return await Promise.race([work,new Promise((_,reject)=>{timer=setTimeout(()=>reject(Error('Browser operation timed out')),ms);})]);}finally{clearTimeout(timer);}}
async function call(method,params={}){const id=++seq;return bounded(new Promise((resolve,reject)=>{pending.set(id,{resolve,reject});ws.send(JSON.stringify({id,method,params}));}));}
async function evaluate(expression){const r=await call('Runtime.evaluate',{expression,returnByValue:true,awaitPromise:true});if(r.exceptionDetails)throw Error(r.exceptionDetails.exception?.description||r.exceptionDetails.text);return r.result.value;}
async function until(expression){const end=Date.now()+30000;while(Date.now()<end){if(await evaluate(expression))return;await sleep(100);}throw Error('Timed out waiting for '+expression);}
async function shot(name){const r=await call('Page.captureScreenshot',{format:'png',captureBeyondViewport:false});await fs.writeFile(path.join(proof,name+'.png'),Buffer.from(r.data,'base64'));}
try{
 let port;for(let i=0;i<80;i++){try{port=Number((await fs.readFile(path.join(profile,'DevToolsActivePort'),'utf8')).split('\n')[0]);break;}catch{await sleep(100);}}
 if(!port)throw Error('Owned browser did not expose its loopback debugger');
 const pages=await fetch(`http://127.0.0.1:${port}/json/list`).then(r=>r.json());ws=new WebSocket(pages.find(p=>p.type==='page').webSocketDebuggerUrl);
 await bounded(new Promise((resolve,reject)=>{ws.onopen=resolve;ws.onerror=reject;}));
 ws.onmessage=event=>{const m=JSON.parse(event.data);if(m.id){const p=pending.get(m.id);pending.delete(m.id);if(m.error)p?.reject(Error(m.error.message));else p?.resolve(m.result);}else if(m.method==='Runtime.exceptionThrown')errors.push(m.params.exceptionDetails);else if(m.method==='Runtime.consoleAPICalled'&&m.params.type==='error')errors.push({console:m.params.args.map(a=>a.description||a.value).join(' ')});};
 await call('Page.enable');await call('Runtime.enable');await call('Emulation.setDeviceMetricsOverride',{width:1440,height:900,deviceScaleFactor:1,mobile:false});
 await call('Page.navigate',{url:'http://127.0.0.1:5291/dev/stun-overlay.html'});await until("!!document.querySelector('#samples .hb-crust svg')");

 await evaluate('(async()=>{const {reloadStore,store}=await import("/editors.mjs");await reloadStore();if(store.error)throw Error(store.error);const {mountBattle}=await import("/battle3d.mjs");const host=document.createElement("div");host.id="loot-qa";host.style="position:fixed;inset:0";document.body.replaceChildren(host);const api=mountBattle(host,{rendering:{maxPixelRatio:1,shadowSize:256}});api.show();const vm=host._vnode.component.proxy;vm.closeAtlas();vm.paused=false;api.battle.phase="cleared";globalThis.qa={api,vm};})()');
 async function status(){return evaluate('(()=>{const b=qa.api.battle;return {paused:qa.vm.paused,phase:b.phase,target:qa.vm.pickupTarget,hero:{x:b.hero.x,y:b.hero.y,dead:b.hero.dead},drops:b.rewards.ground.map(d=>({key:d.key,x:d.x,y:d.y,claimed:d.claimed,id:d.id,owned:!!qa.vm.player.instances[d.id]})),toast:qa.vm.store.toast?.text,labels:[...document.querySelectorAll(".b3-loot-label")].map(l=>({text:l.textContent,hidden:l.hidden,visibility:l.style.visibility}))}})()');}
 async function clickLabel(name=null){const at=await evaluate(`(()=>{const name=${JSON.stringify(name)},e=[...document.querySelectorAll('.b3-loot-label')].find(e=>!e.hidden&&(!name||e.getAttribute('aria-label')==='拾取'+name)),r=e.getBoundingClientRect();return {x:r.x+r.width/2,y:r.y+r.height/2,hit:document.elementFromPoint(r.x+r.width/2,r.y+r.height/2)?.className}})()`);samples.push({label:'hit-test',...at});check(at.hit?.includes('b3-loot-label'),'actual pointer position reaches the loot label');await call('Input.dispatchMouseEvent',{type:'mouseMoved',x:at.x,y:at.y});await call('Input.dispatchMouseEvent',{type:'mousePressed',x:at.x,y:at.y,button:'left',buttons:1,clickCount:1});await sleep(80);await call('Input.dispatchMouseEvent',{type:'mouseReleased',x:at.x,y:at.y,button:'left',buttons:0,clickCount:1});}
 await evaluate('qa.vm.spawnTraitEquipment("frostburst_staff")');await until('!!document.querySelector(".b3-loot-label:not([hidden])")');await sleep(400);await shot('near-before');
 await clickLabel();await sleep(300);let s=await status();samples.push({label:'near',...s});check(s.drops[0].claimed&&s.drops[0].owned,'real click picks up nearby trait equipment into the inventory');
 await evaluate('(()=>{const b=qa.api.battle;qa.vm.spawnTraitEquipment("frostburst_staff");const d=b.rewards.ground.at(-1),h=b.hero;let at=null;for(let x=-6;x<=6;x++)for(let y=-6;y<=6;y++){const p={x:h.x+x*100,y:h.y+y*100};const distance=Math.hypot(p.x-h.x,p.y-h.y);if(distance>480&&distance<650&&b.dungeon.sweep(h.x,h.y,p.x,p.y,h.radius)<0)at=p;}if(!at)throw Error("No reachable far loot point");Object.assign(d,at);globalThis.farKey=d.key;})()');
 await until('!!document.querySelector(".b3-loot-label:not([hidden])")');await sleep(1300);await clickLabel();await sleep(4500);s=await status();samples.push({label:'far',...s});await shot('far-after');
 check(s.drops.at(-1).claimed&&s.drops.at(-1).owned,'clicking distant loot approaches it and then picks it up');
 await evaluate('(()=>{qa.vm.hunter.enabled=false;const b=qa.api.battle;b.rewards.onKill({x:b.hero.x,y:b.hero.y,boss:true});globalThis.firstKillUid=b.rewards.ground.find(d=>d.origin==="drop"&&d.kind==="equipment").id;})()');
 await until('!!document.querySelector(".b3-loot-label:not([hidden])")');await sleep(1300);await clickLabel();await sleep(300);s=await status();samples.push({label:'first-kill',...s});
 check(s.drops.find(d=>d.id.startsWith('drop_'))?.claimed,'first-run kill equipment can be picked up');
 await evaluate('(()=>{qa.vm.restart();qa.vm.closeAtlas();qa.vm.paused=false;const b=qa.api.battle;b.phase="cleared";b.rewards.onKill({x:b.hero.x,y:b.hero.y,boss:true});})()');
 await sleep(1400);await clickLabel();await sleep(300);s=await status();samples.push({label:'repeat-kill',...s});await shot('repeat-kill');
 check(await evaluate('qa.api.battle.rewards.ground.find(d=>d.kind==="equipment").id!==globalThis.firstKillUid'),'restarting an atlas encounter creates distinct equipment instance IDs');
 check(s.drops.find(d=>d.id.startsWith('drop_'))?.claimed,'kill equipment is still collectible after restarting the same map');
 const wallName=await evaluate('(()=>{const b=qa.api.battle,m=b.dungeon,t=m.tiles.find(t=>!m.walkable(t.x,t.y+1)&&m.walkable(t.x,t.y-1)&&m.walkable(t.x,t.y-2)),p=m.tileCenter(t.x,t.y);Object.assign(b.hero,m.tileCenter(t.x,t.y-2));b.rewards.onKill({x:p.x,y:p.y+54,boss:true});const d=b.rewards.ground.filter(d=>d.kind==="equipment").at(-1);globalThis.wallKey=d.key;const at=m.tileOf(d.x,d.y);if(m.walkable(at.x,at.y))throw Error("Fixture did not scatter loot across the wall");return d.name;})()');
 await sleep(1500);await clickLabel(wallName);await until('qa.api.battle.rewards.ground.find(d=>d.key===globalThis.wallKey).claimed');
 s=await status();samples.push({label:'wall-edge',...s});check(s.drops.find(d=>d.key===s.drops.filter(d=>d.id.startsWith('drop_')).at(-1).key)?.claimed,'clicking a wall-edge kill drop approaches a reachable point and grants the item');await shot('wall-picked-up');
 await call('Page.navigate',{url:'http://127.0.0.1:5291/?view=battle'});await until("!!document.querySelector('.b3-stage')");
 const release=JSON.parse(await fs.readFile(path.resolve(proof,'../../RuleStudioWeb/current.json'),'utf8')).id;
 check(await evaluate(`Array.from(document.scripts).some(s=>s.src.includes('/_release/${release}/'))`),'5291 loads the published fix');
 check(errors.length===0,'no browser script errors');
 await fs.writeFile(path.join(proof,'browser-receipt.json'),JSON.stringify({ok:true,release,checks,samples,errors,isolatedBrowserProfile:true},null,2));console.log(JSON.stringify({ok:true,release,checks}));
}catch(error){await fs.writeFile(path.join(proof,'browser-failure.json'),JSON.stringify({ok:false,error:error.stack,checks,samples,errors},null,2));throw error;}
finally{try{await call('Browser.close');}catch{}ws?.close();if(browser.exitCode===null)browser.kill();}
