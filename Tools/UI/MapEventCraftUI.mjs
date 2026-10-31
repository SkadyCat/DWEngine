import {walk,createNode} from '../../CustomTools/UICreatorAlpha/src/core/model.js';

// Bring the same UICreator composition into the saved native event document.
export function installMapEventCraftUI(doc,studio){
 Object.assign(doc.assets,structuredClone(studio.assets));Object.assign(doc.motions,structuredClone(studio.motions));
 doc.viewport.width=3000;doc.viewport.height=1000;
 doc.root.children=doc.root.children.filter(n=>!['event-craft-panel','event-craft-drag','event-craft-scrim'].includes(n.id));
 const root=structuredClone(studio.root);root.id='event-craft-panel';root.binding='event-craft-panel';root.name='祭坛 · 左背包 / 右操作';
 root.slot.offsets={left:1500,top:24,right:1280,bottom:660};root.layout.width=1280;root.layout.height=660;
 walk(root,n=>{
  if(n!==root)n.id='craft-'+n.id;
  if(n.binding==='event-samples')n.visibility='collapsed';
  if(n.source?.reusedPrefab==='battle-prefab-item-cell')n.interactable=true;
  if(n.id.endsWith('battle-cell-icon')){n.type='Svg';n.visibility='visible';n.binding='craft:icon';}
  if(n.id.endsWith('battle-cell-tier')||n.id.endsWith('battle-cell-mark')||n.id.endsWith('battle-cell-count'))n.visibility='visible';
  if(n.binding==='event-bag')n.source.nativeRows=true;
  if(n.binding==='input-0'||n.binding==='input-1'){
   const index=n.binding.endsWith('1')?1:0;n.action={type:'map-event',op:'craft-slot',value:String(index)};
   const content=n.children[0];
   content.children.push({ ...structuredClone(content.children[0]),id:'craft-input-icon-'+index,type:'Image',binding:'craft:input-icon-'+index,text:'',children:[],slot:{...structuredClone(content.slot),offsets:{left:3,top:3,right:70,bottom:70}},layout:{...structuredClone(content.layout),width:70,height:70},style:{...structuredClone(content.style),fill:'transparent',clip:true,radius:6},locked:true });
  }
  if(n.action?.op==='close')n.action={type:'map-event',op:'close'};
  if(n.binding==='event-primary')n.action={type:'map-event',op:'primary'};
  if(n.binding==='event-secondary')n.action={type:'map-event',op:'collect'};
  if(n.binding==='bag-prev'||n.binding==='bag-next')n.action={type:'map-event',op:'craft-page',value:n.binding==='bag-prev'?'-1':'1'};
  if(n.binding==='event-affix-target')n.action={type:'map-event',op:'craft-affix'};
 });
 doc.root.children.push(root);
 const sample=structuredClone(studio.root.children.find(n=>n.id==='event-backpack').children[0].children.find(n=>n.id==='event-bag').children[0]);
 walk(sample,n=>{n.id='craft-drag-'+n.id;if(n.id.endsWith('battle-cell-icon')){n.type='Image';n.binding='craft:icon';n.visibility='visible';}if(n.type==='TextBlock')n.text='';});
 sample.id='event-craft-drag';sample.binding='event-craft-drag';sample.slot.offsets={left:2820,top:24,right:60,bottom:60};sample.motion={preset:'none',assetId:''};
 doc.root.children.push(sample);
 doc.root.children.push(createNode('Border',{id:'event-craft-scrim',name:'祭坛 · 场景遮罩',locked:true,slot:{offsets:{left:2900,top:24,right:16,bottom:9}},layout:{width:16,height:9},style:{fill:'#03070bc9'}}));return doc;
}
