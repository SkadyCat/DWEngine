import {UIRuntime} from '../src/runtime/runtime.js';
const record=await fetch('/api/projects/opus-recent-gameplay').then(r=>r.json()),project=record.document||record;
const slot=structuredClone(project.root.children.find(n=>n.id==='aura-hud-slot'));
slot.slot.offsets={left:0,top:0,right:64,bottom:76};
const demo={...project,root:slot,viewport:{width:64,height:76,background:'transparent'},assets:{...project.assets}};
// The actual runtime icon is data-bound; this preview supplies the authored Wind Breath icon.
const wind=await fetch('/api/projects/opus-wind-breath').then(r=>r.json());
Object.assign(demo.assets,(wind.document||wind).assets);
const iconId=Object.keys((wind.document||wind).assets)[0];
const off=new UIRuntime(document.querySelector('#inactive'),demo,{width:64,height:76,sound:false});
const on=new UIRuntime(document.querySelector('#active'),demo,{width:64,height:76,sound:false});
for(const runtime of [off,on])runtime.set('aura-slot-icon',{svgAsset:iconId});
let enabled=true;
function show(){on.setMany([['aura-slot-active',{motion:{enabled}}],['aura-slot-state',{text:enabled?'开启':'关闭'}]]);document.querySelector('#toggle').textContent=enabled?'关闭右侧光环':'开启右侧光环';document.querySelector('#result').textContent=enabled?'右侧光环已激活':'右侧光环已关闭';}
document.querySelector('#toggle').onclick=()=>{enabled=!enabled;show();};show();
