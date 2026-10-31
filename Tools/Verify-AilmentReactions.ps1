param([int]$TimeoutSeconds=180)
$ErrorActionPreference='Stop'
$reactionRoot=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$reactionProof=Join-Path $reactionRoot 'Saved/Verification/AilmentReactions'
New-Item -ItemType Directory -Path $reactionProof -Force | Out-Null
$reactionLog=Join-Path $reactionProof ('native-'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'.log')
$reactionArgs=@(('"'+$reactionRoot+'/DungeonWorldOpus.uproject"'),'-DWOTransient','-Multiprocess','-unattended','-nosplash','-nosound','-NoLoadingScreen','-as-exit-on-error','-DisablePlugins=UE_MCP_Bridge','-NullRHI','-ExecCmds="Automation RunTests DWRules.Opus.AilmentReactions"','-TestExit="Automation Test Queue Empty"',('-ABSLOG="'+$reactionLog+'"'),('-ReportExportPath="'+$reactionProof+'/automation"'))
$reactionProcess=Start-Process -FilePath 'G:/UnrealEngine-Angelscript-5.8.3/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $reactionArgs -WorkingDirectory $reactionRoot -WindowStyle Hidden -PassThru
$reactionDeadline=(Get-Date).AddSeconds($TimeoutSeconds)
while(!$reactionProcess.HasExited -and (Get-Date)-lt $reactionDeadline){Start-Sleep -Milliseconds 500;$reactionProcess.Refresh()}
if(!$reactionProcess.HasExited){Stop-Process -Id $reactionProcess.Id -Force;throw "Own reaction verification process timed out: $reactionLog"}
$reactionProcess.WaitForExit()
$reactionText=Get-Content -LiteralPath $reactionLog -Raw -Encoding UTF8
if($reactionProcess.ExitCode-ne 0 -or $reactionText-match 'Angelscript: Error:|Fatal error:|Test Completed. Result=\{Fail' -or $reactionText-notmatch 'Test Completed. Result=\{Success\}.*AilmentReactions'){throw "Reaction verification failed: $reactionLog"}
$reactionText -split '\r?\n' | Where-Object {$_-match 'Test Completed|AilmentReactions'} | Select-Object -Last 8 | ForEach-Object {Write-Host $_}
Write-Output "Ailment reactions passed: $reactionLog"
