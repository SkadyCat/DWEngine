# UICreator Alpha 基准 · 2026-10-25

本基准提交位于 DungeonWorldBeta 仓库的 `CustomTools/UICreatorAlpha`。覆盖 SVG 资产及分类、嵌套预制体和覆盖、控件树/Slot/锚点、屏幕模拟、完整战斗 HUD、通用动效、UI 音效，以及逐控件 PNG → 原生 WBP 插件源码和必要 UE 资源。

## 恢复与启动

Node.js 20+，无需安装 npm 依赖。在工具目录运行：

```powershell
node scripts/restore-baseline.mjs
npm start
```

恢复脚本应在服务启动前执行，只创建缺失工程，保留已有本地项目；`--check` 只验证源文件。随后打开 `http://127.0.0.1:5188/`，在工程菜单选择示例。

保存的源工程：

| 工程 | 来源 revision | 文件 |
| --- | --- | --- |
| 完整战斗 HUD | 21 | `Screen/RuleStudio-HUD/hud.uialpha.json` |
| 可复用动效资产 | 2 | `Screen/UI-Motion/hud.uialpha.json` |
| UI 音效 | 2 | `Screen/UI-Audio/hud.uialpha.json` |

这些文件包含 SVG、预制体、共享动效、音效和实例覆盖。旧预设在快照时转为当前资产结构，视觉与布局不变。新本地仓库的保存 revision 从 1 开始，与来源 revision 分开记录。

## 版本范围

源码、文档、测试、源工程和已保存 UE 内容进入 Git。`data/projects` 本地保存、`work`、UE Binaries/Intermediate/Saved/DerivedDataCache、AngelScript 绑定缓存及新生成的导出任务不进入版本库。此前已经跟踪的导出基准继续保留。

原生导入包从网页当前画布通过“导出 → UE · SVG 转 PNG”重新生成，再将新包路径传给 `Unreal/SvgWbpLab/Scripts/Import-Canvas.ps1 -Bundle ...`。部分验证脚本默认路径指向本机历史导出任务，干净检出时须显式传入新路径；新导出及其参考图和完整诊断留在本地。

UE 插件以源码为准，使用 UE 5.8.3 重新编译。已保存 WBP 仍保持独立控件；新增音效绑定需从网页音效示例重新导入。主工程与独立验证副本的区别见 `Unreal/SvgWbpLab/VERIFICATION.md`。

## 已验证与边界

`npm run check` 通过，`npm test` 51/51。三个保存的源工程均通过模型校验。

`baseline/verification` 保留音效网页操作、保存重载、UE 资产导入及实际播放回执和动效原生回执。音效实测包含 7 个资产、873 处引用；原生按下事件播放自定义 WAV，音量 0.5、音高 1.1，防连响和禁用检查通过。

该基准不声明网页与 UE 已达到零像素差异；现有色值差异、布局验收范围及业务接入边界继续按原验证文档记录。
