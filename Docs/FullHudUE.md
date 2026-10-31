# 完整 HUD 导入 Opus UE

当前主 HUD 已更新至 revision 48，右侧新增独立的“天赋”入口，“BD”入口补充可见文字并保留真实构筑列表/快捷应用。源哈希、保存资产和实际点击验证见 [RightRailEntries.md](RightRailEntries.md)。

后续更新：主 HUD 已导入 revision 43（45 状态、3060 节点），背包两个标签底色按文字自适应，空内容隐藏。当前冻结包及原生验证见 [ItemBadgeBackground.md](ItemBadgeBackground.md)；下方保留首次完整 HUD 批次的验证记录。

源工程为 UICreator `hud-darksteel-20261024` revision 40，目标为 `G:/DungeonWorldOpus`。本批次保留完整 HUD 的 45 个界面、3008 个逻辑节点及 6159 个控件缓存，包括资源条、统计与 DPS、右侧功能栏、技能与消耗品栏、背包、装备、洗炼、商店、构筑、地图核心、地图装置及结算界面。

| 资源 | 冻结任务 | 源画布 SHA-256 | UE 保存位置 |
| --- | --- | --- | --- |
| 完整 HUD | `wbp-7127f10b-e6dc-439b-b4be-1f146b2ce309` | `d18273b88f14d410d3ed869b9e3150eec5768f64a9f5bdee856f9a8bdd352e4c` | `/Game/UICreator/WBP_CurrentCanvas`、`DA_CurrentCanvas` |
| 临时状态控件 | `wbp-44c19b09-37d1-47a4-a45b-d35816a4ab51` | `34dc742be9132b18dff531d37e67e2930bdd72d934c1fb56e89148a4f3e0cf1a` | `/Game/UICreatorEffects/WBP_CurrentCanvas`、`DA_CurrentCanvas` |

临时状态保留 UICreator 的 48×48 图标、外框、悬停四态和详情模板。明确来源的技能效果复用原技能图标，异常状态使用同源 SVG 导出的 10 种状态图标；外沿倒计时从顶部顺时针减少，右下角显示数字层数。悬停详情显示名称、来源、层数/上限、剩余时间或下次减层时间及说明，说明区域随内容增高，浮层限制在视口内。

`Script/Opus/PlayerEffects.as` 管理显示、移除、计时和悬停；C++ 只提供只读状态快照与通用控件绘制、测量。规则执行、规则语义和数值未在本任务中改变。新增 `LiveStatusSnapshot` / `MaintainedSnapshot` 查询复制状态，原生集成测试通过 `StateHash` 检查查询不产生规则状态修改。UE 运行时没有引入网页 JS。

导入工具：`Tools/UI/Prepare-FullHud.mjs`、`Import-FullHud.ps1`、`Stage-PlayerEffects.mjs`。导入按状态子控件→完整 HUD 的顺序执行；备份、源节点差异、导入回执与日志在 `Saved/Verification/FullHUD/`。UICreator 源工程备份在 `Saved/UIAuthoringBackups/FullHUD-*`。

本批次两个 WBP 已编译、保存，并在独立 UE 进程中重新加载验证。最终 revision 40 完成 45 个界面、3008 个节点、1608 次按钮四态、30 项原生导航及 225 项响应式界面布局检查，最大布局误差 0.0000814 像素。真实爬塔测试通过技能/状态图标资源可见、真实层数、外圈倒计时、原生 hover、暂停冻结、过期移除及只读状态哈希检查；晕眩五组回归与 AS 游戏内导航、背包交易、装配及挑战流程也通过。最终截图为 `Saved/Verification/FullHUD/player-effects.png`，文件和脚本指纹见 `final-verification.json`。

45 个界面及 Designer 共 46 张原生图与冻结网页参考图做了零容差像素对比，平均绝对通道差为 0.2744/255，并非逐像素完全一致；原始差异图及 `UICreator-pixels.json` 均保留，没有用遮罩消除差异。源工具的既有测试结果为 9/12：三处旧断言分别只允许 `equipment-art-` 名称的资产内嵌 PNG（新技能来源示例也使用原技能 PNG）、仍要求独立 DPS 卡片存在（已合入顶部统计），以及重复冻结报错带 EEXIST 代码，详见 `authoring-tests.log`；未修改断言使其冒充通过。

revision 37 的回执保留于 `Saved/Verification/FullHUD/rev37-before-stun/`。早期 DLL/AS 不同步、AS 语法错误、宝库传递依赖缺失，以及测试误用墙钟等待规则时间的失败证据保留于 `attempts/`。最终计时测试按真实规则时钟等待，暂停和过期断言没有放宽。

晕眩条的完整土质填充已由 `Restore-StunHudSource.mjs` 合入 UICreator 源资产 `rs-svg-stun-earth-fill`，沿用既有 SVG；AS 根据真实完整度裁剪。无需再执行修改 UE 资产及源哈希的旧补丁。技能槽绑定也会在空槽装入技能时恢复图标可见性。

导出器已支持界面目录中的新页面，避免地图装置被旧清单拒绝。可选 `cacheJob` 只复用裁剪范围和 SVG 内容 SHA 完全一致的 PNG，所有参考整图仍从本批冻结源重新渲染。`SvgCanvasRoot` 分目录导入支持已恢复，避免子界面覆盖主 HUD。构建采用 `Build-HudModule.ps1` 调用官方 UBT，不修改模块 BuildId；验证进程单独禁用未构建的 `UE_MCP_Bridge` 编辑器插件，项目配置不变。

复验命令：

```powershell
./Tools/UI/Import-FullHud.ps1 -Verify
./Tools/UI/Verify-PlayerEffects.ps1 -Render
./Tools/UI/Verify-CombatLayout.ps1 -BindingsOnly
./Tools/Verify-PlayerStun.ps1
```
