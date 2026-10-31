// Offline presentation translation of the registered factories' vertex inputs.
// These functions are never staged into or evaluated by the game.
import { portalTextureUrls } from '../../CustomTools/RuleStudio/public/prefabs/royal-treasury/portal-textures.mjs';
const hash=n=>{const v=Math.sin(n*127.1+311.7)*43758.5453;return v-Math.floor(v)};
const random=(seed,k)=>((Math.imul((seed+k*374761393)|0,1664525)+1013904223)>>>0)/4294967296;
export function modernFactory(T,p,mod){
 if(p.prefab==='projectile-hit'){
  const fx=mod.createBatch(T,1);
  fx.group.traverse(mesh=>{
   if(!mesh.geometry?.attributes.impactColor)return;
   const mat=mesh.material,sprite=mat.vertexShader.includes('center.xy+=');
   mesh.userData.nativeBillboard=sprite;mat.userData.nativeImpactSolid=!sprite;
   mat.fragmentShader=mat.fragmentShader.replace(/varying\s+\w+\s+[^;]+;/g,'')
    .replace(/\buvHit\b/g,'vUv').replace(/\btint\b/g,'color').replace(/\bopacityHit\b/g,'fade')
    .replace(/\bstyleHit\b/g,'mode').replace(/\bphaseHit\b/g,'age').replace(/\bseedHit\b/g,'inner')
    .replace(/\bshade\b/g,'(.68+.32*max(0.,dot(normalize(normalHit),normalize(vec3(.4,-.35,1.)))))');
   mat.fragmentShader='varying vec2 vUv;uniform vec3 color,normalHit;uniform float age,fade,mode,inner;\n'+mat.fragmentShader;
  });
  return {...fx,update(age){fx.update([{x:0,y:0,z:0,dx:1,dy:0,dz:0,radius:.2,age,element:p.element??3}]);}};
 }
 if(p.prefab.startsWith('ground-')&&mod.createBatch){
  const source=mod.createBatch(T,1),original=source.group.children[0],fragment=original.material.fragmentShader;
  if(!original.geometry.attributes.splashPart||!fragment.includes('float seed=floor(vLook.x+.5)'))throw Error('Ground vertex contract changed: '+p.id);
  const group=new T.Group(),meshes=[];
  const seed=p.visualSeed??47,look=1+Math.floor(random(seed,2)*2048),speed=.35+random(seed,3)*1.2;
  for(let part=0;part<=8;part++){
   const geometry=new T.PlaneGeometry(part?1:2,part?1:2);if(!part)geometry.rotateX(-Math.PI/2);
   const material=original.material.clone();material.vertexShader='varying vec2 vUv;void main(){vUv=uv;gl_Position=projectionMatrix*modelViewMatrix*vec4(position,1.);}';
   material.uniforms={color:{value:original.material.uniforms.color.value.clone()},age:{value:0},fade:{value:1},mode:{value:look},inner:{value:speed}};
   material.fragmentShader=fragment.replace('varying vec2 vUv;varying vec4 vLook;varying float vPart,vFade;uniform vec3 color;uniform float mirror;',
    `varying vec2 vUv;uniform vec3 color;uniform float age,fade,mode,inner;const float mirror=${p.prefab==='ground-mirror'?'1.':'0.'};`)
    .replace('void main(){',`void main(){vec4 vLook=vec4(mode,age,inner,fade);float vPart=${part}.;float vFade=fade;`);
   const mesh=new T.Mesh(geometry,material);mesh.userData.nativeBillboard=part>0;meshes.push(mesh);group.add(mesh);
  }
  const q=new T.Quaternion().setFromAxisAngle(T.Object3D.DEFAULT_UP,random(seed,1)*Math.PI*2);
  function update(t){
   for(let part=0;part<=8;part++){
    const mesh=meshes[part],u=mesh.material.uniforms;u.age.value=t;u.fade.value=1;
    if(!part){mesh.position.set(0,.055,0);mesh.quaternion.copy(q);continue;}
    const k=look*.08447265625+part*7.13,delay=hash(k+1)*.18,period=2.1+hash(k+2)*3.9;
    const age=Math.max(0,t-delay),cycle=Math.floor(age/period),life=.32+hash(k+3)*.38,raw=(age%period)/life,progress=Math.max(0,Math.min(1,raw));
    const show=Number(t>=delay&&raw<1),angle=hash(k+4)*Math.PI*2,again=cycle>=.5;
    const reach=again?.08+hash(k+5)*.18:.45+hash(k+5)*.42,ox=again?(hash(k+6)-.5)*.8:0,oz=again?(hash(k+7)-.5)*.8:0;
    const height=again?.08+hash(k+8)*.13:.22+hash(k+8)*.38;
    mesh.position.set(ox+Math.cos(angle)*reach*progress,.055+4*height*progress*(1-progress),oz+Math.sin(angle)*reach*progress).applyQuaternion(q);
    const size=(.035+hash(k+9)*.055)*show*Math.pow(Math.max(0,Math.sin(progress*Math.PI)),.3);
    mesh.scale.set(size*.85,size*1.5,1);mesh.visible=show>0;u.fade.value=show*(1-progress*.65);
   }
  }
  source.dispose();return {group,update,dispose(){for(const m of meshes){m.geometry.dispose();m.material.dispose();}group.clear();}};
 }
 if(p.prefab==='royal-treasury-gate'){
  const fx=mod.createBatch(T,1),mesh=fx.group.children[0],variant=p.variant??0;
  mesh.material.userData.nativePortal={texture:portalTextureUrls[variant],variant};
  return {...fx,update(age){fx.update([{x:0,y:0,z:0,dx:1,dy:0,dz:0,radius:.2,age,variant}]);}};
 }
 if(p.prefab==='projectile-treasury-weapon'||p.prefab==='projectile-treasury-ea'){
  const fx=mod.createBatch(T,1),group=new T.Group(),copies=[],ea=p.prefab==='projectile-treasury-ea';
  // OBJ has no vertex-colour channel. Split authored triangle colours exactly.
  const originals=[];fx.group.traverse(o=>{if(o.geometry&&o.material)originals.push(o);});
  for(const original of originals){
   if(ea&&original.geometry.attributes.timing){if(!p.eaGate)continue;original.material.userData.nativePortal={texture:portalTextureUrls[0],variant:0,ea:true};group.add(original.clone());continue;}
   if(p.eaGate)continue;
   if(!original.geometry.attributes.color){group.add(original.clone());continue;}
   if(!original.geometry.attributes.normal)original.geometry.computeVertexNormals();
   const g=original.geometry,colors=g.attributes.color,positions=g.attributes.position,normals=g.attributes.normal,buckets=new Map();
   for(let i=0;i<positions.count;i+=3){const color=[colors.getX(i),colors.getY(i),colors.getZ(i)],key=color.join(',');if(!buckets.has(key))buckets.set(key,{color,p:[],n:[]});const b=buckets.get(key);for(let j=i;j<i+3;j++){b.p.push(positions.getX(j),positions.getY(j),positions.getZ(j));b.n.push(normals.getX(j),normals.getY(j),normals.getZ(j));}}
   for(const b of buckets.values()){
    const geometry=new T.BufferGeometry();geometry.setAttribute('position',new T.Float32BufferAttribute(b.p,3));geometry.setAttribute('normal',new T.Float32BufferAttribute(b.n,3));
    const material=new T.MeshBasicMaterial({color:new T.Color(...b.color),side:original.material.side});material.userData.nativeWeaponClip=true;if(ea)material.userData.nativeWeaponGain=1.15;
    const mesh=new T.InstancedMesh(geometry,material,1);group.add(mesh);copies.push({original,mesh});
   }
  }
  function update(age){const row={x:0,y:0,z:0,dx:24,dy:0,dz:0,radius:ea?.6:.2,age,variant:p.variant??0};if(ea)fx.update(p.eaGate?[]:[row],age,p.eaGate?[row]:[]);else fx.update([row]);for(const c of copies){c.mesh.count=c.original.count;if(c.mesh.count)c.original.getMatrixAt(0,c.mesh.matrixWorld),c.mesh.setMatrixAt(0,c.mesh.matrixWorld);}let tail=group.children.find(m=>!copies.some(c=>c.mesh===m));if(tail){const src=fx.group.children.at(-1);tail.count=src.count;const mat=new T.Matrix4();src.getMatrixAt(0,mat);tail.setMatrixAt(0,mat);}}
  return {group,update,dispose(){fx.dispose();for(const c of copies){c.mesh.geometry.dispose();c.mesh.material.dispose();}group.clear();}};
 }
 return null;
}
