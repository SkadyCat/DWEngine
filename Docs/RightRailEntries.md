# 右侧 BD / 天赋入口

主 UICreator 工程 `hud-darksteel-20261024` revision 48。右栏保留已有入口，新增独立天赋按钮；BD 和天赋均显示文字，BD 打开 `builds`，天赋通过 AS 打开现有完整星盘。P 快捷键保留。

源修复在 `src/assets/rulestudio-hud/function-rail.js`，由 `installCombatLayout` 调用，`Tools/UI/Sync-FunctionRail.mjs` 同步既有工程、UIAuthoring 和 Screen 文件。对 revision 46 展开节点比较无删除，新增 5 个天赋入口节点；重复安装不产生变化。

冻结包：`wbp-b68d7041-2a8c-4150-a7ce-dc25c286c909`。主 UE 保存资产：`/Game/UICreator/WBP_CurrentCanvas`、`DA_CurrentCanvas`，45 状态、3040 节点、6152 光栅缓存，源哈希 `c0c72d9681c6bba1515b9ac2a4b75886b9aaf93fe411dc684470f949650cbcd3`。

`Script/Opus/RightRailTests.as` 在独立瞬态游戏中点击实际 UButton：

- BD 入口打开有内容的构筑列表。
- 天赋入口打开 1000 节点星盘。
- 打开界面前后配装状态不变。
- 实际选择余烬流派并点击一阶应用，验证 5 件装备、原生 UID 和焰爆技能，随后显示角色页。

`Saved/Verification/RightRail/runtime.log` 记录 `RIGHT_RAIL_PASS BDAndTalents preset=embers_echo_1 equipped=5`。实际原生截图为 `hud.png`、`builds.png`、`talents.png`、`applied.png`。应用测试使用 `DWOTransient`，不写用户存档。

首次导入因旧预览锁住音频资源失败，保留 `import-file-locked.log`。正常退出预览后重新导入成功；退出时角色存档已保存、JSON 有效，退出回执及导入前的角色和 WBP/DA 备份位于同一验证目录。未修改规则语义或数值。
