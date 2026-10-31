// Real Studio component and combat policy in an owned browser; no user profile is opened.
import fs from 'node:fs/promises';
import path from 'node:path';
import {spawn} from 'node:child_process';
const proof=path.resolve(import.meta.dirname,'../../Saved/Verification/StudioStunRecovery');
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
async function sample(label){await evaluate('qa.vm.syncHud()');await sleep(40);const s=await evaluate(`(()=>{const s=qa.api.battle.stunCrust.snapshot(),bar=document.querySelector('#stun-qa .b3-stun');return {...s,barNow:+bar.getAttribute('aria-valuenow'),barMax:+bar.getAttribute('aria-valuemax'),barWidth:parseFloat(bar.firstElementChild.style.width),text:bar.previousElementSibling.textContent}})()`);samples.push({label,...s});return s;}
const step=seconds=>evaluate(`(()=>{for(let left=${seconds};left>1e-8;){const dt=Math.min(left,1/60);qa.api.battle.step(dt);left-=dt}})()`);
async function shot(name){const r=await call('Page.captureScreenshot',{format:'png',captureBeyondViewport:false});await fs.writeFile(path.join(proof,name+'.png'),Buffer.from(r.data,'base64'));}
try{
 let port;for(let i=0;i<80;i++){try{port=Number((await fs.readFile(path.join(profile,'DevToolsActivePort'),'utf8')).split('\n')[0]);break;}catch{await sleep(100);}}
 if(!port)throw Error('Owned browser did not expose its loopback debugger');
 const pages=await fetch(`http://127.0.0.1:${port}/json/list`).then(r=>r.json());ws=new WebSocket(pages.find(p=>p.type==='page').webSocketDebuggerUrl);
 await bounded(new Promise((resolve,reject)=>{ws.onopen=resolve;ws.onerror=reject;}));
 ws.onmessage=event=>{const m=JSON.parse(event.data);if(m.id){const p=pending.get(m.id);pending.delete(m.id);if(m.error)p?.reject(Error(m.error.message));else p?.resolve(m.result);}else if(m.method==='Runtime.exceptionThrown')errors.push(m.params.exceptionDetails);else if(m.method==='Runtime.consoleAPICalled'&&m.params.type==='error')errors.push({console:m.params.args.map(a=>a.description||a.value).join(' ')});};
 await call('Page.enable');await call('Runtime.enable');await call('Emulation.setDeviceMetricsOverride',{width:1440,height:900,deviceScaleFactor:1,mobile:false});
 await call('Page.navigate',{url:'http://127.0.0.1:5291/dev/stun-overlay.html'});await until("!!document.querySelector('#samples .hb-crust svg')");
 await evaluate(`(async()=>{const {reloadStore,store}=await import('/editors.mjs');await reloadStore();if(store.error)throw Error(store.error);const {mountBattle}=await import('/battle3d.mjs');const host=document.createElement('div');host.id='stun-qa';host.style='position:fixed;inset:0';document.body.replaceChildren(host);const api=mountBattle(host,{initialPaused:true,rendering:{maxPixelRatio:1,shadowSize:256}});api.show();const vm=host._vnode.component.proxy;vm.closeAtlas();vm.paused=true;api.battle.character.random=()=>.99;api.battle.phase='cleared';globalThis.qa={api,vm};})()`);
 let s=await sample('initial');check(s.barWidth===100&&Math.abs(s.barNow-s.capacity)<.01,'HUD starts full and shows remaining stun durability');
 await evaluate("qa.api.battle.combat.heroDamage({amount:qa.api.battle.stunCrust.capacity/2,element:'physical'})");
 s=await sample('hit');const damaged=s;check(s.barWidth<100&&s.barNow<s.capacity,'resolved hit reduces the actual HUD bar');
 await step(.75);await evaluate("qa.api.battle.combat.heroDamage({amount:1,element:'physical',dot:true})");s=await sample('dot');check(Math.abs(s.barNow-damaged.barNow)<.01,'DOT does not reduce stun durability');
 await step(.25);s=await sample('one-second');check(Math.abs(s.barNow-damaged.barNow)<.01,'recovery holds for exactly one hit-free second');
 await step(1.5);s=await sample('half-recovered');check(Math.abs(s.barNow-(damaged.barNow+s.capacity)/2)<.03&&s.barWidth>damaged.barWidth,'DOT did not restart the delay and HUD recovers halfway after 1.5 seconds');await shot('half-recovered');
 await step(1.5);s=await sample('full');check(Math.abs(s.barNow-s.capacity)<.01&&s.barWidth===100,'HUD is full after three recovery seconds in a cleared dungeon');
 await evaluate("qa.api.battle.combat.heroDamage({amount:qa.api.battle.stunCrust.capacity*2,element:'physical'})");s=await sample('broken');check(s.phase==='broken'&&s.barWidth===0,'empty bar triggers the existing stun and remains visibly empty');
 await step(1);s=await sample('broken-one-second');check(s.stunSeconds===0&&s.barWidth===0,'stun expires while waiting for recovery after clearing');
 await step(3);s=await sample('broken-full');check(s.phase==='ready'&&s.barWidth===100,'broken bar fully recovers after clearing');await shot('full-recovered');
 await call('Page.navigate',{url:'http://127.0.0.1:5291/?view=battle'});await until("!!document.querySelector('.b3-stun')");
 const live=await evaluate("(()=>{const b=document.querySelector('.b3-stun');return {now:+b.getAttribute('aria-valuenow'),max:+b.getAttribute('aria-valuemax'),width:parseFloat(b.firstElementChild.style.width),release:[...document.scripts].map(s=>s.src).find(s=>s.includes('/_release/'))}})()");
 const release=JSON.parse(await fs.readFile(path.resolve(proof,'../../RuleStudioWeb/current.json'),'utf8')).id;
 check(live.now===live.max&&live.width===100,'5291 published page uses the remaining-durability HUD');check(live.release?.includes(release),'5291 loads the current published release');
 check(errors.length===0,'no browser script errors');
 await fs.writeFile(path.join(proof,'browser-receipt.json'),JSON.stringify({ok:true,release,checks,samples,live,errors,isolatedBrowserProfile:true},null,2));console.log(JSON.stringify({ok:true,release,checks}));
}catch(error){await fs.writeFile(path.join(proof,'browser-failure.json'),JSON.stringify({ok:false,error:error.stack,checks,samples,errors},null,2));throw error;}
finally{try{await call('Browser.close');}catch{}ws?.close();if(browser.exitCode===null)browser.kill();}
