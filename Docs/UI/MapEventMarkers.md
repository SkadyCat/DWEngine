# 地图特殊事件标记

Studio 世界地图、地牢小地图与展开地图共用 UICreator 的五类事件图标：商店、百炼法坛、万相合成祭坛、遗珍神龛、封印宝库。世界地图用事件图标替换普通地域符号，保留当前玩家定位；区域详情显示事件名称。地牢标记跟随真实地图坐标投影、平移、缩放与圆形裁剪。完成后降低图标亮度并添加勾记号。

## 作者源与数据

- `CustomTools/UICreatorAlpha/src/assets/rulestudio-hud/map-event-markers.js`：共用图形、正常/完成状态及 UICreator 控件树。
- `UIAuthoring/MapEventMarkers.uialpha.json`：已保存的 `opus-map-event-markers` 工程；作者脚本 `Tools/UI/Author-MapEventMarkers.mjs` 写入并冷读验证。
- `CustomTools/UICreatorAlpha/src/assets/rulestudio-hud/dungeon-map.js`：小地图、展开地图和九项标记图例；独立工程 `opus-dungeon-map`。
- `CustomTools/RuleStudio/public/data/map-events-config.mjs`：作者脚本从 `Content/MapEvents/events.json` 生成，保留原有 75%、1–2 个事件、权重和最低层级。
- `map-event-locations.mjs`：补齐 Studio 的区域事件描述和可达位置；独立随机流，不推进战斗、装备或掉落随机流。地图种子相同则预告和进入后的事件相同。
- `DungeonBattle.mapEvents` → `hud.dungeon.events`：当前地图实际描述；商店坐标来自现有 `outpostMerchantLocation`。右侧既有奇遇界面的完成状态按类型同步到当前地图描述。

地图标识与场景共用 `DungeonBattle.mapEvents`。2026-10-28 补齐原先遗漏的场景实体：百炼法坛、万相合成祭坛、遗珍神龛和封印宝库，各自在实际描述坐标创建模型；完成后保留实体、减弱光效，离开地图时清理。交互气泡复用现有 UICreator 商人预制体的正常、悬停与按下样式，通过事件 ID 打开对应既有奇遇界面。原有右侧入口、奖励事务与装备制作规则继续使用；本次没有新增守库者战斗。事件描述只在当前地图实例中保存，沿用原奇遇界面换图后重建的生命周期。

场景实现位于 `CustomTools/RuleStudio/public/prefabs/dungeon-map-events.mjs`，气泡绑定位于 `battle-map-event-tag.mjs`。`Tools/UI/Verify-StudioMapEventWorld.mjs` 使用独立浏览器资料，在真实生成的百炼与合成祭坛坐标检查实体、UICreator 气泡点击、对应界面、完成状态与旧标记清理。证据位于 `Saved/Verification/StudioMapEventWorld`；22 项相关测试及 8 项浏览器检查通过。

## UE 汇总接入边界

本轮只修改 Studio 与 UICreator 源，没有修改 AS/C++、导入 WBP、编译或重启 UE。UE 汇总时应使用 `Tower.Events.States` 中已经生成的 `Definition.Kind`、`Position`、`Spent` 和真实商人位置驱动这些图标；不要在 UE 另行运行 Studio 的区域随机规划。两端地图几何与随机实现不同，不能声称事件坐标或随机结果已经跨端一致。

## 验证

`node --test tests/map-event-markers.test.mjs tests/dungeon-map.test.mjs tests/map-events.test.mjs`：20 项通过，覆盖作者配置、图标状态、地图投影、T1/T3/T10/T16 世界预告与地牢事件一致、可达性、隐藏区域、制作与合成事务。

`Tools/UI/Verify-MapEventMarkers.mjs`：独立 5354 回环源加载实际地图组件，检查五类世界图标、展开/小地图同组事件、缩放跟随、完成状态，页面脚本错误为零。由于本次 CUA 内核启动失败，截图使用捆绑 Playwright 和独立无头 Edge 上下文取得；未连接或修改用户浏览器资料。

实际游戏入口另通过首页 → 3D 战斗 → 地牢地图 → M 世界地图验证：小地图与展开地图事件一致，当前世界区域显示同组商店/奇遇，页面脚本错误为零。证据位于 `Saved/Verification/MapEventMarkers`，包括 `game-receipt.json`、`game-local.png`、`game-world.png`。
