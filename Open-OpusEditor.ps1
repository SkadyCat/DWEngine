param([Parameter(ValueFromRemainingArguments=$true)][string[]]$EditorArguments)
$ErrorActionPreference='Stop'

$opusEditorPath='G:\UnrealEngine-Angelscript-5.8.3\Engine\Binaries\Win64\UnrealEditor.exe'
$opusProjectPath=Join-Path $PSScriptRoot 'DungeonWorldOpus.uproject'
if(!(Test-Path -LiteralPath $opusEditorPath)){throw "Unreal Editor not found: $opusEditorPath"}
if(!(Test-Path -LiteralPath $opusProjectPath)){throw "Opus project not found: $opusProjectPath"}

# Keep repeated double-clicks from creating another Opus editor during startup.
# A -game process is independent and does not replace an editor session.
$opusExistingEditor=Get-CimInstance Win32_Process -Filter "Name='UnrealEditor.exe'" | Where-Object {
    $_.CommandLine -and
    $_.CommandLine.Replace('/','\').IndexOf($opusProjectPath,[StringComparison]::OrdinalIgnoreCase) -ge 0 -and
    $_.CommandLine -notmatch '(?i)(?:^|\s)-game(?:\s|$)'
} | Select-Object -First 1
if($opusExistingEditor){
    Write-Host "Opus editor is already starting or open (PID $($opusExistingEditor.ProcessId))."
    Write-Host 'Use its existing window; no second editor was started.'
    exit 0
}

Write-Host 'Starting Opus editor. The Unreal startup splash shows loading progress.'
Write-Host "Log: $(Join-Path $PSScriptRoot 'Saved\Logs\DungeonWorldOpus.log')"
$opusLaunchArguments=@('"'+$opusProjectPath+'"')
if($EditorArguments){$opusLaunchArguments+=$EditorArguments}
# Leave the splash enabled so engine initialization has visible progress.
Start-Process -FilePath $opusEditorPath -ArgumentList $opusLaunchArguments -WorkingDirectory $PSScriptRoot -WindowStyle Normal
