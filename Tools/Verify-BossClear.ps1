$ErrorActionPreference='Stop'
$bossRoot=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$bossOut=Join-Path $bossRoot 'Saved/Verification/BossClear'
New-Item -ItemType Directory -Path $bossOut -Force | Out-Null
$bossLog=Join-Path $bossOut 'native.log'
$bossProfile=Join-Path $bossRoot 'Saved/RuleTower/Profile.v2.json'
$bossProfileBefore=if(Test-Path -LiteralPath $bossProfile){(Get-FileHash -LiteralPath $bossProfile).Hash}else{''}
$bossArgs=@(('"'+$bossRoot+'/DungeonWorldOpus.uproject"'),'/Engine/Maps/Entry?game=/Script/Angelscript.DWOTowerGameMode','-game','-DWOTransient','-DWOHeroArena','-DWOBossClearTest','-DWOSmoke=150','-NullRHI','-Multiprocess','-unattended','-nosplash','-nosound','-NoLoadingScreen','-as-exit-on-error','-DisablePlugins=UE_MCP_Bridge',('-ABSLOG="'+$bossLog+'"'),'-ExecCmds="t.MaxFPS 60,t.IdleWhenNotForeground 0,s.AsyncLoadingTimeLimit 8"')
$bossProcess=Start-Process -FilePath 'G:/UnrealEngine-Angelscript-5.8.3/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $bossArgs -WorkingDirectory $bossRoot -WindowStyle Hidden -PassThru
$bossDeadline=(Get-Date).AddMinutes(4)
while(!$bossProcess.HasExited -and (Get-Date)-lt $bossDeadline){Start-Sleep -Milliseconds 500;$bossProcess.Refresh()}
if(!$bossProcess.HasExited){$bossProcess.Kill();throw 'Own transient boss completion test timed out'}
$bossProcess.WaitForExit();$bossText=Get-Content -LiteralPath $bossLog -Raw -Encoding utf8
if($bossProcess.ExitCode-ne 0 -or $bossText-match 'Angelscript: Error:|BOSS_CLEAR_FAIL|AS_TEST_FAIL|TowerError|Fatal error:'){throw "Boss completion test failed: $bossLog"}
$bossChecks=@('BossKillCompletesWithMinions','CompletionPopupAndReturn','PostClearCombatAndSingleSettlement','Complete')
foreach($bossCheck in $bossChecks){if($bossText-notmatch ('BOSS_CLEAR_PASS '+$bossCheck)){throw "Missing boss completion scenario: $bossCheck"}}
if($bossText-notmatch 'AS_TEST_PASS TowerFlow'){throw 'Boss flow regression did not run'}
$bossProfileAfter=if(Test-Path -LiteralPath $bossProfile){(Get-FileHash -LiteralPath $bossProfile).Hash}else{''}
[ordered]@{ok=$true;timestamp=(Get-Date).ToString('o');execution='native Unreal transient world / NullRHI';checks=$bossChecks;profileHashBefore=$bossProfileBefore;profileHashAfter=$bossProfileAfter;profileUnchanged=($bossProfileBefore -eq $bossProfileAfter);formalProfileWrittenByTest=$false;rulesEngineChanged=$false;combatNumbersChanged=$false;log=$bossLog} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $bossOut 'receipt.json') -Encoding utf8
'Boss completion passed: native boss death with minions alive, quest, drops, completion popup, continued combat, single settlement and flow regressions'
