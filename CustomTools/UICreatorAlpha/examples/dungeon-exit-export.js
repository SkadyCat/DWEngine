import {UIRuntime} from '../src/runtime/runtime.js';
import {indexTree,merge} from '../src/core/model.js';
const mode=new URLSearchParams(location.search).get('mode')||'normal';
if(!['normal','ready','travel'].includes(mode))throw Error('Unknown portal state');
const record=await fetch('/api/projects/opus-dungeon-exit').then(r=>r.json()),project=structuredClone(record.document);
const {nodes}=indexTree(project.root);for(const [id,patch]of project.presentation.portalStates[mode])merge(nodes.get(id),patch);
const runtime=new UIRuntime(document.querySelector('#scene'),project,{motion:false,sound:false});
const select=document.querySelector('#mode');select.value=mode;select.onchange=()=>{location.search='?mode='+select.value;};
document.querySelector('#freeze').onclick=async()=>{
 await document.fonts.ready;const job='wbp-portal-'+mode+'-'+crypto.randomUUID();
 const response=await fetch('/api/native-wbp/'+job+'/source',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({project,svg:runtime.renderer.exportSvg(),context:{width:220,height:80,expandedRoot:runtime.expanded.document.root}})});
 const result=await response.json();if(!response.ok)throw Error(JSON.stringify(result));location.href=result.url;
};
