import { mkdir, writeFile } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';

const output = new URL('../../CustomTools/RuleStudio/public/assets/ui/equipment-sockets/', import.meta.url);
await mkdir(output, { recursive: true });
const palettes = { common: [139, 149, 157], magic: [74, 119, 170], rare: [165, 130, 62], legendary: [180, 97, 42], unique: [129, 89, 163], empty: [37, 45, 49] };
const color = (base, target, amount) => '#' + base.map((v, i) => Math.round(v + (target[i] - v) * amount).toString(16).padStart(2, '0')).join('');
const outer = [[6,1],[58,1],[63,6],[63,58],[58,63],[6,63],[1,58],[1,6]];
const inner = [[8,6],[56,6],[58,8],[58,56],[56,58],[8,58],[6,56],[6,8]];
for (const [quality, base] of Object.entries(palettes)) {
  const empty = quality === 'empty', white = [246, 235, 207], black = [6, 10, 14];
  const top = color(base, white, empty ? .16 : .62), side = color(base, white, empty ? .05 : .23);
  const shade = color(base, black, .48), deep = color(base, black, .72);
  const fills = ['url(#top)', side, 'url(#right)', deep, 'url(#bottom)', shade, 'url(#left)', top];
  const planes = outer.map((point, i) => `<polygon points="${[point, outer[(i+1)%8], inner[(i+1)%8], inner[i]].map(p=>p.join(',')).join(' ')}" fill="${fills[i]}"/>`).join('\n');
  const svg = `<svg xmlns="http://www.w3.org/2000/svg" width="64" height="64" viewBox="0 0 64 64">
<defs>
 <linearGradient id="top" x2="0" y2="1"><stop stop-color="${top}"/><stop offset=".5" stop-color="${side}"/><stop offset="1" stop-color="${color(base,black,.08)}"/></linearGradient>
 <linearGradient id="left"><stop stop-color="${side}"/><stop offset="1" stop-color="${color(base,black,.12)}"/></linearGradient>
 <linearGradient id="right"><stop stop-color="${shade}"/><stop offset="1" stop-color="${deep}"/></linearGradient>
 <linearGradient id="bottom" x2="0" y2="1"><stop stop-color="${shade}"/><stop offset="1" stop-color="${deep}"/></linearGradient>
</defs>
<path d="M6 0H58L64 6V58L58 64H6L0 58V6Z M9 8H55L56 9V55L55 56H9L8 55V9Z" fill="#030609" fill-rule="evenodd"/>
${planes}
<path d="M8 6H56L58 8V56L56 58H8L6 56V8Z M9 8H55L56 9V55L55 56H9L8 55V9Z" fill="#05080b" fill-rule="evenodd"/>
<path d="M1.5 6.5 6.5 1.5H57.5L62.5 6.5" fill="none" stroke="${top}" stroke-width="1"/>
<path d="M62.5 7V57.5L57.5 62.5H6.5L1.5 57.5" fill="none" stroke="#05070a" stroke-width="1"/>
<path d="M6.5 55V8L8 6.5H56" fill="none" stroke="#010305" stroke-width="1"/>
<path d="M8 57.5H55.5L57.5 55.5V9" fill="none" stroke="${color(base,white,empty?0:.16)}" stroke-opacity=".7"/>
<path d="M12 2.5H25M34 3.5H49M2.5 16V28" fill="none" stroke="${top}" stroke-opacity=".22" stroke-width=".5"/>
</svg>\n`;
  await writeFile(new URL(`${quality}.svg`, output), svg);
}
console.log(`Exported ${Object.keys(palettes).length} eight-face metal sockets: ${fileURLToPath(output)}`);
