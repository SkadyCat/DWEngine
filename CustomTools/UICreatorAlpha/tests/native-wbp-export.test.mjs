import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { saveNativeWbpBundle, freezeNativeWbpSource } from '../server/native-wbp-export.mjs';
import { createServer } from '../server.mjs';
import { gzipSync } from 'node:zlib';

test('compressed export keeps the complete source, page states and raster bytes', async () => {
  const root = await fs.mkdtemp(path.join(os.tmpdir(), 'native-wbp-gzip-'));
  const server = createServer({ dataDir: path.join(root, 'data'), nativeExportRoot: root });
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  const png = Buffer.from('89504e470d0a1a0a00000000', 'hex').toString('base64');
  const bundle = { schema: 'uicreator-alpha/native-wbp', version: 1, sourceHash: 'c'.repeat(64), nodes: [{ id: 'root' }], project: { assets: { art: { source: '<svg><path/></svg>' } } }, states: { canvas: { nodes: [{ id: 'root' }] }, atlas: { nodes: [{ id: 'atlas' }] } }, rasters: { ['d'.repeat(64)]: { png, svg: '<svg/>' } }, references: { canvas: { png, svg: '<svg/>' } } };
  try {
    const response = await fetch(`http://127.0.0.1:${server.address().port}/api/native-wbp/wbp-gzip`, { method: 'POST', headers: { 'Content-Type': 'application/json', 'Content-Encoding': 'gzip' }, body: gzipSync(JSON.stringify(bundle)) });
    assert.equal(response.status, 200);
    const saved = JSON.parse(await fs.readFile(path.join(root, 'exports/wbp-gzip/bundle.json'), 'utf8'));
    assert.deepEqual(saved.project, bundle.project);
    assert.deepEqual(saved.states, bundle.states);
    assert.deepEqual(saved.rasters, bundle.rasters);
    assert.deepEqual(await fs.readFile(path.join(root, `exports/wbp-gzip/rasters/${'d'.repeat(64)}.png`)), Buffer.from(png, 'base64'));
  } finally {
    await new Promise(resolve => server.close(resolve));
    assert.ok(path.resolve(root).startsWith(path.resolve(os.tmpdir()) + path.sep));
    await fs.rm(root, { recursive: true, force: true });
  }
});

test('source snapshot keeps the displayed in-memory document and refuses to overwrite a frozen job', async () => {
  const root = await fs.mkdtemp(path.join(os.tmpdir(), 'native-wbp-source-'));
  const source = { project: { root: { id: 'displayed-root' }, assets: { displayed: { source: '<svg/>' } } },
    svg: '<svg><text>currently displayed</text></svg>', context: { width: 1920, height: 1080 } };
  try {
    const result = await freezeNativeWbpSource(root, 'wbp-source', source);
    assert.match(result.url, /job=wbp-source/);
    const base = path.join(root, 'exports/wbp-source');
    assert.deepEqual(JSON.parse(await fs.readFile(path.join(base, 'project-record.json'), 'utf8')).document, source.project);
    assert.equal(await fs.readFile(path.join(base, 'current-canvas.svg'), 'utf8'), source.svg);
    await assert.rejects(freezeNativeWbpSource(root, 'wbp-source', source), { code: 'EEXIST' });
  } finally { await fs.rm(root, { recursive: true, force: true }); }
});

test('native WBP export freezes references and rejects changed evidence on repeat export', async () => {
  const root = await fs.mkdtemp(path.join(os.tmpdir(), 'native-wbp-'));
  const png = Buffer.from('89504e470d0a1a0a00000000', 'hex').toString('base64');
  const bundle = { schema: 'uicreator-alpha/native-wbp', version: 1, sourceHash: 'a'.repeat(64),
    nodes: [{ id: 'root' }], project: { assets: {} }, states: { canvas: {} }, rasters: {},
    references: { canvas: { png, svg: '<svg/>' } } };
  try {
    await saveNativeWbpBundle(root, 'wbp-contract', bundle);
    await saveNativeWbpBundle(root, 'wbp-contract', bundle);
    const file = path.join(root, 'exports/wbp-contract/reference/canvas.png');
    const before = await fs.readFile(file);
    bundle.references.canvas.png = Buffer.concat([before, Buffer.from([1])]).toString('base64');
    await assert.rejects(saveNativeWbpBundle(root, 'wbp-contract', bundle), /Frozen reference differs/);
    assert.deepEqual(await fs.readFile(file), before);
    await assert.rejects(saveNativeWbpBundle(root, '../escape', bundle), /Invalid export job/);
  } finally { await fs.rm(root, { recursive: true, force: true }); }
});
