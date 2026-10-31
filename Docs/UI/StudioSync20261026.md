# Studio 最近增强同步到 UE

最近会话的差异清单位于同目录 `StudioSync20261026.json`。本次新增交互和玩法使用 AngelScript；原生代码只提供存档、物品事务、几何和 UICreator 控件适配。没有把网页 JS 作为 UE 运行时引入，没有修改 DWRules 引擎或既有战斗公式。

| 内容 | UE 接入结果 |
| --- | --- |
| 拓荒商人 | 初始区域使用 Studio 原始摊位模型。T1/T2/T3 普通地图无限供应，价格 20/40/60 灵石。6 件 T1–T3 传奇装备每 30 分钟刷新，购买状态保存；重载、时间倒退不会补货。 |
| 商人交互 | 整块气泡可点击；左键选中并保留青玉/暖金描边，右键购买。UICreator 窗框、关闭按钮、四态画面与商品浮层已接入。Ctrl 固定浮层；Esc 先关闭浮层；右键不会穿透固定浮层购买。 |
| 特质装备 | 右下角“特质”打开 UICreator 列表。点击生成独立 UID 的 T1 魔法“霜爆法杖”，绑定冰锥术和消耗冰缓爆破；进入真实地面掉落、拾取与背包流程。 |
| 异常消耗反应 | 将冰缓爆破、感电闪击、冻结碎片及其 T1–T10 数值原样同步到正式加载的 `Content/RulesTower`。其余已有特质数值保留。 |
| 地牢小地图 | 圆形视野、实时原生房间和通道、玩家朝向、入口/首领/出口标记。点击或 Tab 展开；滚轮/加减缩放，拖动，方向键移动，F 定位，0 全图，Esc 返回；边界阻力与 260 ms 回弹。 |
| 首领与出口 | 首领按实际导航路径长度放在最深可达房间。首领死亡时在死亡位置生成 Studio 原始传送阵；远程点击后自动寻路，到达才打开当前区域星图。普通怪物仍可继续战斗。 |
| 地图票 | 中央槽有地图时按实际 T 级消耗 1 张进入。缺少地图或操作失败不扣；当前战场返回不扣。初始区域无地图时可免费进入 T1、可重访。N/通关按钮打开世界地图，不直接绕过地图票跳层。 |
| 物品浮层 | 复用已保存的 UICreator 浮层，去除冗余词条计数和重复洗炼 T 范围提示。修正首次显示时纹理未准备完导致的透明面板；商品装备图标复用现有去黑底资源。 |

双背包、装备右键菜单、祭坛拖拽、光环流光、BD/天赋独立入口、获得物品提示、怪物/召唤物索敌范围沿用现有 UE 实现；本轮没有重做。掉落设计表仍作为草案，没有据此启用新的掉落池和掉率。地牢几何继续使用现有 Zero/ClaudePCG 原生生成器。

## UICreator 与资源

- 工程：`opus-studio-sync-20261026`，revision 4；源文件 `UIAuthoring/StudioSync.uialpha.json`。
- 冻结包：`wbp-708f07d1-192f-42a4-ba4a-a837e89b46d3`。
- SourceHash：`4ae90f59f38caec0cf430e24ab0528a31fec71089c6e533dab30fd613ed1a528`。
- 最终目录：`/Game/UICreatorStudio`；13 个模板、182 个节点、198 张控件栅格，含保存后的 WBP、DA、运行时合同。
- 商人和传送阵：`/Game/WebVFX/Variants/DA_outpost_merchant`、`DA_dungeon_portal`；离线导出 Studio 原始几何与动作，再生成原生资产。

## 验证与复现

`Saved/Verification/StudioSync` 保存模块构建、冻结源、导入/重载收据、瞬态原生测试和实际渲染截图。`Tools/Verify-StudioSync.ps1 -Render` 检查真实 Slate 鼠标气泡、左键选择、右键购买、浮层固定及穿透拦截，并验证库存、拾取、导航和地图票。测试使用 `-DWOTransient`，校验正式人物存档哈希保持一致。

首领通关与残存普通怪战斗使用 `Tools/Verify-BossClear.ps1` 回归；地图装配/实际 T16 入场使用 `Tools/Verify-MapDevice.ps1` 回归；三个反应使用 `Tools/Verify-AilmentReactions.ps1` 原生自动化验证。

37 项原生渲染与交互检查通过，首领通关、地图装配与三个异常反应回归通过。正式 `UnrealEditor-DungeonWorldOpus.dll` 已链接，独立启动的 `TowerFlow` 测试通过。局部模块编译后缺失的加载清单已由 UBT 重新生成，构建脚本同步修复。

用户保存并关闭旧编辑器后，已重新打开 Opus 编辑器（进程 79220）。启动日志确认正常加载正式 DLL，AngelScript 编译完成，编辑器初始化完成并正常响应。证据为 `Saved/Verification/StudioSync/production-startup.log` 和 `editor-startup.log`。本轮没有启动正式人物的战斗或写入人物存档。
