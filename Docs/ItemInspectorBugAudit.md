# 物品浮层动态状态修复（2026-10-25）

截图中的问题主要位于 UE 运行时 UI 适配层，另有资源绑定和示例素材复用错误。此前静态导入检查无法覆盖动态控件移动后的悬停皮肤、不同长度的实际词条和特性卡片。

| 现象 | 根因 | 修复 |
| --- | --- | --- |
| 空白词条、首个材料图标消失、悬停框错位 | 控件移动/增高后，Normal、Hovered、Pressed、Disabled 的栅格原点仍是导出时的坐标；悬停重新套用旧坐标，出现遮挡 | `OpusSvg::PlaceNode` 同步控件、全部后代及四态资源坐标；重复悬停/按下/松开回归 |
| 右侧特性文字重叠或超出卡片 | 沿用示例的固定行数和高度；后面的示例卡片曾被作者滚动区域裁剪，不能当完整动态模板使用 | 复用已完整导出的技能/装备模板，按真实描述逐张测量高度，更新滚动范围，收起示例多余行 |
| `n%`、`n秒` 残留 | 旧文案用连续 `n`，参数实际为 `percent`、`seconds` 等，原格式化器只支持同名替换 | 根据原 JSON 参数顺序填充旧占位符，再使用实例具体 Params 覆盖默认值；不修改规则或参数 |
| 三档灵石都为紫色 | 动态图标绑定覆盖了 UICreator 的品级滤镜资源 | 经 UICreator 冻结导出绿/蓝/金三档原图，逐字节安装 PNG 映射 |

图标导出工程：`UIAuthoring/MaterialIcons.uialpha.json`，批次 `wbp-5ce478db-0ded-4ce0-a29d-36dbddd23dac`，源哈希 `3f46660e65a3af557f4605195142664afa5a9781f613010c9ff3b4b5aacedc8f`。导出和安装脚本位于 `Tools/UI/Prepare-MaterialIcons.mjs`、`Install-MaterialIcons.py`。没有重画 UI。

验证使用当前存档中的 `T1 燎原·余烬衣`：四条实际随机词条，依次悬停所有通货/预兆及词条，再拍摄 1600×900 原生画面。固定属性从真实实例显示为“8%／2秒”。所有测试使用 `DWOTransient`。

- `Tools/UI/Verify-InspectorProjection.ps1`：`ConcreteParameters`、`MovedInteractionBrushes` 两项原生回归通过。
- `Tools/Verify-ItemHover.ps1`：三档灵石各一次真实加工、整件重洗、原位鉴定、固定/关闭等交互通过。
- `Tools/UI/Capture-ItemInspector.ps1 -Name bug-fixed-final -Item '余烬衣' -Probe`：连续悬停后的实机渲染通过并人工检查。
- 最终画面：`Docs/Proof/ItemInspector-bugs-fixed.png`。
- 详细日志：`Saved/Verification/ItemHover/`。此前尝试拍摄旧版时被正在加入的 DropService 尚未编译类型拦住；旧版失败视觉证据来自用户附图，不声称重新拍摄过可比较的旧版画面。

本次没有修改 DWRules、加工费用、随机结果或玩家存档。主游戏恢复后加载新适配模块和图标。
