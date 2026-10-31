// Freeze the real RuleStudio presentation, not the old UI fixture. Tooling only.
import fs from 'node:fs/promises';
import path from 'node:path';
import vm from 'node:vm';
import {createRequire} from 'node:module';
import {createAtlas} from '../../CustomTools/RuleStudio/public/atlas-state.mjs';
const root=path.resolve(import.meta.dirname,'../..');
const require=createRequire(import.meta.url);
const {createCanvas}=require(process.env.ATLAS_CANVAS_MODULE||'C:/Users/罗长生/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/@napi-rs/canvas');
const source=await fs.readFile(path.join(root,'CustomTools/RuleStudio/public/atlas-map.mjs'),'utf8');
const sandbox={document:{createElement:()=>createCanvas(1,1)},Math};
vm.createContext(sandbox);
vm.runInContext(source.slice(source.indexOf('const TONES'),source.indexOf('export const AtlasMap'))+';this.api={createTerrain,createFog,hash};',sandbox);
const state=createAtlas(1),terrain=sandbox.api.createTerrain(state.nodes,state.seed),fog=sandbox.api.createFog(state.nodes,terrain.bounds);
// Independent fidelity oracle: execute the original web draw function unchanged.
const reference=createCanvas(838,600),originReference=state.nodes.find(n=>n.id===state.selectedId),referenceById=new Map(state.nodes.map(n=>[n.id,n]));
Object.assign(sandbox,{canvas:{value:reference},dpr:1,width:838,height:600,terrain,fog,camera:{x:originReference.x,y:originReference.y,zoom:.48},allNodes:{value:state.nodes},selectedId:{value:state.selectedId},hoverId:{value:''},media:{matches:true},edges:state.nodes.flatMap(a=>a.neighbors.filter(id=>a.id<id).map(id=>[a,referenceById.get(id)]))});
vm.runInContext(source.slice(source.indexOf('    function draw(time)'),source.indexOf('    function animate(time)'))+';draw(0);',sandbox);
await fs.mkdir(path.join(root,'Saved/Verification/MapDevice'),{recursive:true});
await fs.writeFile(path.join(root,'Saved/Verification/MapDevice/original-web-atlas.png'),reference.toBuffer('image/png'));
const sharp=require('C:/Users/罗长生/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/sharp');
const encode=async c=>'data:image/webp;base64,'+(await sharp(c.toBuffer('image/png')).webp({lossless:true,effort:6}).toBuffer()).toString('base64');
const terrainUri=await encode(terrain.canvas),fogUri=await encode(fog);
const {x,y,width,height}=terrain.bounds,w=838,h=600,zoom=.48,origin=state.nodes.find(n=>n.id===state.selectedId);
const escape=s=>s.replaceAll('&','&amp;').replaceAll('"','&quot;').replaceAll('<','&lt;');
const tones={hidden:'#495f5b',locked:'#79847a',available:'#d8b570',active:'#8de1d1',cleared:'#c4ddaa',failed:'#d78970'};
const glyphs=[[[[-.5,.35],[0,-.55],[.5,.35],[-.5,.35]],[[0,-.55],[0,.55]]],[[[-.4,-.3],[.4,-.3],[.4,.4],[-.4,.4],[-.4,-.3]],[[-.5,-.3],[0,-.65],[.5,-.3]],[[0,.4],[0,0]]],[[[-.55,.3],[-.1,-.5],[.15,-.1],[.35,-.3],[.6,.3]]],[[[0,-.65],[0,.65]],[[-.65,0],[.65,0]]]];
let body=`<g transform="translate(${w/2},${h/2}) scale(${zoom}) translate(${-origin.x},${-origin.y})"><image href="${fogUri}" x="${x}" y="${y}" width="${width}" height="${height}"/>`;
const by=new Map(state.nodes.map(n=>[n.id,n]));
for(const a of state.nodes)for(const id of a.neighbors)if(a.id<id){const n=by.get(id),hidden=a.status==='hidden'||n.status==='hidden',open=[a,n].every(n=>['available','active','failed','cleared'].includes(n.status)),cleared=a.status==='cleared'&&n.status==='cleared';const curve=(sandbox.api.hash(a.id+n.id)%21-10)*.9,dx=n.x-a.x,dy=n.y-a.y;body+=`<path d="M${a.x} ${a.y}Q${(a.x+n.x)/2-dy/140*curve} ${(a.y+n.y)/2+dx/140*curve} ${n.x} ${n.y}" fill="none" stroke="${hidden?'rgba(120,152,136,.115)':cleared?'#8ea889':open?'rgba(205,174,108,.65)':'rgba(121,143,123,.3)'}" stroke-width="${(open?1.5:.8)/zoom}"${hidden?` stroke-dasharray="${2/zoom} ${6/zoom}"`:''}/>`;}
for(const n of state.nodes){const hidden=n.status==='hidden',selected=n.id===state.selectedId,r=(hidden?3:selected?17:n.status==='locked'?9:13)/Math.sqrt(zoom),tone=tones[n.status];body+=`<g transform="translate(${n.x},${n.y})">`;if(hidden){body+=`<circle r="${r}" fill="rgba(112,148,126,.3)"/></g>`;continue;}if(selected)body+=`<defs><radialGradient id="selected-glow"><stop offset=".125" stop-color="#e7b85e" stop-opacity=".3"/><stop offset="1" stop-color="#e7b85e" stop-opacity="0"/></radialGradient></defs><circle r="${r*3.2}" fill="url(#selected-glow)"/><circle r="${r+6/zoom}" fill="none" stroke="#e3c083" stroke-width="${1/zoom}"/>`;
 body+=`<circle r="${r}" fill="${n.status==='locked'?'#172623':'#152a25'}" stroke="${tone}" stroke-width="${(selected?2:1)/zoom}"/>`;
 if(n.status==='locked')body+=`<circle r="${2/zoom}" fill="${tone}"/>`;else{const g=r*.8,kind=sandbox.api.hash(n.biome)%4;body+=`<g fill="none" stroke="${tone}" stroke-width="1.3">${glyphs[kind].map(p=>`<path d="M${p.map(v=>`${v[0]*g} ${v[1]*g}`).join('L')}"/>`).join('')}${kind===3?`<circle r="${g*.35}"/>`:''}</g>`;}
 if(selected||zoom>.82&&n.status!=='locked'){const tw=n.name.length*11/zoom,ly=r+13/zoom;body+=`<rect x="${-tw/2-8/zoom}" y="${ly-4/zoom}" width="${tw+16/zoom}" height="${20/zoom}" fill="rgba(6,17,19,.87)"/><text x="0" y="${ly}" fill="${selected?'#e6d7b1':'#adbcb0'}" text-anchor="middle" dominant-baseline="text-before-edge" font-family="Microsoft YaHei,sans-serif" font-size="${11/zoom}">${escape(n.name)}</text>`;}body+='</g>';
}
body+='</g>';
const svg=(w,h,b)=>`<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 ${w} ${h}">${b}</svg>`;
const art={bounds:terrain.bounds,terrainUri,glyphs,preview:svg(w,h,body),vignette:svg(w,h,`<defs><radialGradient id="v" gradientUnits="userSpaceOnUse" cx="${w/2}" cy="${h/2}" r="${Math.max(w,h)*.66}"><stop offset="${Math.min(w,h)*.24/(Math.max(w,h)*.66)}" stop-color="#01080b" stop-opacity="0"/><stop offset="1" stop-color="#01080b" stop-opacity=".66"/></radialGradient></defs><rect width="${w}" height="${h}" fill="url(#v)"/>`)};
await fs.writeFile(path.join(root,'CustomTools/UICreatorAlpha/src/assets/rulestudio-hud/atlas-engraved.generated.js'),'// Generated from RuleStudio atlas-map.mjs via Tools/UI/Rebuild-AtlasArt.mjs\nexport const engravedAtlasArt='+JSON.stringify(art)+';\n');
console.log(JSON.stringify({nodes:state.nodes.length,bounds:terrain.bounds,source:'actual RuleStudio atlas-map.mjs',worldArtworkOnly:true}));
