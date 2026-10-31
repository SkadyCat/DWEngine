# SVG Canvas → 原生 WBP

2026-10-25 已补齐技能图片的独立圆角裁剪，当前画布导入包为 `wbp-8662510d-a366-488d-9881-e28753c77337`，见 [技能图标裁剪说明](../../docs/SKILL_ICON_CLIP.md)。此前九宫格半像素接缝修复仍保留，见 [九宫格接缝说明](../../docs/NINE_SLICE_SEAMS.md)。下文原始导入任务保留为历史对照。

独立空工程 `SvgWbpLab.uproject`，插件在 `Plugins/SvgCanvasBridge`。本机使用 UE 5.8.3；插件源码可重新编译，不依赖 DungeonWorldBeta 的游戏模块。

## 使用

1. 启动 UICreatorAlpha，打开界面，选择需要的预览尺寸和状态。
2. 点击 **导出 → UE · SVG 转 PNG**，再点 **生成 UE 导入包**。输入取自当前编辑器内存和实际画布，不会用磁盘中的另一个版本替换当前界面。
3. 导入包保存在 `UICreatorAlpha/exports/wbp-…/bundle.json`。`rasters/` 是逐控件透明 PNG 与对应 SVG；`reference/` 是冻结的网页验收图。
4. 关闭本空工程的编辑器后运行：

   ```powershell
   .\Scripts\Import-Canvas.ps1 -Bundle '完整路径\bundle.json'
   ```

   默认引擎为 `G:\UnrealEngine-Angelscript-5.8.3`，可用 `-EngineRoot` 指定另一套引擎。Python 对比脚本需要 Pillow 和 numpy。
5. 打开本工程，在内容浏览器找到 **`/Game/UICreator/WBP_CurrentCanvas`**。独立运行游戏时，空关卡的 GameMode 会将该 WBP 加入视口。

## 实现

- 浏览器负责 SVG → PNG，包括路径、文字、九宫格、渐变、滤镜、透明边缘。UE 不重新解释 SVG。
- 每个源节点对应一个 `SVG Canvas Node`，保留稳定 ID、名称、父子层级、预制体来源、源属性和原始 Slot。Native Canvas Slot 使用源锚点与对齐，并根据已解析矩形换算 offsets。
- 每个绘制节点各有一个 `SVG Raster Image`；隐藏页面也保留独立编辑缓存。按钮另有透明原生 UButton 接收输入，提供 Normal、Hovered、Pressed、Disabled 缓存。
- PNG 源文件使用直通 Alpha；导入纹理转换为预乘 Alpha，关闭有损压缩、Mip 与流送。图像控件保持浏览器颜色空间，避免 Designer、游戏视口和离屏渲染的 Gamma 差异。
- 源工程 JSON、SVG 原文、缓存映射和命名状态存储在 `DA_CurrentCanvas`，可追溯和重新生成。
- 预制体模板根按实例的填充语义生成原生 Stretch Slot；屏幕背景跟随根视口分配的实际尺寸。右侧和底部控件继续使用原始锚点和像素偏移，改变屏幕宽高会重新定位，控件贴图不会整屏等比放大。Designer 使用屏幕尺寸预览模式；这不修改游戏窗口的全屏设置。
- 生成范围是本工程 `/Game/UICreator/WBP_CurrentCanvas` 的完整 WidgetTree。重复导入会替换生成树，移除旧节点；合法的非激活界面保留为 Collapsed。

## 验证

`SvgCanvasImport` 命令编译、保存资产。`SvgCanvasVerify` 在另一个进程中重新读取资产，核对完整节点集合和父子关系，等待所有纹理完成编译，再以原生 Slate 渲染全部状态。验证图不会作为可见 UI 导入。

`compare_pixels.py` 使用 **0 容差** 比较冻结网页 PNG 和真实 UE 输出，生成 `pixel-report.json` 和每个状态的差异图。只要存在一个差异像素，脚本就返回失败。编译成功或截图成功不代表视觉验收通过。

打开 `http://127.0.0.1:5188/examples/native-wbp-review.html?job=任务ID` 可以查看 PNG 控件资产、网页图、UE 图和逐状态结果。

本轮数据和未通过项见 [VERIFICATION.md](VERIFICATION.md)。最新 HUD 导入任务 ID 为 `wbp-ac273a29-3276-4efb-9efe-9abf74c0cb2b`，来源为 revision 21，179 个 SVG、31 个界面入口。两个导入 / 屏幕验证脚本的默认输入已更新到该导入包。

屏幕适配回归：关闭本空工程编辑器后运行 `Scripts/Verify-Responsive.ps1`。除原尺寸和按钮检查外，还会渲染 5 种尺寸下的默认 / 选槽界面，并启动 4 种尺寸的原生游戏视口，检查根屏幕、预制体根、背景和锚点几何。结果保存到导入包旁的 `responsive/`，保留原始参考图；该命令不会将存在色值差异的结果标成零像素差异通过。

## 在 WBP 中独立编辑

动效节点 `SVG Canvas Node · Motion` 引用共享 `SvgCanvasMotionAsset`，用原生按钮事件触发资产内时间轴。位移、缩放、旋转、透明度、循环、缓动和事件均来自 UICreator 数据，无需为新效果添加代码。WBP 的 `MotionAsset` 可独立替换；`Play Motion(Trigger)` 支持自定义事件。详见 [动效说明](../../docs/MOTION.md)，原生逐帧结果在 `exports/wbp-hud-native-20261025/motion/generic/native/`。

UI 音效随导入包生成共享 `Audio/S_*` SoundWave 和 `Audio/A_*` SvgCanvasSoundAsset。原生节点保存每个事件的引用、音量和音高，通用回调播放 2D 声音；网页配置修改后重新导入即可。默认按钮有悬停和确认声音；技能槽支持选中、面板支持打开/关闭。详见 [音效说明](../../docs/AUDIO.md)。编译插件后，`Scripts/Verify-Audio.ps1 -Bundle '完整路径\bundle.json'` 会导入、在新进程核对保存的 PCM 字节及引用，再启动启用音频的游戏进程检查实际 AudioComponent 播放。运行前关闭本空工程编辑器。

当前打开的编辑器持有旧 DLL 时，先保存并关闭本空工程，再运行 `Scripts/Install-VerifiedMotion.ps1` 安装已验证的插件；`-DryRun` 仅检查条件。默认保留当前画布，重新导入即可生成共享动效资产；只有显式指定 `-InstallVerifiedCanvas` 才复制验证工程里的画布。脚本检查源码一致性、引擎 BuildId 和原生动效回执，备份后复制并逐文件回读哈希。`Scripts/verify_motion_assets.py` 检查已保存 WBP 的动效引用与网页源定义一致。

层级中的节点保留源名称。Canvas Slot 可直接修改位置、尺寸和锚点；`SVG Render Cache` 分类中可替换 Normal / Hovered / Pressed / Disabled 纹理。图像子控件也是原生 UImage，可调整图片、大小和颜色。

默认运行读取已保存的 WidgetTree。状态预览会保留 UE 中已经手工修改的布局、贴图和可见性。`OnSourceAction` 供蓝图或游戏代码接入业务，`SetInteractionEnabled` 同步按钮可点击状态和禁用外观。

重新导入会根据网页契约重建生成树；需要保留 UE 端修改时，先另存 WBP 副本。

## 当前边界

PNG 以导出尺寸烘焙；修改文字、SVG、布局或需要另一种像素尺寸时应重新导出。源容器布局、嵌套预制体定义完整保留在源数据中，生成的 WBP 使用解析后的 Canvas 层级。UE 内的动态重排与预制体再次实例化不等同于网页编辑器的实时求解。

HUD 的选槽弹窗和导出的背包、属性、Hunter、设置状态可以切换。具体游戏数据、技能执行、物品操作、网络和原项目业务逻辑仍由接入项目处理，不能通过 PNG 自动产生。未导出的动作不会被当作已实现的游戏功能。


弹窗适配使用源 ScaleBox 的 `fit / fill / down / user` 规则。UE 直接按实际分配空间安排原生控件树；`down` 大屏保持设计尺寸并居中，小屏等比缩小，画面、裁剪和点击区域使用同一套布局变换。Overlay、Button、Border、SizeBox 和预制体根的内容按源 Slot 对齐 / 填充；屏幕遮罩的图片随控件一起拉伸。

`Scripts/Verify-Dialogs.ps1` 用冻结网页布局核对 43 个界面、5 种屏幕尺寸，并启动角色和商店的真实游戏视口验证。对照页：`http://127.0.0.1:5188/examples/native-responsive-review.html?job=wbp-ac273a29-3276-4efb-9efe-9abf74c0cb2b`。切换尺寸可直接对照网页 SVG 与原生 WBP。逐控件 PNG、独立 WBP 编辑和已有动效 / 音效引用保留。
