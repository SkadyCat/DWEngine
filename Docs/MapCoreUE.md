# 地图核心装配 · UE 接入

界面源为 UICreator 完整工程 `hud-darksteel-20261024` 修订 24，未在 AS 中重画控件。已装配卡片、左侧十核心列表、选中皮肤、关闭按钮均来自保存的原生 WBP。游戏业务由 `Script/Opus/MapCoreUI.as` 和 `TowerGame.as` 承接。

点击地图栏的核心卡片展开列表，点击 T1–T10 直接装配并关闭；Esc 关闭列表。核心目录和名称从真实规则装备数据读取，怪物群分别为 0%、100%……900%。当前选中项保存在 `Saved/RuleTower/MapCore.txt`，重置、换层及重新启动后恢复；首次无配置时使用 T1，仍停在准备状态等待手动开始。

原生规则聚合与既有数量公式保持不变。默认基础数量 20 时，T1 总数 20，T2 总数 39，T10 总数 191，均包含一个首领；原有总数 300 上限保留。开始挑战时小怪和首领一次性全部出现，击败首领后通关。换装更新下次开局数量和未来掉落的阶级上限，当前已生成的怪物、地面物品与商店存货保留。

`UOpusNative` 只提供规则聚合、配置存取和当前掉落上下文的适配；未修改规则引擎计算语义。`UOpusUI::SetSelection` 的皮肤缓存按屏幕和列表前缀分别保存，防止地图核心与 BD 列表互相覆盖选中样式。

导入包：`CustomTools/UICreatorAlpha/exports/wbp-f40c8f4d-ed27-464a-8f60-338b1b03151c/bundle.json`。源哈希 `5283b66dd17eb4f4d7d1054578b67ab37b81adb1f0f93e3d90abff1d902fad50`。原生资产为 `/Game/UICreator/WBP_CurrentCanvas` 与关联文档、纹理。导入前备份位于 `Saved/UIImportBackups/map-cores-20261025-133949`。

导入回执证明编译及保存成功，独立进程重载回执证明 2749 个源节点与 44 个状态存在，布局最大误差约 0.0000814 像素。部署的 7275 个资产文件与验证项目逐文件哈希一致。

严格零容差像素比较没有完全通过，失败报告保存在同一导出目录的 `pixel-report.json`，未将其改写为成功。地图核心选择状态最大通道差 4/255；实际游戏截图另行检查，不能用导出成功代替运行时验收。

验证入口：`Tools/Verify-MapCores.ps1 -Render`，检查真实 UICreator 动作、十核心原生聚合、Esc、与 BD 选中皮肤隔离、T10 实际首波生成、战斗中换装、数量上限、重置保留和独立进程配置恢复。测试只读写 `Saved/Verification/MapCore/selection.txt`，不覆盖玩家配置。另运行 `Tools/Verify-SvgRestore.ps1 -ChecksOnly` 与 `Tools/Verify-ItemHover.ps1` 检查现有 HUD 行为。最终结果见 `MapCoreUE.json`。
