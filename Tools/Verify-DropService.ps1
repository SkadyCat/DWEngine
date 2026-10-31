param([string]$BaseUrl='http://127.0.0.1:5188',[switch]$Subset,[switch]$ExpectFailure)
$ErrorActionPreference='Stop'
$opusRoot=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$out=Join-Path $opusRoot 'Saved/Verification/TradeBackend'
New-Item -ItemType Directory -Path $out -Force | Out-Null
$name=if($ExpectFailure){'service-unavailable'}elseif($Subset){'server-subset'}else{'online-pool'}
$log=Join-Path $out ($name+'.log')
$args=@(('"'+(Join-Path $opusRoot 'DungeonWorldOpus.uproject')+'"'),'/Engine/Maps/Entry?game=/Script/Angelscript.DWOTowerGameMode','-game','-DWOTransient','-DWOHeroArena','-DWOGod','-DWODropServiceTest','-nullrhi','-unattended','-nosplash','-nosound','-NoLoadingScreen','-Multiprocess','-as-exit-on-error',('-ABSLOG='+$log),('-DWODropService='+$BaseUrl))
if($Subset){$args+='-DWODropSubset'}
if($ExpectFailure){$args+='-DWODropExpectFailure'}
$process=Start-Process -FilePath 'G:/UnrealEngine-Angelscript-5.8.3/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $args -WorkingDirectory $opusRoot -WindowStyle Hidden -PassThru
$deadline=(Get-Date).AddMinutes(4)
while(!$process.HasExited -and (Get-Date)-lt $deadline){Start-Sleep -Milliseconds 500;$process.Refresh()}
if(!$process.HasExited){Stop-Process -Id $process.Id;throw "Drop service verification timed out: $log"}
$process.WaitForExit();$text=Get-Content -LiteralPath $log -Raw -Encoding UTF8
if($process.ExitCode-ne 0 -or $text-match 'Angelscript: Error:|DROP_TEST_FAIL|Fatal error:|TowerError'){throw "Drop service test failed: $log"}
$required=if($ExpectFailure){@('ServiceUnavailableFailsClosed')}else{@('InitialPoolAndCoreInvalidation','NativeBossUsesServerPool','MapReloadRequestsAgain')}
foreach($result in $required){if($text-notmatch "DROP_TEST_PASS $result"){throw "Missing $result : $log"}}
Write-Output "Passed: $log"
