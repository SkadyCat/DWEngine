// Export the actual RuleStudio factories, not hand-authored lookalikes.
// Native meshes/materials + sampled local poses. Gameplay inputs stay outside.
import fs from 'node:fs/promises';
import path from 'node:path';
import crypto from 'node:crypto';
import {fileURLToPath,pathToFileURL} from 'node:url';
import * as T from '../../CustomTools/RuleStudio/public/vendor/three.module.js';
import {modules,presets} from '../../CustomTools/RuleStudio/public/vfx/catalog.mjs';
import {modernFactory} from './modern-prefabs.mjs';
import {createHideoutWorld} from '../../CustomTools/RuleStudio/public/hideout/scene.mjs';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'../..');
const studioOnly=process.argv.includes('--studio-only');
const studioFactories={};
if(studioOnly)for(const [id,file,fn]of [['outpost-merchant','dungeon-merchant.mjs','createDungeonMerchant'],['dungeon-portal','dungeon-portal.mjs','createDungeonPortal']]){
 const text=(await fs.readFile(path.join(root,'CustomTools/RuleStudio/public/prefabs',file),'utf8')).replaceAll("'/vendor/three.module.js'",JSON.stringify(pathToFileURL(path.join(root,'CustomTools/RuleStudio/public/vendor/three.module.js')).href));
 studioFactories[id]=(await import('data:text/javascript;base64,'+Buffer.from(text).toString('base64')))[fn];
}
const build=path.join(root,'Saved/WebVFX/Source'),out=path.join(build,'Clips'),data=path.join(root,'Content/WebVFX/Data');
await fs.mkdir(out,{recursive:true});await fs.mkdir(build,{recursive:true});await fs.mkdir(data,{recursive:true});
const hash=s=>crypto.createHash('sha256').update(s).digest('hex').slice(0,20);
const geometryMap=new Map(),materialMap=new Map(),clips=[];
const pointGeometry=new T.PlaneGeometry(1,1);
const finite=a=>{if(!a.every(Number.isFinite))throw Error('Non-finite prefab data');return a};
const colorValue=(v,fallback=[1,1,1])=>v?.isColor?[v.r,v.g,v.b]:fallback;
const point=(x,y,z)=>[x*100,-z*100,y*100];
function geometry(g){
 const data={position:Array.from(g.attributes.position.array),normal:g.attributes.normal?Array.from(g.attributes.normal.array):[],uv:g.attributes.uv?Array.from(g.attributes.uv.array):[],index:g.index?Array.from(g.index.array):Array.from({length:g.attributes.position.count},(_,i)=>i)};
 const id='G_'+hash(JSON.stringify(data));if(!geometryMap.has(id))geometryMap.set(id,{id,...data});return id;
}
function material(m,billboard){
 if(Array.isArray(m))throw Error('Multi-material prefab mesh needs an explicit split');
 if(!['MeshBasicMaterial','MeshStandardMaterial','ShaderMaterial','PointsMaterial'].includes(m.type))throw Error('Unsupported native material: '+m.type);
 const data={type:m.type,additive:m.blending===T.AdditiveBlending,transparent:m.transparent,doubleSided:m.side===T.DoubleSide,billboard:Boolean(billboard),fragment:m.fragmentShader||'',vertex:m.vertexShader||'',roughness:m.roughness??.5,metalness:m.metalness??0,emissive:colorValue(m.emissive,[0,0,0]),emissiveIntensity:m.emissiveIntensity??0};
 if(m.wireframe)data.wireframe=true;
 if(m.userData.nativePortal)data.portal=m.userData.nativePortal;
 if(m.userData.nativeWeaponClip)data.weaponClip=true;
 if(m.userData.nativeWeaponGain)data.weaponGain=m.userData.nativeWeaponGain;
 if(m.userData.nativeImpactSolid)data.impactSolid=true;
 const constants=Object.entries(m.uniforms||{}).filter(([name,u])=>u.value?.isColor&&!['color'].includes(name));
 data.fragment=data.fragment.replace(/uniform\s+vec3\s+([^;]+);/g,(all,names)=>{const kept=names.split(',').map(n=>n.trim()).filter(n=>!constants.some(([name])=>name===n));return kept.length?'uniform vec3 '+kept.join(',')+';':'';});
 for(const [name,u] of constants)data.fragment=data.fragment.replace(new RegExp('\\b'+name+'\\b','g'),`vec3(${u.value.r},${u.value.g},${u.value.b})`);
 const id='M_'+hash(JSON.stringify(data));if(!materialMap.has(id))materialMap.set(id,{id,...data});return id;
}
function factory(p){
 if(studioFactories[p.prefab]){const fx=studioFactories[p.prefab]();return {...fx,update(t){fx.update({x:0,y:0},t)}};}
 if(p.prefab==='hideout-room'){const world=createHideoutWorld();world.avatar.removeFromParent();world.destination.removeFromParent();return {...world,update(){}};}
 const mod=modules[p.prefab];
 const special=modernFactory(T,p,mod);if(special)return special;
 // Batch-only input removes preview projectile movement; the native carrier
 // supplies every real position/velocity. Status and field templates use a
 // single normalized body/region and retain their own animation.
 if(mod.createBatch){
  const fx=mod.createBatch(T,1),projectile=p.prefab.startsWith('projectile-');
  const row={x:0,y:0,z:0,dx:24,dy:0,dz:0,vx:24,vy:0,vz:0,radius:projectile?.24:p.prefab.startsWith('ground-')?1:.5,height:1.6,stacks:1,element:p.element??(p.prefab==='projectile-ice-lance'?1:5)};
  return {...fx,update(t){fx.update([row],t)}};
 }
 return mod.createEffect(T,{element:p.element,groundTint:p.prefab==='whirlwind'?p.element:undefined});
}
const matrix=new T.Matrix4(),world=new T.Matrix4(),p3=new T.Vector3(),q4=new T.Quaternion(),s3=new T.Vector3(),instanceColor=new T.Color();
function pose(mesh,matrix,material,uniforms,extraColor=null,pointScale=null,instance=0){
 matrix.decompose(p3,q4,s3);
 const c=uniforms.color?.value?colorValue(uniforms.color.value):colorValue(material.color);
 if(extraColor){c[0]*=extraColor.r;c[1]*=extraColor.g;c[2]*=extraColor.b}
 const scalars=['age','fade','mode','clock','inner','angle'].map(k=>typeof uniforms[k]?.value==='number'?uniforms[k].value:k==='fade'?1:0);
 if(mesh.geometry.attributes.impactColor){const a=mesh.geometry.attributes;c[0]=a.impactColor.getX(instance);c[1]=a.impactColor.getY(instance);c[2]=a.impactColor.getZ(instance);scalars[0]=a.impactPhase.getX(instance);scalars[1]=a.impactAlpha.getX(instance);scalars[2]=a.impactStyle.getX(instance);scalars[4]=a.impactSeed.getX(instance);scalars[5]=a.impactSpin.getX(instance);if(mesh.userData.nativeBillboard)q4.multiply(new T.Quaternion().setFromAxisAngle(new T.Vector3(0,0,1),scalars[5]));}
 if(material.userData.nativePortal){const timing=mesh.geometry.attributes.portalTiming||mesh.geometry.attributes.timing;scalars[0]=timing.getX(instance);scalars[1]=timing.getY(instance);}
 // Factory previews without a launch expose the complete weapon. Real combat
 // replaces Inner with the authoritative travelled distance for emergence.
 if(material.userData.nativeWeaponClip)scalars[4]=1e6;
 const scale=pointScale?[pointScale,pointScale,pointScale]:[s3.x,s3.z,s3.y];
 return finite([...point(p3.x,p3.y,p3.z),q4.x,-q4.z,q4.y,q4.w,...scale,...c,material.opacity??1,...scalars]);
}
const tintable=p=>/^(projectile-|area-|melee-)/.test(p.prefab);
const expanded=[...presets,...presets.filter(p=>tintable(p)||p.prefab==='whirlwind').flatMap(p=>Array.from({length:6},(_,element)=>({...p,id:p.id+'@'+element,element,internal:true}))),...presets.filter(p=>['royal-treasury-gate','projectile-treasury-weapon'].includes(p.prefab)).flatMap(p=>[0,1,2].map(variant=>({...p,id:p.id+'@style'+variant,variant,internal:true}))),{id:'treasury-ea-gate',prefab:'projectile-treasury-ea',name:'乖离剑巨大法阵',internal:true,eaGate:true,duration:.52},{id:'hideout-room',prefab:'hideout-room',name:'藏身处原始场景',internal:true,continuous:true,static:true,duration:6}];
const selected=studioOnly?[{id:'outpost-merchant',prefab:'outpost-merchant',name:'拓荒商人 · Studio原始模型',internal:true,static:true,continuous:true,duration:6},{id:'dungeon-portal',prefab:'dungeon-portal',name:'地牢出口传送阵',internal:true,continuous:true,duration:6}]:expanded;
for(const preset of selected){
 const fx=factory(preset),parts=[],partIndices=new Map(),frames=[],duration=preset.continuous?6:preset.duration,fps=60;
 const frameCount=preset.static?2:Math.max(2,Math.ceil(duration*fps));
 for(let f=0;f<frameCount;f++){
  const t=f/fps;
  fx.update(t,{element:preset.element,radius:1,inner:.5,angle:Math.PI/2,length:7,width:1.2,from:{x:0,y:0,z:0},to:{x:1,y:0,z:0}});
  fx.group.updateMatrixWorld(true);const rows=[];
  fx.group.traverse(mesh=>{
   if(!mesh.geometry||!mesh.material)return;
   if(mesh.isLine||mesh.isLineSegments)throw Error(preset.id+': unsupported line geometry; convert explicitly');
   const points=mesh.isPoints;
   if(!mesh.isMesh&&!points)throw Error(preset.id+': unrecognized visual primitive');
   const mat=mesh.material;
   if(!partIndices.has(mesh.uuid)){
    const id=parts.length;partIndices.set(mesh.uuid,id);
    parts.push({geometry:geometry(points?pointGeometry:mesh.geometry),material:material(mat,points||mesh.userData.nativeBillboard),billboard:Boolean(points||mesh.userData.nativeBillboard),name:mesh.name||mesh.geometry.type});
   }
   let visible=true;for(let parent=mesh;parent;parent=parent.parent)visible&&=parent.visible;
   const instances=[];
   if(visible&&(!mat.transparent||(mat.opacity??1)>.0001)){
    const uniforms=mat.uniforms||{};
    if(points){
     const positions=mesh.geometry.attributes.position;
     for(let i=0;i<positions.count;i++){
      world.makeTranslation(positions.getX(i),positions.getY(i),positions.getZ(i));world.premultiply(mesh.matrixWorld);
      instances.push(pose(mesh,world,mat,uniforms,null,mat.size??.045));
     }
    }else if(mesh.isInstancedMesh){
     for(let i=0;i<mesh.count;i++){
      mesh.getMatrixAt(i,matrix);world.multiplyMatrices(mesh.matrixWorld,matrix);
      const c=mesh.instanceColor?(mesh.getColorAt(i,instanceColor),instanceColor):null;
      instances.push(pose(mesh,world,mat,uniforms,c,null,i));
     }
    }else instances.push(pose(mesh,mesh.matrixWorld,mat,uniforms));
   }
   rows.push({part:partIndices.get(mesh.uuid),instances});
  });frames.push(rows);
 }
 const chunks=[];const uint=v=>{const b=Buffer.allocUnsafe(4);b.writeUInt32LE(v);chunks.push(b)},float=v=>{const b=Buffer.allocUnsafe(4);b.writeFloatLE(v);chunks.push(b)};
 uint(0x58465744);uint(1);uint(frameCount);float(fps);float(duration);
 for(const frame of frames){uint(frame.length);for(const row of frame){uint(row.part);uint(row.instances.length);const b=Buffer.allocUnsafe(row.instances.length*80);let at=0;for(const v of row.instances.flat()){b.writeFloatLE(v,at);at+=4}chunks.push(b)}}
 const bytes=Buffer.concat(chunks),file=preset.id+'.dwfx';await fs.writeFile(path.join(out,file),bytes);
 clips.push({id:preset.id,prefab:preset.prefab,name:preset.name,internal:Boolean(preset.internal),continuous:Boolean(preset.continuous),element:preset.element??-1,duration,fps,frames:frameCount,file,sha256:hash(bytes),parts});
 fx.dispose();console.log(preset.id,parts.length,bytes.length);
}
for(const g of geometryMap.values()){
 const lines=['o '+g.id];
 // FBX's OBJ path skips axis conversion, but ConvertPos/ConvertDir still
 // negate Y. Precompensate that reflection and its winding at the file boundary.
 for(let i=0;i<g.position.length;i+=3){const p=point(...g.position.slice(i,i+3));p[1]*=-1;lines.push('v '+p.join(' '));}
 // UE's OBJ/FBX importer flips V for DirectX. Undo that here so native custom
 // shaders receive the original Three.js UV, including asymmetric flames.
 for(let i=0;i<g.uv.length;i+=2)lines.push('vt '+g.uv[i]+' '+(1-g.uv[i+1]));
 for(let i=0;i<g.normal.length;i+=3){const [x,y,z]=g.normal.slice(i,i+3);lines.push('vn '+[x,z,y].join(' '))}
 for(let i=0;i<g.index.length;i+=3)lines.push('f '+g.index.slice(i,i+3).reverse().map(v=>{const n=v+1;return g.uv.length?`${n}/${n}${g.normal.length?'/'+n:''}`:g.normal.length?`${n}//${n}`:n}).join(' '));
 await fs.writeFile(path.join(build,g.id+'.obj'),lines.join('\n'));
}
const sourceFiles=(await fs.readdir(path.join(root,'CustomTools/RuleStudio/public/prefabs'),{recursive:true})).filter(n=>/\.(mjs|png)$/.test(n));
const sources=await Promise.all(sourceFiles.map(async name=>({file:'CustomTools/RuleStudio/public/prefabs/'+name,sha256:hash(await fs.readFile(path.join(root,'CustomTools/RuleStudio/public/prefabs',name)))})));
for(const file of ['CustomTools/RuleStudio/public/vfx/catalog.mjs','CustomTools/RuleStudio/public/hideout/scene.mjs','Tools/WebVFX/export-prefabs.mjs','Tools/WebVFX/modern-prefabs.mjs'])sources.push({file,sha256:hash(await fs.readFile(path.join(root,file)))});
const manifest={version:1,source:'RuleStudio/vfx/catalog.mjs',units:'centimeters',poseStride:20,customData:['r','g','b','opacity','age','fade','mode','clock','inner','angle'],sources,geometries:[...geometryMap.values()].map(g=>({id:g.id,file:g.id+'.obj'})),materials:[...materialMap.values()],clips};
if(studioOnly){await fs.writeFile(path.join(build,'studio-manifest.json'),JSON.stringify(manifest,null,2));console.log(JSON.stringify({clips:clips.length,geometries:geometryMap.size,materials:materialMap.size,manifest:'studio-manifest.json'}));process.exit(0);}
if(!studioOnly)await fs.writeFile(path.join(data,'catalog.json'),JSON.stringify({version:1,clips:clips.map(c=>({id:c.id,name:c.name,internal:c.internal,continuous:c.continuous}))}));
await fs.writeFile(path.join(build,'manifest.json'),JSON.stringify(manifest,null,2));
console.log(JSON.stringify({presets:presets.length,clips:clips.length,geometries:geometryMap.size,materials:materialMap.size}));
