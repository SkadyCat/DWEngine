# 可复用动效资产

动效与 SVG、预制体一样，是工程中的一等资产。`document.motions[id]` 保存版本化定义，控件以 `motion.assetId` 引用。多控件与嵌套预制体共享同一份时间轴；实例覆盖只改变引用，不改写模板。资产支持文件夹、改名、复制和引用保护删除。

## 在 UICreator 编辑

1. 左侧“动效”→“＋ 动效”，或在检查器中“新建动效”。
2. 建立时间轴片段；编辑时长、缓动、循环、是否从当前外观接续。
3. 拖动预览时间滑块，再添加关键帧。每帧可编辑 X/Y 位移、X/Y 缩放、旋转角度、透明度。
4. 设置事件到片段的绑定，保存资产；选中控件后点击资产的“应用”，或在检查器选择共享资产。
5. 保存工程。交互预览会使用相同数据；PNG 导出保留静态原始外观。

事件 `normal` 在初次显示或鼠标移出时触发，`hovered` 为悬停，`pressed` 为按下，`disabled` 为禁用。`pressed>hovered` 可单独定义松开回弹；自定义事件可在 UE 蓝图调用 `Play Motion(Trigger)`。不要求为每个动效写 C++、蓝图动画图或效果专用脚本。

## 数据契约 v1

资产包含 `id/name/version/folderId/target/clips/bindings`。

- `target`: `visual` 只动外观；`content` 同时作用于内容图层。
- `target: edge` 在独立透明边缘层播放流光；`edge.width/radius/color` 定义边宽、圆角和默认颜色，旋转通道定义相位，透明度定义亮度。内置「提示 · 边缘流光」可复制或编辑，不改原背景与点击范围。
- 节点可覆盖 `motion.enabled/tint/dismissOnHover`：关闭停止播放；悬停消除后不会因移开自动重亮，重新启用可再次提示。UE 对应 `SetMotionEnabled`、`SetMotionTint`。
- `clips[name]`: `duration` 毫秒、`loop`、`fromCurrent`、`easing`、`keyframes`。
- 每个关键帧完整记录 `t/x/y/scaleX/scaleY/rotation/opacity`，`t` 从 0 到 1，严格递增。
- `easing`: `linear/easeIn/easeOut/easeInOut/backOut`。两端使用相同的逐段插值公式。
- `bindings[event]`: 时间轴片段名称；优先匹配“前状态>当前状态”，再匹配当前状态。

版本、数值、帧顺序、资产与事件引用都会验证。现有轻量、灵动、呼吸是普通内置资产；旧预设仅在编辑/导出边界迁移，不进入 UE 播放器的效果判定。

## UE 导入与播放

原生 WBP 导出包携带 `project.motions` 和逐节点引用。插件自动创建 `/Game/UICreator/Motions/MA_*` 的 `SvgCanvasMotionAsset` DataAsset，保存完整时间轴定义；WBP 节点的 `MotionAsset` 指向共享资产。

`SvgCanvasMotionNode` 监听原生 UButton 事件，通用播放器插值变换和透明度。没有按 HUD 名称或控件 ID 编写动效分支。需要新增效果时只在网页新增/修改资产，再重新导入。

网页使用 Web Animations API；UE 用 Slate ActiveTimer 驱动。视觉变换不改写 Slot、锚点和本控件的透明 Input 按钮。静止且已结束的片段停止计时。隐藏的循环暂停时间；释放 Slate 资源时取消计时器并恢复原始外观。Designer、静态导出和基准截图保持原始外观；网页遵循系统减少动态效果，UE 可用 `-SvgCanvasNoMotion` 禁用自动动作。

边缘流光的 UE 显示由通用透明窄环网格完成，不烘焙到品质 PNG。新物品提示的控件与数据接线见 [控件文档](../Screen/Inventory-Notices/控件文档.md)。

## 验证

示例工程：`http://127.0.0.1:5188/?project=hud-motion-assets-20261025`。第二个技能槽引用自定义的三关键帧“旋转弹跳”，可与其他技能槽对照。

原生 `SvgCanvasMotionVerify` 在新进程中重新加载已保存 WBP 与共享 DataAsset，触发按钮事件并检查悬停、按压、释放、呼吸；另核对网页编辑的自定义片段中间帧六个通道。证据位于 `exports/wbp-hud-native-20261025/motion/generic`。原始参考 SVG/PNG 不改写；通用动效数据可独立更新，无需重新烘焙静态 PNG。

`Unreal/SvgWbpLab/Scripts/Install-VerifiedMotion.ps1` 默认只安装验证后的插件 DLL，并备份主工程原文件；之后导入当前画布即可创建自己的动效资产。显式指定 `-InstallVerifiedCanvas` 才替换为验证副本中的 WBP、源 DataAsset 和动效资产。编辑器须退出以释放 DLL；脚本在检测到该工程仍打开时拒绝写入。`Scripts/verify_motion_assets.py` 可在新进程中回读主工程，检查 WBP 原生节点共享引用与网页源定义一致。
