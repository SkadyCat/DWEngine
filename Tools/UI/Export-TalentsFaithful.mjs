// Faithful import of measured, unchanged RuleStudio HTML. No game logic in this file.
import fs from 'node:fs/promises';
import path from 'node:path';
import crypto from 'node:crypto';
import {fileURLToPath} from 'node:url';
import {createDocument,createNode,validateDocument,walk} from '../../CustomTools/UICreatorAlpha/src/core/model.js';
import {ProjectRepository} from '../../CustomTools/UICreatorAlpha/server/project-repository.mjs';
import {LEGENDARY_ICONS} from '../../CustomTools/RuleStudio/public/talents/legendary-icons.mjs';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'../..'),tool=path.join(root,'CustomTools/UICreatorAlpha');
const tree=JSON.parse(await fs.readFile(path.join(root,'Content/Rules/Talents/constellation.json'),'utf8'));
const job=process.argv[2]||`wbp-opus-talents-faithful-${Date.now()}`;
if(!/^wbp-[a-zA-Z0-9_-]{1,90}$/.test(job))throw Error('Invalid job');
const frozen=path.join(tool,'exports',job);if(await fs.stat(path.join(frozen,'project-record.json')).catch(()=>null))throw Error('Frozen snapshot already exists');await fs.mkdir(path.join(frozen,'source-assets'),{recursive:true});
const refs={};for(const name of ['default','allocated','attribute','legend-tip','legend-dialog','search','attribute-tip','notice'])refs[name]=JSON.parse(await fs.readFile(path.join(root,'UIAuthoring/TalentsReference',name+'.json'),'utf8'));
const doc=createDocument('Opus · 启明星盘 · 原版还原');doc.id='opus-talent-constellation';doc.viewport={width:1600,height:900,background:'transparent'};doc.root.id='talent-screen';doc.root.binding='talent-screen';doc.root.layout={...doc.root.layout,width:1600,height:900};
doc.presentation={independentCaches:true};
const all=[],byid=new Map(),semantic={},sourceAssets=[],sourceRefs=[];
const hex=v=>{const m=v.match(/^rgba?\(([^)]+)\)$/);if(!m)return v;const a=m[1].split(',').map(Number);return '#'+a.slice(0,3).map(v=>Math.round(v).toString(16).padStart(2,'0')).join('')+(a.length===4?Math.round(a[3]*255).toString(16).padStart(2,'0'):'');};
const num=v=>parseFloat(v)||0,esc=v=>String(v).replaceAll('&','&amp;').replaceAll('"','&quot;').replaceAll('<','&lt;'),fmt=v=>Number(v.toFixed(5));
const svg=(w,h,b)=>`<svg xmlns="http://www.w3.org/2000/svg" width="${w}" height="${h}" viewBox="0 0 ${w} ${h}">${b}</svg>`;
const asset=(id,w,h,source)=>{doc.assets[id]={id,name:id,source,viewBox:[0,0,w,h],scaleMode:'stretch',slice:{top:0,right:0,bottom:0,left:0},folderId:''};return id;};
const node=(type,id,x,y,w,h,props={})=>{const n=createNode(type,{id,name:id,binding:id,slot:{offsets:{left:fmt(x),top:fmt(y),right:fmt(w),bottom:fmt(h)}},layout:{width:fmt(w),height:fmt(h)},style:{fill:'transparent',strokeWidth:0,radius:0},motion:{preset:'none'},...props,style:{fill:'transparent',strokeWidth:0,radius:0,...props.style}});all.push(n);byid.set(id,n);return n;};
async function png(file,name){const bytes=await fs.readFile(file);await fs.writeFile(path.join(frozen,'source-assets',name),bytes);sourceAssets.push({name,sha256:crypto.createHash('sha256').update(bytes).digest('hex')});return `/exports/${job}/source-assets/${name}`;}
const hrefs=new Map();for(const a of tree.attributes)hrefs.set('/talents/icons/'+a.id+'.png',await png(path.join(root,'CustomTools/RuleStudio/public/talents/icons',a.id+'.png'),a.id+'.png'));
for(const info of Object.values(LEGENDARY_ICONS)){if(!hrefs.has(info.sheet))hrefs.set(info.sheet,await png(path.join(root,'CustomTools/RuleStudio/public',info.sheet),path.basename(info.sheet)));}
for(const a of tree.attributes)asset('attr-'+a.id,1,1,svg(1,1,`<image width="1" height="1" href="${hrefs.get('/talents/icons/'+a.id+'.png')}"/>`));
for(const [id,info]of Object.entries(LEGENDARY_ICONS))asset('legend-'+id,1,1,svg(1,1,`<svg viewBox="${info.column} ${info.row} 1 1" width="1" height="1"><image width="3" height="3" href="${hrefs.get(info.sheet)}"/></svg>`));
function splitCSS(s){let level=0,start=0,out=[];for(let i=0;i<s.length;i++){if(s[i]=='(')level++;if(s[i]==')')level--;if(s[i]==','&&level==0){out.push(s.slice(start,i).trim());start=i+1;}}out.push(s.slice(start).trim());return out;}
function background(r,id){const s=r.style,w=r.rect.width,h=r.rect.height,rad=num(s.borderTopLeftRadius),bw=num(s.borderTopWidth);let defs='',body='',fill=s.backgroundColor;
 if(s.backgroundImage!=='none'){
  const pieces=splitCSS(s.backgroundImage);for(let i=pieces.length-1;i>=0;i--){const p=pieces[i],linear=p.startsWith('linear'),inside=p.slice(p.indexOf('(')+1,-1),args=splitCSS(inside),g='g'+i;let first=args[0],stops=args.slice(1);if(first.startsWith('rgb')||first.startsWith('#')||first==='transparent'){stops=args;first='180deg';}
   const stopsSVG=stops.map((v,j)=>{const m=v.match(/^(.*?)(?:\s+([\d.]+)(%|px))?$/);return `<stop offset="${m[2]??j*100/(stops.length-1)}%" stop-color="${esc(m[1])}"/>`;}).join('');
   if(linear){const angle=num(first)*Math.PI/180,x=Math.sin(angle)*50,y=-Math.cos(angle)*50;defs+=`<linearGradient id="${g}" x1="${50-x}%" y1="${50-y}%" x2="${50+x}%" y2="${50+y}%">${stopsSVG}</linearGradient>`;}
   else{const at=first.match(/at ([\d.]+)% ([\d.]+)%/);const cx=Number(at?.[1]||50)/100,cy=Number(at?.[2]||50)/100;defs+=`<radialGradient id="${g}" gradientUnits="userSpaceOnUse" cx="0" cy="0" r="1" gradientTransform="translate(${cx*w} ${cy*h}) scale(${Math.max(cx,1-cx)*w*Math.SQRT2} ${Math.max(cy,1-cy)*h*Math.SQRT2})">${stopsSVG}</radialGradient>`;}
   if(i===pieces.length-1)body+=`<rect width="${w}" height="${h}" rx="${rad}" fill="${esc(fill)}"/>`;body+=`<rect width="${w}" height="${h}" rx="${rad}" fill="url(#${g})"/>`;
  }
 }else if(fill!=='rgba(0, 0, 0, 0)')body+=`<rect width="${w}" height="${h}" rx="${rad}" fill="${esc(fill)}"/>`;
 const sides=['Top','Right','Bottom','Left'],widths=sides.map(v=>num(s['border'+v+'Width']));
 if(widths.every(v=>v===bw)&&bw>0)body+=`<rect x="${bw/2}" y="${bw/2}" width="${w-bw}" height="${h-bw}" rx="${Math.max(0,rad-bw/2)}" fill="none" stroke="${esc(s.borderTopColor)}" stroke-width="${bw}"/>`;
 else for(let i=0;i<4;i++)if(widths[i]){const v=widths[i],coords=[`x="0" y="0" width="${w}" height="${v}"`,`x="${w-v}" y="0" width="${v}" height="${h}"`,`x="0" y="${h-v}" width="${w}" height="${v}"`,`x="0" y="0" width="${v}" height="${h}"`][i];body+=`<rect ${coords} fill="${esc(s['border'+sides[i]+'Color'])}"/>`;}
 return body?asset('bg-'+id,w,h,svg(w,h,`<defs>${defs}</defs>${body}`)):'';
}
const has=(r,c)=>r.classes.split(' ').includes(c);
function flat(r,out=[]){out.push(r);for(const c of r.children)flat(c,out);return out;}
const main=flat(refs.default.root),find=c=>main.find(r=>has(r,c));
const names={0:'talent-window',1:'talent-window-head',3:'talent-close',13:'talent-budget-input',15:'talent-spent',16:'talent-budget-total',18:'talent-live',20:'talent-atlas',24:'talent-search',26:'talent-cat-all',28:'talent-cat-attack',30:'talent-cat-defense',32:'talent-cat-special',34:'talent-sector',35:'talent-viewport',39:'talent-map-controls',40:'talent-zoom-out',41:'talent-zoom-value',42:'talent-zoom-in',43:'talent-overview',44:'talent-inspector',47:'talent-legend-count',48:'talent-inspector-scroll',53:'talent-plan-meter',55:'talent-plan-spent',56:'talent-plan-remaining',59:'talent-attr-count',64:'talent-total-strength',68:'talent-total-dexterity',72:'talent-total-intelligence',76:'talent-total-constitution',83:'talent-live-strength',86:'talent-live-dexterity',89:'talent-live-intelligence',92:'talent-live-constitution',93:'talent-selected-list',95:'talent-empty',96:'talent-preview',99:'talent-preview-skill',100:'talent-preview-result',103:'talent-clear',104:'talent-export',105:'talent-import',107:'talent-footer',109:'talent-remaining'};
const ops={'talent-close':'close','talent-clear':'clear','talent-export':'export','talent-import':'import','talent-overview':'overview','talent-zoom-in':'zoom-in','talent-zoom-out':'zoom-out'};
for(const c of ['all','attack','defense','special'])ops['talent-cat-'+c]='category';
function convert(r,parentRect,prefix='',mapping=names){const id=mapping[num(r.id.slice(4))]||prefix+r.id;const R=r.rect,S=r.style,x=R.x-parentRect.x,y=R.y-parentRect.y;let type=r.tag==='button'||r.tag==='select'||r.tag==='summary'?'Button':r.tag==='svg'?'Svg':'CanvasPanel';
 const n=node(type,id,x,y,R.width,R.height,{name:r.texts.map(t=>t.text).join(' ')||r.classes||id,style:{clip:S.overflow==='hidden'||S.overflowY==='auto',opacity:num(S.opacity)},source:{htmlId:r.id,tag:r.tag,classes:r.classes,computed:S,rect:R}});sourceRefs.push({id,htmlId:r.id,sourceRect:R});
 if(r.tag==='graph')return graph(r,n);
 if(r.tag==='svg'){let source=r.svg;for(const [a,b]of hrefs)source=source.replaceAll(a,b);n.svgAsset=asset('svg-'+id,R.width,R.height,svg(R.width,R.height,`<svg width="${R.width}" height="${R.height}" viewBox="${r.viewBox}">${source.replace(/^<svg[^>]*>/,'').replace(/<\/svg>$/,'')}</svg>`));return n;}
 n.svgAsset=background(r,id);
 if(type==='Button'){n.action={type:'talent',op:ops[id]||'',value:id.startsWith('talent-cat-')?id.slice(11):''};}
 let content=n;if(type==='Button'){content=node('CanvasPanel',id+'-content',0,0,R.width,R.height);n.children=[content];}
 let texts=r.texts;if(r.tag==='input'||r.tag==='select'){let value=r.tag==='select'?r.options.find(o=>o.value===r.value)?.text:r.value||r.placeholder;texts=[{text:value,rect:{...R,x:R.x+num(S.paddingLeft)+num(S.borderLeftWidth),y:R.y+(R.height-num(S.fontSize)*1.4)/2,width:R.width-num(S.paddingLeft)-num(S.paddingRight)-8,height:num(S.fontSize)*1.4}}];n.source.options=r.options;n.source.placeholder=r.placeholder;n.source.input=true;}
 for(let i=0;i<texts.length;i++){const t=texts[i],f=num(S.fontSize),tr=t.rect;const tw=Math.max(tr.width+1,2),th=Math.max(tr.height,f*1.4);const tid=id+'-text'+(i?'-'+i:'');content.children.push(node('TextBlock',tid,tr.x-R.x,tr.y-R.y-1,tw,th,{text:t.text,style:{fontSize:f,fontWeight:num(S.fontWeight),fontFamily:S.fontFamily,color:hex(S.color),letterSpacing:num(S.letterSpacing),lineHeight:num(S.lineHeight),textAlign:'left',verticalAlign:'top',wrap:tr.height>f*1.8,textOverflow:'visible'}}));}
 for(const child of r.children)content.children.push(convert(child,R,prefix,mapping));return n;
}
const graphRef=find('stc-map'),[vx,vy,vw,vh]=graphRef.viewBox.split(' ').map(Number),gs=graphRef.rect.width/vw,gx=-vx*gs,gy=-vy*gs;
function graph(r,n){n.type='CanvasPanel';n.style.clip=true;const g=node('CanvasPanel','talent-graph',0,0,r.rect.width,r.rect.height);n.children.push(g);
 g.children.push(node('Svg','talent-halo',0,0,r.rect.width,r.rect.height,{svgAsset:asset('halo',r.rect.width,r.rect.height,svg(r.rect.width,r.rect.height,`<defs><radialGradient id="halo"><stop stop-color="#889fc8" stop-opacity=".08"/><stop offset="1" stop-color="#889fc8" stop-opacity="0"/></radialGradient></defs><ellipse cx="${gx}" cy="${gy}" rx="${2150*gs}" ry="${2150*gs}" fill="url(#halo)"/>`))}));
 const paths=r.paths.map(p=>`<path d="${p.d}" stroke="${p.stroke}" stroke-width="${num(p.width)}" stroke-opacity="${p.opacity}" vector-effect="non-scaling-stroke"/>`).join('');
 g.children.push(node('Svg','talent-lines',0,0,r.rect.width,r.rect.height,{svgAsset:asset('graph-lines',r.rect.width,r.rect.height,svg(r.rect.width,r.rect.height,`<g fill="none" transform="translate(${gx} ${gy}) scale(${gs})">${paths}</g>`)),source:{vectorPaths:tree.edges.map(e=>({key:[...e].sort().join(':'),points:tree.layout.edgeRoutes[[...e].sort().join(':')]?.points||e.map(id=>tree.nodes.find(n=>n.id===id))})),graphScale:gs,origin:{x:gx,y:gy}}}));
 for(const p of r.points){const item=tree.nodes.find(n=>n.id===p.id),w=p.rect.width,h=p.rect.height,id='point-'+p.id;const button=node('Button',id,p.rect.x-r.rect.x,p.rect.y-r.rect.y,w,h,{name:p.label,action:{type:'talent',op:'node',value:p.id},source:{world:item,skinKey:item.kind+'-'+(item.kind==='origin'?31:item.kind==='attribute'?9.5:item.sourceBuild?26:22)}});const c=node('CanvasPanel',id+'-content',0,0,w,h);button.children=[c];const skin=refs.default.skins[button.source.skinKey],circles=skin.states[2*(item.kind==='origin')].circles;
 const circleSVG=circles.map(v=>`<circle cx="${v.cx*w}" cy="${v.cy*h}" r="${v.r*w}" fill="${v.fill==='node'?nodeColor(item):v.fill}" fill-opacity="${v.fillOpacity}" stroke="${v.stroke==='node'?nodeColor(item):v.stroke}" stroke-opacity="${v.strokeOpacity}" stroke-width="${v.width}"/>`).join('');
 const ring=node('Svg','ring-'+p.id,0,0,w,h,{svgAsset:asset('ring-'+p.id,w,h,svg(w,h,circleSVG)),source:{circleStates:skin.states,nodeColor:nodeColor(item)}});c.children.push(ring);
 const cx=(-skin.bbox.x/skin.bbox.width)*w,cy=(-skin.bbox.y/skin.bbox.height)*h;
 if(item.kind==='legendary'){c.children.push(node('Svg','icon-'+p.id,cx-17*gs,cy-17*gs,34*gs,34*gs,{svgAsset:'legend-'+p.id,style:{opacity:.85}}));c.children.push(node('Svg','crown-'+p.id,cx-4*gs,cy-36*gs,8*gs,10*gs,{svgAsset:asset('crown',8,10,svg(8,10,'<path d="M0 5 4 0 8 5 4 10Z" fill="#c5a875"/>'))}));}
 else if(item.kind==='origin')c.children.push(node('Svg','star-origin',cx-19*gs,cy-19*gs,38*gs,38*gs,{svgAsset:asset('origin-star',38,38,svg(38,38,'<path d="M19 0 25 13 38 19 25 25 19 38 13 25 0 19 13 13Z" fill="#cdb37b" stroke="#ead9af" stroke-width="1"/>'))}));
 else c.children.push(node('Svg','icon-'+p.id,cx-7.5*gs,cy-7.5*gs,15*gs,15*gs,{svgAsset:'attr-intelligence',visibility:'hidden'}));
 g.children.push(button);
 }
 return n;
}
function nodeColor(n){return n.kind==='origin'?'#cdb37b':n.kind==='attribute'?'#9bb3d7':tree.sectors.find(s=>s.id===n.sector)?.color||'#b1a27a';}
// SVG map paints first; toolbar and zoom chrome remain above it as in CSS stacking.
const atlas=find('stc-atlas');atlas.children.sort((a,b)=>(a.tag==='graph'?-1:b.tag==='graph'?1:0));
doc.root.children.push(convert(refs.default.root,{x:0,y:0}));
// Dynamic meter remains an authored child.
byid.get('talent-plan-meter').children.push(node('Svg','talent-plan-fill',0,0,281,3,{svgAsset:asset('meter',281,3,svg(281,3,'<rect width="281" height="3" fill="#c7a76a"/>')),visibility:'hidden'}));
// Actual popup states are independent editable UICreator controls, hidden by default.
for(const [state,klass,id]of [['attribute','stc-node-dialog','talent-attribute-menu'],['legend-tip','stc-node-tip','talent-legend-tip'],['legend-dialog','stc-node-dialog','talent-card'],['search','stc-results','talent-results'],['attribute-tip','stc-node-tip','talent-attribute-tip'],['notice','stc-node-tip','talent-tip']]){
 const r=flat(refs[state].root).find(n=>has(n,klass));const mapping={[num(r.id.slice(4))]:id};if(!r)throw Error('Missing original state '+state);const n=convert(r,{x:0,y:0},state+'-',mapping);n.visibility='collapsed';doc.root.children.push(n);semantic[id]={source:state,root:id,ids:flat(r).map(v=>({id:mapping[num(v.id.slice(4))]||state+'-'+v.id,classes:v.classes,tag:v.tag,text:v.texts.map(t=>t.text).join('\n'),rect:v.rect}))};
}
// Reusable selected/search rows retain the original row geometry and style.
for(const [state,klass,id]of [['allocated','stc-selected-list','talent-selected-row'],['search','stc-results','talent-result-row']]){
 const section=flat(refs[state].root).find(n=>has(n,klass)),r=flat(section).find(n=>n.tag==='button');const n=convert(r,{x:r.rect.x,y:r.rect.y},id+'-',{[num(r.id.slice(4))]:id});n.visibility='collapsed';doc.root.children.push(n);semantic[id]={source:state,root:id,ids:flat(r).map(v=>({id:v===r?id:id+'-'+v.id,classes:v.classes,tag:v.tag,text:v.texts.map(t=>t.text).join('\n'),rect:v.rect}))};
}
const categoryAsset=byid.get('talent-cat-all').svgAsset;
const library=node('CanvasPanel','talent-icon-library',1300,100,96,96,{visibility:'collapsed'});for(const id of Object.keys(LEGENDARY_ICONS))library.children.push(node('Svg','legend-sample-'+id,0,0,96,96,{svgAsset:'legend-'+id}));for(const a of tree.attributes)library.children.push(node('Svg','attr-sample-'+a.id,0,0,96,96,{svgAsset:'attr-'+a.id}));library.children.push(node('Svg','talent-category-sample',0,0,62.12,28.5,{svgAsset:categoryAsset}));doc.root.children.push(library);
// Stable runtime bindings refer to measured original controls.
const aliases={'attribute-dom-114':'talent-details-close','attribute-dom-125-text':'talent-menu-cost','legend-dialog-dom-119':'talent-card-close','legend-dialog-dom-117-text':'talent-card-title','legend-dialog-dom-121-text':'talent-card-description','legend-dialog-dom-124-text':'talent-card-requirement','legend-dialog-dom-115':'talent-card-icon','legend-dialog-dom-112-text':'talent-card-kind','legend-dialog-dom-113-text':'talent-card-state','legend-dialog-dom-118-text':'talent-card-sector','legend-dialog-dom-152-text':'talent-card-remaining','legend-dialog-dom-154':'talent-card-allocate','legend-dialog-dom-154-text':'talent-card-allocate-label','legend-dialog-dom-133-text':'talent-card-source','legend-tip-dom-112':'talent-hover-icon','legend-tip-dom-114-text':'talent-hover-kind','legend-tip-dom-115-text':'talent-hover-title','legend-tip-dom-116-text':'talent-hover-state','legend-tip-dom-118-text':'talent-hover-description','legend-tip-dom-122-text':'talent-hover-requirement','legend-tip-dom-125-text':'talent-hover-reach','legend-tip-dom-126-text':'talent-hover-hint','talent-selected-row-dom-97':'talent-row-icon','talent-selected-row-dom-98-text':'talent-row-name','talent-selected-row-dom-99-text':'talent-row-category','talent-result-row-text':'talent-result-name','talent-result-row-dom-41-text':'talent-result-sector'};
for(let i=0;i<4;i++)aliases['attribute-dom-'+(116+i*2)]='talent-choose-'+tree.attributes[i].id;
for(const n of all)if(aliases[n.id]){byid.delete(n.id);n.id=n.binding=aliases[n.id];byid.set(n.id,n);}
for(const [id,op]of Object.entries({'talent-details-close':'details-close','talent-card-close':'details-close','talent-card-allocate':'allocate','talent-selected-row':'focus','talent-result-row':'focus'}))byid.get(id).action={type:'talent',op,value:''};
for(const a of tree.attributes)byid.get('talent-choose-'+a.id).action={type:'talent',op:'attribute',value:a.id};
// Refund reuses the original icon-button styling from page.css.
const refund=node('Button','talent-refund',215,99,25,22,{action:{type:'talent',op:'refund'},style:{radius:3},children:[node('TextBlock','talent-refund-label',0,0,25,22,{text:'↶',style:{color:'#b5a380',fontSize:20,textAlign:'center'}})]});byid.get('talent-attribute-menu').children.push(refund);
for(const n of all){if(n.id==='talent-search'||n.id==='talent-budget-input'){n.style.fontSize=n.source?num(n.source.computed.fontSize):11;n.style.color='#a9b8ce';}if(n.id==='talent-results'){for(const c of n.children)if(c.source?.classes==='')c.visibility='collapsed';}}
// Exact menu and state fields can be resolved without native UI designs.
for(const v of Object.values(semantic))for(const r of v.ids){if(aliases[r.id])r.id=aliases[r.id];}
const selected=byid.get('talent-selected-list');selected.children.push(node('CanvasPanel','talent-selected-mount',19,47.5,281,42,{style:{rowGap:0}}));
for(const n of all){if(n.id==='talent-card-title'){n.slot.offsets.right=n.layout.width=321;}if(n.id.startsWith('talent-total-')&&n.id.endsWith('-text')||n.id.startsWith('talent-live-')&&n.id.endsWith('-text')||n.id==='talent-remaining-text'){n.slot.offsets.left-=25;n.slot.offsets.right=n.layout.width+=25;n.style.textAlign='right';}if(n.id==='legend-dialog-dom-127-text'){n.style.wrap=true;n.slot.offsets.right=n.layout.width=368;}if(n.id==='talent-preview-result-text'){n.slot.offsets.right=n.layout.width=281;n.style.wrap=true;}}
doc.provenance={source:'Unmodified RuleStudio battle talent DOM',sourceUrls:Object.values(refs).map(r=>r.url),job,treeId:tree.id,revision:tree.revision,sourceAssets,nodeCount:1000,legendaryCount:100,businessRuntime:'AngelScript',reference:'Saved/Verification/Talents/faithful-source/default.png',sourceLayoutHash:crypto.createHash('sha256').update(JSON.stringify(refs.default)).digest('hex')};
doc.presentation.talents={semantic,skins:refs.default.skins,graph:{x:graphRef.rect.x,y:graphRef.rect.y,width:graphRef.rect.width,height:graphRef.rect.height,scale:gs,origin:{x:gx,y:gy},viewBox:[vx,vy,vw,vh]}};
const errors=validateDocument(doc);if(errors.length)throw Error(errors.slice(0,20).join('\n'));
for(const dir of ['UIAuthoring/projects','CustomTools/UICreatorAlpha/data/projects']){const repo=new ProjectRepository(path.join(root,dir));const prev=await repo.pointer(doc.id);await repo.write(doc,prev?.revision||0);}
await fs.writeFile(path.join(root,'UIAuthoring/TalentConstellation.uialpha.json'),JSON.stringify(doc,null,2));await fs.writeFile(path.join(frozen,'authoring-project.json'),JSON.stringify(doc));await fs.writeFile(path.join(root,'Saved/Verification/Talents/faithful-source/authoring-map.json'),JSON.stringify({sourceRefs,semantic,job},null,2));
console.log(JSON.stringify({job,nodes:all.length+1,assets:Object.keys(doc.assets).length,graphScale:gs,semantic:Object.keys(semantic)}));
