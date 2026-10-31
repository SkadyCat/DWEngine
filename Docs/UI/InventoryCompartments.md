# 普通背包与装备背包

按穿戴状态分区。普通背包装未穿戴装备和其他物品；九个装备槽只显示当前穿戴。装备、卸下、更换、出售、丢弃和拾取仍使用既有所有权与原生事务，不新增一份装备数据或修改规则数值。

同一模板有多件时，穿戴只占一件，其余数量留在普通背包。技能与消耗品的配置状态不归入装备背包。旧背包位置会自动对齐当前普通背包内容；分页在末页物品移出后自动回到有效页。

角色背包默认显示“全部物品”：地图、药品、通货、预兆及其他物品都使用同一套背包格子。分类筛选只影响显示，不改变库存归属；地图装置和快捷栏引用背包中的物品，实际使用时扣除同一份库存。已有物品无需搬迁。Studio、UE 的默认筛选和 UICreator 源标签保持一致。

Studio 使用 `public/inventory-compartments.mjs` 投影，保留普通背包分页、搜索、拆堆和整理。九槽沿用角色穿戴数据。鼠标右键、拖到对应装备槽、拖回普通格均使用原有装备 API。

UE 对应 `Script/Opus/InventoryCompartments.as`、`SvgHUD.as`、`InventoryDrag.as` 与 `ItemContextMenu.as`。左右两区共用既有 UICreator 物品格，拖动影像使用 `UICreatorStudio/inventory-drag-icon`，仅显示物品图标，指针偏移 12px，鼠标是否松开以真实 Slate 按钮状态判断。交互与分区策略保留在 AS。`InventoryLayout.as` 复用规则 Profile 中的 `bagLayout` 位置元数据，保留拖走后的空格；角色背包默认 36 格，拖动阈值 6px，放下会抑制尾随点击，Esc 取消拖拽。点击选中，再点取消；悬停查看、Ctrl 固定详情。

2026-10-27 拖拽修复：位置表只保存普通背包的副本数，穿戴的一件不再作为隐藏占位；绘制格子也不再二次扣除穿戴数量。旧位置数据在 Reconcile 中自动校准并保留其他有效格子。拖回指定空格、换装与 Profile 重载后，物品必须仍位于鼠标指向的可见格子，不能只比较保存的索引。Esc 取消同样抑制松手产生的点击；拖动经过其他物品时不创建悬停详情。落下时再次检查锁定和当前分区的可用数量。

`Tools/Verify-InventoryDrag.ps1` 在独立的渲染 UE 临时实例中复现与检查，证据在 `Saved/Verification/InventoryDrag`。`before.log` 保留原逻辑失败，`final.log` / `final-receipt.json` 记录最终结果。Slate 注入真实按下/松开；移动采样进入生产 UpdatePointer，并使用实际控件几何核对落点、6px 阈值、Ghost 清理、Esc 和数量守恒。测试不写正式 Profile；其哈希前后校验。验证类新增后，已经运行的 PIE 可能因反射结构变更而推迟 AS 完整热重载；停止再播放即可，不需要重启编辑器。

本轮验收：专项 29 项通过，跨背包与右键联动回归 11 项通过，正式 Profile 哈希前后均为 `1194B9408302597C84E66E36C9700BC8706BFBA73932F8E34AD5995276F8C646`。旧回归的回拖目标由固定首格改为当前真实空格：换装后首格可能已经被换下的装备占用，拒绝该落点是正确行为。测试源控件也限定在当前背包区域。既有编辑器和正式 PIE 未关闭，未运行 C++ 构建或导入界面资产。

UICreator 主工程 `hud-darksteel-20261024` revision 49；源为 `UIAuthoring/RuleStudio3D.uialpha.json`。`Tools/UI/Sync-InventoryCompartments.mjs` 在当前工程上更新背包预制体，备份并保留其他界面。冻结包 `wbp-24431810-399e-469f-9e77-8091516ae72f`，源 Hash `86508637da4f3cf042b6e09de96cd61989a930c8b0c9d3f2bef693111ed7051e`。

导入入口 `Tools/UI/Import-InventoryCompartments.ps1 -Job <job> [-Verify]`；生成目标 `/Game/UICreator/WBP_CurrentCanvas`。完整 HUD 含 45 个合法状态；保存后在独立进程检查重载、控件树和响应布局。

`Tools/Verify-InventoryCompartments.ps1` 使用临时原生运行实例检查真实右键、装备/卸下后的唯一归属、换装回流、出售一次、地面拾回、祭坛锁定、Esc 与跨背包拖拽策略。正式 Profile 验收前后计算 Hash，不写入或恢复正式存档。证据位于 `Saved/Verification/InventoryCompartments`。

验收：Studio 分区与布局测试 9/9 通过，真实网页右键装卸与双向跨包拖动通过；UE 冷启动、Slate 交互及跨包拖拽策略回归通过，正式存档 Hash 前后一致。已保存 Studio 与 UE 运行截图，45 个 UICreator 状态独立保存重载检查通过。

2026-10-27 合并迁移使用主工程 revision 56 的原图和布局，原生投影 `opus-studio-iteration-hud-20261027` 只增加品质选中态的资源映射。冻结 HUD `wbp-74c4e52c-de6c-460a-9d1b-e451baa07be5`；商人、地图与新任务追踪控件见 `Tools/UI/Author-StudioSync.mjs`。运行 `Tools/Verify-StudioIteration.ps1` 在临时 UE 游戏中验证原生鼠标事件、位置存档、任务收展、购买确认与旋风碰撞后的两秒寿命；正式存档在验收前后校验 Hash。
