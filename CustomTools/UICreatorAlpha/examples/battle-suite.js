import { UIRuntime } from '../src/runtime/runtime.js';
import { attachPresentation } from '../src/runtime/battle-hud.js';
import { validateDocument } from '../src/core/model.js';
import { materializeSounds } from '../src/assets/ui-sounds.js';
const runtimeErrors=[];window.addEventListener('error',e=>runtimeErrors.push(e.message));window.addEventListener('unhandledrejection',e=>runtimeErrors.push(String(e.reason)));
const record=await(await fetch('/api/projects/hud-darksteel-20261024')).json(),project=materializeSounds(record.document);
const shouldVerify=new URLSearchParams(location.search).get('verify')==='1';
const svg=document.querySelector('#scene'),select=document.querySelector('#screen'),status=document.querySelector('#status');
for(const page of project.interfaceCatalog){const option=document.createElement('option');option.value=page.id;option.textContent=page.title;select.append(option);}
let runtime,controller,session,active;
function show(id,preserve=true){
 if(controller&&preserve)session={hud:controller.state,suite:controller.suite.state};controller?.destroy();runtime?.destroy();active=id;
 const page=project.interfaceCatalog.find(p=>p.id===id),part=project.prefabs[page.prefabId],standalone=!['normal','boss','skills'].includes(id);
 if(session){session.hud.close();session.suite.overlays=[];}
 const view=standalone?{...project,root:part.root,viewport:{width:part.width,height:part.height,background:'#0c131b'},presentation:{...project.presentation,interfaceId:id}}:project;
 runtime=new UIRuntime(svg,view);runtime.addEventListener('navigate',e=>queueMicrotask(()=>show(e.detail.id)));
 controller=attachPresentation(runtime,{session,keyboard:true,onStatus:v=>status.textContent=v});
 if(id==='boss')controller.state.phase='boss';if(id==='skills')controller.state.open('skills');controller.refresh();select.value=id;
 document.querySelector('#editor').href=`/?project=${project.id}&interface=${id}&immersive=1`;history.replaceState(null,'',`?screen=${id}`);
}
select.onchange=()=>show(select.value);show(new URLSearchParams(location.search).get('screen')||'character');
const check=(ok,label)=>{results.push(`${ok?'PASS':'FAIL'} ${label}`);if(!ok)failures++;};let results=[],failures=0;
function glyphOverflows(view=runtime){const errors=[];for(const [id,record]of view.renderer.groups){const entry=view.layout.entries.get(id);if(!entry?.visible)continue;for(const element of record.visual.querySelectorAll('text')){const box=element.getBBox();if(box.x<-.75||box.y<-.75||box.x+box.width>entry.width+.75||box.y+box.height>entry.height+.75)errors.push(`${entry.node.name}: ${JSON.stringify({x:box.x,y:box.y,w:box.width,h:box.height,bw:entry.width,bh:entry.height})}`);}}return errors;}
async function verify(){
 results=[];failures=0;runtimeErrors.length=0;const original=JSON.stringify(project);check(!validateDocument(project).length,'工程结构与界面目录引用有效');
 for(const asset of Object.values(project.assets))check(!/<(?:image|foreignObject|text)\b/i.test(asset.source),`矢量资产 ${asset.id}`);
 for(const page of project.interfaceCatalog){
  const start=performance.now();show(page.id,false);const labels=[...runtime.renderer.groups.values()].filter(r=>r.group.getAttribute('display')!=='none');check(labels.length>8,`界面 ${page.id} 可渲染`);const overflow=glyphOverflows();check(!overflow.length,`界面 ${page.id} 字形边界${overflow.length?' '+overflow.slice(0,3).join('; '):''}`);results.push(`METRIC ${page.id}: ${runtime.layout.entries.size} nodes / ${Math.round(performance.now()-start)} ms`);
 }
 show('inventory',false);const bag=[...runtime.layout.entries.values()],grid=bag.find(e=>e.node.binding==='bag-grid');
 check(runtime.layout.width===650&&runtime.layout.height===448,'背包保持参考图 650×448 原尺寸');
 check(grid.globalX===12&&grid.globalY===98&&grid.width===624&&grid.height===312&&bag.filter(e=>e.node.binding==='item-cell').length===72,'背包格子位置、区域及 12×6 槽位保持一致');
 const quantity=bag.find(e=>e.node.binding==='bag-quantity');check(runtime.renderer.groups.get(quantity.node.id).visual.textContent==='×1','底栏数量完整绘制，不被省略号替换');
 const inspector=controller.suite.inspector,selectedBefore=controller.suite.state.selected;
 const itemHit=id=>{const n=[...runtime.nodes.values()].find(n=>n.binding==='item-cell'&&n.action?.value===id);return runtime.renderer.groups.get(n.id).hit;};
 const enterItem=id=>itemHit(id).dispatchEvent(new PointerEvent('pointerover',{bubbles:true}));
 const leaveItem=id=>itemHit(id).dispatchEvent(new PointerEvent('pointerout',{bubbles:true,relatedTarget:document.body}));
 enterItem('gear-3');check(inspector.itemId==='gear-3'&&inspector.host.getAttribute('role')==='tooltip','背包装备真实悬停事件打开 SVG 详情');
 check(controller.suite.state.selected===selectedBefore,'悬停不改变背包选中物品');
 check(inspector.host.textContent.includes('余烬回响')&&inspector.host.textContent.includes('范围技能重复次数 +1'),'装备悬停详情显示当前物品和词条');
 const hoverGlyphs=glyphOverflows(inspector.runtime);check(!hoverGlyphs.length,`悬停面板真实字形边界${hoverGlyphs.length?' '+hoverGlyphs.slice(0,3).join('; '):''}`);
 check(Object.keys(inspector.runtime.project.assets).length<12&&inspector.host.style.pointerEvents==='none','非固定悬停面板不挡鼠标且只加载自身引用的 SVG');
 leaveItem('gear-3');enterItem('omen-ice');check(inspector.host.textContent.includes('异常预兆')&&inspector.host.textContent.includes('数量 2')&&inspector.host.textContent.includes('异常词缀族'),'通货和预兆悬停显示实际说明及数量');
 const popupBounds=inspector.host.getBoundingClientRect();check(popupBounds.left>=11.9&&popupBounds.top>=11.9&&popupBounds.right<=innerWidth-11.9&&popupBounds.bottom<=innerHeight-11.9,'悬停面板定位在浏览器可见区域内');
 leaveItem('omen-ice');check(!document.querySelector('[data-ui-item-tooltip]'),'移出物品即关闭非固定悬停面板');
 const empty=[...runtime.nodes.values()].find(n=>n.binding==='item-cell'&&!n.action);runtime.renderer.groups.get(empty.id).hit.dispatchEvent(new PointerEvent('pointerover',{bubbles:true}));check(!inspector.host,'空格不会打开详情');
 itemHit('omen-ice').dispatchEvent(new FocusEvent('focusin',{bubbles:true}));window.dispatchEvent(new KeyboardEvent('keydown',{code:'ControlLeft',ctrlKey:true,bubbles:true}));leaveItem('omen-ice');enterItem('gear-0');
 check(inspector.pinned&&inspector.itemId==='omen-ice'&&inspector.host.getAttribute('role')==='dialog','键盘焦点与 Ctrl 固定可用，移开及悬停其他物品不会替换固定详情');
 window.dispatchEvent(new KeyboardEvent('keydown',{code:'Escape',bubbles:true}));check(!inspector.host&&active==='inventory','Esc 只关闭物品详情，保留背包窗口');
 enterItem('gear-3');inspector.pin();const equippedBefore=controller.suite.state.items.find(i=>i.id==='gear-3').equipped;
 const equipHit=inspector.runtime.renderer.groups.get('battle-item-hover-equip').hit;equipHit.dispatchEvent(new PointerEvent('pointerdown',{bubbles:true,button:0}));equipHit.dispatchEvent(new PointerEvent('pointerup',{bubbles:true,button:0}));
 check(controller.suite.state.items.find(i=>i.id==='gear-3').equipped!==equippedBefore&&inspector.pinned,'固定面板装备按钮操作对应物品并同步保留详情');
 const pinGlyphs=glyphOverflows(inspector.runtime);check(!pinGlyphs.length,`固定面板操作按钮的字形边界${pinGlyphs.length?' '+pinGlyphs.slice(0,3).join('; '):''}`);
 const detailHit=inspector.runtime.renderer.groups.get('battle-item-hover-details').hit;detailHit.dispatchEvent(new PointerEvent('pointerdown',{bubbles:true,button:0}));detailHit.dispatchEvent(new PointerEvent('pointerup',{bubbles:true,button:0}));check(!inspector.host&&controller.suite.state.overlays.at(-1)==='item'&&controller.suite.state.selected==='gear-3','固定详情可打开对应物品的完整详情界面');controller.suite.state.overlays=[];controller.refresh();
 const unknown=controller.suite.state.items.find(i=>i.id==='gear-7');unknown.unidentified=true;controller.refresh();enterItem('gear-7');check(inspector.host.textContent.includes('未鉴定装备')&&!inspector.host.textContent.includes('范围技能重复次数'),'未鉴定物品的悬停详情不泄露属性和词条');inspector.close();unknown.unidentified=false;controller.refresh();
 inspector.close();enterItem('gear-0');itemHit('gear-0').dispatchEvent(new PointerEvent('pointerdown',{bubbles:true,button:0,clientX:10,clientY:10}));itemHit('gear-0').dispatchEvent(new PointerEvent('pointermove',{bubbles:true,buttons:1,clientX:30,clientY:10}));window.dispatchEvent(new PointerEvent('pointerup',{button:0}));check(!inspector.host,'拖动物品时关闭悬停面板');
 enterItem('gear-0');itemHit('gear-0').dispatchEvent(new WheelEvent('wheel',{bubbles:true,deltaY:40}));check(!inspector.host,'滚动背包时关闭非固定详情，避免显示错位物品');
 enterItem('gear-0');inspector.pin();show('character',false);check(!document.querySelector('[data-ui-item-tooltip]'),'切换界面销毁详情面板与独立运行时');
 show('character',false);let s=controller.suite.state;s.setField('characterQuery','霜棘');controller.refresh();check([...runtime.nodes.values()].filter(n=>n.binding==='item-cell').length===1,'角色搜索实际过滤物品');s.setField('characterQuery','');s.selected='gear-0';s.equip();controller.refresh();check(!s.items.find(i=>i.id==='gear-0').equipped,'卸下只改变对应装备');check([...runtime.nodes.values()].filter(n=>n.binding==='gear-count').every(n=>n.text==='3 / 9'),'装备数量同步');
 s.selected='stone-low';const before=s.item.count;s.setField('splitAmount','2');s.overlays=['split'];controller.refresh();check(s.split(),'有效拆分成功');controller.refresh();check(s.item.count===before-2&&s.items.at(-1).count===2,'拆分数量守恒');s.setField('splitAmount','0');check(!s.split(),'无效拆分不改变物品');
 s.overlays=['item'];controller.refresh();s.selected='gear-3';const count=s.items.find(i=>i.id==='stone-low').count;s.act('item-affix',0);s.act('item-affix',0);controller.refresh();check(s.targets.has(0)&&s.rolls===1&&s.items.find(i=>i.id==='stone-low').count===count-2,'选定与洗炼消耗通货并更新词条');
 check([...runtime.nodes.values()].filter(n=>n.binding==='material-count:stone-low').every(n=>n.text.includes(String(count-2))),'通货数量在当前界面同步');
 show('hunter',false);s=controller.suite.state;s.setField('hunterQuery','火焰');controller.refresh();check([...runtime.nodes.values()].filter(n=>n.action?.op==='hunter-affix').every(n=>n.action.value.includes('火焰')),'Hunter 搜索只显示匹配条件');s.act('hunter-affix','火焰伤害 +n%');s.act('hunter-only');controller.refresh();check(s.hunterRows.length===1,'仅看已选过滤有效');
 show('shop',false);s=controller.suite.state;const wallet=s.wallet,owned=s.items.length;s.act('buy','gear-0');controller.refresh();s.act('buy','gear-0');check(s.wallet===wallet-24&&s.items.length===owned+1,'购买限购一次并同步钱包与背包');
 show('belt',false);const choice=[...runtime.nodes.values()].find(n=>n.binding==='suite-belt-0'),hit=runtime.renderer.groups.get(choice.id).hit;hit.dispatchEvent(new PointerEvent('pointerdown',{bubbles:true,button:0}));hit.dispatchEvent(new PointerEvent('pointerup',{bubbles:true,button:0}));window.dispatchEvent(new KeyboardEvent('keydown',{bubbles:true,code:'KeyQ'}));check(controller.state.belt[0]==='health-potion'&&controller.state.beltCount===11,'点击配置第一种消耗品后 Q 正确消耗一次');
 show('atlas',false);s=controller.suite.state;
 check(runtime.layout.width===1040&&runtime.layout.height===660,'星图使用 1040×660 的窗口布局');
 const clickRole=binding=>{const n=[...runtime.nodes.values()].find(n=>n.binding===binding),hit=runtime.renderer.groups.get(n.id).hit;hit.dispatchEvent(new PointerEvent('pointerdown',{bubbles:true,button:0}));hit.dispatchEvent(new PointerEvent('pointerup',{bubbles:true,button:0}));};
 clickRole('region:4');check(s.region===4&&[...runtime.nodes.values()].find(n=>n.binding==='region-enter').interactable===false,'未连通区域可查看详情但不可进入');
 const selected=[...runtime.nodes.values()].find(n=>n.binding==='region:4');check(selected.brushes.normal===selected.brushes.hovered&&selected.brushes.normal.includes('locked-selected'),'地域选中状态独立于悬停，并保留未连通标记');
 clickRole('region:2');check([...runtime.nodes.values()].find(n=>n.binding==='region-enter').interactable&&[...runtime.nodes.values()].find(n=>n.binding==='region-neighbors').text==='3 处','可探索区域同步进入权限与真实相邻地域数量');
 s.act('atlas-zoom',1);controller.refresh();check([...runtime.nodes.values()].some(n=>n.binding==='atlas-camera'&&n.layout.scale===1.25),'星图缩放改变画面变换');
 for(let i=0;i<10;i++)s.act('atlas-zoom',1);controller.refresh();check(s.zoom===200&&![...runtime.nodes.values()].find(n=>n.binding==='atlas-zoom-in').interactable,'星图达到缩放上限后禁用放大');
 for(let i=0;i<10;i++)s.act('atlas-zoom',-1);controller.refresh();check(s.zoom===50&&![...runtime.nodes.values()].find(n=>n.binding==='atlas-zoom-out').interactable,'星图达到缩放下限后禁用缩小');
 show('normal',false);controller.state.open('atlas');controller.refresh();
 const mapWindow=[...runtime.layout.entries.values()].find(e=>e.node.binding==='window-atlas');check(mapWindow.globalWidth===1040&&mapWindow.globalHeight===660&&mapWindow.globalX>16&&mapWindow.globalY>16,'HUD 中地图居中显示，保留周围空间');
 runtime.render(800,600);controller.refresh();const smallMap=[...runtime.layout.entries.values()].find(e=>e.node.binding==='window-atlas');check(smallMap.globalWidth<=768&&smallMap.globalHeight<=568&&smallMap.globalX>=15.9&&smallMap.globalY>=15.9,'小视口下地图缩小并保留窗口边距');
 s=controller.suite.state;const mapEntry=[...runtime.layout.entries.values()].find(e=>e.node.binding==='atlas-viewport'),mapHit=runtime.renderer.groups.get(mapEntry.node.id).hit,box=mapHit.getBoundingClientRect(),dx=36,dy=20;
 mapHit.dispatchEvent(new PointerEvent('pointerdown',{bubbles:true,button:0,clientX:box.left+20,clientY:box.top+20}));window.dispatchEvent(new PointerEvent('pointermove',{clientX:box.left+20+dx,clientY:box.top+20+dy}));window.dispatchEvent(new PointerEvent('pointerup',{button:0}));
 check(Math.abs(s.panX-dx*mapEntry.width/box.width)<.01&&Math.abs(s.panY-dy*mapEntry.width/box.width)<.01,'缩小的地图窗口拖动距离正确换算到地图坐标');
 const beltBefore=JSON.stringify(controller.state.belt);window.dispatchEvent(new KeyboardEvent('keydown',{code:'KeyF',bubbles:true}));check(s.zoom===100&&s.panX===0&&s.panY===0&&JSON.stringify(controller.state.belt)===beltBefore,'F 复位地图且不触发消耗品快捷键');
 show('normal',false);controller.state.open('character');controller.refresh();check([...runtime.nodes.values()].some(n=>n.binding==='window-character'),'HUD 可打开完整角色窗口');check(![...runtime.nodes.values()].some(n=>n.id.endsWith('battle-shop')),'未打开界面不展开到 HUD');
 const attr=[...runtime.nodes.values()].find(n=>n.id.endsWith('battle-character-attributes'));const id=attr.id;
 runtime.renderer.groups.get(id).hit.dispatchEvent(new WheelEvent('wheel',{bubbles:true,cancelable:true,deltaY:250}));check(runtime.nodes.get(id).layout.scrollOffset===250,'鼠标滚轮独立滚动属性栏目');
 const trait=[...runtime.nodes.values()].find(n=>n.binding==='character-traits-list');check(runtime.nodes.get(trait.id).layout.scrollOffset===0,'属性滚动不带动特性栏目');
 check(JSON.stringify(project)===original,'预览交互不修改工程 / 共享资产');
 check(!runtimeErrors.length,`交互过程中没有未捕获异常${runtimeErrors.length?' '+runtimeErrors.slice(0,3).join('; '):''}`);
 const output=document.querySelector('#checks');output.hidden=false;output.textContent=`${results.filter(v=>v.startsWith('PASS')).length} PASS / ${failures} FAIL\n${results.join('\n')}`;output.dataset.complete='true';output.dataset.failures=String(failures);show('character',false);
}
document.querySelector('#verify').onclick=verify;
if(shouldVerify)await verify();
