# 背包标签底色

`battle-cell-mark` 和 `battle-cell-count` 使用 UICreator 的 `textBackgroundFit` 样式。底色按实际文字宽度加水平内边距计算，保留原来的左、右对齐；空字符串及纯空白隐藏底色，重新填入文字后恢复。其他文字背景保持既有行为。

UICreator 主工程为 `hud-darksteel-20261024` revision 43，已同步 `UIAuthoring` 和 `Screen/RuleStudio-HUD/hud.uialpha.json`。冻结包为 `wbp-ca76e22f-6bf6-4080-adc1-b050d64d7fa7`，源哈希 `19256410c7b9c47dd9ce63bd4dc7636668ac5f28fafe2e2dca932c38c5017d9e`。186 个展开标签均带此样式，整包保留 45 个状态。

UE 资产：`/Game/UICreator/WBP_CurrentCanvas`、`/Game/UICreator/DA_CurrentCanvas`。动态文字通过原生通用绑定测量宽度，并覆盖静态光栅中的示例文字；没有新增玩法或规则逻辑。

验证记录在 `Saved/Verification/ItemBadges/`：

- `import-receipt.json`：新源哈希、编译与保存成功。
- `automation/`：冷进程运行 `DungeonWorldOpus.UI.TextBackground`，检查短文本、长文本、右对齐、清空、纯空白和重新显示，成功。
- `inventory.png`、`hover.png`：真实原生角色背包及悬浮信息截图。
- `unchanged-texture-after.json`：相同像素的已有纹理保持文件哈希和修改时间，导入器仍更新变化的图像。

验收时修复了构建脚本遗漏 `DWRules` 模块的问题。`Tools/UI/Build-HudModule.ps1` 现在同时编译规则核心与规则适配模块，避免两者共享结构变化后 DLL 不一致。没有修改规则语义或数值。首次脚本接口错误与 DLL 不一致的失败日志均保留在同一验证目录。
