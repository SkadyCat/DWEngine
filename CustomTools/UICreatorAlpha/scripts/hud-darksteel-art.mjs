// Vector source definitions for the supplied 688×170 HUD crop.
// Text, key labels and selected state remain separate UI nodes.
const svg = (w, h, body) => `<svg xmlns="http://www.w3.org/2000/svg" width="${w}" height="${h}" viewBox="0 0 ${w} ${h}">${body}</svg>\n`;

export function hudVectorSources(flameGlyph) {
  const sources = {
    'frame-left.svg': svg(184, 170, `<defs><linearGradient id="steel" x1="0" y1="0" x2="1" y2="1"><stop stop-color="#424d65"/><stop offset="1" stop-color="#3e485f"/></linearGradient></defs><path d="M0 0H88L80 47H31L39 0Z" fill="#242b38"/><path d="M88 0H98L91 48H81Z" fill="#070b13"/><path d="M98 0H176L171 49H90Z" fill="url(#steel)"/><path d="M176 0H184L178 50 171 65V49Z" fill="#0b0f18"/><path d="M11 47H92L76 170H0V112Z" fill="url(#steel)"/><path d="M6 51H11L0 112V89Z" fill="#090e17"/><path d="M91 48L98 49 81 170H76Z" fill="#080d16"/><path d="M12 47H91" stroke="#59637a" stroke-opacity=".13"/>`),
    'frame-right.svg': svg(144, 170, `<defs><linearGradient id="steel" x1="0" y1="0" x2="1" y2="1"><stop stop-color="#424d66"/><stop offset="1" stop-color="#3e485f"/></linearGradient></defs><path d="M3 0H13L20 49 3 65 11 48Z" fill="#060a12"/><path d="M13 0H91L100 48 20 49Z" fill="url(#steel)"/><path d="M91 0H144V47H99Z" fill="#222938"/><path d="M20 49 99 48 85 64H3Z" fill="#0a0e17"/><path d="M99 47H144V132H114Z" fill="url(#steel)"/><path d="M98 48 114 132H105L86 64Z" fill="#090e18"/><path d="M100 48H144" stroke="#59667f" stroke-opacity=".15"/>`),
    'backplate.svg': svg(608, 106, `<defs><linearGradient id="base" x2="0" y2="1"><stop stop-color="#0b0f16"/><stop offset="1" stop-color="#0c0f17"/></linearGradient></defs><path d="M27 0H549L567 84 550 106H0Z" fill="url(#base)"/>`),
    'status-ring.svg': svg(28, 14, `<path d="M3.1 0a10.7 10.7 0 0 0 21.4 0" fill="none" stroke="#7b838a" stroke-width="2.2"/>`),
    'gold-pin.svg': svg(10, 12, `<path d="M1 0H8.5V6.7L4.7 9.6 0 6.9V1.2Z" fill="#f9b969"/><path d="M1 0H8.5L8 2.1 1 2.4Z" fill="#ffc77a"/>`),
    'plus.svg': svg(12, 12, `<path d="M6 1.8v8.4M1.8 6h8.4" fill="none" stroke="#8e9aad" stroke-width="1.5" stroke-linecap="square"/>`),
    'selection.svg': svg(90, 90, `<defs><filter id="glow" x="-35%" y="-35%" width="170%" height="170%"><feGaussianBlur stdDeviation="2.3"/></filter></defs><rect x="8.3" y="8.3" width="73.4" height="73.4" rx="13" fill="none" stroke="#25bce9" stroke-width="1.7" opacity=".7" filter="url(#glow)"/><rect x="8.5" y="8.5" width="73" height="73" rx="12.5" fill="none" stroke="#56d8fd" stroke-width="1.3"/><rect x="9.1" y="9.1" width="71.8" height="71.8" rx="12" fill="none" stroke="#c0f3ff" stroke-width=".85" stroke-dasharray="2.4 3.3"/>`),
    'flame-glyph.svg': flameGlyph,
  };
  for (const family of ['skill-filled', 'skill-empty', 'consumable']) {
    const small = family === 'consumable', dashed = family === 'skill-empty', w = small ? 64 : 74, h = w;
    for (const state of ['normal', 'hovered', 'pressed', 'disabled']) {
      const stroke = ({ normal: '#292e39', hovered: '#59616d', pressed: '#7c8795', disabled: '#222733' })[state];
      const fill = ({ normal: '#0c0f17', hovered: '#131923', pressed: '#080b11', disabled: '#0b0e15' })[state];
      const opacity = state === 'disabled' ? '.58' : '1';
      const glow = '';
      sources[`${family}-${state}.svg`] = svg(w, h, `${glow}<rect x=".5" y=".5" width="${w - 1}" height="${h - 1}" rx="12" fill="${fill}" fill-opacity="${opacity}" stroke="${stroke}" stroke-opacity="${dashed && state === 'normal' ? '.85' : '1'}" stroke-width="1"${dashed ? ' stroke-dasharray="2.2 3"' : ''}/>`);
    }
  }
  return sources;
}
