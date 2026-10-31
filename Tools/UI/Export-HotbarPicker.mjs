import fs from 'node:fs/promises';import path from 'node:path';import os from 'node:os';import crypto from 'node:crypto';import {pathToFileURL} from 'node:url';
import {createServer} from '../../CustomTools/UICreatorAlpha/server.mjs';
const {chromium}=await import(pathToFileURL(path.join(os.homedir(),'.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/playwright/index.mjs')));
const root=path.resolve(import.meta.dirname,'../..'),proof=path.join(root,'Saved/Verification/HotbarPickerUE');await fs.mkdir(proof,{recursive:true});
const doc=JSON.parse(await fs.readFile(path.join(root,'UIAuthoring/HotbarPicker.uialpha.json'),'utf8')),job='wbp-hotbar-'+crypto.randomUUID();
const server=createServer();await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));const origin='http://127.0.0.1:'+server.address().port;
const browser=await chromium.launch({headless:true,executablePath:'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe'});
try{
 const page=await browser.newPage({viewport:{width:1500,height:760}});const errors=[];page.on('pageerror',e=>errors.push(e.message));
 await page.route('**/hotbar-freeze',route=>route.fulfill({contentType:'text/html',body:'<!doctype html><meta charset="utf-8"><body style="margin:0;background:#090d13"><svg id="scene"></svg></body>'}));await page.goto(origin+'/hotbar-freeze');
 const frozen=await page.evaluate(async({doc,job})=>{const {UIRuntime}=await import('/src/runtime/runtime.js');const runtime=new UIRuntime(document.querySelector('#scene'),doc,{motion:false,sound:false});await document.fonts.ready;const svg=new XMLSerializer().serializeToString(document.querySelector('#scene'));const r=await fetch('/api/native-wbp/'+job+'/source',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({project:doc,svg,context:{width:1400,height:650,expandedRoot:runtime.expanded.document.root}})});if(!r.ok)throw Error(await r.text());return r.json();},{doc,job});
 await page.screenshot({path:path.join(proof,'uicreator-source.png')});await page.goto(origin+frozen.url);await page.locator('#export').waitFor({state:'visible'});await page.waitForFunction(()=>!document.querySelector('#export').disabled);await page.locator('#states').uncheck();await page.locator('#export').click();await page.waitForFunction(()=>document.querySelector('#status')?.dataset.result,{timeout:120000});
 const result=await page.locator('#status').getAttribute('data-result');if(result!=='passed'||errors.length)throw Error(await page.locator('#status').textContent()+'\n'+errors.join('\n'));
 const bundle=JSON.parse(await fs.readFile(path.join(root,'CustomTools/UICreatorAlpha/exports',job,'bundle.json'),'utf8'));
 await fs.writeFile(path.join(proof,'export.json'),JSON.stringify({job,sourceHash:bundle.sourceHash,templates:doc.root.children.map(n=>n.id),nodes:bundle.nodes.length,rasters:Object.keys(bundle.rasters).length,errors},null,2));console.log(JSON.stringify({job,sourceHash:bundle.sourceHash,nodes:bundle.nodes.length,rasters:Object.keys(bundle.rasters).length}));
}finally{await browser.close();await new Promise(resolve=>server.close(resolve));}
