param(
    [string]$ProjectRoot = (Split-Path -Parent $PSScriptRoot),
    [Parameter(Mandatory=$true)][string]$Bundle,
    [string]$Output,
    [string]$EngineRoot = 'G:\UnrealEngine-Angelscript-5.8.3',
    [switch]$SkipImport
)
$ErrorActionPreference = 'Stop'
$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path
$Bundle = (Resolve-Path -LiteralPath $Bundle).Path
if (-not $Output) { $Output = Join-Path (Split-Path -Parent $Bundle) 'audio-verification' }
$Output = [IO.Path]::GetFullPath($Output)
New-Item -ItemType Directory -Force -Path $Output | Out-Null
$project = Join-Path $ProjectRoot 'SvgWbpLab.uproject'
$openEditor = Get-CimInstance Win32_Process | Where-Object { $_.Name -like 'UnrealEditor*.exe' -and $_.CommandLine.Replace('/', '\').Contains($project) }
if ($openEditor) { throw 'Save and close this SvgWbpLab project before verifying saved assets.' }
$cmd = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
$common = @("`"$project`"", '-Unattended', '-NoSplash', '-UTF8Output')
$started = Get-Date
if (-not $SkipImport) {
    $process = Start-Process -FilePath $cmd -ArgumentList ($common + @('-run=SvgCanvasImport', "`"-Bundle=$Bundle`"", "`"-Output=$Output\import-receipt.json`"", "`"-ABSLOG=$Output\import.log`"", '-NullRHI', '-NoSound')) -WindowStyle Hidden -PassThru -Wait
    if ($process.ExitCode -ne 0) { throw "Audio import failed; inspect $Output\import.log" }
}
$process = Start-Process -FilePath $cmd -ArgumentList ($common + @('-run=SvgCanvasSoundVerify', "`"-Output=$Output\native`"", "`"-ABSLOG=$Output\reload.log`"", '-NullRHI', '-NoSound')) -WindowStyle Hidden -PassThru -Wait
if ($process.ExitCode -ne 0) { throw "Saved sound verification failed; inspect $Output\reload.log" }
# Audio must be enabled for the actual game-world playback check.
$process = Start-Process -FilePath $editor -ArgumentList ($common + @('/Engine/Maps/Entry', '-game', '-Windowed', '-ResX=1280', '-ResY=720', '-ForceRes', '-RenderOffscreen', "`"-SvgCanvasVerifyAudio=$Output\game`"", "`"-ABSLOG=$Output\game.log`"")) -WindowStyle Hidden -PassThru -Wait
if ($process.ExitCode -ne 0) { throw "Game playback verification failed; inspect $Output\game.log" }
foreach ($relative in @('native\sound-receipt.json', 'game\audio-runtime.json')) {
    $file = Join-Path $Output $relative
    if (-not (Test-Path -LiteralPath $file) -or (Get-Item -LiteralPath $file).LastWriteTime -lt $started) { throw "Missing fresh receipt: $relative" }
    $receipt = Get-Content -LiteralPath $file -Raw | ConvertFrom-Json
    if (-not $receipt.ok) { throw "Verification failed: $relative" }
    $receipt | ConvertTo-Json -Depth 5
}
