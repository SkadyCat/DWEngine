import { UIRuntime } from '../src/runtime/runtime.js';
import { BattleHudController } from '../src/runtime/battle-hud.js';
import { createNode } from '../src/core/model.js';
import { materializeSounds } from '../src/assets/ui-sounds.js';
const project = materializeSounds((await (await fetch('/api/projects/hud-darksteel-20261024')).json()).document);
const original = JSON.stringify(project), svg = document.querySelector('#ui');
const runtime = new UIRuntime(svg, project);
const controller = new BattleHudController(runtime, { onStatus: text => document.querySelector('#status').textContent = text });
document.querySelector('#state').onchange = e => { controller.state.close(); if (e.target.value !== 'normal') controller.state.open(e.target.value); controller.refresh(); };
if (new URLSearchParams(location.search).has('verify')) {
  await document.fonts.ready;
  const checks = [], assert = (name, pass) => checks.push({ name, pass: !!pass });
  const nodes = () => [...runtime.nodes.values()];
  const ability = slot => nodes().find(n => n.binding === 'ability' && n.action.value === slot);
  const click = node => { const group = runtime.renderer.groups.get(node.id).group; group.dispatchEvent(new PointerEvent('pointerdown', { bubbles: true, button: 0 })); group.dispatchEvent(new PointerEvent('pointerup', { bubbles: true, button: 0 })); };
  const selected = () => [...runtime.layout.entries.values()].filter(e => e.visible && e.node.id.endsWith('hud-ability-selected'));
  const filled = () => [...runtime.layout.entries.values()].filter(e => e.visible && e.node.id.endsWith('hud-ability-flame'));
  assert('四个实例持有不同动作对象及槽索引', new Set([0, 1, 2, 3].map(i => ability(i).action)).size === 4 && [0, 1, 2, 3].every(i => ability(i)?.action.value === i));
  assert('初始仅第一槽有技能及选中框', selected().length === 1 && filled().length === 1 && selected()[0].node.id.includes('instance-1::'));
  assert('参考列表仅有装备授予的毒瓶，不将完整目录当作已拥有技能', controller.state.choices.length === 1 && controller.state.choices[0].id === 'poison_flask');
  const role = name => nodes().find(n => n.binding === name);
  click(ability(0));
  assert('已装配条目显示图标、来源、当前槽位与唯一已装配标记', controller.state.choiceRows()[0].caption === '装备授予 · 当前槽位' && [...runtime.layout.entries.values()].filter(e => e.visible && e.node.binding === 'choice-equipped').length === 1 && role('choice-icon').svgAsset === 'rs-svg-poison');
  assert('单条技能弹窗为 314×144', runtime.layout.entries.get(role('picker').id).width === 314 && runtime.layout.entries.get(role('picker').id).height === 144);
  window.dispatchEvent(new KeyboardEvent('keydown', { code: 'Escape', bubbles: true }));
  for (let i = 0; i < 4; i++) {
    click(ability(i)); assert(`点击槽 ${i + 1} 仅选中该槽并打开装配`, controller.state.selected === i && controller.state.picker === i && selected().length === 1 && selected()[0].node.id.includes(`instance-${i + 1}::`));
    window.dispatchEvent(new KeyboardEvent('keydown', { code: `Digit${(i + 1) % 4 + 1}`, bubbles: true }));
    assert(`数字键切槽 ${i + 1} 不打开装配及复制图标`, controller.state.picker === -1 && selected().length === 1 && filled().length === 1);
  }
  click(ability(2));
  assert('空槽弹窗显示从槽 1 移入，不显示已装配标记', controller.state.choiceRows()[0].caption === '装备授予 · 从槽 1 移入' && [...runtime.layout.entries.values()].filter(e => e.visible && e.node.binding === 'choice-equipped').length === 0);
  assert('空槽清空按钮禁用', role('picker-clear').interactable === false);
  click(nodes().find(n => n.binding === 'skill-choice' && n.action.value === 'poison_flask'));
  assert('毒瓶移到第三槽，原槽空且图标与选中框各一个', controller.state.skills.join(',') === ',,poison_flask,' && filled().length === 1 && selected().length === 1 && selected()[0].node.id.includes('instance-3::'));
  assert('装备授予标记不随选中复制', [...runtime.layout.entries.values()].filter(e => e.visible && e.node.id.endsWith('hud-ability-equipped')).length === 1);
  controller.state.setAvailability(['poison_flask', 'frost_lance']); controller.refresh();
  click(ability(1)); click(nodes().find(n => n.action?.type === 'assign' && n.action.value === 'frost_lance'));
  assert('新增可用技能自动生成第二条预制体并保持槽位稳定', controller.state.skills.join(',') === ',frost_lance,poison_flask,' && filled().length === 2 && selected().length === 1 && nodes().filter(n => n.binding === 'skill-choice').length === 2);
  click(ability(2)); click(nodes().find(n => n.id.endsWith('rs-clear-skill')));
  assert('清空第三槽不移动第二槽', controller.state.skills.join(',') === ',frost_lance,,' && filled().length === 1 && selected().length === 1);
  controller.state.setAvailability([]); controller.refresh(); click(ability(0));
  assert('没有可用技能时显示空列表提示和禁用清空按钮', runtime.layout.entries.get(role('picker-empty').id)?.visible && !nodes().some(n => n.binding === 'skill-choice') && !role('picker-clear').interactable);
  controller.state.catalog = Array.from({ length: 20 }, (_, i) => ({ id: `test_${i}`, name: `技能 ${i + 1}`, granted: true, icon: 'rs-svg-poison' }));
  controller.state.setAvailability(controller.state.catalog.map(s => s.id)); controller.refresh();
  assert('列表容量不受固定三行限制，20 条独立预制体可滚动', nodes().filter(n => n.binding === 'skill-choice').length === 20 && runtime.layout.entries.get(role('picker').id).height === 280);
  const list = role('picker-list'); runtime.renderer.groups.get(list.id).group.dispatchEvent(new WheelEvent('wheel', { bubbles: true, deltaY: 9999 }));
  assert('滚动在内容边界停止', runtime.nodes.get(list.id).layout.scrollOffset > 0 && runtime.nodes.get(list.id).layout.scrollOffset < 1100);
  const mountsBefore = JSON.stringify([...runtime.mounts]); let listRejected = false;
  try { runtime.setChildren(list.id, [createNode('PrefabInstance', { prefabId: 'missing' })]); } catch { listRejected = true; }
  assert('无效动态预制体被拒绝且保留原列表', listRejected && mountsBefore === JSON.stringify([...runtime.mounts]));
  svg.dispatchEvent(new PointerEvent('pointerdown', { bubbles: true, button: 0 }));
  assert('点击装配区域外关闭面板但保留当前槽', controller.state.picker === -1 && controller.state.selected === 0);
  controller.state = new (controller.state.constructor)(project.presentation); controller.refresh();
  const textProblems = [];
  const checkText = view => {
    for (const e of runtime.layout.entries.values()) {
      if (!e.visible || e.node.type !== 'TextBlock' || !e.node.text) continue;
      const text = runtime.renderer.groups.get(e.node.id)?.visual.querySelector('text'); if (!text) continue;
      const b = text.getBBox(); if (b.x < -1 || b.y < -1 || b.x + b.width > e.width + 1 || b.y + b.height > e.height + 1) textProblems.push(`${view}: ${e.node.name} / ${b.x.toFixed(1)},${b.y.toFixed(1)},${b.width.toFixed(1)},${b.height.toFixed(1)} / ${e.width},${e.height}`);
    }
  };
  click(ability(0)); checkText('picker-current'); click(ability(2)); checkText('picker-move'); controller.state.close(); controller.refresh();
  for (const kind of ['inventory', 'character', 'equipment', 'hunter', 'settings', 'shop', 'craft', 'tooltip', 'completion', 'death']) {
    controller.state.close(); controller.state.open(kind); controller.refresh();
    const panel = ['equipment', 'belt'].includes(kind) ? 'loadout' : kind;
    assert(`${kind} 窗口独立显示`, [...runtime.layout.entries.values()].filter(e => e.node.binding?.startsWith('window-') && e.visible).length === 1 && controller.state.panel === panel);
    checkText(kind);
  }
  assert(`窗口文字处于安全矩形${textProblems.length ? '\n' + [...new Set(textProblems)].join('\n') : ''}`, !textProblems.length);
  controller.state.close(); controller.refresh();
  for (const [w, h] of [[1024, 720], [1280, 720], [1600, 900], [2048, 780]]) {
    runtime.render(w, h); const bar = [...runtime.layout.entries.values()].find(e => e.node.id.endsWith('rs-hotbar'));
    assert(`${w}×${h} 快捷栏底部中央`, Math.abs(bar.globalX + bar.globalWidth / 2 - w / 2) < .01 && bar.globalY + bar.globalHeight === h - 28);
  }
  const patchesBefore = JSON.stringify([...runtime.patches]); let failed = false;
  try { runtime.setMany([[ability(0).id, { visibility: 'collapsed' }], ['missing', { text: 'bad' }]]); } catch { failed = true; }
  assert('批量更新失败时无部分写入', failed && JSON.stringify([...runtime.patches]) === patchesBefore);
  assert('所有交互不修改源工程及预制体', JSON.stringify(project) === original);
  assert('导出的界面无位图或 foreignObject', !/<(?:image|foreignObject)\b/.test(runtime.renderer.exportSvg()));
  controller.state = new (controller.state.constructor)(project.presentation); controller.applied.clear(); runtime.render(1600, 900); controller.refresh();
  const result = document.querySelector('#results'), failures = checks.filter(c => !c.pass);
  result.hidden = false; result.dataset.result = failures.length ? 'failed' : 'passed'; result.textContent = checks.map(c => `${c.pass ? 'PASS' : 'FAIL'} ${c.name}`).join('\n') + `\n\n${checks.length - failures.length}/${checks.length} passed`;
}
