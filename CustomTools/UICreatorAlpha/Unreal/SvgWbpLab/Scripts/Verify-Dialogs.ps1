param(
    [string]$Bundle = "$PSScriptRoot\..\..\..\exports\wbp-ac273a29-3276-4efb-9efe-9abf74c0cb2b\bundle.json",
    [string]$EngineRoot = 'G:\UnrealEngine-Angelscript-5.8.3',
    [switch]$SkipBuild,
    [switch]$SkipImport,
    [switch]$SkipNative
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$projectFile = Join-Path $projectRoot 'SvgWbpLab.uproject'
$Bundle = (Resolve-Path -LiteralPath $Bundle).Path
$job = Split-Path -Parent $Bundle
$outputRoot = Join-Path $job 'dialogs'
$active = Get-CimInstance Win32_Process -Filter "Name = 'UnrealEditor.exe' OR Name = 'UnrealEditor-Cmd.exe'" | Where-Object { $_.CommandLine -match [regex]::Escape($projectFile) }
if ($active) { throw 'Close this SvgWbpLab editor before building or importing its generated WBP.' }
if (-not $SkipBuild) { & "$PSScriptRoot\Build.ps1" -EngineRoot $EngineRoot }
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$common = @($projectFile, "-Bundle=$Bundle", '-Unattended', '-NoSplash', '-NoSound', '-UTF8Output')
if (-not $SkipImport) {
    & $editor @common -run=SvgCanvasImport "-Output=$job\import-receipt.json" -NullRHI *> "$job\import.log"
    if ($LASTEXITCODE -ne 0) { throw "Dialog import failed. See $job\import.log" }
}
if (-not $SkipNative) {
    & $editor @common -run=SvgCanvasVerify "-Output=$job\native" -AllowCommandletRendering -RenderOffscreen *> "$job\verify.log"
    if ($LASTEXITCODE -ne 0) { throw "Native dialog verification failed. See $job\verify.log" }
    $native = Get-Content -Raw -LiteralPath "$job\native\reload-receipt.json" | ConvertFrom-Json
    if ($native.responsiveSceneChecks.Count -lt 215) { throw 'Native verification did not cover every exported interface and screen size.' }
}
$contract = Get-Content -Raw -LiteralPath $Bundle | ConvertFrom-Json -Depth 100
$runtime = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
foreach ($case in @(@('character',2258,1080), @('character',1280,720), @('character',1024,768), @('shop',2560,1080))) {
    $state=$case[0]; $width=$case[1]; $height=$case[2]
    $captureRoot=Join-Path $outputRoot "$state-$width-$height"
    New-Item -ItemType Directory -Path $captureRoot -Force | Out-Null
    $expected=$contract.responsiveLayouts.$state | Where-Object {$_.width -eq $width -and $_.height -eq $height}
    if (-not $expected) { throw "No independent source fixture for $state $width x $height" }
    $expectedFile=Join-Path $captureRoot 'source-layout.json'
    [IO.File]::WriteAllText($expectedFile, ($expected | ConvertTo-Json -Depth 20 -Compress), [Text.UTF8Encoding]::new($false))
    $arguments=@('"'+$projectFile+'"','-game','-windowed','-RenderOffscreen','-ForceRes',"-ResX=$width","-ResY=$height",'-NoSplash','-NoSound','-Unattended','-NoScreenMessages','-UTF8Output',"-SvgCanvasRuntimeState=$state",'"-SvgCanvasExpectedLayout='+$expectedFile+'"','"-SvgCanvasRuntimeOutput='+$captureRoot+'"','"-ABSLOG='+(Join-Path $captureRoot 'game.log')+'"')
    $started=Get-Date
    $process=Start-Process -FilePath $runtime -ArgumentList $arguments -WindowStyle Hidden -PassThru
    if (-not $process.WaitForExit(180000)) {
        $owned=Get-CimInstance Win32_Process -Filter "ProcessId = $($process.Id)"
        if ($owned.CommandLine -match [regex]::Escape($captureRoot)) { Stop-Process -Id $process.Id }
        throw "Runtime dialog verification timed out: $captureRoot"
    }
    $receiptFile=Join-Path $captureRoot 'runtime-receipt.json'
    if (-not (Test-Path -LiteralPath $receiptFile) -or (Get-Item -LiteralPath $receiptFile).LastWriteTime -lt $started) { throw "Missing fresh game receipt: $captureRoot" }
    $receipt=Get-Content -Raw -LiteralPath $receiptFile | ConvertFrom-Json
    if ($process.ExitCode -ne 0 -or -not $receipt.ok -or $receipt.viewportWidth -ne $width -or $receipt.viewportHeight -ne $height -or $receipt.responsiveSourceNodesChecked -ne $expected.nodes.Count) { throw "Runtime dialog layout differs: $captureRoot" }
    Write-Output "Game dialog verified: $state $width x $height, $($receipt.responsiveSourceNodesChecked) controls, error $($receipt.maxLayoutErrorPixels) px"
}
Write-Output "Dialog verification completed: $outputRoot"
