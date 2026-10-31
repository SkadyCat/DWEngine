# RuleStudio 3D HUD

已同步实际网页：Beta `http://127.0.0.1:5290/`，Opus `http://127.0.0.1:5291/`，进入「3D 战斗」。

- 右侧增加独立星图、重置按钮；BD 按钮直接开关原有构筑列表。
- 左侧删除重复入口。构筑列表改为按需展开的浮层，支持 Esc 关闭。
- 灵石余额移到背包；单体秒伤与战斗统计位于顶部。
- 保留 Opus 的任务、天赋、地图核心等独立改动。

网页实测：右栏打开/关闭 BD，打开/关闭真正的异界星图，重置后同层计时由 0:08 回到 0:00，背包显示实际灵石余额。两份 `battle3d.mjs` 与图标栏模块语法检查通过，规则引擎未修改。

源文件：各工程 `CustomTools/RuleStudio/public/battle3d.mjs`、`battle-functions.mjs`、`play.css`。增量同步脚本为 `Tools/UI/Sync-RuleStudioHud.mjs`，原文件备份在 `Saved/UIAuthoringBackups/RuleStudioHUD-*`。

实际页面截图：`Docs/Proof/RuleStudio3D-HUD.png`、`RuleStudio3D-Opus-HUD.png`、`RuleStudio3D-inventory.png`。

## 玩家临时状态

顶部统计下方按需显示玩家身上的异常状态、临时增益、持续效果和技能维护层数。每项是 48×48 图标，外沿为从顶部开始顺时针减少的倒计时，右下角仅显示数字层数。明确来源的技能效果复用对应技能原图；异常状态使用独立 SVG 图标。悬停或键盘聚焦时外框高亮，浮层显示名称、来源、层数/上限、剩余时间和说明；维护层数显示「下次减层」。浮层限制在视口内，状态消失后自动关闭。计时只读取游戏时钟，暂停时保持不变，重新施加时刷新外圈。

UI 控件源位于 UICreator 工程 `hud-darksteel-20261024` 的 `rs-prefab-player-effect` 和 `rs-prefab-player-effect-tooltip`。revision 36 保存了图标、倒计时外圈和悬停详情的绑定协议；网页使用同源 SVG 皮肤与状态图标，进度由网页运行时绑定。此次未重新导入旧的 UE WBP 导出任务。`battle-player-effects.mjs` 只读取真实规则状态，未改引擎或数值。

实机验证王之宝库技能原图、3 层数字与冰缓图标同时显示，悬停显示 3/10 层、4.5s 下次减层和实际衰减间隔。截图：`Docs/Proof/RuleStudio3D-player-effects-icons-hover.png`。测试使用独立浏览器存档，不改变现有玩家配装。两工程测试共 9 项通过（包含计时减少、暂停保持、刷新重置、自身技能来源和敌方状态归属）；旧 Beta 引擎没有维护层数机制，对应 1 项按能力跳过。日志：`Saved/Verification/PlayerEffects/tests.log`。
