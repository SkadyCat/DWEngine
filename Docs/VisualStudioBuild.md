# Opus 的 Visual Studio 构建

使用 `DungeonWorldOpus.sln` 的 **Development Editor / Win64**，启动项目选择 DungeonWorldOpus。更新工程文件后，在 VS 接受重新加载，或者关闭后重新打开解决方案。

截图中的 3,171 项是引擎编译和链接任务。原生成结果同时启用了 UE5 项目的 Build.0，并且项目的 NMake 命令缺少 `-UsePrecompiled`，还附带 ShaderCompileWorker 目标。这样会重建共享引擎；其他编辑器占用引擎 DLL 时，链接报 LNK1104 和 UBA 9001。地牢资源不参与这些引擎链接任务。

修复包含两层：

- `Generate-Solution.ps1` 为生成的项目命令补齐预编译参数，调用 `Tools/Build/Configure-OpusSolution.ps1` 取消 UE5 构建项，保留引擎源码浏览。
- 根目录的 `Directory.Build.props` 和 `Tools/Build/OpusEditorBuild.targets` 在中间工程重新生成后仍有效。它只作用于 Opus 的 `Intermediate/ProjectFiles`，将项目的生成、重新生成和清理转到 `Build-Opus.ps1`。本解决方案里的 UE5 节点明确跳过共享引擎的生成和清理，防止重新勾选后再次全量构建。

`Build-Opus.ps1` 始终使用 `-UsePrecompiled -NoEngineChanges -NoHotReloadFromIDE`，只构建 DungeonWorldOpusEditor，不单独构建 ShaderCompileWorker。普通生成保留增量判断；重新生成会重编项目模块，属于预期行为。清理和重新生成同样通过 UBT 的预编译模式排除引擎路径。Opus 自己的编辑器或游戏运行时会拒绝清理和重新生成，提示关闭对应进程。普通生成允许无改动检查；修改 C++ 后需要先退出本工程的窗口，释放待链接的项目 DLL。构建脚本不会关闭任何进程。

此引擎缺少 PCG、PoseSearch、AnimationWarpingRuntime 的导入库。只使用项目本地替代库会让原始 UBT 输入持续缺失，每次重复链接。因此 `repair_editor_target.py --restore-sdk` 从实际引擎 DLL 导出生成导入库，并只补齐引擎 `Intermediate/Build/Win64` 下缺失的 `.lib` 输入，保留原生成时间，不覆盖已有文件、不重编引擎 DLL。根构建入口在仅缺导入库时执行此恢复，再由原生 UBT 完成真实链接与元数据写入。VS 以完整流程的最终退出码判断结果；编译错误和未恢复的链接错误仍会失败。恢复路径、来源和 SHA256 记录在 `Saved/EditorTargetRepair/*/restored-engine-imports.json`。

完整日志位于 `Saved/Verification/OpusVSBuild/native-*.log`，原始 UBT 日志位于 `Saved/Build/build-*.log`。

复现实际解决方案生成：

```powershell
.\Generate-Solution.ps1
& 'G:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' .\DungeonWorldOpus.sln /t:Build '/p:Configuration=Development Editor' /p:Platform=Win64 /m:1
```

历史错误 Makefile 已保留到 `Saved/Verification/OpusVSBuild/Makefile.before-project-only.bin`。只失效这份任务图，保留已编译的项目对象文件和引擎文件。

实际验证：首次恢复后构建 49 项项目任务成功；补齐 SDK 后，原解决方案连续两次普通生成报告 `Target is up to date`，UBT 分别用时 4.04 秒、3.58 秒，均无编译或链接动作。还使用未经后处理的原始 GenerateProjectFiles 重新生成中间工程，确认项目构建入口和 UE5 跳过逻辑仍有效。单独调用 UE5 的 Build、Rebuild、Clean 均明确跳过，544 个顶层引擎 DLL 的大小和修改时间保持不变。证据位于 `Saved/Verification/OpusVSBuild`。
