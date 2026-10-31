// Re-export the saved UICreator artwork, including its authored SVG grade filters.
import fs from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { createDocument, createNode, clone, validateDocument } from '../../CustomTools/UICreatorAlpha/src/core/model.js';
import { ProjectRepository } from '../../CustomTools/UICreatorAlpha/server/project-repository.mjs';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'../..');
const main=JSON.parse(await fs.readFile(path.join(root,'UIAuthoring/RuleStudio3D.uialpha.json'),'utf8'));
const doc=createDocument('RuleStudio3D · 通货品级图标');doc.id='opus-native-material-icons';doc.folders=clone(main.folders);doc.viewport={width:168,height:40,background:'transparent'};
const names=['stone-green','stone-blue','stone-purple','stone-gold'];
for(const [index,name]of names.entries()){
 const id='battle-svg-'+name;doc.assets[id]=clone(main.assets[id]);
 doc.root.children.push(createNode('Svg',{id:name,name,svgAsset:id,slot:{offsets:{left:index*42,top:0,right:40,bottom:40}},layout:{width:40,height:40}}));
}
const errors=validateDocument(doc);if(errors.length)throw new Error(errors.join('\n'));
await fs.writeFile(path.join(root,'UIAuthoring/MaterialIcons.uialpha.json'),JSON.stringify(doc,null,2));
const repo=new ProjectRepository(path.join(root,'CustomTools/UICreatorAlpha/data/projects'));const prior=await repo.read(doc.id);await repo.write(doc,prior?.revision||0);
console.log(JSON.stringify({project:doc.id,errors,url:'http://127.0.0.1:5199/?project='+doc.id}));
