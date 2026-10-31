import fs from 'node:fs/promises';
import path from 'node:path';

export async function freezeNativeWbpSource(root, job, source) {
  if (!/^wbp-[a-zA-Z0-9_-]{1,90}$/.test(job)) throw new Error('Invalid export job');
  if (!source.project?.root || !source.svg?.includes('<svg') || !source.context ||
      ![source.context.width, source.context.height].every(n => Number.isInteger(n) && n >= 16 && n <= 8192)) throw new Error('Invalid canvas snapshot');
  const directory = path.join(root, 'exports', job);
  await fs.mkdir(path.join(root, 'exports'), { recursive: true });
  // A producer may reserve a job to freeze its PNG source resources first.
  // Existing snapshots remain immutable; only a resource-only job can be filled.
  try { await fs.mkdir(directory, { recursive: false }); }
  catch (error) { if(error.code !== 'EEXIST') throw error;
    const entries=await fs.readdir(directory);
    if(entries.some(name=>!['source-assets','authoring-project.json'].includes(name)))throw new Error('Source snapshot already frozen');
  }
  const pngRefs=new Set(source.svg.match(/\/exports\/wbp-[a-zA-Z0-9_-]{1,90}\/source-assets\/[a-zA-Z0-9_-]+\.png/g)||[]);
  if(pngRefs.size)await fs.mkdir(path.join(directory,'source-assets'),{recursive:true});
  for(const ref of pngRefs){
    const data=await fs.readFile(path.join(root,ref.slice(1)));
    if(data.subarray(0,8).toString('hex')!=='89504e470d0a1a0a')throw new Error('Frozen resource is not PNG');
    const target=path.join(directory,'source-assets',path.basename(ref));
    try{await fs.writeFile(target,data,{flag:'wx'});}
    catch(error){if(error.code!=='EEXIST'||!(await fs.readFile(target)).equals(data))throw new Error('Frozen PNG resource differs');}
  }
  await Promise.all([
    fs.writeFile(path.join(directory, 'project-record.json'), JSON.stringify({ source: 'current-editor', document: source.project }), 'utf8'),
    fs.writeFile(path.join(directory, 'canvas-context.json'), JSON.stringify(source.context), 'utf8'),
    fs.writeFile(path.join(directory, 'current-canvas.svg'), source.svg, 'utf8')
  ]);
  return { job, url: `/examples/native-wbp-export.html?job=${job}` };
}

export async function saveNativeWbpBundle(root, job, bundle) {
  if (!/^wbp-[a-zA-Z0-9_-]{1,90}$/.test(job)) throw new Error('Invalid export job');
  if (bundle.schema !== 'uicreator-alpha/native-wbp' || bundle.version !== 1 || !Array.isArray(bundle.nodes) || !bundle.states?.canvas || !bundle.project?.assets) throw new Error('Invalid WBP export contract');
  if (!/^[a-f0-9]{64}$/.test(bundle.sourceHash) || bundle.nodes.length > 20000) throw new Error('Invalid source identity');
  const directory = path.join(root, 'exports', job); await fs.mkdir(directory, { recursive: true });
  const references = bundle.references || {}, referenceFiles = {};
  for (const [state, image] of Object.entries(references)) {
    if (!/^[a-z0-9_-]{1,60}$/.test(state)) throw new Error('Invalid state identity');
    const data = Buffer.from(image.png, 'base64');
    if (data.subarray(0, 8).toString('hex') !== '89504e470d0a1a0a') throw new Error('Reference is not PNG');
    const referenceDir = path.join(directory, 'reference'); await fs.mkdir(referenceDir, { recursive: true });
    const file = path.join(referenceDir, `${state}.png`);
    try { await fs.writeFile(file, data, { flag: 'wx' }); }
    catch (e) { if (e.code !== 'EEXIST' || !(await fs.readFile(file)).equals(data)) throw new Error(`Frozen reference differs: ${state}`); }
    referenceFiles[state] = `reference/${state}.png`;
    if (image.svg) await fs.writeFile(path.join(referenceDir, `${state}.svg`), image.svg, 'utf8');
  }
  const output = { ...bundle, references: referenceFiles };
  const rasterDir = path.join(directory, 'rasters'); await fs.mkdir(rasterDir, { recursive: true });
  for (const [key, raster] of Object.entries(bundle.rasters || {})) {
    if (!/^[a-f0-9]{64}$/.test(key)) throw new Error('Invalid raster identity');
    const data = Buffer.from(raster.png, 'base64');
    if (data.subarray(0, 8).toString('hex') !== '89504e470d0a1a0a') throw new Error('Raster is not PNG');
    await fs.writeFile(path.join(rasterDir, `${key}.png`), data);
    if (raster.svg) await fs.writeFile(path.join(rasterDir, `${key}.svg`), raster.svg, 'utf8');
  }
  await fs.writeFile(path.join(directory, 'bundle.json'), JSON.stringify(output), 'utf8');
  return { job, sourceHash: bundle.sourceHash, nodes: bundle.nodes.length, svgAssets: Object.keys(bundle.project.assets).length, rasters: Object.keys(bundle.rasters).length, states: Object.keys(bundle.states), bundle: path.join(directory, 'bundle.json') };
}
