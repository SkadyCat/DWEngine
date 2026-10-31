import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { createServer } from '../server.mjs';
import { createDocument } from '../src/core/model.js';
import { starterProject } from '../src/assets/starter.js';

test('project API saves, reloads and rejects stale writes without data loss', async () => {
  const directory = await fs.mkdtemp(path.join(os.tmpdir(), 'uialpha-test-'));
  const server = createServer({ dataDir: directory }); await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  const base = `http://127.0.0.1:${server.address().port}`;
  try {
    const doc = starterProject(); doc.name = 'Persistence test'; const url = `${base}/api/projects/${doc.id}`;
    const put = (revision, document = doc) => fetch(url, { method: 'PUT', headers: { 'Content-Type': 'application/json', 'If-Match': `"${revision}"` }, body: JSON.stringify(document) });
    assert.equal((await fetch(`${base}/api/health`)).status, 200); assert.equal((await fetch(base)).status, 200);
    assert.equal((await put(0)).status, 200); assert.equal((await fetch(url).then(r => r.json())).document.name, 'Persistence test');
    const responses = await Promise.all([put(1, { ...doc, name: 'Window A' }), put(1, { ...doc, name: 'Window B' })]);
    assert.deepEqual(responses.map(r => r.status).sort(), [200, 409]);
    const record = await fetch(url).then(r => r.json()); assert.equal(record.revision, 2);
    assert.equal(Object.keys(record.document.assets).length, 13); assert.equal(Object.keys(record.document.prefabs).length, 3); assert.deepEqual(record.document.folders, doc.folders);
    const bundleRoot = path.join(directory, doc.id), pointer = JSON.parse(await fs.readFile(path.join(bundleRoot, 'project.json'), 'utf8'));
    const manifest = JSON.parse(await fs.readFile(path.join(bundleRoot, pointer.manifest), 'utf8'));
    const first = Object.keys(doc.assets)[0]; assert.ok(manifest.assets[first].sourceFile.endsWith('.svg')); assert.equal(await fs.readFile(path.join(bundleRoot, manifest.assets[first].sourceFile), 'utf8'), doc.assets[first].source);
    assert.equal((await fs.readdir(path.join(bundleRoot, 'assets', first))).length, 1, 'unchanged SVG source is reused across revisions');
    assert.equal((await put(0)).status, 409); assert.equal((await fetch(`${base}/api/projects`).then(r => r.json())).projects.length, 1);
    assert.equal((await fetch(`${base}/server.mjs`)).status, 404); assert.equal((await fetch(`${base}/data/projects/${doc.id}.json`)).status, 404);
    assert.equal((await fetch(url, { method: 'PUT', headers: { Origin: 'https://example.com' }, body: '{}' })).status, 403);
    assert.equal((await put(2, { ...doc, id: 'another' })).status, 400);
  } finally {
    await new Promise(resolve => server.close(resolve));
    // Only the exact temporary test directory created above is removed.
    assert.ok(path.resolve(directory).startsWith(path.resolve(os.tmpdir()) + path.sep));
    await fs.rm(directory, { recursive: true, force: true });
  }
});
