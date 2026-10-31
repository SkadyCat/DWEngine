param([switch]$SurfaceOnly)
$ErrorActionPreference='Stop'
$windGroundRoot=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$windGroundProof=Join-Path $windGroundRoot 'Saved/Verification/WhirlwindParity/DungeonGround'
New-Item -ItemType Directory -Force -Path $windGroundProof | Out-Null
function Invoke-WindGroundProbe($ProbeArgs,$LogPath,$Marker) {
 $probeProcess=Start-Process 'G:/UnrealEngine-Angelscript-5.8.3/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $ProbeArgs -WorkingDirectory $windGroundRoot -WindowStyle Hidden -PassThru
 $probeDeadline=(Get-Date).AddMinutes(4)
 while(!$probeProcess.HasExited -and (Get-Date)-lt $probeDeadline){Start-Sleep -Milliseconds 500;$probeProcess.Refresh()}
 if(!$probeProcess.HasExited){Stop-Process -Id $probeProcess.Id;throw "Own ground probe timed out: $LogPath"}
 $probeProcess.WaitForExit();$probeText=Get-Content -LiteralPath $LogPath -Raw
 if($probeProcess.ExitCode-ne 0 -or $probeText-match 'DUNGEON_GROUND_FAIL|Angelscript: Error:|Fatal error:' -or $probeText-notmatch $Marker){throw "Ground probe failed: $LogPath"}
}
$windProject='"'+$windGroundRoot+'/DungeonWorldOpus.uproject"'
$windCommon=@('-nullrhi','-unattended','-nosplash','-nosound','-NoLoadingScreen','-DisablePlugins=UE_MCP_Bridge','-as-exit-on-error')
if(!$SurfaceOnly){
 $windNativeLog=Join-Path $windGroundProof 'native-regression.log'
 $windNativeReport=Join-Path $windGroundProof 'NativeAutomation'
 $windNativeArgs=@($windProject,'/Engine/Maps/Entry')+$windCommon+@('-ExecCmds="Automation RunTests DungeonWorldOpus.Recent"','-TestExit="Automation Test Queue Empty"',('-ReportExportPath="'+$windNativeReport+'"'),('-ABSLOG="'+$windNativeLog+'"'))
 Invoke-WindGroundProbe $windNativeArgs $windNativeLog 'Test Completed'
 $windNativeResult=Get-Content (Join-Path $windNativeReport 'index.json') -Raw | ConvertFrom-Json
 if($windNativeResult.failed-ne 0 -or $windNativeResult.succeeded-lt 9){throw "Native regression did not pass: $windNativeReport"}
 Write-Output "Native regression passed: $($windNativeResult.succeeded)"
}
$windSurfaceLog=Join-Path $windGroundProof 'actual-dungeon.log'
$windSurfaceArgs=@($windProject,'/Game/ClaudeDungeon/Maps/L_OpusDungeon?game=/Script/Angelscript.DWODungeonGroundProbeMode')+$windCommon+@('-game','-DWOTransient',('-ABSLOG="'+$windSurfaceLog+'"'))
Invoke-WindGroundProbe $windSurfaceArgs $windSurfaceLog 'DUNGEON_GROUND_DONE'
Get-Content $windSurfaceLog | Where-Object {$_-match 'DUNGEON_GROUND_(PASS|DONE)'}
