param([switch]$Visuals)
$ErrorActionPreference='Stop'
$entityRoot=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$entityProof=Join-Path $entityRoot 'Saved/Verification/EntityOverlays'
New-Item -ItemType Directory -Path $entityProof -Force | Out-Null
$entityLog=Join-Path $entityProof ($(if($Visuals){'visual'}else{'integration'})+'.log')
$entityArgs=@('"'+(Join-Path $entityRoot 'DungeonWorldOpus.uproject')+'"','/Engine/Maps/Entry?game=/Script/Angelscript.DWOTowerGameMode','-game','-DWOTransient','-DWOEntityUITest','-DWOGod','-DWOSmoke=30','-Multiprocess','-unattended','-nosplash','-nosound','-NoLoadingScreen',"-ABSLOG=$entityLog")
if($Visuals){$entityCapture=(Join-Path $entityProof 'native-statuses.png').Replace('\','/');$entityChillCapture=(Join-Path $entityProof 'native-frost-chill.png').Replace('\','/');$entityArgs+=@('-windowed','-ResX=1600','-ResY=900','-SvgCanvasNoMotion',"-DWOEntityCapture=$entityCapture","-DWOEntityChillCapture=$entityChillCapture")}else{$entityArgs+='-nullrhi'}
$entityStarted=Get-Date
$entityProcess=Start-Process -FilePath 'G:/UnrealEngine-Angelscript-5.8.3/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $entityArgs -WorkingDirectory $entityRoot -WindowStyle Hidden -PassThru
$entityDeadline=(Get-Date).AddMinutes(3)
while(!$entityProcess.HasExited -and (Get-Date)-lt $entityDeadline){Start-Sleep -Milliseconds 500;$entityProcess.Refresh()}
if(!$entityProcess.HasExited){Stop-Process -Id $entityProcess.Id -Force;throw "Entity overlay verification timed out: $entityLog"}
$entityProcess.WaitForExit();$entityText=Get-Content -LiteralPath $entityLog -Raw -Encoding UTF8
if($entityProcess.ExitCode-ne 0 -or $entityText-match 'Angelscript: Error:|ENTITY_UI_FAIL|TowerError|Fatal error:'){throw "Entity overlay verification failed: $entityLog"}
foreach($entityCheck in @('ReadyPlayerAndInput','FrostHitChill','NativePoolsAndNineStatuses','Pause','ExpiryCleanup','HealRespawnDespawn','RuntimeIntegration')){if($entityText-notmatch ('ENTITY_UI_PASS '+$entityCheck)){throw "Missing entity overlay result: $entityCheck"}}
if($Visuals){foreach($entityImagePath in @($entityCapture,$entityChillCapture)){$entityImage=Get-Item -LiteralPath $entityImagePath;if($entityImage.LastWriteTime-lt $entityStarted){throw 'Stale entity overlay capture'}}}
Write-Host "Entity overlays verified: $entityLog"
