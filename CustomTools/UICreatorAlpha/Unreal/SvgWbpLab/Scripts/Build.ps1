param([string]$EngineRoot = 'G:\UnrealEngine-Angelscript-5.8.3')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$projectFile = Join-Path $projectRoot 'SvgWbpLab.uproject'
& "$EngineRoot\Engine\Build\BatchFiles\Build.bat" SvgWbpLabEditor Win64 Development "-Project=$projectFile" -WaitMutex -NoHotReloadFromIDE -UsePrecompiled -NoEngineChanges -Module=SvgCanvasRuntime -Module=SvgCanvasEditor -Module=SvgWbpLab -MaxParallelActions=2
if ($LASTEXITCODE -ne 0) { throw 'Plugin build failed' }
# A module-only UBT build does not emit the final target metadata action.
# All DLLs above were built against this exact local engine; register that BuildId.
$engineManifest = Get-Content -Raw -LiteralPath "$EngineRoot\Engine\Binaries\Win64\UnrealEditor.modules" | ConvertFrom-Json
$targets = @{
    'Binaries\Win64' = @{ SvgWbpLab = 'UnrealEditor-SvgWbpLab.dll' }
    'Plugins\SvgCanvasBridge\Binaries\Win64' = @{ SvgCanvasRuntime = 'UnrealEditor-SvgCanvasRuntime.dll'; SvgCanvasEditor = 'UnrealEditor-SvgCanvasEditor.dll' }
}
foreach ($item in $targets.GetEnumerator()) {
    foreach ($dll in $item.Value.Values) {
        if (-not (Test-Path -LiteralPath (Join-Path $projectRoot "$($item.Key)\$dll"))) { throw "Missing output: $dll" }
    }
    $manifest = @{ BuildId = $engineManifest.BuildId; Modules = $item.Value } | ConvertTo-Json -Depth 5
    [IO.File]::WriteAllText((Join-Path $projectRoot "$($item.Key)\UnrealEditor.modules"), $manifest, [Text.UTF8Encoding]::new($false))
}
# Use the receipt UBT already prepared for these binaries. The metadata writer only
# publishes this project's .target; the helper excludes engine versions/manifests.
& python "$PSScriptRoot\prepare_build_receipt.py" $projectRoot $EngineRoot
if ($LASTEXITCODE -ne 0) { throw 'Could not prepare the module-build receipt' }
& "$EngineRoot\Engine\Build\BatchFiles\RunUBT.bat" -Mode=WriteMetadata -Version=2 "-Input=$projectRoot\Binaries\Win64\ModuleBuildReceipt.json"
if ($LASTEXITCODE -ne 0) { throw 'Could not publish the module-build receipt' }
