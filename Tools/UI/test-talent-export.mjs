import {test} from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import path from 'node:path';
import {freezeNativeWbpSource} from '../../CustomTools/UICreatorAlpha/server/native-wbp-export.mjs';
import {safeImageUrl} from '../../CustomTools/UICreatorAlpha/src/assets/svg.js';
const proof=new URL('../../Saved/Verification/Talents/',import.meta.url);await fs.mkdir(proof,{recursive:true});
test('local raster links only allow frozen PNG resources',()=>{assert.ok(safeImageUrl('/exports/wbp-icons/source-assets/sheet-01.png'));for(const value of ['https://example.com/a.png','/exports/wbp-x/source-assets/../../secret.png','/api/project.json','file:///C:/secret.png'])assert.equal(safeImageUrl(value),'');});
test('reserved atlas job freezes once and rejects source replacement',async()=>{const root=await fs.mkdtemp(path.join(proof.pathname.replace(/^\/([A-Za-z]:)/,'$1'),'freeze-'));const dir=path.join(root,'exports/wbp-source/source-assets');await fs.mkdir(dir,{recursive:true});const png=Buffer.from('89504e470d0a1a0a','hex');await fs.writeFile(path.join(dir,'sheet.png'),png);const source={project:{root:{id:'root'}},svg:'<svg><image href="/exports/wbp-source/source-assets/sheet.png"/></svg>',context:{width:1600,height:900}};await freezeNativeWbpSource(root,'wbp-source',source);await assert.rejects(freezeNativeWbpSource(root,'wbp-source',source),/already frozen/);await freezeNativeWbpSource(root,'wbp-next',source);assert.deepEqual(await fs.readFile(path.join(root,'exports/wbp-next/source-assets/sheet.png')),png);});
