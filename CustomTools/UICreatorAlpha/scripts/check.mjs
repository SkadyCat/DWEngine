import { readdir } from 'node:fs/promises';
import { spawnSync } from 'node:child_process';
async function check(dir) {
  for (const e of await readdir(dir, { withFileTypes: true })) {
    if (['node_modules', 'data', 'work', '.git'].includes(e.name)) continue;
    const file = `${dir}/${e.name}`;
    if (e.isDirectory()) await check(file);
    else if (/\.(m?js)$/.test(e.name)) {
      const result = spawnSync(process.execPath, ['--check', file], { encoding: 'utf8' });
      if (result.status) { console.error(result.stderr); process.exitCode = 1; }
    }
  }
}
await check('.');
if (!process.exitCode) console.log('All JavaScript modules pass syntax checks.');
