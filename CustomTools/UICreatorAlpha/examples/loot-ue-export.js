// Authoring/export only. The UE runtime uses the frozen contract and AS.
import { UIRuntime } from '../src/runtime/runtime.js';
import { createNode, validateDocument, walk } from '../src/core/model.js';
const record=await fetch('/api/projects/opus-rulestudio-loot-20261025').then(r=>r.json());
const p=structuredClone(record.document), original=p.root;
p.id='opus-rulestudio-loot-ue';p.name='RuleStudio 掉落 · UE 控件';p.viewport={width:1600,height:900,background:'transparent'};
p.root=createNode('CanvasPanel',{id:'loot-ue-palette',layout:{width:1600,height:900},slot:{offsets:{left:0,top:0,right:1600,bottom:900}}});
const put=(root,x,y)=>{root=structuredClone(root);root.slot.offsets.left=x;root.slot.offsets.top=y;p.root.children.push(root);return root;};
const families=['common','magic','rare','legendary','unique','currency-1','currency-2','currency-3','currency-special','omen'];
families.forEach((family,i)=>put(p.prefabs['loot-prefab-'+family].root,20,30+i*42));
const hunter=put(p.prefabs['loot-prefab-hunter'].root,380,30);
walk(hunter,n=>{if(n.id==='hunter-search')n.binding='field:hunterQuery';if(n.id==='hunter-search-text'){n.text='';n.style.textAlign='left';}});
put(p.prefabs['loot-prefab-details'].root,760,30);
// Materialize the approved prototype's runtime row as editable UICreator
// controls; layout/style below are identical to loot-prototype.js rows().
for(const selected of [false,true]){
 const id=selected?'hunter-row-selected':'hunter-row';
 const root=createNode('CanvasPanel',{id:id+'-template',layout:{width:308,height:28},slot:{offsets:{right:308,bottom:28}},children:[createNode('Button',{id,binding:'hunter:row',slot:{offsets:{left:0,top:0,right:308,bottom:28},horizontalAlignment:'fill'},layout:{width:308,height:28},style:{fill:selected?'#253643':'#111b28',stroke:'#35475b',strokeWidth:1,radius:3},action:{type:'loot',op:'hunter-target',value:'$id'},children:[createNode('TextBlock',{id:id+'-text',binding:'hunter:row-name',text:'',style:{fontSize:11,textPadding:0,textPaddingX:7,color:'#c8d5e5',wrap:false,textOverflow:'ellipsis'}})]})]});
 p.prefabs['loot-prefab-'+id]={id:'loot-prefab-'+id,name:'Hunter 筛选行'+(selected?' · 已选':''),width:308,height:28,root:structuredClone(root)};
 put(root,1120,30+(selected?42:0));
}
p.prefabs={};p.interfaceCatalog=[];
p.provenance={...p.provenance,prototype:false,approvedProject:record.document.id,approvedRevision:record.revision,sourceRoot:original.id,deployment:'Standalone prefab palette; original node IDs, hierarchy, geometry and brush states retained. Browser review ground/toolbar excluded.'};
const errors=validateDocument(p);if(errors.length)throw new Error(errors.join('\n'));
const runtime=new UIRuntime(document.querySelector('#palette'),p,{width:1600,height:900,motion:false,sound:false});
document.querySelector('#status').textContent=`已保存原型 revision ${record.revision} · 10 种标牌 + Hunter + 详情 + 两态筛选行 · 诊断 ${errors.length}`;
document.querySelector('#freeze').disabled=false;
document.querySelector('#freeze').onclick=async()=>{
 await document.fonts.ready;const job='wbp-'+crypto.randomUUID();
 const response=await fetch(`/api/native-wbp/${job}/source`,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({project:p,svg:runtime.renderer.exportSvg(),context:{width:1600,height:900,name:p.name,entries:[...runtime.layout.entries].map(([id,e])=>[id,{...e,node:structuredClone(e.node)}]),expandedRoot:runtime.expanded.document.root}})});
 const result=await response.json();if(!response.ok)throw new Error(result.error);
 const link=document.querySelector('#export');link.href=result.url;link.hidden=false;document.querySelector('#freeze').disabled=true;document.querySelector('#status').textContent=JSON.stringify(result);
};
