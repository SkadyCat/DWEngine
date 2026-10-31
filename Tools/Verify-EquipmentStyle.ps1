param([switch]$Render)
$ErrorActionPreference='Stop'
$equipmentRoot=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$equipmentProof=Join-Path $equipmentRoot 'Saved/Verification/EquipmentStyle'
New-Item -ItemType Directory -Path $equipmentProof -Force | Out-Null
$equipmentViews=if($Render){@('character','inventory')}else{@('character')}
foreach($equipmentView in $equipmentViews){
 $equipmentName=if($Render){$equipmentView}else{'integration'}
 $equipmentLog=Join-Path $equipmentProof ($equipmentName+'.log')
 $equipmentArgs=@(('"'+$equipmentRoot+'/DungeonWorldOpus.uproject"'),'/Engine/Maps/Entry?game=/Script/Angelscript.DWOTowerGameMode','-game','-DWOTransient','-DWOEquipmentStyleTest','-Multiprocess','-as-exit-on-error','-unattended','-nosplash','-nosound','-NoLoadingScreen','-DWOSmoke=8',('-DWOView='+$equipmentView),('-ABSLOG="'+$equipmentLog+'"'))
 if($Render){$equipmentArgs+=@('-windowed','-RenderOffscreen','-ForceRes','-ResX=1600','-ResY=900','-SvgCanvasNoMotion',('-DWOCapture='+$equipmentProof.Replace('\','/')+'/'+$equipmentName+'.png'),'-ExecCmds="t.MaxFPS 60,t.IdleWhenNotForeground 0,Slate.bAllowThrottling 0"')}else{$equipmentArgs+='-nullrhi'}
 $equipmentStarted=Get-Date
 $equipmentProcess=Start-Process -FilePath 'G:/UnrealEngine-Angelscript-5.8.3/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $equipmentArgs -WorkingDirectory $equipmentRoot -WindowStyle Hidden -PassThru
 $equipmentDeadline=(Get-Date).AddMinutes(4)
 while(!$equipmentProcess.HasExited -and (Get-Date)-lt $equipmentDeadline){Start-Sleep -Milliseconds 500;$equipmentProcess.Refresh()}
 if(!$equipmentProcess.HasExited){Stop-Process -Id $equipmentProcess.Id -Force;throw "Equipment check timed out: $equipmentLog"}
 $equipmentProcess.WaitForExit();$equipmentText=Get-Content -LiteralPath $equipmentLog -Raw -Encoding UTF8
 if($equipmentProcess.ExitCode-ne 0 -or $equipmentText-match 'Angelscript: Error:|EQUIPMENT_STYLE_FAIL|TowerError|Fatal error:'){throw "Equipment check failed: $equipmentLog"}
 if($equipmentText-notmatch 'EQUIPMENT_STYLE_PASS FourRaritiesAndReplacement'){throw "Equipment checks did not complete: $equipmentLog"}
 if($Render){$equipmentImage=Get-Item -LiteralPath (Join-Path $equipmentProof ($equipmentName+'.png'));if($equipmentImage.LastWriteTime-lt $equipmentStarted){throw 'Stale equipment screenshot'}}
 Write-Output "Passed $equipmentName"
}
