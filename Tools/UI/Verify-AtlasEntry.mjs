// Exercise the production Vue atlas on its isolated test origin, without a player save.
import fs from 'node:fs/promises';import path from 'node:path';import {spawn} from 'node:child_process';
const proof=path.resolve(import.meta.dirname,'../../Saved/Verification/MapEntry');await fs.mkdir(proof,{recursive:true});
const profile=path.join(proof,'browser-'+Date.now());
const browser=spawn('C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',['--headless=new','--no-first-run','--no-default-browser-check','--remote-debugging-port=0','--remote-debugging-address=127.0.0.1','--user-data-dir='+profile,'about:blank'],{windowsHide:true,stdio:'ignore'});
const sleep=ms=>new Promise(r=>setTimeout(r,ms));let ws,seq=0;const pending=new Map(),errors=[],checks=[];
function check(value,name){if(!value)throw Error(name);checks.push(name);}
async function bounded(work,ms=12000){let timer;try{return await Promise.race([work,new Promise((_,reject)=>{timer=setTimeout(()=>reject(Error('Browser operation timed out')),ms);})]);}finally{clearTimeout(timer);}}
async function call(method,params={}){const id=++seq;return bounded(new Promise((resolve,reject)=>{pending.set(id,{resolve,reject});ws.send(JSON.stringify({id,method,params}));}));}
async function evaluate(expression){const r=await call('Runtime.evaluate',{expression,returnByValue:true,awaitPromise:true});if(r.exceptionDetails)throw Error(r.exceptionDetails.text);return r.result.value;}
async function until(expression){const end=Date.now()+25000;while(Date.now()<end){if(await evaluate(expression))return;await sleep(100);}throw Error('Timed out waiting for '+expression);}
async function click(selector){const rect=await evaluate(`(()=>{const r=document.querySelector(${JSON.stringify(selector)}).getBoundingClientRect();return {x:r.x+r.width/2,y:r.y+r.height/2}})()`);await call('Input.dispatchMouseEvent',{type:'mousePressed',...rect,button:'left',clickCount:1});await call('Input.dispatchMouseEvent',{type:'mouseReleased',...rect,button:'left',clickCount:1});}
async function shot(name){const r=await call('Page.captureScreenshot',{format:'png',captureBeyondViewport:false});await fs.writeFile(path.join(proof,name+'.png'),Buffer.from(r.data,'base64'));}
try{
 let port;for(let i=0;i<80;i++){try{port=Number((await fs.readFile(path.join(profile,'DevToolsActivePort'),'utf8')).split('\n')[0]);break;}catch{await sleep(100);}}
 if(!port)throw Error('Owned browser did not expose its loopback debugger');
 const pages=await fetch(`http://127.0.0.1:${port}/json/list`).then(r=>r.json());ws=new WebSocket(pages.find(p=>p.type==='page').webSocketDebuggerUrl);
 await bounded(new Promise((resolve,reject)=>{ws.onopen=resolve;ws.onerror=reject;}));
 ws.onmessage=event=>{const m=JSON.parse(event.data);if(m.id){const p=pending.get(m.id);pending.delete(m.id);if(m.error)p?.reject(Error(m.error.message));else p?.resolve(m.result);}else if(m.method==='Runtime.exceptionThrown')errors.push(m.params.exceptionDetails);else if(m.method==='Runtime.consoleAPICalled'&&m.params.type==='error')errors.push({console:m.params.args.map(a=>a.description||a.value).join(' ')});};
 await call('Page.enable');await call('Runtime.enable');await call('Emulation.setDeviceMetricsOverride',{width:1440,height:900,deviceScaleFactor:1,mobile:false});await call('Emulation.setEmulatedMedia',{features:[{name:'prefers-reduced-motion',value:'no-preference'}]});
 if(process.argv.includes('--drag')){
  await call('Emulation.setEmulatedMedia',{features:[{name:'prefers-reduced-motion',value:'reduce'}]});
  await call('Page.navigate',{url:'http://127.0.0.1:5340/dev/atlas-tiers.html'});await until("!!document.querySelector('.atlas-viewport canvas')&&!!document.querySelector('.atlas-enter svg')");await sleep(400);
  const box=await evaluate("(()=>{const r=document.querySelector('.atlas-viewport').getBoundingClientRect();return {x:r.x,y:r.y,w:r.width,h:r.height}})()");
  // Observe the actual draw transform; hover decoration may legitimately change pixels.
  await evaluate("(()=>{const c=document.querySelector('.atlas-viewport canvas').getContext('2d'),clear=c.clearRect,translate=c.translate;let step=0;c.clearRect=function(...a){step=0;return clear.apply(this,a)};c.translate=function(x,y){if(++step===2)globalThis.__atlasDrawPan={x,y};return translate.call(this,x,y)}})()");
  const move=(x,y,buttons=0)=>call('Input.dispatchMouseEvent',{type:'mouseMoved',x,y,button:'none',buttons});
  const down=(x,y)=>call('Input.dispatchMouseEvent',{type:'mousePressed',x,y,button:'left',buttons:1,clickCount:1});
  const up=(x,y)=>call('Input.dispatchMouseEvent',{type:'mouseReleased',x,y,button:'left',buttons:0,clickCount:1});
  const dragging=()=>evaluate("document.querySelector('.atlas-viewport').classList.contains('dragging')");
  const pixels=()=>evaluate("(()=>{const c=document.querySelector('.atlas-viewport canvas'),p=c.getContext('2d').getImageData(0,0,c.width,c.height).data;let h=2166136261;for(let i=0;i<p.length;i+=4)h=Math.imul(h^p[i]^p[i+1]<<8^p[i+2]<<16,16777619);return h>>>0})()");
  // The map stops redrawing below .03 world units of camera easing (< .1 px).
  const pan=()=>evaluate('globalThis.__atlasDrawPan');const samePan=(a,b)=>a&&b&&Math.hypot(a.x-b.x,a.y-b.y)<.05;
  const x=box.x+30,y=box.y+70;
  await down(box.x+box.w/2,box.y+box.h/2);await up(box.x+box.w/2,box.y+box.h/2);await move(x,y);await sleep(300);
  check(errors.length===0,'single click has no selection-handler exception');
  check(!await dragging(),'single click and release never latch map dragging');
  check(await evaluate("document.querySelector('.atlas-region-title h3').textContent==='初始界域'"),'single click still selects the actual region');
  const idle=await pixels(),idlePan=await pan();await move(x+2,y+2);await sleep(300);check(samePan(await pan(),idlePan),'hover after release does not pan the map');
  await down(x+2,y+2);await move(x+5,y+5,1);await up(x+5,y+5);await move(x,y);await sleep(300);check(samePan(await pan(),idlePan),'sub-threshold click jitter does not pan');
  const selected=await evaluate("document.querySelector('.atlas-region-title h3').textContent");await down(x,y);await move(x+100,y+60,1);await sleep(300);
  check(await dragging()&&await pixels()!==idle,'held pointer movement performs real map panning');await shot('atlas-real-drag');
  await up(box.x+box.w+40,y+60);await move(x,y);await sleep(850);check(!await dragging(),'release outside viewport clears dragging');
  check(await evaluate("document.querySelector('.atlas-region-title h3').textContent")===selected,'panning does not accidentally select a region');
  const released=await pan();await move(x+2,y+2);await sleep(250);check(samePan(await pan(),released),'movement after outside release stays idle');
  await down(x,y);await move(x+60,y+30,1);await move(x+70,y+40,0);check(!await dragging(),'missing mouse-up is recovered from released buttons');await up(x+70,y+40);
  await down(x,y);await move(x+40,y+20,1);await evaluate("window.dispatchEvent(new Event('blur'))");check(!await dragging(),'window blur cancels the gesture');await up(x+40,y+20);
  check(errors.length===0,'dragging and cancellation produce no script exceptions');
  await fs.writeFile(path.join(proof,'atlas-drag-receipt.json'),JSON.stringify({ok:true,checks,errors,isolatedBrowserProfile:true},null,2));console.log(JSON.stringify({ok:true,checks}));
 }else if(process.argv.includes('--studio')){
  const release=JSON.parse(await fs.readFile(path.resolve(proof,'../../RuleStudioWeb/current.json'),'utf8')).id;
  await call('Page.navigate',{url:'http://127.0.0.1:5291/?view=battle'});
  await until("!!document.querySelector('.b3-stage,.b3-window-atlas,.atlas-enter')");
  if(!await evaluate("!!document.querySelector('.atlas-enter')")){await sleep(1500);await call('Input.dispatchKeyEvent',{type:'keyDown',key:'m',code:'KeyM',windowsVirtualKeyCode:77});await call('Input.dispatchKeyEvent',{type:'keyUp',key:'m',code:'KeyM',windowsVirtualKeyCode:77});}
  await until("!!document.querySelector('.atlas-enter svg')");await sleep(300);
  check(await evaluate("!document.querySelector('.atlas-facts,.atlas-route-select')"),'5291 production atlas has simplified details');
  check(await evaluate(`[...document.scripts].some(s=>s.src.includes('/_release/${release}/'))`),'5291 serves rebuilt Studio release');
  check(await evaluate("!!document.querySelector('.atlas-enter[data-active]')"),'5291 mounts shared entry state control');
  await click('.atlas-viewport');
  const mapCorner=await evaluate("(()=>{const r=document.querySelector('.atlas-viewport').getBoundingClientRect();return{x:r.x+30,y:r.y+70}})()");await call('Input.dispatchMouseEvent',{type:'mouseMoved',...mapCorner,button:'none',buttons:0});await sleep(100);
  check(await evaluate("!document.querySelector('.atlas-viewport').classList.contains('dragging')"),'5291 click and release does not latch map dragging');
  check(errors.length===0,'5291 has no browser script exceptions');await shot('studio-5291');
  await fs.writeFile(path.join(proof,'studio-receipt.json'),JSON.stringify({ok:true,url:'http://127.0.0.1:5291/?view=battle',release,checks,errors,isolatedBrowserProfile:true},null,2));console.log(JSON.stringify({ok:true,release,checks}));
 }else if(process.argv.includes('--export')){
  await call('Page.navigate',{url:'http://127.0.0.1:5190/examples/full-hud-export.html?project=opus-map-device'});
  await until("!!document.querySelector('#freeze:not(:disabled)')");await click('#freeze');await until("!!document.querySelector('#export:not([hidden])')");
  const exportUrl=await evaluate("document.querySelector('#export').href"),job=new URL(exportUrl).searchParams.get('job');
  await call('Page.navigate',{url:exportUrl+'&cacheJob=wbp-761aa9d5-694e-4c4f-88a6-4051b7629623'});await until("typeof document.querySelector('#export')?.onclick === 'function'");await click('#export');
  const deadline=Date.now()+90000;while(Date.now()<deadline){const status=await evaluate("document.querySelector('#status')?.dataset.result");if(status==='failed')throw Error(await evaluate("document.querySelector('#status').textContent"));if(status==='passed'){checks.push('frozen browser export saved');break;}await sleep(500);}
  check(checks.includes('frozen browser export saved'),'native bundle export completed');await fs.writeFile(path.join(proof,'export.json'),JSON.stringify({job,url:exportUrl,checks},null,2));console.log(JSON.stringify({job,url:exportUrl,checks}));
 }else{
 await call('Page.navigate',{url:'http://127.0.0.1:5340/dev/atlas-tiers.html'});
 await until("!!document.querySelector('.atlas-enter:disabled[data-active=false]')");
 check(await evaluate("!document.querySelector('.atlas-facts,.atlas-route-select')"),'crossed-out controls absent');await shot('web-disabled');
 await click('#tier-qa button:nth-of-type(4)');await until("!!document.querySelector('.atlas-enter:enabled[data-active=true]')");
 check(await evaluate("document.querySelector('.atlas-enter').getAttribute('aria-label').includes('进入区域')"),'owned map enables entry');
 await sleep(300);const before=await evaluate("document.getAnimations().filter(a=>a.effect?.target?.closest('.atlas-enter')).map(a=>({time:a.currentTime,state:a.playState}))");await shot('web-active-a');await sleep(500);
 const after=await evaluate("document.getAnimations().filter(a=>a.effect?.target?.closest('.atlas-enter')).map(a=>({time:a.currentTime,state:a.playState}))");await shot('web-active-b');
 check(before.length>0&&after.some((a,i)=>a.state==='running'&&a.time>before[i]?.time),'saved border animation advances across real frames');
 await click('#tier-qa button:nth-of-type(1)');await until("!!document.querySelector('.atlas-enter:disabled[data-active=false]')");
 check(await evaluate("document.getAnimations().filter(a=>a.effect?.target?.closest('.atlas-enter')&&a.playState==='running').length===0"),'taking map out disables entry and stops the timeline');
 await click('#tier-qa button:nth-of-type(2)');await until("!!document.querySelector('.atlas-enter:enabled[data-active=true]')");await click('.atlas-enter');
 await until("document.querySelector('#tier-qa').textContent.includes('已进入 T1')");
 check(await evaluate("!!document.querySelector('.atlas-enter:enabled[data-active=false]')"),'admitted battle returns without paid-map flow');
 check(errors.length===0,'no browser script exceptions');
 await fs.writeFile(path.join(proof,'web-receipt.json'),JSON.stringify({ok:true,checks,animationBefore:before,animationAfter:after,errors,formalProfileUsed:false},null,2));console.log(JSON.stringify({ok:true,checks}));
 }
}catch(error){await fs.writeFile(path.join(proof,process.argv.includes('--drag')?'atlas-drag-failure.json':'web-failure.json'),JSON.stringify({ok:false,error:error.stack,errors,checks},null,2));throw error;}
finally{try{await call('Browser.close');}catch{}ws?.close();if(browser.exitCode===null)browser.kill();}
