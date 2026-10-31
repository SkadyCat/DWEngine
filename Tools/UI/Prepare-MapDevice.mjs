// Native projection of the approved UICreator page. No game or inventory policy.
import fs from 'node:fs/promises';
import path from 'node:path';
import {ProjectRepository} from '../../CustomTools/UICreatorAlpha/server/project-repository.mjs';
import {createNode,validateDocument,walk} from '../../CustomTools/UICreatorAlpha/src/core/model.js';
import {mapDeviceCell} from '../../CustomTools/UICreatorAlpha/src/assets/rulestudio-hud/map-device.js';
import {dynamicQualityBrushes} from '../../CustomTools/UICreatorAlpha/src/assets/rulestudio-hud/equipment-style.js';
import {engravedAtlasArt} from '../../CustomTools/UICreatorAlpha/src/assets/rulestudio-hud/atlas-engraved.generated.js';
import {atlasPlayerLocationDocument} from '../../CustomTools/UICreatorAlpha/src/assets/rulestudio-hud/atlas-player-location.js';
import {art} from '../../CustomTools/UICreatorAlpha/src/assets/rulestudio-hud/suite-kit.js';
const root=path.resolve(import.meta.dirname,'../..'),tool=path.join(root,'CustomTools/UICreatorAlpha');
const repo=new ProjectRepository(path.join(tool,'data/projects')),record=await repo.read('hud-darksteel-20261024');
const doc=structuredClone(record.document),page=structuredClone(doc.prefabs['battle-prefab-map-device'].root);
const by=new Map();walk(page,n=>by.set(n.id,n));const find=id=>by.get('map-device-preview-'+id);
const fixture={key:'native-template',name:'地图',rarity:'common',tier:1,count:2};
function cell(id,x,y,size){const n=mapDeviceCell(id,fixture,x,y,size);n.source.dynamicBrushes=dynamicQualityBrushes('cell');walk(n,c=>{if(c.id.endsWith('-icon'))c.style.equipmentAlpha=true;});return n;}
const bag=find('map-device-bag');bag.source.nativeRows=true;
find('map-device-backpack').binding='map-device-backpack';
walk(page,n=>{if(n.id.startsWith('map-device-preview-map-device-empty-label-'))n.binding='map-device-empty-label:'+n.id.slice('map-device-preview-map-device-empty-label-'.length);else if(n.id.startsWith('map-device-preview-map-device-empty-'))n.binding='map-device-empty:'+n.id.slice('map-device-preview-map-device-empty-'.length);});
find('battle-atlas-enter-label').binding='map-device-enter-label';
bag.children=bag.children.map((n,i)=>cell(n.id,i%7*52,Math.floor(i/7)*52,46));
for(const slot of ['map','modifier-0','modifier-1','modifier-2','modifier-3','modifier-4']){
 const content=find('map-device-content-'+slot),size=slot==='map'?56:44;
 // Empty glyphs stay authored. A separate list overlays the occupied item.
 content.children.push(createNode('CanvasPanel',{id:'map-device-mounted-'+slot,binding:'map-device-mounted:'+slot,layout:{width:size,height:size},slot:{offsets:{right:size,bottom:size}},source:{nativeRows:true},children:[cell('map-device-cell-'+slot,0,0,size)]}));
}
const terrainResource=createNode('Image',{id:'atlas-terrain-resource',src:engravedAtlasArt.terrainUri,slot:{offsets:{left:0,top:820,right:1536,bottom:1536}},layout:{width:1536,height:1536}});
const playerDoc=atlasPlayerLocationDocument();Object.assign(doc.assets,playerDoc.assets);const playerMarker=structuredClone(playerDoc.root);playerMarker.slot.offsets={left:800,top:750,right:176,bottom:112};walk(playerMarker,n=>{n.binding=n.id;});
const ghost=createNode('CanvasPanel',{id:'map-device-drag-template',slot:{offsets:{left:0,top:750,right:46,bottom:46}},layout:{width:46,height:46},children:[createNode('CanvasPanel',{id:'map-device-drag-content',binding:'map-device-drag-content',slot:{offsets:{right:46,bottom:46}},layout:{width:46,height:46},children:[createNode('Image',{id:'map-device-drag-icon',binding:'map-device-drag-icon',slot:{offsets:{right:46,bottom:46}},layout:{width:46,height:46}})]})]});
doc.id='opus-map-device';doc.name='Opus · 星图与地图装配';doc.viewport={width:1536,height:2360,background:'transparent'};
doc.root=createNode('CanvasPanel',{id:'map-device-library',layout:{width:1536,height:2360},slot:{offsets:{right:1536,bottom:2360}},children:[page,ghost,playerMarker,terrainResource]});
doc.prefabs={};doc.interfaceCatalog=[];doc.presentation={independentCaches:true,qualityColors:record.document.presentation?.qualityColors};
const assets=new Set();walk(doc.root,n=>{if(n.svgAsset)assets.add(n.svgAsset);for(const a of Object.values(n.brushes||{}))if(a)assets.add(a);for(const family of Object.values(n.source?.dynamicBrushes||{}))for(const a of Object.values(family))if(a)assets.add(a);});
doc.assets=Object.fromEntries([...assets].map(id=>[id,doc.assets[id]]));
doc.provenance={source:record.document.id,revision:record.revision,scope:'map-device',businessRuntime:'AngelScript',slots:6,modifierEffects:'deferred',nativeProjection:'authored row templates replace example values'};
const errors=validateDocument(doc);if(errors.length)throw Error(errors.join('\n'));
const proof=path.join(root,'Saved/Verification/MapDevice');await fs.mkdir(proof,{recursive:true});await fs.writeFile(path.join(proof,'source-project.json'),JSON.stringify(record));
await repo.write(doc,(await repo.pointer(doc.id))?.revision||0);await fs.writeFile(path.join(root,'UIAuthoring/MapDevice.uialpha.json'),JSON.stringify(doc,null,2));
console.log(JSON.stringify({id:doc.id,sourceRevision:record.revision,assets:assets.size,nodes:(()=>{let n=0;walk(doc.root,()=>n++);return n;})()}));
