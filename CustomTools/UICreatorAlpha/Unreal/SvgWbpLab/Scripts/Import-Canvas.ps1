param(
    [string]$Bundle = "$PSScriptRoot\..\..\..\exports\wbp-8662510d-a366-488d-9881-e28753c77337\bundle.json",
    [string]$EngineRoot = 'G:\UnrealEngine-Angelscript-5.8.3',
    [switch]$SkipBuild
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$Bundle = (Resolve-Path -LiteralPath $Bundle).Path
$exportRoot = Split-Path -Parent $Bundle
if (-not $SkipBuild) { & "$PSScriptRoot\Build.ps1" -EngineRoot $EngineRoot }
$editor = "$EngineRoot\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$common = @((Join-Path $projectRoot 'SvgWbpLab.uproject'), "-Bundle=$Bundle", '-Unattended', '-NoSplash', '-NoSound', '-UTF8Output')
$started = Get-Date
& $editor @common -run=SvgCanvasImport "-Output=$exportRoot\import-receipt.json" -NullRHI
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath "$exportRoot\import-receipt.json")) { throw 'Import did not produce a saved asset receipt' }
if ((Get-Item -LiteralPath "$exportRoot\import-receipt.json").LastWriteTime -lt $started) { throw 'Import receipt was not refreshed by this process' }
if (-not (Get-Content -Raw -LiteralPath "$exportRoot\import-receipt.json" | ConvertFrom-Json).ok) { throw 'Import receipt reports failure' }
# A separate editor process is required to prove the saved WBP reloads from disk.
& $editor @common -run=SvgCanvasVerify "-Output=$exportRoot\native" -AllowCommandletRendering -RenderOffscreen
if ($LASTEXITCODE -ne 0) { throw 'Native WBP reload/render failed' }
& python "$PSScriptRoot\compare_pixels.py" $Bundle
if ($LASTEXITCODE -ne 0) { throw "Pixel equality failed. Inspect $exportRoot\pixel-report.json" }
