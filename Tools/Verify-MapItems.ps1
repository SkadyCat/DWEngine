param([switch]$Render,[switch]$Reload,[switch]$Online)
$ErrorActionPreference='Stop'
$mapRoot=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$mapProof=Join-Path $mapRoot 'Saved/Verification/MapItems'
New-Item -ItemType Directory -Path $mapProof -Force | Out-Null
$mapName=if($Render){'inspector'}elseif($Reload){'reload'}else{'integration'}
$mapLog=Join-Path $mapProof ($mapName+'.log')
$mapArgs=@(('"'+$mapRoot+'/DungeonWorldOpus.uproject"'),'/Game/ClaudeDungeon/Maps/L_OpusDungeon?game=/Script/Angelscript.DWOTowerGameMode','-game','-DWOTransient','-DWOMapItemTest','-DWOMapItemProfileTest','-Multiprocess','-as-exit-on-error','-unattended','-nosplash','-nosound','-NoLoadingScreen','-DWOSmoke=12',('-ABSLOG="'+$mapLog+'"'))
if($Render){$mapArgs+=@('-DWOMapItemPreview','-windowed','-RenderOffscreen','-ForceRes','-ResX=1600','-ResY=900','-SvgCanvasNoMotion',('-DWOCapture='+$mapProof.Replace('\','/')+'/inspector.png'),'-ExecCmds="t.MaxFPS 60,t.IdleWhenNotForeground 0,Slate.bAllowThrottling 0"')}else{$mapArgs+='-nullrhi'}
if($Reload){$mapArgs+='-DWOMapItemReloadTest'}
if($Online){$mapArgs+='-DWODropsOnline'}
# A game holds native DLLs open. Wait for queued builds before launching verification.
$mapBuildDeadline=(Get-Date).AddMinutes(12)
$mapBuildNotice=$false
while(@(Get-CimInstance Win32_Process -Filter "Name = 'dotnet.exe' OR Name = 'cmd.exe'" | Where-Object { $_.CommandLine -match 'DungeonWorldOpusEditor' -and $_.CommandLine -match 'DungeonWorldOpus.uproject' -and $_.CommandLine -match 'UnrealBuildTool.dll|Build.bat' }).Count){
 if(!$mapBuildNotice){Write-Output 'Waiting for native builds before map verification';$mapBuildNotice=$true}
 if((Get-Date)-gt$mapBuildDeadline){throw 'Native build queue did not finish'}
 Start-Sleep -Seconds 2
}
$mapStarted=Get-Date
$mapProcess=Start-Process -FilePath 'G:/UnrealEngine-Angelscript-5.8.3/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $mapArgs -WorkingDirectory $mapRoot -WindowStyle Hidden -PassThru
$mapDeadline=(Get-Date).AddMinutes(4)
while(!$mapProcess.HasExited -and (Get-Date)-lt $mapDeadline){Start-Sleep -Milliseconds 500;$mapProcess.Refresh()}
if(!$mapProcess.HasExited){Stop-Process -Id $mapProcess.Id -Force;throw "Map check timed out: $mapLog"}
$mapProcess.WaitForExit();$mapText=Get-Content -LiteralPath $mapLog -Raw -Encoding UTF8
if($mapProcess.ExitCode-ne 0 -or $mapText-match 'Angelscript: Error:|MAP_ITEMS_FAIL|TowerError|Fatal error:'){throw "Map check failed: $mapLog"}
$mapExpected=if($Render){'InspectorPreview'}elseif($Reload){'SeparateProcessReload'}else{'PurchasePersistenceUseAndTravel'}
if($mapText-notmatch ('MAP_ITEMS_PASS '+$mapExpected)){throw "Map checks did not complete: $mapLog"}
if($Render){$mapImage=Get-Item -LiteralPath (Join-Path $mapProof 'inspector.png');if($mapImage.LastWriteTime-lt $mapStarted){throw 'Stale map screenshot'}}
Write-Output "Passed $mapName"
