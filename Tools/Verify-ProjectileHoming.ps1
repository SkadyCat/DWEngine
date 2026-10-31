param([int]$TimeoutSeconds=180)
$ErrorActionPreference='Stop'
$homingRoot=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$homingProof=Join-Path $homingRoot 'Saved/Verification/ProjectileHoming'
New-Item -ItemType Directory -Path $homingProof -Force | Out-Null
$homingLog=Join-Path $homingProof ('native-'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'.log')
$homingArgs=@(('"'+$homingRoot+'/DungeonWorldOpus.uproject"'),'-DWOTransient','-Multiprocess','-unattended','-nosplash','-nosound','-NoLoadingScreen','-as-exit-on-error','-DisablePlugins=UE_MCP_Bridge','-NullRHI','-ExecCmds="Automation RunTests DWRules.Opus.ProjectileHoming"','-TestExit="Automation Test Queue Empty"',('-ABSLOG="'+$homingLog+'"'),('-ReportExportPath="'+$homingProof+'/automation"'))
$homingProcess=Start-Process -FilePath 'G:/UnrealEngine-Angelscript-5.8.3/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $homingArgs -WorkingDirectory $homingRoot -WindowStyle Hidden -PassThru
$homingDeadline=(Get-Date).AddSeconds($TimeoutSeconds)
while(!$homingProcess.HasExited -and (Get-Date)-lt $homingDeadline){Start-Sleep -Milliseconds 500;$homingProcess.Refresh()}
if(!$homingProcess.HasExited){Stop-Process -Id $homingProcess.Id -Force;throw "Own homing verification process timed out: $homingLog"}
$homingProcess.WaitForExit()
$homingText=Get-Content -LiteralPath $homingLog -Raw -Encoding UTF8
if($homingProcess.ExitCode-ne 0 -or $homingText-match 'Angelscript: Error:|Fatal error:|Test Completed. Result=\{Fail' -or $homingText-notmatch 'Test Completed. Result=\{Success\}.*ProjectileHoming'){throw "Homing verification failed: $homingLog"}
$homingText -split '\r?\n' | Where-Object {$_-match 'Test Completed|ProjectileHoming'} | Select-Object -Last 8 | ForEach-Object {Write-Host $_}
Write-Output "Projectile homing passed: $homingLog"
