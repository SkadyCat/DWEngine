// Exercise the production Vue atlas on its isolated test origin, without a player save.
import fs from 'node:fs/promises';import path from 'node:path';import {spawn} from 'node:child_process';
const proof=path.resolve(import.meta.dirname,'../../Saved/Verification/AtlasStreaming');await fs.mkdir(proof,{recursive:true});
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

 await call('Emulation.setEmulatedMedia',{features:[{name:'prefers-reduced-motion',value:'reduce'}]});
 await call('Page.navigate',{url:'http://127.0.0.1:5340/dev/atlas-tiers.html'});await until("!!document.querySelector('.atlas-viewport[data-atlas-window]')");await sleep(700);
 const samples=[];
 const read=()=>evaluate("(()=>{const e=document.querySelector('.atlas-viewport'),d=e.dataset;return {window:d.atlasWindow,nodes:+d.loadedNodes,depth:+d.maxDepth,tiles:+d.terrainTiles,current:d.currentNode,destinations:d.destinationCount,dragging:e.classList.contains('dragging')}})()");
 const saved=await evaluate("JSON.stringify(document.querySelector('#app')._vnode.component.proxy.state)");
 const initial=await read();samples.push({phase:'initial',...initial});check(initial.nodes>0,'initial visible graph is generated');
 const box=await evaluate("(()=>{const r=document.querySelector('.atlas-viewport').getBoundingClientRect();return{x:r.x,y:r.y,w:r.width,h:r.height}})()");
 async function drag(dx,dy=0){const x=box.x+box.w*(dx<0?.8:.2),y=box.y+box.h*.54;await call('Input.dispatchMouseEvent',{type:'mouseMoved',x,y,buttons:0});await call('Input.dispatchMouseEvent',{type:'mousePressed',x,y,button:'left',buttons:1,clickCount:1});for(let i=1;i<=5;i++){await call('Input.dispatchMouseEvent',{type:'mouseMoved',x:x+dx*i/5,y:y+dy*i/5,button:'none',buttons:1});await sleep(15);}await call('Input.dispatchMouseEvent',{type:'mouseReleased',x:x+dx,y:y+dy,button:'left',buttons:0,clickCount:1});await sleep(60);const s=await read();check(s.nodes>0&&s.nodes<2000&&s.tiles<=64&&!s.dragging,'drag keeps a populated bounded window and releases pointer capture');return s;}
 for(let i=0;i<12;i++)await drag(-box.w*.6);
 let far=await read();samples.push({phase:'far',...far});check(far.depth>35&&far.window!==initial.window,'real mouse dragging generates nodes beyond the old radius-ten map');await sleep(400);await shot('far-drag');
 for(let i=0;i<7;i++)await call('Input.dispatchMouseEvent',{type:'mouseWheel',x:box.x+box.w/2,y:box.y+box.h/2,deltaX:0,deltaY:180});await sleep(400);
 for(let i=0;i<22;i++)await drag(-box.w*.6,box.h*.12);
 far=await read();samples.push({phase:'distant-zoomed-out',...far});check(far.depth>180,'generation continues as browsing moves much farther away');check(far.tiles<=64,'terrain cache stays bounded after visiting many windows');
 await call('Input.dispatchKeyEvent',{type:'keyDown',key:'f',code:'KeyF',windowsVirtualKeyCode:70});await call('Input.dispatchKeyEvent',{type:'keyUp',key:'f',code:'KeyF',windowsVirtualKeyCode:70});await sleep(1100);
 check(await evaluate("document.querySelector('.atlas-player-marker').style.display!=='none'"),'F returns to the player after distant browsing');
 check(await evaluate("JSON.stringify(document.querySelector('#app')._vnode.component.proxy.state)")===saved,'panning and zooming never change exploration, selection, tickets or attempts');
 const reloaded=await evaluate("(async()=>{const {atlasNodesInBounds,normalizeAtlas}=await import('/atlas-state.mjs');const s=document.querySelector('#app')._vnode.component.proxy.state,b={minX:50000,minY:-75000,maxX:52000,maxY:-73000};return JSON.stringify(atlasNodesInBounds(s,b))===JSON.stringify(atlasNodesInBounds(normalizeAtlas(JSON.parse(JSON.stringify(s))),b));})()");check(reloaded,'reloading preserves identical generated node positions and roads');await shot('returned-player');
 await call('Page.navigate',{url:'http://127.0.0.1:5291/?view=battle'});await until("!!document.querySelector('.b3-stage')");
 const release=JSON.parse(await fs.readFile(path.resolve(proof,'../../RuleStudioWeb/current.json'),'utf8')).id;
 if(!await evaluate("!!document.querySelector('.atlas-viewport')")){await call('Input.dispatchKeyEvent',{type:'keyDown',key:'m',code:'KeyM',windowsVirtualKeyCode:77});await call('Input.dispatchKeyEvent',{type:'keyUp',key:'m',code:'KeyM',windowsVirtualKeyCode:77});}
 await until("!!document.querySelector('.atlas-viewport[data-atlas-window]')");check(await evaluate('Array.from(document.scripts).some(s=>s.src.includes('+JSON.stringify('/_release/'+release+'/')+'))'),'5291 loads the published streaming atlas');check(errors.length===0,'no browser script errors');
 await fs.writeFile(path.join(proof,'browser-receipt.json'),JSON.stringify({ok:true,release,checks,samples,errors,isolatedBrowserProfile:true},null,2));console.log(JSON.stringify({ok:true,release,checkCount:checks.length,samples}));
}catch(error){await fs.writeFile(path.join(proof,'browser-failure.json'),JSON.stringify({ok:false,error:error.stack,checks,errors},null,2));throw error;}
finally{try{await call('Browser.close');}catch{}ws?.close();if(browser.exitCode===null)browser.kill();}
