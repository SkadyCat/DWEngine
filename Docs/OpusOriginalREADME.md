# DungeonWorld Opus

双击根目录 `Open-OpusEditor.bat` 打开 UE 编辑器。启动时保留 UE 加载进度窗口；重复点击会复用正在启动或已经打开的 Opus 编辑器进程。仅运行游戏则使用 `Start-RuleTower.cmd`。

由 G:\DungeonWorldBeta 的当前规则与爬塔玩法迁移。Beta 仅作为读取来源。本工程运行不需要 Node、浏览器、Vue、Puerts 或 JavaScript。

业务逻辑在 Script/Opus 的 AngelScript 中；Content/Script/opus 提供 Lua 调用入口。Plugins/DWRules 保留原生 C++ 编译器、战斗 VM、角色、状态、召唤、装备、打造、掉落和存档。C++ 游戏模块只处理原生接口、数据查询、文件与 SVG/UMG 显示。

Content/Rules 的 1047 份 JSON 保持迁移快照原值。按已确认的授权，原生引擎修复当前规则的兼容缺口，包括控制单位上限、装备授予技能、献祭、元数据、毒层及召唤物投射物支持；没有重新调整规则或玩法数值。具体文件和哈希见 Docs/NativeCompatibility.json 与 Docs/RulesSnapshot.json。

默认是第 1 层、T1 霜棘法杖、单技能、准备状态，按回车或设置页的挑战按钮后开始。每层三波普通敌人再进入守卫战；支持 20–300 怪、1–100 层、可选 30 秒限时、手动或自动跨层、死亡重试及跨层资源继承。怪物沿用 Beta/Zero 谱系的模型和动画。

场景已换为 Zero 的 Claude 地牢：11 个房间、走廊、原有装饰与灯光，7,448 个实例。启动、重置、重试和跨层均进入 `/Game/ClaudeDungeon/Maps/L_OpusDungeon`；已移除旧方形塔场和坐标边界，玩家用真实碰撞，怪物沿地牢导航追击，投射物使用实际墙体碰撞。迁移边界与复现见 [地牢接入说明](Docs/ZeroDungeon/README.md)。

怪物的全局配置保存在 `/Game/Monsters/Config/DA_GlobalMonsterSettings` DataAsset。`AggroRadiusMeters=8.0` 控制所有普通怪、首领和事件守卫的水平索敌半径；进入范围才会追击、主动施法，超出范围会停止并清除旧追击路径。已释放的攻击仍按原规则结算。

同一份 DataAsset 的 `PlayerSpawnExclusionRadiusMeters=10.0` 设置玩家出生点周围的水平刷怪禁区，普通怪、首领、事件守卫和远处位置兜底都遵守此限制；玩家移动不会移动该禁区。已有怪物在索敌范围内仍可正常追击进入该区域。修改资产后重新运行当前预览即可。`Tools/Verify-MonsterAggro.ps1` 验证索敌边界、进入/离开范围、主动施法和全局共享配置；`Tools/Verify-Dungeon.ps1 -SpawnSafetyOnly` 验证出生点禁刷与 300 怪位置容量。

按“只接地牢、使用原有可运行规则”的选择，爬塔入口读取 `Content/RulesTower` 的固定规则快照（基准2，e2abf77），原生 DWRules 引擎未改。`Content/Rules` 保留规则制作中的文件，新网页词条不会阻断地牢开局。快照逐文件哈希见 `Docs/ZeroDungeon/runtime-rules.json`。

接入当前 RuleStudio3D 的 SVG HUD 原生资产，包含背包、装备检查、技能槽、构筑、打造、商店、通关商人、掉落过滤、角色属性及挑战设置的 AS 绑定。地图编辑、网页编辑器、异界探索和调价工具不属于本次爬塔范围；没有迁入原游戏的其他 UI。SVG 源快照标识见 Docs/SvgHUDSnapshot.json。

运行 Start-RuleTower.cmd，或在 PowerShell 执行 .\Start-RuleTower.ps1。引擎路径是 G:\UnrealEngine-Angelscript-5.8.3；需要重新构建时执行 .\Build-Opus.ps1。

通过 Visual Studio 2022 运行：打开 DungeonWorldOpus.sln，选择 Development Editor / Win64，启动项目为 DungeonWorldOpus，按 F5（或 Ctrl+F5）进入爬塔游戏窗口。启动时仍为准备状态，回车开始。调试参数已写入 Intermediate/ProjectFiles/DungeonWorldOpus.vcxproj.user。需要刷新解决方案时运行 .\Generate-Solution.ps1；加 -Open 可同时打开 Visual Studio。该脚本保留预编译引擎构建参数及游戏启动参数。

VS 的生成、重新生成和清理已通过根目录的 `Directory.Build.props` 接到项目构建入口，仅处理 Opus 模块，保留共享 UE 引擎。即使编辑器重新生成了中间工程，也不会启动 UE5 全量构建。无代码改动的普通生成可以在游戏运行时检查；修改 C++ 后需要先关闭 Opus 自己的编辑器或游戏窗口，其他工程可继续运行。构建日志与修复原因见 [VS 构建说明](Docs/VisualStudioBuild.md)。

WASD 移动，鼠标瞄准，左键或空格施法，1–4 选技能，Shift 闪避，F 切换自动施法，Q/E/T/G 使用快捷消耗品，B 背包，C 属性，M 挑战设置，Esc 关闭窗口或暂停，死亡后 R 重试，通关后 N 挑战下一层，P 拾取附近掉落。点击背包物品可检查和装备。

存档在 Saved/RuleTower/Profile.v2.json，保存配装与背包；启动没有复制 Beta 存档。Start-RuleTower.ps1 -Fresh 会先备份本工程存档。-NoHUD 关闭 SVG HUD；-AutoStart 直接进入本层。

Tools/Verify-Opus.ps1 运行原生自动化、AS 流程、SVG 业务绑定、手动准备、实战通关/跨层、死亡重试和磁盘存档往返检查。对照样本以压缩 JSON 保存在 Tools/Verification/Fixtures，首次验证自动解压，不需要 JS。战斗测试使用临时会话，存档测试使用 Saved/Verification/ProfileRoundtrip.json。

Web 特效已使用最近会话研究中的原生资产管线重建为网格、材质和帧数据，运行由 DWRules 原生表现层驱动。Tools/WebVFX 的 Python 仅用于离线 UE 资产重建，不参与战斗判定或游戏运行。

UE 特效覆盖表位于 /Game/WebVFX/DA_UEVFXMap，当前将冰锥飞行及命中映射到从 CustomTools/特效 单独迁入的两个 Niagara 系统。未配置或不可用时继续原默认表现；规则和伤害保持原有实现。编辑与验证说明见 Docs/UEVFX映射.md。
