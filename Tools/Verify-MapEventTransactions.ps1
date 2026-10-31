$ErrorActionPreference='Stop'
$eventRoot=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$eventProof=Join-Path $eventRoot 'Saved/Verification/MapEvents'
New-Item -ItemType Directory -Path $eventProof -Force | Out-Null
$eventLog=Join-Path $eventProof 'transactions.log'
$eventArgs=@(('"'+(Join-Path $eventRoot 'DungeonWorldOpus.uproject')+'"'),'/Engine/Maps/Entry','-NullRHI','-unattended','-nosplash','-nosound','-Multiprocess','-as-exit-on-error','-ExecCmds="Automation RunTests Opus.MapEvents"','-TestExit="Automation Test Queue Empty"',('-ReportExportPath="'+(Join-Path $eventProof 'automation')+'"'),('-ABSLOG="'+$eventLog+'"'))
$eventProcess=Start-Process -FilePath 'G:/UnrealEngine-Angelscript-5.8.3/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $eventArgs -WorkingDirectory $eventRoot -WindowStyle Hidden -PassThru
$eventDeadline=(Get-Date).AddMinutes(5)
while(!$eventProcess.HasExited -and (Get-Date)-lt $eventDeadline){Start-Sleep -Milliseconds 500;$eventProcess.Refresh()}
if(!$eventProcess.HasExited){Stop-Process -Id $eventProcess.Id;throw "Event automation timed out: $eventLog"}
$eventProcess.WaitForExit();$eventText=Get-Content -LiteralPath $eventLog -Raw -Encoding UTF8
if($eventProcess.ExitCode-ne 0 -or $eventText-match 'Angelscript: Error:|Result=\{Fail\}|Fatal error:' -or $eventText-notmatch 'Result=\{Success\}.*ItemTransactions'){throw "Event transactions failed: $eventLog"}
Write-Output "Passed: $eventLog"
