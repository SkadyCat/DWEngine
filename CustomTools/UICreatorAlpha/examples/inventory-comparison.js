import { UIRuntime } from '../src/runtime/runtime.js';
import { attachPresentation } from '../src/runtime/battle-hud.js';
const resize=()=>{const k=Math.min(1,(document.documentElement.clientWidth-68)/1300);document.documentElement.style.setProperty('--k',k);document.documentElement.style.setProperty('--figureHeight',`${448*k+30}px`);};resize();window.addEventListener('resize',resize);
const record=await(await fetch('/api/projects/hud-darksteel-20261024')).json(),p=record.document,prefab=p.prefabs['battle-prefab-inventory'];
const view={...p,root:prefab.root,viewport:{width:prefab.width,height:prefab.height,background:'transparent'},presentation:{...p.presentation,interfaceId:'inventory'}};
const runtime=new UIRuntime(document.querySelector('#ui'),view);attachPresentation(runtime);
const problems=[];for(const [id,r]of runtime.renderer.groups){const entry=runtime.layout.entries.get(id);if(!entry?.visible)continue;for(const element of r.visual.querySelectorAll('text')){const box=element.getBBox();if(box.x<-.8||box.y<-.8||box.x+box.width>entry.width+.8||box.y+box.height>entry.height+.8)problems.push(entry.node.name);}}
const entries=[...runtime.layout.entries.values()],cells=entries.filter(e=>e.node.binding==='item-cell'),grid=entries.find(e=>e.node.binding==='bag-grid');
const output=document.querySelector('#metrics');output.textContent=JSON.stringify({revision:record.revision,window:[runtime.layout.width,runtime.layout.height],grid:[grid.globalX,grid.globalY,grid.width,grid.height],cells:cells.length,items:cells.filter(e=>e.node.interactable).length,vectorOnly:!/<(?:image|foreignObject)\b/.test(runtime.renderer.exportSvg()),textProblems:problems},null,2);output.dataset.ready='true';
resize();
