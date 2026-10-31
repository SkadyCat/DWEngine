param([ValidateSet('Build','Rebuild','Clean')][string]$Mode='Build')
$ErrorActionPreference = 'Stop'

# Builds the Opus editor target against the prebuilt engine (never relinks engine DLLs).
# If linking fails only because the installed engine lacks import .lib files, the repair script
# restores missing generated SDK inputs from genuine DLL exports, then retries UBT.
$projectRoot = $PSScriptRoot
$engineRoot = 'G:\UnrealEngine-Angelscript-5.8.3'
# Clean/rebuild necessarily replaces this project's DLLs. Ordinary build can
# still report an unchanged target while its game is running.
$opusRunning = @(Get-CimInstance Win32_Process -Filter "Name = 'UnrealEditor.exe' OR Name = 'UnrealEditor-Cmd.exe'" | Where-Object {
    $_.CommandLine -match '(?i)DungeonWorldOpus\.uproject(?:["\s]|$)'
})
if ($opusRunning.Count -and $Mode -ne 'Build') {
    throw "DungeonWorldOpus is running (PID $($opusRunning.ProcessId -join ', ')). Close its editor/game before a normal native build, or use Live Coding inside it."
}
$runDir = Join-Path $projectRoot 'Saved\Build'
New-Item -ItemType Directory -Path $runDir -Force | Out-Null
$env:TEMP = Join-Path $projectRoot 'Saved\Temp'
$env:TMP = $env:TEMP
$env:DOTNET_CLI_HOME = 'G:\UE-Angelscript-Build\DotNetHome'
$env:NUGET_PACKAGES = 'G:\UE-Angelscript-Build\NuGetPackages'
$env:NUGET_HTTP_CACHE_PATH = 'G:\UE-Angelscript-Build\NuGetHttpCache'
$env:DOTNET_CLI_TELEMETRY_OPTOUT = '1'
$env:DOTNET_NOLOGO = '1'
New-Item -ItemType Directory -Path $env:TEMP -Force | Out-Null
$logPath = Join-Path $runDir ("build-{0}.log" -f (Get-Date -Format 'yyyyMMdd-HHmmss'))
$opusModeArgs=@()
if ($Mode -eq 'Rebuild') {$opusModeArgs+='-Rebuild'}
if ($Mode -eq 'Clean') {$opusModeArgs+='-Clean'}
& "$engineRoot\Engine\Build\BatchFiles\Build.bat" DungeonWorldOpusEditor Win64 Development "-Project=$projectRoot\DungeonWorldOpus.uproject" -WaitMutex -UsePrecompiled -NoEngineChanges -NoHotReloadFromIDE "-log=$logPath" -NoUBA -MaxParallelActions=8 @opusModeArgs
$code = $LASTEXITCODE
if ($code -ne 0 -and $Mode -ne 'Clean' -and (Test-Path -LiteralPath $logPath)) {
    $text = Get-Content -LiteralPath $logPath -Raw
    if ($text -match 'LNK1181' -and $text -notmatch '(?im)(error\s+(C|D|RC)\d+|(?:^|\s)Error:|LNK2019|LNK2001)') {
        'link-only failure: restoring missing precompiled SDK imports'
        & python "$projectRoot\Tools\Build\repair_editor_target.py" --restore-sdk
        $code = $LASTEXITCODE
        if ($code -eq 0) {
            # Rebuild already compiled the objects. Retry only the remaining work.
            $logPath = Join-Path $runDir ("build-{0}-sdk-retry.log" -f (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
            & "$engineRoot\Engine\Build\BatchFiles\Build.bat" DungeonWorldOpusEditor Win64 Development "-Project=$projectRoot\DungeonWorldOpus.uproject" -WaitMutex -UsePrecompiled -NoEngineChanges -NoHotReloadFromIDE "-log=$logPath" -NoUBA -MaxParallelActions=8
            $code = $LASTEXITCODE
        }
    }
}
"exit=$code log=$logPath"
exit $code
