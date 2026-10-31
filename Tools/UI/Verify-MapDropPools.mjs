// Exercise the published Studio UI in an isolated browser profile.
import fs from 'node:fs/promises';
import path from 'node:path';
import {spawn} from 'node:child_process';
const proof=path.resolve(import.meta.dirname,'../../Saved/Verification/MapDropPools-20261027');
await fs.mkdir(proof,{recursive:true});
const profile=path.join(proof,'browser-'+Date.now());
const browser=spawn('C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',[
  '--headless=new','--no-first-run','--no-default-browser-check','--remote-debugging-port=0',
  '--remote-debugging-address=127.0.0.1','--user-data-dir='+profile,'about:blank'],{windowsHide:true,stdio:'ignore'});
const sleep=ms=>new Promise(resolve=>setTimeout(resolve,ms));
let ws,sequence=0;const pending=new Map(),errors=[],checks=[];
function check(value,name){if(!value)throw Error(name);checks.push(name);}
async function bounded(work,ms=15000){let timer;try{return await Promise.race([work,new Promise((_,reject)=>{timer=setTimeout(()=>reject(Error('Browser operation timed out')),ms);})]);}finally{clearTimeout(timer);}}
async function call(method,params={}){const id=++sequence;return bounded(new Promise((resolve,reject)=>{pending.set(id,{resolve,reject});ws.send(JSON.stringify({id,method,params}));}));}
async function evaluate(expression){const result=await call('Runtime.evaluate',{expression,returnByValue:true,awaitPromise:true});if(result.exceptionDetails)throw Error(result.exceptionDetails.exception?.description||result.exceptionDetails.text);return result.result.value;}
async function until(expression){const deadline=Date.now()+30000;while(Date.now()<deadline){if(await evaluate(expression))return;await sleep(100);}throw Error('Timed out: '+expression);}
async function click(id){
  const at=await evaluate(`(()=>{const e=document.querySelector('[data-node-id="${id}"]');if(!e)throw Error('Missing ${id}');e.scrollIntoView({block:'center'});const r=e.getBoundingClientRect();return {x:r.x+r.width/2,y:r.y+r.height/2};})()`);
  await call('Input.dispatchMouseEvent',{type:'mouseMoved',...at});
  await call('Input.dispatchMouseEvent',{type:'mousePressed',...at,button:'left',buttons:1,clickCount:1});
  await call('Input.dispatchMouseEvent',{type:'mouseReleased',...at,button:'left',buttons:0,clickCount:1});
}
const cells="[...document.querySelectorAll('[data-candidate-id]')].map(e=>e.dataset.candidateId)";
async function shot(name){const result=await call('Page.captureScreenshot',{format:'png',captureBeyondViewport:false});await fs.writeFile(path.join(proof,name+'.png'),Buffer.from(result.data,'base64'));}
try {
  let port;
  for(let i=0;i<100;i++){try{port=Number((await fs.readFile(path.join(profile,'DevToolsActivePort'),'utf8')).split('\n')[0]);break;}catch{await sleep(100);}}
  if(!port)throw Error('Isolated browser did not start');
  const tabs=await fetch(`http://127.0.0.1:${port}/json/list`).then(r=>r.json());
  ws=new WebSocket(tabs.find(tab=>tab.type==='page').webSocketDebuggerUrl);
  await bounded(new Promise((resolve,reject)=>{ws.onopen=resolve;ws.onerror=reject;}));
  ws.onmessage=event=>{const m=JSON.parse(event.data);if(m.id){const p=pending.get(m.id);pending.delete(m.id);if(m.error)p?.reject(Error(m.error.message));else p?.resolve(m.result);}else if(m.method==='Runtime.exceptionThrown')errors.push(m.params.exceptionDetails);else if(m.method==='Runtime.consoleAPICalled'&&m.params.type==='error')errors.push(m.params.args.map(a=>a.description||a.value).join(' '));};
  await call('Page.enable');await call('Runtime.enable');await call('Network.enable');
  await call('Emulation.setDeviceMetricsOverride',{width:1440,height:1100,deviceScaleFactor:1,mobile:false});
  await call('Page.navigate',{url:'http://127.0.0.1:5298/?view=drops'});
  await until("document.querySelector('svg.drop-studio-ui')?.dataset.mapPoolState==='ready'");
  const data=await fetch('http://127.0.0.1:5298/api/drop-pools').then(r=>r.json());
  const release=JSON.parse(await fs.readFile(path.resolve(proof,'../../RuleStudioWeb/current.json'),'utf8')).id;
  check(await evaluate(`globalThis.ruleStudioAssetBase?.includes(${JSON.stringify(release)})`),'published release loads the updated UICreator grid');
  check(await evaluate("document.querySelectorAll('[data-map-id]').length")===data.maps.length,'every configured map has a selector');
  const all=[];
  for(let page=0;page<Math.ceil((data.maps[0].equipmentIds.length+data.maps[0].itemIds.length)/36);page++){
    const visible=await evaluate(cells);all.push(...visible);
    check(visible.length===new Set(visible).size,'page '+(page+1)+' contains no duplicate candidates');
    const images=await evaluate("Promise.all([...document.querySelectorAll('[data-candidate-id] image')].map(e=>{const i=new Image();i.src=e.getAttribute('href');return i.decode().then(()=>true,()=>false);})).then(v=>({count:v.length,loaded:v.every(Boolean)}))");
    check(images.count===visible.length&&images.loaded,'page '+(page+1)+' every candidate has a decoded visible image');
    if(page<Math.ceil((data.maps[0].equipmentIds.length+data.maps[0].itemIds.length)/36)-1)await click('ds-map-next');
  }
  check(JSON.stringify([...new Set(all)].sort())===JSON.stringify([...data.maps[0].equipmentIds,...data.maps[0].itemIds].sort()),'pagination exposes the complete real pool without truncation or extras');
  await click('ds-map-cell-0');
  check(await evaluate("document.querySelector('[data-node-id=ds-map-cell-0]').getAttribute('aria-pressed')==='true'"),'actual item click selects the authored cell');
  check(await evaluate("!!document.querySelector('[data-node-id=ds-map-detail-name] text')?.textContent"),'selected item displays its name in the detail area');
  await click('ds-map-filter-currency');
  const currencies=data.entries.filter(entry=>entry.kind==='item'&&entry.category==='currency').map(e=>e.id).sort();
  check(JSON.stringify((await evaluate(cells)).sort())===JSON.stringify(currencies),'currency tab shows only configured currencies');
  check(await evaluate("document.querySelector('svg.drop-studio-ui').dataset.mapPage==='0'"),'changing category resets pagination');
  for(let i=0;i<data.maps.length;i++){
    await click('ds-map-'+i);
    check(await evaluate(`document.querySelector('svg.drop-studio-ui').dataset.mapCore===${JSON.stringify(data.maps[i].id)}`),'map '+data.maps[i].id+' activates its own pool');
    const allowed=new Set(data.maps[i].itemIds);
    check((await evaluate(cells)).every(id=>allowed.has(id)),'map '+data.maps[i].id+' contains only its configured candidates');
  }
  await click('ds-map-filter-material');
  check((await evaluate(cells)).length===0,'empty material pool does not fall back to shared candidates');
  check(await evaluate("document.querySelector('[data-node-id=ds-map-empty]').textContent.includes('为空')"),'empty pool has an explicit message');
  await shot('empty-pool');
  await click('ds-map-0');await click('ds-map-filter-all');
  await evaluate("document.querySelector('[data-node-id=ds-map-cell-1]').focus()");
  await call('Input.dispatchKeyEvent',{type:'keyDown',key:'Enter',code:'Enter',windowsVirtualKeyCode:13});
  await call('Input.dispatchKeyEvent',{type:'keyUp',key:'Enter',code:'Enter',windowsVirtualKeyCode:13});
  check(await evaluate("document.querySelector('[data-node-id=ds-map-cell-1]').getAttribute('aria-pressed')==='true'"),'keyboard selection uses the same authored cell state');
  await evaluate("document.getElementById('tab-drops').scrollTop=0;document.scrollingElement.scrollTop=0");
  await sleep(300);await shot('map-pool-grid');
  await click('ds-map-refresh');await until("document.querySelector('svg.drop-studio-ui')?.dataset.mapPoolState==='ready'");
  check((await evaluate(cells)).length===36,'refresh reloads the live configured pool');
  check(errors.length===0,'no browser runtime errors');
  await fs.writeFile(path.join(proof,'browser-receipt.json'),JSON.stringify({ok:true,release,maps:data.maps.length,totalCandidates:all.length,checks,errors,isolatedBrowserProfile:true},null,2));
  console.log(JSON.stringify({ok:true,release,maps:data.maps.length,totalCandidates:all.length,checks:checks.length}));
}catch(error){try{await shot('failure');}catch{}await fs.writeFile(path.join(proof,'browser-failure.json'),JSON.stringify({ok:false,error:error.stack,checks,errors},null,2));throw error;}
finally{try{await call('Browser.close');}catch{}ws?.close();if(browser.exitCode===null)browser.kill();}
