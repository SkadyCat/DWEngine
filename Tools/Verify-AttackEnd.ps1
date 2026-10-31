param()
$ErrorActionPreference='Stop'
$endRoot=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$endOut=Join-Path $endRoot 'Saved/Verification/AttackEnd'
New-Item -ItemType Directory -Path $endOut -Force | Out-Null
$endLog=Join-Path $endOut 'after.log'
$endArgs=@(('"'+$endRoot+'/DungeonWorldOpus.uproject"'),'/Engine/Maps/Entry?game=/Script/Angelscript.DWOTowerGameMode','-game','-DWOTransient','-DWOHeroArena','-DWOAttackEndTest','-DWODebug','-NullRHI','-NoLoadingScreen','-Multiprocess','-unattended','-nosplash','-nosound','-as-exit-on-error','-DisablePlugins=UE_MCP_Bridge',('-ABSLOG="'+$endLog+'"'),'-ExecCmds="t.MaxFPS 60,t.IdleWhenNotForeground 0"')
$endProcess=Start-Process -FilePath 'G:/UnrealEngine-Angelscript-5.8.3/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $endArgs -WorkingDirectory $endRoot -WindowStyle Hidden -PassThru
$endDeadline=(Get-Date).AddMinutes(3)
while(!$endProcess.HasExited -and (Get-Date)-lt $endDeadline){Start-Sleep -Milliseconds 500;$endProcess.Refresh()}
if(!$endProcess.HasExited){Stop-Process -Id $endProcess.Id -Force;throw 'Own Attack End probe timed out'}
$endProcess.WaitForExit();$endText=Get-Content -LiteralPath $endLog -Raw -Encoding utf8
if($endProcess.ExitCode-ne 0 -or $endText-match 'Angelscript: Error:|Fatal error:|ATTACK_END_FAIL'){throw "Attack End verification failed: $endLog"}
foreach($endMarker in @('ImmediateChainAndLocks','WhirlwindNoCooldown','NativeCooldownPreserved','Complete')){
 if($endText-notmatch ('ATTACK_END_PASS '+$endMarker)){throw "Missing Attack End check: $endMarker"}
}
Write-Output 'Attack End passed: actual authored marker, immediate next input, action locks, whirlwind configuration, native cooldown guards'
