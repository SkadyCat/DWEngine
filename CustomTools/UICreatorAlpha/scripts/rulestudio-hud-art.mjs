import { BATTLE_FUNCTIONS } from '../src/assets/rulestudio-hud/definition.js';
const svg = (body, size = 96) => `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 ${size} ${size}">${body}</svg>`;
const frame = (fill, stroke, radius = 10) => svg(`<rect x=".5" y=".5" width="95" height="95" rx="${radius}" fill="${fill}" stroke="${stroke}"/><path d="M${radius} 1h${96 - 2 * radius}" stroke="white" stroke-opacity=".045"/>`);
export function battleHudSources() {
  const result = {
    'panel.svg': frame('#0c1018ed', '#ffffff14'),
    'panel-gold.svg': frame('#0c1018eb', '#4d4938', 7),
    'window.svg': frame('#101822', '#415064', 9),
    'cell.svg': frame('#101721', '#2a3545', 5),
    'cell-selected.svg': frame('#21364a', '#72bbdc', 5),
    'rail.svg': frame('#0b111be8', '#68583e66', 8),
    'tab.svg': frame('#342f25', '#4d4938', 3),
    'track.svg': svg('<rect width="96" height="96" rx="3" fill="#1b2832"/>'),
    'gem.svg': svg('<defs><linearGradient id="gem" x2="1" y2="1"><stop stop-color="#b9ffe6"/><stop offset=".55" stop-color="#3ddc97"/><stop offset="1" stop-color="#157a5c"/></linearGradient><filter id="glow"><feGaussianBlur stdDeviation="3"/></filter></defs><path d="m48 12 32 36-32 36L16 48Z" fill="#3ddc97" filter="url(#glow)" opacity=".5"/><path d="m48 12 32 36-32 36L16 48Z" fill="url(#gem)"/>'),
    'map-room.svg': svg('<rect width="96" height="96" fill="#070b12"/><path d="M12 10h72v76H12z" fill="#5a6b8a"/><path d="M12 10h72v76H12z" fill="none" stroke="#72809a" stroke-opacity=".18"/><circle cx="48" cy="48" r="5.3" fill="#ffe19a" stroke="#141920" stroke-width="1.7"/>'),
    'portrait.svg': svg('<path d="M48 17a15 15 0 1 0 0 30 15 15 0 0 0 0-30ZM17 83V72a31 26 0 0 1 62 0v11Z" fill="#233446" stroke="#526981" stroke-width="1.4"/><path d="m40 53 8 19 8-19M48 72v11" fill="none" stroke="#b3945b"/>'),
    'frost.svg': svg('<path d="m52 5-25 38 8 20-15 27 49-39-9-17 18-24-27 14Z" fill="#82d5fa" stroke="#d5f6ff" stroke-width="2"/><path d="m52 18-8 32 13 1-24 27" fill="none" stroke="#e5ffff" stroke-width="3"/>'),
    'sentinel.svg': svg('<path d="m48 10 25 13-4 39-21 24-21-24-4-39Z" fill="#263e58" stroke="#91b3d4" stroke-width="3"/><path d="m48 24 12 8-3 23-9 13-9-13-3-23Z" fill="#536e91"/><path d="M48 25v42M36 44h24" stroke="#ccdcf0" stroke-width="3"/>'),
    'potion.svg': svg('<path d="M36 10h24v19L78 62a17 17 0 0 1-15 25H33a17 17 0 0 1-15-25l18-33Z" fill="#183b31" stroke="#7fc9ac" stroke-width="3"/><path d="m27 55-5 13a12 12 0 0 0 11 15h30a12 12 0 0 0 11-15l-5-13Z" fill="#49b593"/><path d="M33 16h30M36 26h24" stroke="#ddc98a" stroke-width="4"/>'),
    'currency.svg': svg('<path d="m48 7 30 24-6 39-24 19-24-19-6-39Z" fill="#342049" stroke="#ae8bcf" stroke-width="3"/><path d="m48 20 15 16-3 27-12 13-12-13-3-27Z" fill="#9c6aba"/><path d="M48 20v56M33 36h30" stroke="#d5b9f0" stroke-width="2"/>'),
    'sword.svg': svg('<path d="m70 9 15 2-2 15-39 39-13-13Z" fill="#7996ad" stroke="#c5d9e6" stroke-width="2"/><path d="m19 48 29 29m-7-21L19 78m-5-3 8 8" stroke="#c7a36c" stroke-width="7" stroke-linecap="round"/>'),
    'picker-panel.svg': frame('#0d1521fa', '#415c75', 10),
    'choice-normal.svg': svg(''),
    'choice-hovered.svg': frame('#23354a', '#4c7598', 6),
    'choice-pressed.svg': frame('#142837', '#638b9e', 6),
    'choice-selected.svg': frame('#1c3344', '#1c3344', 6),
    'selection-filled.svg': svg('<defs><filter id="glow" x="-.4" y="-.4" width="1.8" height="1.8"><feGaussianBlur stdDeviation="2.6"/></filter></defs><rect x="8.2" y="8.2" width="73.6" height="73.6" rx="12.5" fill="none" stroke="#3dc6ed" stroke-width="2" opacity=".7" filter="url(#glow)"/><rect x="8.5" y="8.5" width="73" height="73" rx="12" fill="none" stroke="#56c8f5" stroke-width="1.3"/>', 90),
    'poison.svg': svg('<defs><radialGradient id="bg"><stop stop-color="#1a4811"/><stop offset="1" stop-color="#030b08"/></radialGradient><linearGradient id="glass" x2="1" y2="1"><stop stop-color="#c5ff6a"/><stop offset=".3" stop-color="#5ebd10"/><stop offset=".7" stop-color="#285d08"/><stop offset="1" stop-color="#9def22"/></linearGradient><linearGradient id="liquid" x2="0" y2="1"><stop stop-color="#c6ff24"/><stop offset="1" stop-color="#2e9800"/></linearGradient><filter id="aura" x="-.5" y="-.5" width="2" height="2"><feGaussianBlur stdDeviation="2.2"/></filter></defs><rect width="96" height="96" rx="12" fill="url(#bg)"/><g fill="none" stroke="#86ff0a" stroke-linecap="round"><path d="M24 75C-5 63 7 36 18 40s-4 18 11 14M76 83c26-23 14-36 3-36s13 21-7 19M55 12C28-6 2 8 13 32M72 24c-12-11-8-25 4-19" stroke-width="8" opacity=".35" filter="url(#aura)"/><path d="M24 75C-5 63 7 36 18 40s-4 18 11 14M76 83c26-23 14-36 3-36s13 21-7 19M55 12C28-6 2 8 13 32" stroke-width="3.2"/></g><g transform="rotate(-25 48 49)"><path d="M35 9h26v9l-4 3v14c0 6 16 12 17 26 2 19-11 29-26 29S20 80 22 61c1-14 17-20 17-26V21l-4-3Z" fill="url(#glass)" stroke="#b1ee61" stroke-width="1.6"/><path d="M29 60c12-11 25 7 38-1 6 18-3 25-19 25S24 74 29 60Z" fill="url(#liquid)"/><path d="M35 10h26v8H35Z" fill="#393c1c" stroke="#b9c38a"/><path d="M36 17h24M39 25h18" stroke="#d7f5bd" stroke-width="2"/><path d="M39 42c-14 12-13 25-9 31" fill="none" stroke="#e1ffbc" stroke-width="3.1" opacity=".85"/><path d="m43 58 8-3 7 5-2 10-9 3-7-6Z" fill="#214500"/><path d="m44 60 3 2m6-4-1 5m-6 4 7 1" stroke="#b1fa33" stroke-width="2"/></g><g fill="#d9ff55"><circle cx="13" cy="22" r="2"/><circle cx="82" cy="34" r="2.4"/><circle cx="17" cy="84" r="1.7"/><circle cx="76" cy="12" r="1.2"/></g>'),
  };
  for (const [state, fill, stroke] of [['normal', '#182230', '#38495c'], ['hovered', '#273b4e', '#73a3bf'], ['pressed', '#0e1925', '#9cc6db'], ['disabled', '#131b25', '#28333f']]) result[`button-${state}.svg`] = frame(fill, stroke, 5);
  for (const [state, a, b, stroke] of [['normal', '#303343', '#131b27', '#68583e'], ['hovered', '#514534', '#1d2837', '#d6ac62'], ['pressed', '#28231b', '#131b27', '#ffe1a0'], ['disabled', '#1a1e27', '#3a3b3c']]) {
    result[`function-${state}.svg`] = svg(`<defs><linearGradient id="bg" x2="1" y2="1"><stop stop-color="${a}"/><stop offset=".65" stop-color="${b}"/></linearGradient></defs><rect x=".7" y=".7" width="94.6" height="94.6" rx="7" fill="url(#bg)" stroke="${stroke}" stroke-width="1.4"/>`);
  }
  for (const { id, path } of BATTLE_FUNCTIONS) result[`icon-${id}.svg`] = svg(`<path d="${path}" fill="#bc975415" stroke="#d1b882" stroke-width="1.65" stroke-linejoin="round" stroke-linecap="round"/>`, 24);
  return result;
}
