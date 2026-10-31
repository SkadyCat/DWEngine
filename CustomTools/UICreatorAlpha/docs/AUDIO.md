# UI 音效资产

音效与 SVG、动效和预制体并列保存。控件按稳定 ID 引用 WAV，多控件共享同一资源；嵌套预制体的实例可以局部覆盖事件、音量和音高。

## 制作与试听

1. 左侧切换到“音效”。内置悬停、按下、确认、选中、打开、关闭六个短音效，可以直接试听。
2. 创建音效文件夹，点击“导入 WAV”；支持拖拽归类、重命名、替换 WAV、另存副本，以及有引用保护的删除。
3. 选中控件，在检查器“UI 音效”设置事件引用；也可以在音效卡片选择事件后点击“应用”。资产可设置音量、音高与防连响间隔；控件另有独立音量、音高和启用开关。
4. 点击顶部“开启音效”，再进入交互预览。浏览器需要一次点击才能解除音频播放限制；初次渲染、编辑选择和静态导出保持安静。
5. 保存工程。顶部静音和总音量只保存在本机，导出保留资产与控件配置。

WAV 限制：16 位 PCM，单声道或双声道，8–96 kHz，最长 10 秒、最大 1 MB。内置声音也是真实 WAV 数据，网页与 UE 使用相同字节，不依赖浏览器临时合成。

事件为 `hovered / unhovered / pressed / released / clicked / selected / opened / closed`。按钮默认绑定悬停和确认；技能槽另绑定选中。悬停进出仅在按钮边界触发，经过子元素不会重复响。打开/关闭只在已有控件可见性变化时触发。通用选中事件可由网页 `runtime.playSound(nodeId, 'selected')` 或 UE 蓝图 `Play Ui Sound(selected)` 触发；HUD 的数字键切换已接入。

播放器缓存解码结果，按共享资产抑制短时间重复播放，最多同时播放 8 个声音。音量为资产 × 控件 × 本机预览总音量；音高为资产 × 控件，最终限制到 0.25–4。禁用按钮不播放事件音。

## 数据与 UE 导入

`document.sounds[id]` 保存 `version/name/folderId/source/duration/sampleRate/channels/bytes/volume/pitch/cooldown`，`source` 为内嵌 WAV data URL。`node.audio` 保存 `enabled/volume/pitch/events`。音效随完整工程、WBP 契约和逐控件 PNG 导出包携带。旧工程在编辑/导出边界补齐默认资产，不改动 SVG 外观或布局。

UE 插件自动创建 `/Game/UICreator/Audio/S_*` SoundWave 与 `/Game/UICreator/Audio/A_*` SvgCanvasSoundAsset。SoundWave 使用 PCM、ForceInline；共享 DataAsset 保存音量、音高和防连响间隔。WBP 原生节点引用这些资产，由通用 UButton 回调、可见性事件或 `Play Ui Sound` 播放 2D UI 声音。网页换声音或改绑定后重新导入即可，无需逐音效写 C++ 或蓝图逻辑。

## 验证入口

- 音效制作示例：[HUD · UI 音效](http://127.0.0.1:5188/?project=hud-ui-audio-20261025)。包含“交互”文件夹、自定义 WAV、第二个技能按钮的按下实例覆盖。
- 交互演示：[HUD · 动效与音效](http://127.0.0.1:5188/examples/hud-motion.html)。点击“开启音效”，悬停、点击或按 1–4；页面显示实际启动播放的事件。
- `tests/audio.test.mjs` 验证 WAV、引用、实例覆盖、契约往返、播放器增益/音高、缓存、防连响、静音和释放。
- `SvgCanvasSoundVerify` 在新 UE 进程重新加载已保存资产，核对 WAV 的 PCM 字节与 WBP 事件引用；`-SvgCanvasVerifyAudio=<目录>` 在启用音频的游戏进程触发原生按钮，并记录 AudioComponent 播放、间隔限制及禁用音效结果。
