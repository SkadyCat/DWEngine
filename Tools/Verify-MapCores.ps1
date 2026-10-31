param([switch]$Render)
$ErrorActionPreference='Stop'
$opusRoot=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$opusOut=Join-Path $opusRoot 'Saved\Verification\MapCore'
New-Item -ItemType Directory -Path $opusOut -Force | Out-Null
function Run-MapCoreCheck([string]$Name,[string[]]$Flags,[bool]$Visual=$false){
 $opusLog=Join-Path $opusOut ($Name+'.log')
 $opusArgs=@('"'+(Join-Path $opusRoot 'DungeonWorldOpus.uproject')+'"','/Engine/Maps/Entry?game=/Script/Angelscript.DWOTowerGameMode','-game','-DWOTransient','-unattended','-nosplash','-nosound','-NoLoadingScreen',"-ABSLOG=$opusLog")+$Flags
 if(!$Visual){$opusArgs+='-nullrhi'}else{$opusArgs+=@('-windowed','-ResX=1600','-ResY=900')}
 $opusProcess=Start-Process -FilePath 'G:\UnrealEngine-Angelscript-5.8.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' -ArgumentList $opusArgs -WorkingDirectory $opusRoot -WindowStyle Hidden -PassThru
 $opusDeadline=(Get-Date).AddMinutes(4)
 while(!$opusProcess.HasExited -and (Get-Date)-lt $opusDeadline){Start-Sleep -Milliseconds 500;$opusProcess.Refresh()}
 if(!$opusProcess.HasExited){Stop-Process -Id $opusProcess.Id -Force;throw "Map core check timed out: $Name"}
 $opusProcess.WaitForExit();$opusText=Get-Content -LiteralPath $opusLog -Raw -Encoding UTF8
 if($opusProcess.ExitCode-ne 0 -or $opusText-match 'Angelscript: Error:|MAP_CORE_TEST_FAIL|TowerError|Fatal error:'){throw "Map core check failed: $opusLog"}
 Write-Host "Passed $Name"
 return $opusText
}
$opusText=Run-MapCoreCheck 'integration' @('-DWOMapCoreTest','-DWOMapCoreProfileTest','-DWOGod','-DWOSmoke=120')
foreach($opusResult in @('NativeCatalogAndUICreatorActions','LiveWaveAndScaling','RetryPreservesCore')){if($opusText-notmatch "MAP_CORE_TEST_PASS $opusResult"){throw "Map core scenario missing: $opusResult"}}
$opusText=Run-MapCoreCheck 'disk-reload' @('-DWOMapCoreReloadTest','-DWOMapCoreProfileTest','-DWOSmoke=20')
if($opusText-notmatch 'MAP_CORE_TEST_PASS DiskReload'){throw 'Persisted core was not restored'}
if($Render){
 $opusText=Run-MapCoreCheck 'preview' @('-DWOMapCore=map_core_equipment_t10','-DWOView=map-core-picker',('-DWOCapture='+($opusOut.Replace('\','/')+'/preview.png')),'-DWOSmoke=9') $true
 if(!(Test-Path -LiteralPath (Join-Path $opusOut 'preview.png'))){throw 'UE screenshot missing'}
}
Write-Host "Map core verification passed: $opusOut"
