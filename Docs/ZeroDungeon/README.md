# Zero 地牢接入 Opus

来源为 Zero 当前默认 `/Game/ClaudeDungeon/Maps/L_ClaudeDungeon`。只抽取独立 ClaudePCG 几何插件、PCG 图、地牢网格/材质/贴图/火焰资源以及原场景的环境参数。没有迁入 Zero 的 GameMode、UI、账号、商人、人口、战斗或地图业务 DA。

目标地图为 `/Game/ClaudeDungeon/Maps/L_OpusDungeon`，保留 seed 20261008、26×26 布局、11 房间、7,448 实例、41 火把和14烛台。使用 Opus 原有角色、模型谱系、AS 遭遇编排、HUD、原生 DWRules 与掉落。原地图没有直接复制：目标地图从几何生成器与环境导出重新建立，因此不包含源商人蓝图或源 GameMode 引用。

`Plugins/ClaudeDungeon` 保留原始 `/Script/ClaudePCG` 类型与源码；`Content/ClaudeDungeonKit` 和 PCG 图保留资产包名。143 个资源包、159 个源文件，逐文件 SHA256 见 [source-assets.json](source-assets.json)，环境与布局参数见 [scene.json](scene.json)。源码插件和源资源保持原样；仅目标工程新增导航/物理接口。

旧的 `ADWOTowerRoom` 方形场景、±1500 坐标钳制及矩形投射物边界已移除。玩家沿原 CharacterMovement 的碰撞移动；AS 怪物定期查询完整导航路径，并以真实胶囊碰撞移动。刷怪从可达且留出95cm空间的地牢点中选取，保持190cm间隔。2026-10-26 起按用户要求，一次性发布全图小怪和一个首领，击败首领后通关；候选点补充了经导航投影和95cm胶囊扫掠验证的半格位置，以容纳300只上限。详见 [地牢整批出怪](../DungeonSpawning.md)。技能、生命、速度和掉落公式仍由原有规则决定。

冷启动时，PCG 实例虽已保存，游戏世界仍可能没有导航瓦片。目标保留 Dynamic / Force Rebuild on Load，并由 AS 在组件注册后请求一次导航重建，直到起点能投影且191个候选点与入口连通后才允许开始。期间仍推进原有角色资源加载。启动、重置和跨层都经过相同入口。

## 规则版本

用户确认只接地牢，继续使用已有可运行规则。`Content/RulesTower` 固定为基准2提交 `e2abf77cf5a00f75a601aaefa71e7b1945eaaaae` 的完整规则文件，逐字节哈希见 [runtime-rules.json](runtime-rules.json)。运行加载879个词条、17个技能构筑、9个状态。没有修改 DWRules 插件的解析器或战斗引擎，也没有调整这些数据的语义或数值。

接入过程中 `Content/Rules` 出现尚未支持的网页游走词条，曾造成正常开局失败；用户明确选择保留可运行规则。制作目录继续保留原文件，爬塔入口改读固定快照。最初隔离的副本保存在 `Saved/Verification/DungeonExtraction/DeferredRules`，后续写入的制作数据没有继续搬动。更新运行规则应在其原生支持和验证完成后显式更新快照，不随制作目录自动混入。

## 制作与验证

运行游戏仍使用根目录 `Start-RuleTower.cmd`。F5 的生成入口和当前用户调试配置已指向新地图，旧 Entry 启动参数也会转入新地图。

多个编辑器同时占用显存时，可执行 `.\Start-RuleTower.ps1 -LowMemory`，使用本轮真实渲染通过的显存预算；默认画质配置保持原样。该选项降低纹理、Nanite和虚拟阴影缓存预算，以85%场景比例渲染、关闭硬件光追，保留全部场景几何、动态灯光、Lumen和阴影；同时给冷加载更多时间片。本轮显卡12GB显存几乎用满，原始截图仍有显存告警，不能据此宣称帧率或常规画质下的显存验收通过。

```powershell
python Tools/Dungeon/export_zero.py # 仅在独立 Zero UE Python 环境中执行
python Tools/Dungeon/extract_zero.py
python Tools/Dungeon/freeze_rules.py
.\Tools\Verify-Dungeon.ps1
.\Tools\Verify-Dungeon.ps1 -Reset
.\Tools\Verify-Dungeon.ps1 -Visuals
```

`author_opus.py` 在自己的 Opus 编辑器中通过 `py` 执行，只允许创建尚不存在的目标地图；请使用 `-SCCProvider=None`，避免 Git 对新地图的检出弹窗。脚本等待真实 PCG 输出和导航后直接保存，有重入保护。已存在地图的保存检查使用 `verify_save.py`，冷读全部资产及地图使用独立 `-run=pythonscript -script=.../audit_opus.py`。

原始本地证据在 `Saved/Verification/DungeonExtraction`。最终验证结果见 [verification.json](verification.json)。NullRHI 逻辑检查、DX12画面检查与性能结论分开记录；没有声称打包、DS 或稳定帧率已经验收。
