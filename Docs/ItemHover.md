# RuleStudio3D 物品原位加工

按 Beta 的 `CustomTools/RuleStudio/public/battle-item-panel.mjs` 重做 UICreator 物品浮层。固定后直接选择通货、预兆并点击随机词条加工；鉴定也更新同一个浮层。底部只保留装备／卸下，旧的“查看详情”和跳转“洗炼”按钮已从源稿及导出节点删除。

## 作者资源

- UICreator 源控件：`CustomTools/UICreatorAlpha/src/assets/rulestudio-hud/item-tooltip.js`。
- 主稿：`UIAuthoring/RuleStudio3D.uialpha.json`，同时更新 Screen 内的 HUD 源稿和 UICreator 保存项目。
- 独立导出工程：`UIAuthoring/HoverSource.uialpha.json`，项目 ID `opus-item-inspector-3d-20261025`。
- 导出批次：`wbp-2bcc4ef5-37cc-4fae-9aac-506d7245801a`。
- 资源哈希：`fae51032a46d7f429aa32ff3d5ac1f1f0f3eb872d1d4092ed8a70f7b61fbd53a`。
- 运行合同：`Content/UICreatorHover/contract.json`。7 个品质模板及独立说明浮层，共 396 个节点、892 张导出 PNG。

`examples/item-inspector-review.html` 可检查操作顺序。网页使用明确标注的作者示例，不运行生产随机生成或结算规则。游戏行为在 `Script/Opus/ItemHover.as`；C++ 适配读取规则数据、克隆 UICreator 原始控件并绑定原生输入。生产 Content、Script 不增加 JS。此次未修改 DWRules。

## 操作契约

- 普通悬停移出关闭；Ctrl 固定。角色页点击物品固定，背包页点击仍选中物品。浮层在滚动裁剪区域外，按内容增高并限位。
- 通货及预兆只列实际持有的物品。点击图标激活，再次点击取消；激活本身不扣数量、不改变随机状态。
- 下／中／上品按现有规则依次绑定至多 3 条不同词条。可加工行高亮，点击后调用一次 `CraftPlayerEquipment`；显示真实绑定品级、词条及持有数量。
- 整件通货显示本浮层内的整件加工按钮，操作次数、词条数、消耗及固定属性保留都由原生事务决定。
- 未鉴定物品隐藏词条，原位鉴定后跟随真实新 UID，并显示同一物品的加工区域。
- 通货、预兆和有说明的词条可悬停查看说明；Ctrl／点击固定说明。Esc 先关闭说明，再关闭物品浮层。
- 加工和鉴定保留当前物品、固定浮层及所在页面。主 HUD 数据按帧更新，避免在一次点击中同步重建全部面板。
- 可用词条数来自当前原生词条池及预兆族过滤；此次只修复交互，不修改原生池或规则数值。

## 导入和验证

1. `node Tools/UI/Sync-ItemInspector.mjs` 同步作者源稿，仅替换物品浮层预制体。
2. 在 UICreator 导出 SVG 转 PNG，冻结批次后运行 `python Tools/UI/Install-ItemInspector.py <job>`。
3. `Tools/UI/Import-ItemInspector.ps1 -Job <job>` 保存原生 WBP，再用 `-Verify` 在独立进程重新加载并检查层级、纹理及布局。
4. `Tools/Verify-ItemHover.ps1` 测试真实导出 UButton 的悬停、固定、材料切换、下中上品真实加工、整件重洗、装备、原位鉴定和页面清理。事务分帧执行，均使用 `DWOTransient`，不写玩家存档。
5. `Tools/UI/Capture-ItemInspector.ps1` 与 `-Currency low -Name inspector-3d-active` 输出真实存档装备的 UE 实机渲染。

原生验证日志、导入和重载收据位于 `Saved/Verification/ItemHover/`。最终证据及文件摘要记录于 `Docs/ItemHover.json`。独立重载和渲染仅证明保存、层级及布局正确，不等同于网页与 UE 全部像素一致。

验证时发现新加入的 `player_stun_buildup`、`player_stun_threshold`、`player_stun_recovery` 被放在 Traits 下，触发既有的 `sheet/grant_traits must be authored under Affixes or Timed` 启动校验。三份 JSON 原字节移至 Affixes，内容哈希均与移动前备份一致；没有放宽校验或改动公式、参数。
