# RuleStudio3D 战斗 HUD 整理

UICreator 工程：`hud-darksteel-20261024`，revision 33。
冻结导出：`wbp-35ee4933-2fb3-4b4a-8853-2fdd2eb32331`。
源画布 SHA-256：`cf34b435bbbcb059ba7aaae8ed2896429d150fbe52a86aa880f27c41656bcb4d`。

按标注整理战斗区：灵石显示移入背包标题栏；单体秒伤并入顶部统计条；右侧地图上移。左侧移除星图、构筑与重置控件。资源条、眩晕条、召唤单位和技能栏保留。

右栏采用直接按钮：

| 按钮 | 操作 |
| --- | --- |
| 构筑（层叠图标） | 直接打开已有构筑选择页 |
| 异界星图（齿轮，M） | 打开原星图入口对应的原生挑战设置 |
| 重置（底部回转箭头） | 调用 AS `Tower.Retry(true)`，重开并立即开始当前层 |
| 装备洗炼 | 保留原洗炼入口 |

UI 布局与按钮动作由 `src/assets/rulestudio-hud/combat-layout.js` 安装到 UICreator 源文档；`Sync-CombatLayout.mjs` 从最新工程增量更新，并同步项目仓库、`UIAuthoring/RuleStudio3D.uialpha.json` 和 Screen 镜像。不会用旧工厂全量重建来覆盖地图核心或资源组件。生产运行仍由 AS / 原生规则引擎驱动。

导出包包含 44 个交互状态、2707 个逻辑节点；冻结包内已无左侧旧按钮、HUD 钱包和独立秒伤卡片实例。

验证与证据：

- `Tools/UI/Import-CombatLayout.ps1 -Job wbp-35ee4933-2fb3-4b4a-8853-2fdd2eb32331`
- 同命令加 `-Verify`，独立进程重新加载保存资产，核对源版本、节点、父子关系、交互和响应布局。
- `Tools/UI/Verify-CombatLayout.ps1` 验证原生按钮导航、第 3 层重置、初始准备状态，并输出真实 3D 场景截图。
- 回执与日志：`Saved/Verification/CombatLayout/`。
- UICreator 实际预览：`Docs/Proof/CombatLayout-UICreator.png`。

2026-10-25 原生验证完成：独立进程重载、2707 节点层级一致、44 个界面、220 个响应界面布局检查通过；原生按钮导航、第三层重开、初始准备状态通过。实际 3D 与背包截图位于 `Saved/Verification/CombatLayout/selected-1.png`、`inventory.png`。像素级一致性未单独测量。

实际 RuleStudio 网页另见 `Docs/RuleStudio3DHud.md`。
