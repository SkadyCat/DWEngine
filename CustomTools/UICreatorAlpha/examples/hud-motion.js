import { UIRuntime } from '../src/runtime/runtime.js';
import { attachPresentation } from '../src/runtime/battle-hud.js';
import { createNode } from '../src/core/model.js';
import { materializeSounds } from '../src/assets/ui-sounds.js';

const $ = selector => document.querySelector(selector);
try {
  const response = await fetch('/exports/wbp-hud-native-20261025/project-record.json');
  if (!response.ok) throw new Error('无法读取 HUD SVG 工程');
  const { document: project } = await response.json();
  materializeSounds(project);
  if (!project.assets['rs-svg-selection-filled'] && project.assets['hud-svg-selection']) {
    project.assets['rs-svg-selection-filled'] = { ...structuredClone(project.assets['hud-svg-selection']), id: 'rs-svg-selection-filled' };
  }
  project.viewport = { width: 960, height: 220, background: '#090c12' };
  project.root = createNode('CanvasPanel', { id: 'motion-scene', children: [createNode('PrefabInstance', {
    id: 'motion-hotbar', prefabId: 'rs-prefab-hotbar',
    slot: { anchors: { minX: .5, maxX: .5, minY: .5, maxY: .5 }, alignment: { x: .5, y: .5 }, offsets: { left: 0, top: 0, right: 606, bottom: 74 } },
  })] });
  const runtime = new UIRuntime($('#hud'), project);
  runtime.audioPlayer.volume = .8;
  const soundStatus = document.createElement('p'); soundStatus.id = 'sound-status'; soundStatus.setAttribute('role', 'status'); soundStatus.textContent = '点击“开启音效”，体验悬停、确认和 1–4 切换音效。'; $('#status').after(soundStatus);
  let soundCount = 0;
  runtime.addEventListener('sound', ({ detail }) => { soundStatus.textContent = `♪ ${detail.name} · ${detail.event} · ${Math.round(detail.duration * 1000)} ms`; soundStatus.dataset.playCount = String(++soundCount); soundStatus.dataset.lastEvent = detail.event; soundStatus.dataset.contextState = runtime.audioPlayer.context.state; $('#sound-toggle').textContent = '♪ 音效开'; $('#sound-toggle').setAttribute('aria-pressed', 'true'); });
  $('#sound-toggle').onclick = async () => { const audio = runtime.audioPlayer; if (!audio.muted && audio.context?.state === 'running') { audio.setMuted(true); $('#sound-toggle').textContent = '♪ 音效关'; $('#sound-toggle').setAttribute('aria-pressed', 'false'); soundStatus.textContent = '已静音'; return; } audio.setMuted(false); const ready = await audio.unlock(); $('#sound-toggle').textContent = ready ? '♪ 音效开' : '♪ 点击重试'; $('#sound-toggle').setAttribute('aria-pressed', String(ready)); soundStatus.textContent = ready ? '音效已开启 · 移过技能槽、点击或按 1–4 试听。' : '浏览器尚未允许播放，请再次点击。'; };
  const controller = attachPresentation(runtime, { onStatus: text => { $('#status').textContent = text; } });
  $('#motion').onchange = event => { runtime.motion = event.target.checked; runtime.paint(); $('#status').textContent = runtime.motion ? '动效已开启' : '动效已关闭 · 四态 SVG 仍可切换'; };
  $('#status').textContent = '动效已开启 · 点击技能槽或按 1–4 切换选中态';
  window.addEventListener('pagehide', () => { controller?.destroy(); runtime.destroy(); }, { once: true });
} catch (error) { $('#status').textContent = error.message; }
