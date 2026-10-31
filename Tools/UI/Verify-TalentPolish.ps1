param()
$ErrorActionPreference='Stop'
$zoomRoot=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$zoomProof=Join-Path $zoomRoot 'Saved/Verification/Talents'
$zoomPhase='attribute-polish'
$zoomLog=Join-Path $zoomProof 'AttributePolish/native-proof.log'
$zoomArgs=@(('"'+$zoomRoot+'/DungeonWorldOpus.uproject"'),'/Game/ClaudeDungeon/Maps/L_OpusDungeon?game=/Script/Angelscript.DWOTowerGameMode','-game','-DWOTransient','-DWOTalentTest','-DWOTalentPolishProof','-DWOTalentVisual','-Multiprocess','-as-exit-on-error','-unattended','-nosplash','-nosound','-DisablePlugins=UE_MCP_Bridge','-NoLoadingScreen','-windowed','-RenderOffscreen','-ResX=1600','-ResY=900','-ForceRes',('-ABSLOG="'+$zoomLog+'"'),'-ExecCmds="t.MaxFPS 60,t.IdleWhenNotForeground 0,Slate.bAllowThrottling 0"')
$zoomStart=Get-Date
$zoomProcess=Start-Process 'G:/UnrealEngine-Angelscript-5.8.3/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $zoomArgs -WorkingDirectory $zoomRoot -WindowStyle Hidden -PassThru
$zoomDeadline=(Get-Date).AddMinutes(4)
while(!$zoomProcess.HasExited -and (Get-Date)-lt $zoomDeadline){Start-Sleep -Milliseconds 500;$zoomProcess.Refresh()}
if(!$zoomProcess.HasExited){Stop-Process -Id $zoomProcess.Id;throw "Own zoom test timed out: $zoomLog"}
$zoomProcess.WaitForExit()
$zoomText=Get-Content -LiteralPath $zoomLog -Raw
if($zoomProcess.ExitCode-ne 0 -or $zoomText-match 'TALENT_TEST_FAIL|Angelscript: Error:|Fatal error:' -or $zoomText-notmatch 'TALENT_TEST_PASS AttributePolishScreenshotsComplete'){throw "Zoom verification failed: $zoomLog"}
foreach($zoomName in @('native-hover-563','native-origin-1400','native-legend-1400')){
 $zoomImage=Join-Path $zoomProof ('AttributePolish/'+$zoomName+'.png')
 if(!(Test-Path -LiteralPath $zoomImage) -or (Get-Item -LiteralPath $zoomImage).LastWriteTime-lt $zoomStart){throw "Missing or stale screenshot: $zoomImage"}
}
Write-Output "Native talent attribute-hover, origin and crown captures verified"
