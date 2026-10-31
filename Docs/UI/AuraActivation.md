# 光环激活流光

激活的 Q / E / F / G 光环槽使用青白色边缘流光。关闭、清空或隐藏槽位时停止；重新激活时恢复。悬停不会消费或关闭流光。鼠标配置与键盘开关行为沿用现有入口。

## 作者源与接入

- UICreator 项目：`opus-recent-gameplay`，源文件 `UIAuthoring/RecentGameplay.uialpha.json`。
- 可重建作者脚本：`Tools/UI/Author-RecentGameplay.mjs`。
- 可交互对照：`http://127.0.0.1:5199/examples/aura-activation.html`。
- `aura-slot-active` 是独立的 64 × 64 透明动效层，绑定 `aura-slot:active`；默认关闭。图标、文字、交互区域保持独立。
- 共享动效 `motion-aura-active`：edge target，2px 宽，7px 圆角，颜色 `#8fe8d9`，2200ms 匀速循环。
- `OpusWorldUI.cpp` 从导入器保存的 MotionAsset 装配引用，保留 enabled、tint、dismissOnHover。模板创建时不再丢弃 UICreator 动效。
- `RecentUI.as` 仅根据真实光环状态调用 `SetMotionEnabled`。没有改动规则、魔力封印或伤害计算。JS 仅用于 UICreator 作者预览与导出。

## 冻结与验证

冻结任务：`wbp-recent-d1b8b241-9167-4aa6-abf8-4354c912f885`。

SourceHash：`7b3e34ac36bbf40447c80b3916344507df5d62a4ce98fb5e60cb00ea4cfb446d`。

2026-10-26 已完成：

- 定向原生构建、186 节点与 5 个动效资产导入保存、独立进程 WBP 冷读通过。
- 9 个模板与冻结网页的静态像素对比通过，整体平均绝对误差 0.00540 / 255，超过 8 的像素比例 0。
- `Tools/Verify-AuraActivation.ps1` 使用瞬态档案运行实际 D3D 游戏：默认关闭、空槽关闭、激活、悬停、停用、重新激活、清空验证通过；正式档案 SHA 保持一致。
- 动效验证未传 `SvgCanvasNoMotion`。两帧实际截图在槽位同一坐标中有 270 个边框像素发生变化，图标内部变化为 0；关闭后的流光消失。

证据目录：`Saved/Verification/AuraActivation`。`runtime-receipt.json` 的通过次数包含等待纹理就绪时重复的断言，不代表独立测试用例数量。

重新导入与冷读可执行 `Tools/UI/Import-RecentUICreator.ps1`，配对部署使用 `Tools/UI/Install-RecentGameplay.py`，像素对比使用 `Tools/UI/Measure-RecentUICreator.py`；三个脚本默认任务均为以上冻结任务。更新原生资产时须协调运行中的游戏，保证保存文档与 contract 的 SourceHash 同步。
