param(
    [string]$Bundle = "$PSScriptRoot\..\..\..\exports\wbp-ac273a29-3276-4efb-9efe-9abf74c0cb2b\bundle.json",
    [string]$EngineRoot = 'G:\UnrealEngine-Angelscript-5.8.3',
    [switch]$SkipBuild
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$projectFile = Join-Path $projectRoot 'SvgWbpLab.uproject'
$Bundle = (Resolve-Path -LiteralPath $Bundle).Path
$outputRoot = Join-Path (Split-Path -Parent $Bundle) 'responsive'
$active = Get-CimInstance Win32_Process -Filter "Name = 'UnrealEditor.exe' OR Name = 'UnrealEditor-Cmd.exe'" | Where-Object { $_.CommandLine -match [regex]::Escape($projectFile) }
if ($active) { throw 'Close this SvgWbpLab editor before rebuilding and importing its generated WBP. Other projects can stay open.' }
if (-not $SkipBuild) { & "$PSScriptRoot\Build.ps1" -EngineRoot $EngineRoot }
New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$common = @($projectFile, "-Bundle=$Bundle", '-Unattended', '-NoSplash', '-NoSound', '-UTF8Output')
& $editor @common -run=SvgCanvasImport "-Output=$outputRoot\import-receipt.json" -NullRHI
if ($LASTEXITCODE -ne 0) { throw 'Responsive WBP import failed' }
& $editor @common -run=SvgCanvasVerify "-Output=$outputRoot\native" -AllowCommandletRendering -RenderOffscreen
if ($LASTEXITCODE -ne 0) { throw 'Responsive native layout verification failed' }
$native = Get-Content -Raw -LiteralPath "$outputRoot\native\reload-receipt.json" | ConvertFrom-Json
if (-not $native.responsiveViewportVerified) { throw 'Native verification did not test changing screen sizes' }

$runtime = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
foreach ($screen in @(@(1280,720), @(1024,768), @(2258,1080), @(2560,1080))) {
    $width = $screen[0]; $height = $screen[1]
    $captureRoot = Join-Path $outputRoot "game-$width-$height"
    New-Item -ItemType Directory -Path $captureRoot -Force | Out-Null
    $arguments = @('"' + $projectFile + '"', '-game', '-windowed', '-RenderOffscreen', '-ForceRes', "-ResX=$width", "-ResY=$height", '-NoSplash', '-NoSound', '-Unattended', '-NoScreenMessages', '-UTF8Output', '"-SvgCanvasRuntimeOutput=' + $captureRoot + '"', '"-ABSLOG=' + (Join-Path $captureRoot 'game.log') + '"')
    $started = Get-Date
    $process = Start-Process -FilePath $runtime -ArgumentList $arguments -WindowStyle Hidden -PassThru
    if (-not $process.WaitForExit(180000)) {
        $owned = Get-CimInstance Win32_Process -Filter "ProcessId = $($process.Id)"
        if ($owned.CommandLine -match [regex]::Escape($captureRoot)) { Stop-Process -Id $process.Id }
        throw "Runtime capture timed out: $captureRoot"
    }
    $receiptFile = Join-Path $captureRoot 'runtime-receipt.json'
    if (-not (Test-Path -LiteralPath $receiptFile) -or (Get-Item -LiteralPath $receiptFile).LastWriteTime -lt $started) { throw "Game did not produce a fresh runtime receipt: $captureRoot" }
    $receipt = Get-Content -Raw -LiteralPath $receiptFile | ConvertFrom-Json
    if ($process.ExitCode -ne 0 -or -not $receipt.ok) { throw "Game viewport layout differs: $captureRoot" }
    if ($receipt.viewportWidth -ne $width -or $receipt.viewportHeight -ne $height) { throw "Game viewport did not use the requested screen size: $captureRoot" }
    Write-Output "Game viewport verified: $width x $height, layout error $($receipt.maxLayoutErrorPixels)"
}
Write-Output "Native and game viewport verification passed. Evidence: $outputRoot"
