// Reward selection through real pointer events in a separate browser profile.
import fs from 'node:fs/promises';
import path from 'node:path';
import {spawn} from 'node:child_process';
const proof=path.resolve(import.meta.dirname,'../../Saved/Verification/StudioEventRewardDrag');
await fs.mkdir(proof,{recursive:true});
const profile=path.join(proof,'browser-'+Date.now());
const browser=spawn('C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',['--headless=new','--no-first-run','--no-default-browser-check','--remote-debugging-port=0','--remote-debugging-address=127.0.0.1','--user-data-dir='+profile,'about:blank'],{windowsHide:true,stdio:'ignore'});
const sleep=ms=>new Promise(r=>setTimeout(r,ms));
let ws,seq=0;const pending=new Map(),errors=[],checks=[],logs=[];
function check(value,name){if(!value)throw Error(name);checks.push(name);}
async function bounded(work){let timer;try{return await Promise.race([work,new Promise((_,reject)=>{timer=setTimeout(()=>reject(Error('Browser operation timed out')),15000);})]);}finally{clearTimeout(timer);}}
async function call(method,params={}){const id=++seq;return bounded(new Promise((resolve,reject)=>{pending.set(id,{resolve,reject});ws.send(JSON.stringify({id,method,params}));}));}
async function evaluate(expression){const r=await call('Runtime.evaluate',{expression,returnByValue:true,awaitPromise:true});if(r.exceptionDetails)throw Error(r.exceptionDetails.exception?.description||r.exceptionDetails.text);return r.result.value;}
async function until(expression){const end=Date.now()+30000;while(Date.now()<end){if(await evaluate(expression))return;await sleep(100);}throw Error('Timed out waiting for '+expression);}
async function shot(name){const r=await call('Page.captureScreenshot',{format:'png',captureBeyondViewport:false});await fs.writeFile(path.join(proof,name+'.png'),Buffer.from(r.data,'base64'));}
try{
 let port;for(let i=0;i<80;i++){try{port=Number((await fs.readFile(path.join(profile,'DevToolsActivePort'),'utf8')).split('\n')[0]);break;}catch{await sleep(100);}}
 if(!port)throw Error('Owned browser did not expose loopback debugger');
 const pages=await fetch(`http://127.0.0.1:${port}/json/list`).then(r=>r.json());ws=new WebSocket(pages.find(p=>p.type==='page').webSocketDebuggerUrl);
 await bounded(new Promise((resolve,reject)=>{ws.onopen=resolve;ws.onerror=reject;}));
 ws.onmessage=event=>{const m=JSON.parse(event.data);if(m.id){const p=pending.get(m.id);pending.delete(m.id);if(m.error)p?.reject(Error(m.error.message));else p?.resolve(m.result);}else if(m.method==='Runtime.exceptionThrown')errors.push(m.params.exceptionDetails);else if(m.method==='Runtime.consoleAPICalled'&&m.params.type==='debug')logs.push(m.params.args.map(a=>a.value??a.preview??a.description));else if(m.method==='Runtime.consoleAPICalled'&&m.params.type==='error')errors.push({console:m.params.args.map(a=>a.description||a.value).join(' ')});};
 await call('Page.enable');await call('Runtime.enable');await call('Emulation.setDeviceMetricsOverride',{width:1440,height:900,deviceScaleFactor:1,mobile:false});
 await call('Page.navigate',{url:'http://127.0.0.1:5291/dev/stun-overlay.html'});await until("!!document.querySelector('#samples .hb-crust svg')");
 await evaluate(`(async()=>{
   await new Promise((resolve,reject)=>{const link=document.createElement('link');link.rel='stylesheet';link.href='/battle-map-events.css';link.onload=resolve;link.onerror=reject;document.head.append(link);});
   const {reloadStore,store}=await import('/editors.mjs');await reloadStore();if(store.error)throw Error(store.error);
   const {mountBattle}=await import('/battle3d.mjs');const host=document.createElement('div');host.id='event-drag-qa';host.style='position:fixed;inset:0';document.body.replaceChildren(host);
   const api=mountBattle(host,{rendering:{maxPixelRatio:1,shadowSize:256}});api.show();const vm=host._vnode.component.proxy;vm.closeAtlas();vm.paused=true;globalThis.qa={api,vm};
   globalThis.pointerLog=[];for(const type of ['pointerdown','pointerup','lostpointercapture'])window.addEventListener(type,e=>pointerLog.push({type,x:e.clientX,y:e.clientY,node:e.target.closest?.('[data-node-type=Button]')?.dataset.nodeId,tag:e.target.tagName}),true);
 })()`);
 await until("!!document.querySelector('#battle-event-menu [data-node-id=launch-0]')");
 async function point(selector){return evaluate(`(()=>{const e=document.querySelector(${JSON.stringify(selector)}),r=e.getBoundingClientRect();if(!r.width||!r.height)throw Error('Invisible pointer target');return {x:r.x+r.width/2,y:r.y+r.height/2}})()`);}
 async function move(p,buttons=0){await call('Input.dispatchMouseEvent',{type:'mouseMoved',...p,buttons});}
 async function down(p){await move(p);await call('Input.dispatchMouseEvent',{type:'mousePressed',...p,button:'left',buttons:1,clickCount:1});}
 async function up(p){await call('Input.dispatchMouseEvent',{type:'mouseReleased',...p,button:'left',buttons:0,clickCount:1});}
 async function click(selector){const p=await point(selector);await down(p);await up(p);}
 async function drag(source,destination){const from=await point(source),to=typeof destination==='string'?await point(destination):destination;await down(from);await move({x:from.x+20,y:from.y+10},1);await move(to,1);return to;}
 const grid=i=>'.b3-event-layer [data-node-id=event-bag-'+i+']';
 const slot=i=>'.b3-event-layer [data-node-id=input-'+i+']';
 const owned=()=>evaluate('JSON.stringify(qa.vm.player.instances)');
 async function selected(){return evaluate("Array.from(document.querySelectorAll('.b3-event-layer [data-node-id^=event-bag-]')).filter(e=>/^event-bag-\\d+$/.test(e.dataset.nodeId)).findIndex(e=>e.textContent.includes('已选中'))");}
 async function open(kind){await evaluate(`qa.vm.mapEventUI.open(${JSON.stringify(kind)})`);await until("document.querySelector('.b3-event-layer')?.getBoundingClientRect().height===innerHeight");await sleep(200);}
 await open('choice');const before=await owned();
 check(await selected()===0,'initial selection stays on the first candidate');
 const p=await point(grid(1));await down(p);await sleep(100);
 check(await evaluate("!document.querySelector('.event-drag-ghost')"),'press without movement does not drag');
 await move({x:p.x+3,y:p.y+2},1);check(await evaluate("!document.querySelector('.event-drag-ghost')"),'small pointer jitter stays a click');
 await up({x:p.x+3,y:p.y+2});await sleep(150);
 check(await selected()===1,'normal click selects a candidate once');
 const drop=await drag(grid(2),slot(0));
 check(await evaluate("!!document.querySelector('.event-drag-ghost')"),'held movement creates UICreator drag ghost');
 check(await evaluate("!!document.querySelector('[data-node-id=input-frame-0][data-drop-target]')"),'current reward slot highlights as valid target');
 check(await selected()===1,'movement alone does not commit selection');
 await shot('choice-drag');await up(drop);await sleep(200);
 check(await selected()===2,'release over current reward selects the dragged candidate');
 check(await owned()===before,'candidate selection never grants an item');
 const invalid=await drag(grid(0),slot(1));await up(invalid);await sleep(150);
 check(await selected()===2,'drop on rules slot preserves current selection');
 const cancel=await drag(grid(0),{x:20,y:20});
 await call('Input.dispatchKeyEvent',{type:'keyDown',key:'Escape',code:'Escape',windowsVirtualKeyCode:27});await up(cancel);await sleep(100);
 check(await evaluate("!!document.querySelector('.b3-event-layer')&&!document.querySelector('.event-drag-ghost')"),'Escape cancels drag without closing reward panel');
 check(await selected()===2&&await owned()===before,'cancel preserves both selection and inventory');
 const blurDrop=await drag(grid(0),slot(0));
 await evaluate("window.dispatchEvent(new Event('blur'))");await up(blurDrop);
 check(await evaluate("!document.querySelector('.event-drag-ghost')&&!document.querySelector('[data-drop-target]')")&&await selected()===2,'losing window focus cancels ghost and highlight without selecting');
 await shot('choice-selected');await click('.b3-event-layer [data-node-id=event-close]');await open('choice');
 check(await selected()===2,'reopening preserves selected fixed candidate');
 const countBefore=await evaluate('Object.keys(qa.vm.player.instances).length');
 await click('.b3-event-layer [data-node-id=event-primary]');await sleep(200);
 check(await evaluate(`Object.keys(qa.vm.player.instances).length===${countBefore+1}`),'explicit confirmation grants exactly one item');
 const after=await owned(),spent=await drag(grid(0),slot(0));await up(spent);await sleep(150);
 check(await owned()===after&&await selected()===2,'completed reward cannot select or grant another candidate');
 await click('.b3-event-layer [data-node-id=event-close]');await open('vault');await click('.b3-event-layer [data-node-id=event-primary]');await sleep(200);
 const vaultBefore=await owned(),vaultDrop=await drag(grid(1),slot(0));await up(vaultDrop);await sleep(150);
 check(await selected()===1&&await owned()===vaultBefore,'vault uses the same selection-only drag path');
 await click('.b3-event-layer [data-node-id=event-close]');await open('fusion');await click('.b3-event-layer [data-node-id=event-samples]');await sleep(200);
 const eligible=await evaluate("Array.from(document.querySelectorAll('.b3-event-layer [data-node-id^=event-bag-]')).filter(e=>/^event-bag-\\d+$/.test(e.dataset.nodeId)&&e.getAttribute('aria-label')?.startsWith('T')&&!e.textContent.includes('已穿戴')).map(e=>e.dataset.nodeId)");
 // Generated sample weapons are the final two entries, following initial gear and one claimed reward.
 const ids=eligible.slice(-2);if(ids.length!==2)throw Error('Missing owned sample equipment');
 for(let i=0;i<2;i++){const to=await drag('.b3-event-layer [data-node-id='+ids[i]+']',slot(i));await up(to);await sleep(150);}
 check(await evaluate(`document.querySelector('.b3-event-layer [data-node-id=${ids[0]}]').textContent.includes('已放入')&&document.querySelector('.b3-event-layer [data-node-id=${ids[1]}]').textContent.includes('已放入')`),'owned equipment still drags into both fusion slots');
 await call('Page.navigate',{url:'http://127.0.0.1:5291/?view=battle'});await until("!!document.querySelector('.b3-stage')");
 const release=JSON.parse(await fs.readFile(path.resolve(proof,'../../RuleStudioWeb/current.json'),'utf8')).id;
 check(await evaluate(`Array.from(document.scripts).some(s=>s.src.includes('/_release/${release}/'))`),'5291 serves published release');
 check(errors.length===0,'no browser script or console errors');
 await fs.writeFile(path.join(proof,'browser-receipt.json'),JSON.stringify({ok:true,release,checks,errors,isolatedBrowserProfile:true},null,2));console.log(JSON.stringify({ok:true,release,checks}));
}catch(error){let debug;try{debug=await evaluate("({log:globalThis.pointerLog,text:document.querySelector('.b3-event-layer')?.textContent})");await shot('failure');}catch{}await fs.writeFile(path.join(proof,'browser-failure.json'),JSON.stringify({ok:false,error:error.stack,checks,errors,logs,debug},null,2));throw error;}
finally{try{await call('Browser.close');}catch{}ws?.close();if(browser.exitCode===null)browser.kill();}
