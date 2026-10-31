param([switch]$Preview,[ValidateSet('fusion','fusion-result','altar','reward')][string]$View='fusion')
$ErrorActionPreference='Stop'
$eventRoot=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$eventProof=Join-Path $eventRoot 'Saved/Verification/MapEvents'
New-Item -ItemType Directory -Path $eventProof -Force | Out-Null
$eventName=if($Preview){'preview-'+$View}else{'integration'}
$eventLog=Join-Path $eventProof ($eventName+'.log')
if(Test-Path -LiteralPath $eventLog){$eventAttempts=Join-Path $eventProof 'attempts';New-Item -ItemType Directory -Path $eventAttempts -Force|Out-Null;Copy-Item -LiteralPath $eventLog -Destination (Join-Path $eventAttempts ($eventName+'-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff')+'.log'))}

$eventArgs=@(('"'+(Join-Path $eventRoot 'DungeonWorldOpus.uproject')+'"'),'/Engine/Maps/Entry?game=/Script/Angelscript.DWOTowerGameMode','-game','-DWOTransient','-DWOHeroArena','-DWOGod','-unattended','-nosplash','-nosound','-NoLoadingScreen','-Multiprocess','-as-exit-on-error',('-ABSLOG="'+$eventLog+'"'))
if($Preview){$eventArgs+=@('-DWOEventsPreview',('-DWOEventView='+$View),('-DWOEventCapture="'+(Join-Path $eventProof ($eventName+'.png'))+'"'),'-RenderOffscreen','-windowed','-ResX=1600','-ResY=1000')}else{$eventArgs+=@('-DWOMapEventTest','-nullrhi')}
$eventProcess=Start-Process -FilePath 'G:/UnrealEngine-Angelscript-5.8.3/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $eventArgs -WorkingDirectory $eventRoot -WindowStyle Hidden -PassThru
$eventDeadline=(Get-Date).AddMinutes(5)
while(!$eventProcess.HasExited -and (Get-Date)-lt $eventDeadline){Start-Sleep -Milliseconds 500;$eventProcess.Refresh()}
if(!$eventProcess.HasExited){Stop-Process -Id $eventProcess.Id;throw "Event verification timed out: $eventLog"}
$eventProcess.WaitForExit();$eventText=Get-Content -LiteralPath $eventLog -Raw -Encoding UTF8
if($eventProcess.ExitCode-ne 0 -or $eventText-match 'Angelscript: Error:|EVENT_TEST_FAIL|Fatal error:|TowerError'){throw "Event verification failed: $eventLog"}
if(!$Preview -and $eventText-notmatch 'EVENT_TEST_PASS EventItemsSaveReload'){throw 'Integration did not finish'}
if($Preview){$eventImage=Join-Path $eventProof ($eventName+'.png');if(!(Test-Path -LiteralPath $eventImage) -or (Get-Item -LiteralPath $eventImage).Length-lt 1000){throw "Missing native preview image: $eventImage"}}
Write-Output "Passed: $eventLog"
