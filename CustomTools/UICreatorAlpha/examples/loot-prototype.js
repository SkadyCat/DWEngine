import { UIRuntime } from '../src/runtime/runtime.js';
import { createNode, walk, validateDocument } from '../src/core/model.js';
import { measureText } from '../src/render/svg-renderer.js';
import { LootPrototypeSession, packLoot, lootName, lootStyle, visibleDrops } from '../src/runtime/loot-prototype.js';
import { emptyHunter, hunterAllows } from '../src/assets/loot-prototype/reference/engine/index.js';
import { affixText } from '../src/assets/loot-prototype/reference/public/affix-text.js';
const errors=[];window.addEventListener('error',e=>errors.push(e.message));window.addEventListener('unhandledrejection',e=>errors.push(String(e.reason)));
const [record,files,samples]=await Promise.all([fetch('/api/projects/opus-rulestudio-loot-20261025').then(r=>r.json()),fetch('/src/assets/loot-prototype/reference/rules.json').then(r=>r.json()),fetch('/src/assets/loot-prototype/reference/samples.json').then(r=>r.json())]);
const clean=new URLSearchParams(location.search).get('clean')==='1';
const project=structuredClone(record.document);
if(clean)walk(project.root,n=>{if(n.type==='TextBlock')n.text='';});
const runtime=new UIRuntime(document.querySelector('#scene'),project,{width:1600,height:900,motion:false,sound:false});
const session=new LootPrototypeSession(files,samples),keys=new Set();
const root=document.querySelector('#context');let detail='',pinned=false,hunterOpen=true,selectedOnly=false,previous=0,refresh=0,packed=[],paused=false,rowSignature='',query='',hunterPage=0;
const map=d=>({x:870+d.x*.58,y:428-d.y*.43});
const projectLabel=d=>{if(!session.presentation(d).landed)return null;const at=map(d);return{x:at.x,y:at.y-8.6};};
const textWidth=text=>measureText(text,{fontSize:11,fontWeight:400,fontFamily:'Segoe UI, Microsoft YaHei, sans-serif'});
function rows(id,values,op,selected){
 runtime.setChildren(id,values.map((v,i)=>createNode('Button',{id:`${op}-${i}`,name:v.name,slot:{offsets:{left:0,top:0,right:308,bottom:28},horizontalAlignment:'fill'},layout:{width:308,height:28},style:{fill:selected.includes(v.id)?'#253643':'#111b28',stroke:'#35475b',strokeWidth:1,radius:3},action:{type:'loot',op,value:v.id},children:[createNode('TextBlock',{id:`${op}-${i}-text`,text:(selected.includes(v.id)?'✓  ':'□  ')+v.name,style:{fontSize:11,textPadding:0,textPaddingX:7,color:'#c8d5e5',wrap:false,textOverflow:'ellipsis'}})]})));
}
function patches(binding,patch){return [...runtime.nodes].filter(([,n])=>n.binding===binding).map(([id])=>[id,patch]);}
function label(rect){
 const drop=rect.drop,appearance=lootStyle(drop),family=appearance.family==='currency'?`currency-${drop.currencyGrade||'special'}`:appearance.family==='omen'?'omen':drop.rarity;
 const template=project.prefabs['loot-prefab-'+family]||project.prefabs['loot-prefab-common'];const n=structuredClone(template.root);n.slot.offsets.left=rect.x;n.slot.offsets.top=rect.y;n.layout.width=n.slot.offsets.right=rect.w;
 walk(n,child=>{child.id=`drop-${drop.key}-${child.id}`;if(child.layout.width===260)child.layout.width=child.slot.offsets.right=rect.w;
  if(child.binding==='loot:name'){child.text=lootName(drop);child.layout.width=child.slot.offsets.right=rect.w-(appearance.family?45:18);}
  if(child.binding==='loot:pick'){child.name='拾取'+drop.name;child.action.value=drop.key;if(session.target===drop.key)child.brushes.normal=child.brushes.hovered;}
 });return n;
}
function details(key){
 const drop=session.remaining.find(d=>d.key===key);if(!drop){detail='';pinned=false;return;}
 detail=key;const f=lootStyle(drop),affixes=drop.item?.identified===false?'尚未鉴定':drop.item?[...(drop.item.bases||[]),...(drop.item.affixes||[]),...(drop.item.legendary?[drop.item.legendary]:[])].map(a=>affixText(a,session.library)).join('\n'):drop.description;
 const at=packed.find(r=>r.key===key),x=Math.min(1240,Math.max(380,(at?.x||910)+(at?.w||160)+12)),y=Math.min(540,Math.max(90,at?.y||260));
 const category={currency:'通货',omen:'征兆',consumable:'消耗品',material:'材料'}[drop.category]||'';
 runtime.setMany([['details-window',{visibility:'visible',slot:{offsets:{left:x,top:y}}}],...patches('details:name',{text:lootName(drop),style:{color:f.cssColor}}),...patches('details:type',{text:(drop.kind==='equipment'?'装备':'道具')+(category?' · '+category:'')+(pinned?' · 已固定':'')}),...patches('details:description',{text:affixes||drop.description||drop.name})]);
}
function draw(){
 if(clean)return;
 const visible=session.visible;packed=packLoot(visible,projectLabel,textWidth);runtime.setChildren('ground-labels',packed.map(label));
 const chosen=session.hunter.targets.map(id=>session.affixPool.find(a=>a.id===id)?.name||id).join(' · ');
 runtime.setMany([...patches('loot:summary',{text:`显示 ${visible.length} / ${session.remaining.length} 件 · 已拾取 ${session.claims.length} 件`}),...patches('loot:feedback',{text:session.feedback}),...patches('hunter:count',{text:`显示 ${visible.length} / ${session.remaining.length} 件`}),...patches('hunter:selected',{text:chosen||'尚未选择装备条件'}),['hunter-window',{visibility:hunterOpen?'visible':'collapsed'}],['hunter-window::hunter-enabled-text',{text:session.hunter.enabled?'✓ 启用过滤':'□ 过滤已关闭'}],['hunter-window::hunter-only-text',{text:selectedOnly?'查看全部':'仅看已选'}]]);
 const q=query.trim().toLocaleLowerCase(),filtered=session.affixPool.filter(a=>(!selectedOnly||session.hunter.targets.includes(a.id))&&(!q||`${a.name} ${a.id}`.toLocaleLowerCase().includes(q))).sort((a,b)=>Number(b.id===q)-Number(a.id===q)||a.name.localeCompare(b.name,'zh-CN')||a.id.localeCompare(b.id));
 const pages=Math.max(1,Math.ceil(filtered.length/12));hunterPage=Math.min(hunterPage,pages-1);const affixes=filtered.slice(hunterPage*12,hunterPage*12+12);
 runtime.setMany([...patches('hunter:pages',{text:`${hunterPage+1} / ${pages} · ${filtered.length} 种`}),['hunter-window::hunter-search-text',{text:query||'搜索现有词条',style:{textAlign:'left',color:query?'#cfdaea':'#8798ad'}}],['hunter-window::hunter-prev',{interactable:hunterPage>0}],['hunter-window::hunter-next',{interactable:hunterPage+1<pages}]]);
 const nextRows=JSON.stringify([affixes.map(a=>a.id),session.hunter.targets,session.hunter.currencies]);
 if(nextRows!==rowSignature){rowSignature=nextRows;rows('hunter-window::hunter-affixes',affixes,'hunter-target',session.hunter.targets);rows('hunter-window::hunter-currencies',session.currencyPool,'hunter-currency',session.hunter.currencies);}
 if(detail&&!session.remaining.some(d=>d.key===detail)){detail='';pinned=false;runtime.set('details-window',{visibility:'collapsed'});}
 if(detail)details(detail);
 const hero=map(session.hero),range=document.querySelector('#range').checked;
 const markers=session.showLoot?visible.map(d=>{const v=session.presentation(d),at=map({x:v.x*100,y:-v.z*100}),s=lootStyle(d);return `<g transform="translate(${at.x},${at.y-v.y*43})"><ellipse cy="18" rx="9" ry="4" fill="#000" opacity=".6"/>${v.landed?`<path d="M0 -18V10" stroke="${s.cssColor}" stroke-width="4" opacity=".2"/>`:''}<path d="M0 -3L6 2 0 7 -6 2Z" fill="${s.cssColor}"/></g>`;}).join(''):'';
 root.innerHTML=`<defs><radialGradient id="floor"><stop stop-color="#273341"/><stop offset="1" stop-color="#101823"/></radialGradient></defs><path d="M400 150H1300L1510 790H210Z" fill="url(#floor)" stroke="#34475c"/><path d="M310 650H1460M356 510H1410M400 370H1364M445 230H1320M520 150L430 790M700 150L680 790M880 150L930 790M1060 150L1180 790" stroke="#6d8194" opacity=".055"/>${markers}${range?`<ellipse cx="${hero.x}" cy="${hero.y}" rx="116" ry="86" fill="#63e8c4" fill-opacity=".018" stroke="#63e8c4" stroke-opacity=".2" stroke-dasharray="4 8"/>`:''}<ellipse cx="${hero.x}" cy="${hero.y+8}" rx="17" ry="7" fill="#000" opacity=".5"/><path d="M${hero.x} ${hero.y-18}l11 18 -11 18 -11-18Z" fill="#8be9d1" stroke="#d3fff2" stroke-width="2"/>`;
 document.querySelector('#status').textContent=`${session.rewards.ground.length} 个地面记录 · ${session.claims.length} 次领取 · ${pinned?'详情已固定':'悬停查看详情'} · 全部掉落预览场景（初始过滤关闭）`;
}
runtime.addEventListener('activate',({detail:{node}})=>{
 const a=node.action;if(a?.type!=='loot')return;
 if(a.op==='pickup')session.requestPickup(a.value);
 if(a.op==='pickup-detail'&&detail)session.requestPickup(detail);
 if(a.op==='hunter-toggle')session.hunter.enabled=!session.hunter.enabled;
 if(a.op==='hunter-open')hunterOpen=true;if(a.op==='hunter-close')hunterOpen=false;
 if(a.op==='hunter-target')session.toggleTarget(a.value);if(a.op==='hunter-currency')session.toggleCurrency(a.value);
 if(a.op==='hunter-clear')session.hunter.targets=[];if(a.op==='hunter-only')selectedOnly=!selectedOnly;
 if(a.op==='hunter-only')hunterPage=0;if(a.op==='hunter-page')hunterPage=Math.max(0,hunterPage+Number(a.value));
 if(a.op==='hunter-search'){
  document.querySelector('#hunter-query')?.remove();const input=document.createElement('input');input.id='hunter-query';input.type='search';input.setAttribute('aria-label','搜索 Hunter 词条');input.placeholder='搜索现有词条';input.value=query;
  const e=runtime.layout.entries.get('hunter-window::hunter-search'),canvas=document.querySelector('#canvas');
  Object.assign(input.style,{position:'absolute',left:`${e.globalX/16}%`,top:`${e.globalY/9}%`,width:`${e.width/16}%`,height:`${e.height/9}%`,background:'#101925',color:'#cfdaea',border:'1px solid #77b8cf',borderRadius:'4px',padding:'0 8px',font:'11px Segoe UI, Microsoft YaHei,sans-serif',zIndex:'4'});
  input.oninput=()=>{query=input.value;hunterPage=0;draw();};input.onblur=()=>{input.remove();draw();};input.onkeydown=event=>{if(['Enter','Escape'].includes(event.key)){event.stopPropagation();input.blur();}};canvas.append(input);input.focus();
 }
 if(a.op==='toggle-loot')session.showLoot=!session.showLoot;
 if(a.op==='details-close'){detail='';pinned=false;runtime.set('details-window',{visibility:'collapsed'});}
 keys.clear();draw();
});
document.querySelector('#scene').addEventListener('pointerover',e=>{const id=e.target.closest?.('[data-node-type="Button"]')?.getAttribute('data-node-id'),a=runtime.nodes.get(id)?.action;if(!pinned&&a?.op==='pickup')details(a.value);});
document.querySelector('#scene').addEventListener('pointerout',e=>{const id=e.target.closest?.('[data-node-type="Button"]')?.getAttribute('data-node-id'),other=e.relatedTarget?.closest?.('[data-node-type="Button"]')?.getAttribute('data-node-id');if(!pinned&&id!==other&&runtime.nodes.get(id)?.action?.op==='pickup'){detail='';runtime.set('details-window',{visibility:'collapsed'});}});
document.querySelector('#scene').addEventListener('focusin',e=>{const id=e.target.closest?.('[data-node-type="Button"]')?.getAttribute('data-node-id'),a=runtime.nodes.get(id)?.action;if(!pinned&&a?.op==='pickup')details(a.value);});
document.querySelector('#scene').addEventListener('focusout',e=>{const id=e.target.closest?.('[data-node-type="Button"]')?.getAttribute('data-node-id');if(!pinned&&runtime.nodes.get(id)?.action?.op==='pickup'){detail='';runtime.set('details-window',{visibility:'collapsed'});}});
window.addEventListener('keydown',e=>{if(/INPUT|SELECT|TEXTAREA/.test(e.target.tagName))return;if(e.code==='ControlLeft'||e.code==='ControlRight'){if(detail){pinned=true;details(detail);}return;}if(e.code==='Escape'){pinned=false;detail='';runtime.set('details-window',{visibility:'collapsed'});}if(e.code==='KeyZ'&&!e.repeat){session.showLoot=!session.showLoot;draw();}if(['KeyW','KeyA','KeyS','KeyD'].includes(e.code)){e.preventDefault();keys.add(e.code);}});
window.addEventListener('keyup',e=>keys.delete(e.code));window.addEventListener('blur',()=>keys.clear());
document.querySelector('#normal').onclick=()=>{session.kill(false);draw();};document.querySelector('#boss').onclick=()=>{session.kill(true);draw();};document.querySelector('#reset').onclick=()=>{session.reset();detail='';pinned=false;draw();};
document.querySelector('#crowded').onclick=()=>{session.reset();for(let i=0;i<120;i++){const d=structuredClone(samples[i%samples.length]);d.key+='-stress-'+i;d.x=(i%8)*24-70;d.y=Math.floor(i/8)*18-100;session.rewards.ground.push(d);}draw();};document.querySelector('#range').onchange=draw;
function frame(now){const dt=previous?Math.min(.05,(now-previous)/1000):0;previous=now;if(!paused&&!clean){const active=keys.size||session.target||session.animating;session.step(dt,{x:Number(keys.has('KeyD'))-Number(keys.has('KeyA')),y:Number(keys.has('KeyW'))-Number(keys.has('KeyS'))});refresh+=dt;if(active&&refresh>.05){refresh=0;draw();}}requestAnimationFrame(frame);}
async function verify(){
 paused=true;keys.clear();const checks=[];const check=(yes,name)=>checks.push({ok:!!yes,name});const s=new LootPrototypeSession(files,samples,{reviewAll:false});
 check(!validateDocument(project).length,'UICreator 工程、预制体和节点有效');check(s.hunter.enabled&&s.visible.length===0,'RuleStudio 默认 Hunter 开启，空条件隐藏掉落');
 s.hunter.enabled=false;check(s.visible.length===s.remaining.length,'关闭过滤显示全部');
 const d=s.remaining.find(d=>d.kind==='equipment'),before=JSON.stringify(s.state);s.hero={x:4000,y:4000,dead:false};check(!!s.pickup(d.key).error&&JSON.stringify(s.state)===before,'远距离拒绝且背包不变');
 s.hero={x:d.x,y:d.y,dead:false};check(!!s.pickup(d.key).drop&&s.state.instances[d.id],'200 cm 内一次入包');const after=JSON.stringify(s.state);check(!!s.pickup(d.key).error&&JSON.stringify(s.state)===after,'重复领取拒绝');
 const next=s.remaining[0];s.requestPickup(next.key);s.step(.016,{x:1,y:0});check(!s.target,'WASD 取消靠近');
 const ground=JSON.stringify(s.rewards.ground);s.hunter.enabled=true;s.hunter.targets=['missing'];check(JSON.stringify(s.rewards.ground)===ground,'Hunter 仅过滤显示，不改变已生成掉落');
 const own={...structuredClone(next),key:'own-fixture',origin:'inventory',x:0,y:0,claimed:false};check(visibleDrops([own],s.hero,s.hunter).length===1,'背包丢出物品绕过 Hunter');
 const stress=Array.from({length:150},(_,i)=>({...samples[i%samples.length],key:'s'+i,x:i,y:0})),visible=visibleDrops(stress,{x:0,y:0},{enabled:false});check(visible.length===96&&visible[0].key==='s0'&&visible[95].key==='s95','最近 96 个标牌预算');
 const boxes=packLoot(visible,()=>({x:900,y:420}),textWidth);check(boxes.every((a,i)=>boxes.slice(0,i).every(b=>!(a.x<b.x+b.w+3&&a.x+a.w+3>b.x&&a.y<b.y+b.h+3&&a.y+a.h+3>b.y))),'拥挤标牌不重叠');
 check(samples.filter(d=>d.category==='currency'&&[1,2,3].includes(d.currencyGrade)).every(d=>lootStyle(d).mark===['','Ⅰ','Ⅱ','Ⅲ'][d.currencyGrade]),'通货等级和颜色直接复用 RuleStudio');
 check(!errors.length,'浏览器无异常');document.querySelector('#checks').textContent=checks.map(c=>(c.ok?'✓ ':'✗ ')+c.name).join(' · ');document.querySelector('#checks').dataset.result=checks.every(c=>c.ok)?'pass':'fail';paused=false;return checks;
}
document.querySelector('#verify').onclick=verify;draw();requestAnimationFrame(frame);
if(new URLSearchParams(location.search).get('verify')==='1')await verify();
