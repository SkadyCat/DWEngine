# DWEngine

DungeonWorldOpus 精简开发基线，保留规则引擎、AngelScript/Lua 玩法、UE UI 资源和可编辑的 UICreator UI 工程。

此副本约 1.46 GiB，已去除角色、怪物、动画、地牢美术等大型资源，以及缓存、编译产物和历史导出。资产引用保持原样；这是源码与 UI 开发基线，尚未修复缺失美术引用。

## 获取工程

需要 Git LFS。

```powershell
git lfs install
git clone https://github.com/SkadyCat/DWEngine.git
cd DWEngine
git lfs pull
```

## 工程入口

- UE 主工程：`DungeonWorldOpus.uproject`。原工程使用 AngelScript 版 UE 5.8.3。
- UI 编辑器：`CustomTools/UICreatorAlpha`，包含 34 个当前 UI 项目与所引用的素材。
- SVG UE UI 工程：`CustomTools/UICreatorAlpha/Unreal/SvgWbpLab/SvgWbpLab.uproject`。
- 规则引擎：`Plugins/DWRules`。玩法脚本：`Script/Opus` 和 `Content/Script/opus`。
- 打包范围及校验：`_LitePackage/manifest.json`。

## 启动 UI 编辑器

需要 Node.js 20 或以上，无需安装 npm 依赖。

```powershell
cd CustomTools/UICreatorAlpha
.\Start-Editor.ps1 -Port 5190 -OpenBrowser
```

可通过 `http://127.0.0.1:5190/?project=hud-darksteel-20261024` 打开 HUD 源工程。端口被占用时可改用其他端口。

大型文件使用 Git LFS，其余文件直接由 Git 版本管理。源工程未修改，没有进行 UE 编译、资源重建或引用修复。

原完整版使用说明保存在 [Docs/OpusOriginalREADME.md](Docs/OpusOriginalREADME.md)，其中提到的模型与地牢资产不包含在此精简副本中。
