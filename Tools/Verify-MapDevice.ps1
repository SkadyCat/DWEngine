param([switch]$Render,[switch]$Reload)
$ErrorActionPreference='Stop'
$mapRoot=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$mapProof=Join-Path $mapRoot 'Saved/Verification/MapDevice';New-Item -ItemType Directory -Force $mapProof | Out-Null
$mapName=if($Render){'game-render'}elseif($Reload){'game-reload'}else{'game-integration'}
$mapLog=Join-Path $mapProof ($mapName+'.log')
$mapArgs=@(('"'+$mapRoot+'/DungeonWorldOpus.uproject"'),'/Game/ClaudeDungeon/Maps/L_OpusDungeon?game=/Script/Angelscript.DWOTowerGameMode','-game','-DWOTransient','-DWOMapDeviceTest','-DWOMapItemProfileTest','-Multiprocess','-as-exit-on-error','-unattended','-nosplash','-nosound','-NoLoadingScreen','-DisablePlugins=UE_MCP_Bridge','-DWOSmoke=15',('-ABSLOG="'+$mapLog+'"'))
if($Render){$mapArgs+=@('-DWOMapDevicePreview','-windowed','-RenderOffscreen','-ForceRes','-ResX=1600','-ResY=900',('-DWOCapture='+$mapProof.Replace('\','/')+'/game.png'),'-ExecCmds="t.MaxFPS 60,t.IdleWhenNotForeground 0,Slate.bAllowThrottling 0"')}else{$mapArgs+='-nullrhi'}
if($Reload){$mapArgs+='-DWOMapItemReloadTest'}
$mapStarted=Get-Date
$mapProcess=Start-Process -FilePath 'G:/UnrealEngine-Angelscript-5.8.3/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $mapArgs -WorkingDirectory $mapRoot -WindowStyle Hidden -PassThru
$mapDeadline=(Get-Date).AddMinutes(4)
while(!$mapProcess.HasExited -and (Get-Date)-lt $mapDeadline){Start-Sleep -Milliseconds 500;$mapProcess.Refresh()}
if(!$mapProcess.HasExited){Stop-Process -Id $mapProcess.Id -Force;throw "Map device check timed out: $mapLog"}
$mapProcess.WaitForExit();$mapText=Get-Content -LiteralPath $mapLog -Raw -Encoding UTF8
if($mapProcess.ExitCode-ne 0 -or $mapText-match 'Angelscript: Error:|MAP_DEVICE_FAIL|TowerError|Fatal error:'){throw "Map device check failed: $mapLog"}
$mapExpected=if($Render){'Preview'}elseif($Reload){'Reload'}else{'Integration'}
if($mapText-notmatch ('MAP_DEVICE_PASS '+$mapExpected)){throw "Map device verification did not complete: $mapLog"}
if($Render){$mapImage=Get-Item -LiteralPath (Join-Path $mapProof 'game.png');if($mapImage.LastWriteTime-lt $mapStarted){throw 'Stale map screenshot'}}
Write-Output "Passed $mapName"
