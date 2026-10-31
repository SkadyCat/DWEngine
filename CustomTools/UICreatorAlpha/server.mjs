import http from 'node:http';
import fs from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { createGunzip } from 'node:zlib';
import { validateDocument } from './src/core/model.js';
import { ProjectRepository } from './server/project-repository.mjs';
import { saveNativeWbpBundle, freezeNativeWbpSource } from './server/native-wbp-export.mjs';

const ROOT = path.dirname(fileURLToPath(import.meta.url));
const MIME = { '.html': 'text/html; charset=utf-8', '.js': 'text/javascript; charset=utf-8', '.css': 'text/css; charset=utf-8', '.svg': 'image/svg+xml', '.json': 'application/json; charset=utf-8', '.png': 'image/png', '.wav': 'audio/wav', '.woff2': 'font/woff2' };
const MAX_BODY = 16 * 1024 * 1024;

export function createServer({ dataDir = path.join(ROOT, 'data/projects'), nativeExportRoot = ROOT } = {}) {
  const writes = new Map();
  const repository = new ProjectRepository(dataDir);
  const reply = (res, status, value, headers = {}) => {
    res.writeHead(status, { 'Content-Type': 'application/json; charset=utf-8', 'Cache-Control': 'no-store', ...headers });
    res.end(JSON.stringify(value));
  };
  const readRecord = id => repository.read(id);
  const server = http.createServer(async (req, res) => {
    try {
      // This is a loopback authoring service, never a proxy into the UE project.
      const host = req.headers.host || '';
      if (!/^(127\.0\.0\.1|localhost|\[::1\])(?::\d+)?$/.test(host)) return reply(res, 403, { error: 'Only loopback hosts are allowed.' });
      if (req.headers.origin && ![`http://${host}`, `https://${host}`].includes(req.headers.origin)) return reply(res, 403, { error: 'Cross-origin requests are disabled.' });
      const url = new URL(req.url, `http://${host}`);
      if (url.pathname === '/api/health') return reply(res, 200, { ok: true, app: 'UICreator Alpha', version: '0.1.0', renderer: 'native-svg', toolRoot: ROOT });
      if (url.pathname === '/api/projects' && req.method === 'GET') {
        return reply(res, 200, { projects: await repository.list() });
      }
      const project = url.pathname.match(/^\/api\/projects\/([a-zA-Z0-9_-]{1,100})$/);
      if (project) {
        const id = project[1];
        if (req.method === 'GET') {
          const record = await readRecord(id);
          return record ? reply(res, 200, record, { ETag: `"${record.revision}"` }) : reply(res, 404, { error: 'Project not found.' });
        }
        if (req.method === 'PUT') {
          let bytes = 0;
          const chunks = [];
          for await (const chunk of req) {
            bytes += chunk.length;
            if (bytes > MAX_BODY) { reply(res, 413, { error: 'Project exceeds 16 MB.' }); return; }
            chunks.push(chunk);
          }
          let document;
          try { document = JSON.parse(Buffer.concat(chunks).toString('utf8')); }
          catch { return reply(res, 400, { error: 'Invalid JSON.' }); }
          if (document.id !== id) return reply(res, 400, { error: 'Project identity does not match the URL.' });
          const errors = validateDocument(document);
          if (errors.length) return reply(res, 422, { error: errors.slice(0, 8).join('\n') });
          // Serialize optimistic writes per project, including concurrent first saves.
          const previous = writes.get(id) || Promise.resolve();
          const write = previous.catch(() => {}).then(async () => {
            const existing = await readRecord(id);
            const expected = req.headers['if-match'];
            if ((existing && expected !== `"${existing.revision}"`) || (!existing && expected && expected !== '"0"')) {
              reply(res, 409, { error: '文件已被另一个窗口修改。请另存副本或重新打开，避免覆盖。', revision: existing?.revision || 0 }); return;
            }
            const record = await repository.write(document, existing?.revision || 0);
            reply(res, 200, record, { ETag: `"${record.revision}"` });
          });
          writes.set(id, write);
          try { await write; } finally { if (writes.get(id) === write) writes.delete(id); }
          return;
        }
        return reply(res, 405, { error: 'Method not allowed.' });
      }
      const nativeExport = url.pathname.match(/^\/api\/native-wbp\/(wbp-[a-zA-Z0-9_-]{1,90})(\/source)?$/);
      if (nativeExport && req.method === 'POST') {
        const encoding = req.headers['content-encoding'];
        if (encoding && encoding !== 'gzip') return reply(res, 415, { error: 'Unsupported export encoding' });
        const input = encoding === 'gzip' ? req.pipe(createGunzip()) : req;
        const chunks = []; let bytes = 0;
        for await (const chunk of input) { bytes += chunk.length; if (bytes > 512 * 1024 * 1024) return reply(res, 413, { error: 'WBP bundle exceeds 512 MB' }); chunks.push(chunk); }
        const bundle = JSON.parse(Buffer.concat(chunks).toString('utf8'));
        return reply(res, 200, nativeExport[2] ? await freezeNativeWbpSource(nativeExportRoot, nativeExport[1], bundle) : await saveNativeWbpBundle(nativeExportRoot, nativeExport[1], bundle));
      }
      if (!['GET', 'HEAD'].includes(req.method)) return reply(res, 405, { error: 'Method not allowed.' });
      // Only explicit frontend roots are public; project data and source tooling are private.
      const pathname = decodeURIComponent(url.pathname);
      if (pathname !== '/' && !/^\/(src|examples)\//.test(pathname) && !/^\/exports\/wbp-[a-zA-Z0-9_-]+\//.test(pathname) && pathname !== '/index.html') return reply(res, 404, { error: 'Not found.' });
      const file = path.resolve(ROOT, `.${pathname === '/' ? '/index.html' : pathname}`);
      if (!file.startsWith(`${ROOT}${path.sep}`) || !MIME[path.extname(file)]) return reply(res, 404, { error: 'Not found.' });
      const body = await fs.readFile(file);
      res.writeHead(200, { 'Content-Type': MIME[path.extname(file)], 'Cache-Control': 'no-cache', 'X-Content-Type-Options': 'nosniff' });
      res.end(req.method === 'HEAD' ? undefined : body);
    } catch (error) {
      if (!res.headersSent) reply(res, error.code === 'ENOENT' ? 404 : 500, { error: error.code === 'ENOENT' ? 'Not found.' : error.message });
      else res.end();
    }
  });
  return server;
}

if (process.argv[1] && import.meta.url === pathToFileURL(path.resolve(process.argv[1])).href) {
  const port = Number(process.env.UI_ALPHA_PORT || 5190);
  const server = createServer();
  server.listen(port, '127.0.0.1', () => console.log(`UICreator Alpha · http://127.0.0.1:${port}`));
  server.on('error', error => { console.error(error.message); process.exitCode = 1; });
}
