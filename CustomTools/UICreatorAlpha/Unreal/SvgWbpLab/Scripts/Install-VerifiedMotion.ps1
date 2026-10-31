param(
    [string]$VerifiedProject = '',
    [string]$EngineRoot = 'G:\UnrealEngine-Angelscript-5.8.3',
    [string]$CanvasReceipt = '',
    [switch]$InstallVerifiedCanvas,
    [switch]$DryRun
)
$ErrorActionPreference = 'Stop'
$taskProjectRoot = Split-Path -Parent $PSScriptRoot
$taskRepoRoot = (Resolve-Path -LiteralPath (Join-Path $taskProjectRoot '..\..')).Path
if (-not $VerifiedProject) { $VerifiedProject = Join-Path $taskRepoRoot 'work\ue-motion-20261025\SvgWbpLab' }
$VerifiedProject = (Resolve-Path -LiteralPath $VerifiedProject).Path
$taskEvidence = Join-Path $taskRepoRoot 'exports\wbp-hud-native-20261025\motion\generic'
$taskReceipt = Get-Content -Raw -LiteralPath (Join-Path $taskEvidence 'native\motion-receipt.json') | ConvertFrom-Json
if (-not $taskReceipt.ok -or -not $taskReceipt.layoutAndHitAreaUnchanged -or -not $taskReceipt.authoredSixChannelsPassed) { throw 'Native authored-motion verification has not passed.' }
if (-not $CanvasReceipt) { $CanvasReceipt = Join-Path $taskEvidence 'import-receipt.json' }
$taskImport = Get-Content -Raw -LiteralPath $CanvasReceipt | ConvertFrom-Json
if (-not $taskImport.ok -or -not $taskImport.saved) { throw 'Verified WBP import is missing.' }
$taskEngineManifest = Get-Content -Raw -LiteralPath (Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.modules') | ConvertFrom-Json
$taskPluginManifest = Get-Content -Raw -LiteralPath (Join-Path $VerifiedProject 'Plugins\SvgCanvasBridge\Binaries\Win64\UnrealEditor.modules') | ConvertFrom-Json
if ($taskPluginManifest.BuildId -ne $taskEngineManifest.BuildId) { throw 'Verified DLLs belong to another engine build.' }
$taskSourceRoot = Join-Path $taskProjectRoot 'Plugins\SvgCanvasBridge\Source'
foreach ($taskSourceFile in Get-ChildItem -LiteralPath $taskSourceRoot -Recurse -File) {
    $taskRelative = $taskSourceFile.FullName.Substring($taskSourceRoot.Length + 1)
    $taskVerifiedFile = Join-Path $VerifiedProject "Plugins\SvgCanvasBridge\Source\$taskRelative"
    if (-not (Test-Path -LiteralPath $taskVerifiedFile) -or (Get-FileHash -LiteralPath $taskSourceFile.FullName).Hash -ne (Get-FileHash -LiteralPath $taskVerifiedFile).Hash) { throw "Source changed since verification: $taskRelative" }
}
$taskFiles = @(
    'Plugins\SvgCanvasBridge\Binaries\Win64\UnrealEditor-SvgCanvasRuntime.dll',
    'Plugins\SvgCanvasBridge\Binaries\Win64\UnrealEditor-SvgCanvasEditor.dll',
    'Plugins\SvgCanvasBridge\Binaries\Win64\UnrealEditor.modules'
)
if ($InstallVerifiedCanvas) {
    $taskFiles += @('Content\UICreator\WBP_CurrentCanvas.uasset', 'Content\UICreator\DA_CurrentCanvas.uasset')
    $taskMotionAssets = Join-Path $VerifiedProject 'Content\UICreator\Motions'
    foreach ($taskAsset in Get-ChildItem -LiteralPath $taskMotionAssets -Filter '*.uasset' -File) { $taskFiles += 'Content\UICreator\Motions\' + $taskAsset.Name }
}
foreach ($taskRelative in $taskFiles) { if (-not (Test-Path -LiteralPath (Join-Path $VerifiedProject $taskRelative))) { throw "Missing verified file: $taskRelative" } }
$taskProjectFile = (Join-Path $taskProjectRoot 'SvgWbpLab.uproject').Replace('\','/').ToLowerInvariant()
$taskEditors = @(Get-CimInstance Win32_Process -Filter "Name = 'UnrealEditor.exe' OR Name = 'UnrealEditor-Cmd.exe'" | Where-Object { $_.CommandLine -and $_.CommandLine.Replace('\','/').ToLowerInvariant().Contains($taskProjectFile) })
if ($DryRun) { [pscustomobject]@{ Ready = $true; Files = $taskFiles; OpenEditorPids = @($taskEditors | ForEach-Object { $_.ProcessId }); SourceHash = $taskImport.sourceHash } | ConvertTo-Json -Depth 4; return }
if ($taskEditors.Count) { throw 'Save and close the SvgWbpLab editor before replacing its loaded DLLs and WBP. Other projects can stay open.' }
$taskBackup = Join-Path $taskProjectRoot ('Saved\MotionBackups\' + [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ'))
foreach ($taskRelative in $taskFiles) {
    $taskDestination = Join-Path $taskProjectRoot $taskRelative
    if (Test-Path -LiteralPath $taskDestination) {
        $taskBackupFile = Join-Path $taskBackup $taskRelative
        New-Item -ItemType Directory -Path (Split-Path -Parent $taskBackupFile) -Force | Out-Null
        Copy-Item -LiteralPath $taskDestination -Destination $taskBackupFile
    }
    $taskVerifiedFile = Join-Path $VerifiedProject $taskRelative
    New-Item -ItemType Directory -Path (Split-Path -Parent $taskDestination) -Force | Out-Null
    Copy-Item -LiteralPath $taskVerifiedFile -Destination $taskDestination -Force
    if ((Get-FileHash -LiteralPath $taskVerifiedFile).Hash -ne (Get-FileHash -LiteralPath $taskDestination).Hash) { throw "Installed file readback differs: $taskRelative" }
}
@{ ok = $true; backup = $taskBackup; verifiedSourceHash = $taskImport.sourceHash; installedCanvas = [bool]$InstallVerifiedCanvas; files = $taskFiles } | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $taskEvidence 'installed-receipt.json') -Encoding utf8
Write-Output "Verified motion plugin installed. Canvas replacement: $InstallVerifiedCanvas. Backup: $taskBackup"
