$ErrorActionPreference='Stop'
$aggroRoot=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$aggroProof=Join-Path $aggroRoot 'Saved/Verification/MonsterAggro'
New-Item -ItemType Directory -Path $aggroProof -Force | Out-Null
$aggroLog=Join-Path $aggroProof 'native-aggro.log'
$aggroArgs=@(('"'+(Join-Path $aggroRoot 'DungeonWorldOpus.uproject')+'"'),'/Engine/Maps/Entry?game=/Script/Angelscript.DWOTowerGameMode','-game','-DWOTransient','-DWOHeroArena','-DWOAggroTest','-Multiprocess','-NullRHI','-unattended','-nosplash','-nosound','-as-exit-on-error',('-ABSLOG="'+$aggroLog+'"'))
$aggroProcess=Start-Process -FilePath 'G:/UnrealEngine-Angelscript-5.8.3/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $aggroArgs -WorkingDirectory $aggroRoot -WindowStyle Hidden -PassThru
Write-Output "Monster aggro verification PID=$($aggroProcess.Id) log=$aggroLog"
$aggroDeadline=(Get-Date).AddMinutes(3)
while(!$aggroProcess.HasExited -and (Get-Date)-lt $aggroDeadline){Start-Sleep -Milliseconds 300;$aggroProcess.Refresh()}
if(!$aggroProcess.HasExited){Stop-Process -Id $aggroProcess.Id;throw "Monster aggro verification timed out: $aggroLog"}
$aggroProcess.WaitForExit()
$aggroText=Get-Content -LiteralPath $aggroLog -Raw -Encoding UTF8
if($aggroProcess.ExitCode-ne 0 -or $aggroText-match 'AGGRO_TEST_FAIL|Angelscript: Error:|TowerError|Fatal error:' -or $aggroText-notmatch 'AGGRO_TEST_PASS Complete'){throw "Monster aggro verification failed: $aggroLog"}
Write-Output "Monster aggro verification passed: $aggroLog"
