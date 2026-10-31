import { assetGraphic, scopedSvgContent, svgElement as el } from '/src/assets/svg.js';
const report=document.querySelector('#report'),images=document.querySelector('#images');
function fixture(mode,alpha=1,width=93,height=23,offset=.0,scale=1){
 const asset={id:'probe',name:'probe',viewBox:[0,0,96,96],scaleMode:'nine-slice',slice:{left:12,right:12,top:12,bottom:12},source:`<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 96 96"><rect x=".5" y=".5" width="95" height="95" rx="5" fill="#342f25" fill-opacity="${alpha}" stroke="#4d4938"/></svg>`};
 const svg=el('svg',{xmlns:'http://www.w3.org/2000/svg',width:Math.ceil((width+2)*scale+offset),height:Math.ceil((height+2)*scale+offset)}),defs=el('defs');defs.append(scopedSvgContent(asset).group);svg.append(defs);const wrapper=el('g',{transform:`translate(${offset} ${offset}) scale(${scale})`});svg.append(wrapper);const graphic=assetGraphic(asset,width,height,'',{x:offset,y:offset,scale});wrapper.append(graphic);
 if(mode==='legacy'){
  const k=Math.min(1,width/24,height/24),dx=[0,12*k,width-12*k],dy=[0,12*k,height-12*k],dw=[12*k,width-24*k,12*k],dh=[12*k,height-24*k,12*k];
  for(const part of graphic.children){const [x,y]=part.getAttribute('viewBox').split(' ').map(Number),c=x===0?0:x===12?1:2,r=y===0?0:y===12?1:2;for(const [key,value] of Object.entries({x:dx[c],y:dy[r],width:dw[c],height:dh[r]}))part.setAttribute(key,value);}
 }
 return svg;
}
async function raster(svg){
 const source=new XMLSerializer().serializeToString(svg),url=URL.createObjectURL(new Blob([source],{type:'image/svg+xml'})),image=new Image();
 try{await new Promise((resolve,reject)=>{image.onload=resolve;image.onerror=reject;image.src=url;});const canvas=document.createElement('canvas');canvas.width=Number(svg.getAttribute('width'));canvas.height=Number(svg.getAttribute('height'));canvas.getContext('2d').drawImage(image,0,0);return canvas;}finally{URL.revokeObjectURL(url);}
}
async function run(){
 images.replaceChildren();const results=[];
 const old=await raster(fixture('legacy')),oldAlpha=old.getContext('2d').getImageData(11,5,1,1).data[3];
 const review=document.createElement('canvas');review.width=800;review.height=390;const ctx=review.getContext('2d');ctx.fillStyle='#101720';ctx.fillRect(0,0,800,390);ctx.font='16px sans-serif';ctx.fillStyle='#dce4ee';ctx.imageSmoothingEnabled=false;
 for(const [index,[mode,alpha,title]] of [['legacy',1,'Before: 93 x 23'],['current',1,'Fixed: opaque'],['current',.5,'Fixed: 50% opacity']].entries()){
  const image=await raster(fixture(mode,alpha));ctx.fillStyle='#dce4ee';ctx.fillText(title,20,30+index*125);ctx.drawImage(image,250,8+index*125,380,100);}
 images.append(review);
 for(const [width,height] of [[93,23],[180,30],[45,11],[93,23.5]])for(const alpha of [1,.5])for(const [offset,scale] of [[0,1],[.25,1],[.5,.72],[.25,1.25],[.25,2]]){
  const canvas=await raster(fixture('current',alpha,width,height,offset,scale)),context=canvas.getContext('2d');let min=255,max=0,bad=0;
  for(let y=Math.ceil(offset+3*scale);y<Math.floor(offset+(height-3)*scale);y++)for(let x=Math.ceil(offset+3*scale);x<Math.floor(offset+(width-3)*scale);x++){
   const a=context.getImageData(x,y,1,1).data[3];min=Math.min(min,a);max=Math.max(max,a);if(Math.abs(a-Math.round(alpha*255))>1)bad++;}
  results.push({width,height,alpha,offset,scale,minAlpha:min,maxAlpha:max,badPixels:bad,pass:bad===0});
 }
 const passed=results.filter(r=>r.pass).length;
 report.textContent=JSON.stringify({passed,total:results.length,legacySeamAlpha:oldAlpha,failed:results.filter(r=>!r.pass)},null,2);
 if(passed===results.length&&oldAlpha<255){
  const hash=[...new Uint8Array(await crypto.subtle.digest('SHA-256',new TextEncoder().encode(JSON.stringify(results))))].map(n=>n.toString(16).padStart(2,'0')).join('');
  const job='wbp-nine-slice-'+crypto.randomUUID(),bundle={schema:'uicreator-alpha/native-wbp',version:1,sourceHash:hash,project:{assets:{}},nodes:[],states:{canvas:{nodes:[]}},rasters:{},references:{comparison:{png:review.toDataURL('image/png').split(',')[1]}},pixelRegression:{passed,total:results.length,legacySeamAlpha:oldAlpha,results}};
  const response=await fetch('/api/native-wbp/'+job,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(bundle)}),saved=await response.json();
  report.textContent+='\n'+JSON.stringify({job,saved:response.ok,bundle:saved.bundle},null,2);
 }
}
document.querySelector('#run').onclick=run;await run();
