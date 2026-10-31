# 选中与焦点反馈

2026-10-27 用户明确要求：禁止使用生硬、刺眼的白色矩形选中框或浏览器默认白色焦点轮廓。最近相关工作 session 已同步此要求。

- 可视状态使用 UICreator 已有控件的品质色细边、底色、柔和流光，Studio 与 UE 共用设计来源。
- 鼠标点击后的焦点不得再叠一层白框。键盘焦点仍须可辨识，可用与控件配色一致的柔和光晕。
- HTML 按钮包裹装饰性 SVG 时，仅外层按钮进入键盘 Tab 顺序，避免内层 SVG Button 再产生一次焦点与操作。
- 完整 UICreator SVG 场景须隔离全局线性图标的 `stroke: currentColor` 样式。文字、背景及透明命中矩形不能继承图标描边。

本次修复：`CustomTools/RuleStudio/public/atlas-map.css` 隔离地图入场按钮描边并替换地图装置默认焦点轮廓；`atlas-entry-button.mjs` 去除装饰性内层 SVG 的重复 Tab 停靠点。未改动入场条件、地图消耗或规则数值。

浏览器验收：`Saved/Verification/AtlasEntryRendering`，覆盖禁用、地图页签选中、键盘焦点、T3 装图激活、键盘进入及返回当前战斗。

补充修复：不能仅靠单个页面的 `.atlas-enter svg` 补丁。`SvgRenderer` 统一标记 `data-ui-scene`，宿主的线性图标规则必须排除该场景及所有嵌套 SVG；否则资产 SVG 会被图标规则改成 16px，文字、背景和透明命中区也可能继承 2px 描边。渲染器为这些非线条节点显式设置无描边，资产自身的细边和流光保留。

回归证据：`Saved/Verification/AtlasSvgIsolation`，包含移除地图局部补丁后的同款问题复现、修复前后对比，以及完整游戏发布包中的键盘焦点、进入地图与返回战斗验收。
