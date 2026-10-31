import fs from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { ProjectRepository } from '../server/project-repository.mjs';
import { validateDocument } from '../src/core/model.js';

// Run before starting the server on a fresh checkout. Existing projects are preserved.
const root = fileURLToPath(new URL('../', import.meta.url));
const manifest = JSON.parse(await fs.readFile(path.join(root, 'docs/baseline/projects.json'), 'utf8'));
const repository = new ProjectRepository(path.join(root, 'data/projects'));
for (const entry of manifest.projects) {
  const document = JSON.parse(await fs.readFile(path.join(root, entry.file), 'utf8'));
  const errors = validateDocument(document);
  if (document.id !== entry.id || errors.length) throw new Error(`${entry.file}: ${errors.join('; ') || 'Project ID mismatch'}`);
  if (process.argv.includes('--check')) { console.log(`Validated ${entry.id}`); continue; }
  if (await repository.pointer(entry.id)) { console.log(`Preserved existing project: ${entry.id}`); continue; }
  await repository.write(document, 0);
  console.log(`Restored ${entry.id}`);
}
