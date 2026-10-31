# 地图核心装配 · UICreator 同步

完整工程为 `hud-darksteel-20261024`（HUD · RuleStudio 3D 战斗）。地图栏中新增已装配核心卡片；点击在左侧展开 T1–T10 核心列表，选择后更新卡片、地图 T 阶和三条词条，关闭列表。该工程的预览状态只演示界面，规则计算仍由 RuleStudio 原引擎处理。

- 规则数据：`Content/Rules/Equipment/map_core_equipment*.json`，怪物群加成 0%～900%。
- 独立控件源：`UIAuthoring/MapCoreSelector.uialpha.json`。
- 完整 HUD 快照：`UIAuthoring/RuleStudio3D.uialpha.json`。
- 完整工程同时保存在 `CustomTools/UICreatorAlpha/data/projects` 与 `UIAuthoring/projects`；旧修订可恢复。
- 地图核心列表为独立预制体 `rs-prefab-map-core-picker`，图标、按钮底图、文本和动作均可分别编辑。

重新同步：在 Opus 根目录依次执行 `node Tools/UI/Export-MapCoreSelector.mjs`、`node Tools/UI/Sync-MapCoreUICreator.mjs`。同步保留其他 HUD 控件及用户编辑，并在 `Saved/UIAuthoringBackups` 备份原工程。

验证：UICreator 的 `npm run check` 和 `npm test`；新增检查覆盖十核心目录、实际装配状态、重置保留、无效 ID、四种桌面尺寸及重复同步不生成重复控件。浏览器中另检查打开、选择 T10、关闭和文字安全区。原生导出流程已增加 `map-core-picker` 状态，后续已接入 Opus UE，详见 [UE 接入记录](MapCoreUE.md)。
