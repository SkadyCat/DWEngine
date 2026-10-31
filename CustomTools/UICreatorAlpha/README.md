# UICreator Alpha

基准版本恢复与版本范围见 [BASELINE.md](docs/BASELINE.md)。干净检出后先运行 `node scripts/restore-baseline.mjs`，再启动服务，可恢复完整 HUD、动效和音效三个示例；现有本地工程会保留。

完整 RuleStudio 战斗 HUD 已保存为 `hud-darksteel-20261024`：打开 `http://127.0.0.1:5190/?project=hud-darksteel-20261024&interface=character&immersive=1`，从顶部选择框或左侧“界面”切换。包含 31 个界面 / 状态、179 个 SVG 资产与 59 个预制体，覆盖角色、背包、详情、Hunter、结算、星图、商店与洗炼。背包按参考图的 650×448 布局重做，38 个原图标转换为纯 SVG 路径资产。星图改为 1040×660 的居中窗口，复用角色界面的深色与金色样式，HUD 中按 M 打开。说明见 `Screen/RuleStudio-HUD/界面目录与交互.md`。

背包物品、角色背包及装备栏已接入 SVG 悬停详情：移入显示，移开关闭，Ctrl 固定，Esc / × 关闭；固定后可查看完整详情、装备或进入洗炼。悬停不修改选中项。

SVG 是核心资产的 UI 制作系统。借鉴 Unity 的资源、预制体、实例和隔离编辑流程，使用 WBP 的控件树及 Slot 布局语义。全新独立实现，不加载旧 UICreator 的游戏业务模块。

## 启动

需要 Node.js 20 或更高版本。没有 npm 运行时依赖，无需安装依赖或构建。

```powershell
cd G:\DungeonWorldOpus\CustomTools\UICreatorAlpha
npm start
# http://127.0.0.1:5190
```

也可运行 `./Start-Editor.ps1 -OpenBrowser`；端口可用 `-Port 5190` 或环境变量 `UI_ALPHA_PORT` 指定。

## 已实现

- SVG 资产：原生 paths、渐变、蒙版、滤镜、基础 class/inline 样式归一化；源码编辑、Stretch / Contain / 九宫格。
- SVG 和预制体各自的多级文件夹：创建、重命名、移动、拖拽归类、路径导航、全库搜索、删除空目录。分类通过 ID 保存，移动不会破坏引用。
- Button：Normal / Hover / Pressed / Disabled 四态 SVG；独立文本子节点，交互状态不修改布局。
- 可复用动效资产：事件绑定、时间轴、六通道关键帧、循环、缓动与预制体实例引用；网页编辑后自动导入 UE 共享 DataAsset，由通用播放器执行。见 [动效说明](docs/MOTION.md) 和 [编辑示例](http://127.0.0.1:5190/?project=hud-motion-assets-20261025)。
- UI 音效资产：六个默认声音、WAV 导入、文件夹、试听、音量/音高/防连响；控件事件绑定与预制体实例覆盖，随画布导入 UE 共享 SoundWave/DataAsset。见 [音效说明](docs/AUDIO.md) 和 [音效示例](http://127.0.0.1:5190/?project=hud-ui-audio-20261025)。
- 可嵌套的界面预制体：隔离编辑、实例局部覆盖、还原、应用到模板、解除关联；稳定模板路径和循环引用校验。
- CanvasPanel、Overlay、HorizontalBox、VerticalBox、SizeBox、ScaleBox、ScrollBox、Border、Button、TextBlock、Image、ProgressBar、Svg。
- WBP Canvas 锚点、偏移、Alignment、AutoSize、ZOrder；面板 Padding、Auto / Fill 权重；裁剪与滚动。
- 层级挂接、排序、拖动、八方向缩放、网格吸附、缩放平移、撤销重做、多尺寸预览。
- HTML 测量导入、旧版 Frame JSON / 声明式 WBP 树导入；完整工程、原生 SVG、离线 HTML、WBP 布局契约导出。
- 独立事件驱动运行时，复用编辑器内核；SVG/预制体独立文件存储、不可变内容复用、原子保存和 revision 冲突保护。

## 基本操作

1. 在“SVG 资产”中新建文件夹，进入文件夹后新建或导入 SVG。在资产编辑器中选择分类和缩放策略。
2. 添加 Button，在检查器绑定四个状态的 SVG。点击“预览”测试悬停和按下。
3. 选中控件或整个界面，点击“保存为预制体”。从预制体库点击缩略图插入实例，点击“编辑”隔离编辑。
4. 展开实例、选中继承节点后修改文字或样式，即创建局部覆盖。实例根检查器提供还原、应用到模板和解除关联。
5. SVG 可拖到文件夹；预制体卡片同样支持拖拽，`⋯` 菜单可重命名和选择文件夹。
6. 点击“保存工程”。下次启动会恢复上次保存的工程；工程菜单可打开其他工程。

Ctrl S 保存；Ctrl Z / Ctrl Shift Z 撤销重做；Ctrl D 复制；Delete 删除；Space 平移；F 适应画布。

屏幕预览：点击顶部 **框选屏幕**，在工作区拖出矩形作为模拟屏幕。绿色边界标明屏幕原点和像素尺寸，拖动八个边界控制点或输入宽高，会按锚点重新布局控件。选中控件时显示屏幕坐标、尺寸、距右和距下；越出屏幕的数值显示为橙色。屏幕尺寸与查看倍率独立，滚轮和 F 只调整查看倍率；↺ 恢复工程设计尺寸。此操作只改变当前预览，不修改控件 Slot 或工程设计尺寸，Esc 可取消本次框选 / 调整。

## 验证与说明

```powershell
npm run check
npm test
```

- 浏览器集成验证：`http://127.0.0.1:5190/examples/verify.html`
- 独立运行时：`http://127.0.0.1:5190/examples/runtime.html`
- [架构及扩展边界](docs/ARCHITECTURE.md)
- [数据与存储协议](docs/FORMAT.md)
- [本轮验证记录](docs/VERIFICATION.md)

## HUD 示例

RuleStudio 掉落交互原型：打开 `http://127.0.0.1:5190/examples/loot-prototype.html`。UI 来自独立 UICreator 工程 `opus-rulestudio-loot-20261025`，支持掉落落地、靠近拾取、详情固定、通货等级和 Hunter 搜索分页。原型直接复用 RuleStudio 掉落源快照，仅运行于作者工具；说明见 [掉落控件与接线契约](Screen/RuleStudio-Loot/控件文档.md)。

已提供用户参考图制作的“HUD · 暗钢快捷栏”：20 个 SVG、6 个嵌套预制体、技能槽 / 消耗品槽四态和独立选中框。

- 编辑：`http://127.0.0.1:5190/?project=hud-darksteel-20261024`
- 原尺寸参考对照与交互：`http://127.0.0.1:5190/examples/hud-darksteel.html`
- [HUD 控件及资源说明](Screen/HUD-DarkSteel/控件文档.md)

HTML 导入是当前视口的测量快照，不自动推断所有 CSS 响应式行为；外部资源、复杂 CSS、UE 动态材质、任意 CSS 动画和业务绑定有明确缺口提示。UICreator 自身的通用动效资产支持直接导入 UE。

UE 导出新增 **导出 → UE · SVG 转 PNG**：按当前画布尺寸生成逐控件透明 PNG、层级和按钮四态，通过独立插件生成原生 WBP。空项目及使用说明见 [Unreal/SvgWbpLab](Unreal/SvgWbpLab/README.md)。源 SVG 与预制体继续保留在工程中；PNG 按导出尺寸烘焙，具体游戏业务仍由接入项目实现。严格像素结果由冻结参考图和原生 UE 渲染的实际对比报告决定。
