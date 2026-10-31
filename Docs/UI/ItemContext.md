# 物品右键菜单

UICreator 项目为 `opus-item-context`，源文件为 `UIAuthoring/ItemContext.uialpha.json`。
普通、魔法、稀有、传奇、独特装备各有一个 240×184 模板，物品使用 240×108 模板。
背景、品质颜色、图标、按钮四态全部由 UICreator 导出；运行时不重新绘制菜单。

冻结包：`wbp-context-8605178c-a5a2-40ad-b2e6-6a3223d35e3d`。
源 Hash：`95674cf399e4aae59c06e5ae39255e955bceae085f5cfa4e34b3ed00ad3c4649`。
131 个逻辑节点、16 个 SVG 资产、228 个透明栅格，资源位于 `/Game/UICreatorContext`。

## 操作

- 背包或装备格右键打开菜单；装备、卸下、鉴定共用首行，状态根据当前物品切换。
- 出售显示原生规则的实际报价，执行时重新校验并只结算一次。
- 丢到地面把物品转移到当前地图的真实掉落列表，装备保留 UID、词条、重铸信息；可以原样拾回。
- 祭坛锁定装备不可装备、出售或丢出，原生事务也会拦截，不能通过直接调用绕过。
- 点击菜单外关闭；Esc/Tab 关闭菜单；上下键切换、Enter/空格执行。Shift+F10 可打开当前悬停格的菜单。

AS 业务入口：`Script/Opus/ItemContextMenu.as`，主 HUD 通过 `ItemHover.as` 连接。
通用指针桥：`USvgCanvasScreen::NativeOnPreviewMouseButtonDown`，在左键按钮动作前转交当前鼠标键和视口坐标。
丢弃事务：`UDWRuleWorldSubsystem::DropPlayerInventory`，使用 Profile 和 FloorRewards 副本验证后一起提交。
拾回玩家主动丢出的装备不会增加怪物掉落统计，也不会额外消耗战斗或掉落随机数。

## 导入与验证

```powershell
node Tools/UI/Author-ItemContext.mjs
# 浏览器打开 examples/item-context-export.html，冻结并生成导入包。
Tools/UI/Import-ItemContext.ps1
python Tools/UI/Install-ItemContext.py
Tools/UI/Import-ItemContext.ps1 -Verify
Tools/Verify-ItemContext.ps1
```

原生单测为 `DWO.Inventory.ContextDrop`，覆盖装备实例、静态装备、物品堆叠、死亡及无效数量、重复拾取和掉落计数。
渲染验收使用独立 `-DWOTransient` 实例中的真实 Slate 鼠标路由，检查菜单打开、装备/卸下、出售、掉落拾回、祭坛锁定和 Esc。
验收前后比较正式存档 SHA256，不写入或恢复正式存档。证据保存在 `Saved/Verification/ItemContext`。

2026-10-26 验收：原生单测 1/1，通过 7 项 Slate/事务检查，独立进程重载成功。
1120×540 导出图与 UE 渲染逐像素对比，平均通道误差 0.082/255，没有 RGB 误差超过 8 的像素。
正式存档前后均为 `42B422425748472234F682CFD38697BA3B0FB0D8227D1D6E6E05DB62F6A30386`。
