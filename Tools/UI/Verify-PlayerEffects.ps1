param([switch]$Render)
$ErrorActionPreference='Stop'
$effectRoot=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$effectProof=Join-Path $effectRoot 'Saved/Verification/FullHUD';New-Item -ItemType Directory -Force $effectProof | Out-Null
$effectLog=Join-Path $effectProof $(if($Render){'effects-visual.log'}else{'effects-runtime.log'})
$effectArgs=@(('"'+(Join-Path $effectRoot 'DungeonWorldOpus.uproject')+'"'),'/Game/ClaudeDungeon/Maps/L_OpusDungeon?game=/Script/Angelscript.DWOTowerGameMode','-game','-DWOTransient','-DWOPlayerEffectsTest','-Multiprocess','-as-exit-on-error','-unattended','-nosplash','-nosound','-NoLoadingScreen',('-ABSLOG="'+$effectLog+'"'))
if($Render){$effectArgs+=@('-DWOEffectsVisual','-windowed','-RenderOffscreen','-ResX=1600','-ResY=900','-ForceRes','-asyncassetcompilationmaxconcurrency=2','-asynctexturecompilationmaxconcurrency=1','-ExecCmds="r.SetRes 1600x900w,t.MaxFPS 60,t.IdleWhenNotForeground 0,Slate.bAllowThrottling 0,r.MotionBlurQuality 0"')}else{$effectArgs+='-NullRHI'}
$effectStarted=Get-Date
$effectArgs+='-DisablePlugins=UE_MCP_Bridge'
$effectProcess=Start-Process -FilePath 'G:/UnrealEngine-Angelscript-5.8.3/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $effectArgs -WorkingDirectory $effectRoot -WindowStyle Hidden -PassThru
$effectDeadline=(Get-Date).AddMinutes(4)
while(!$effectProcess.HasExited -and (Get-Date)-lt $effectDeadline){
 Start-Sleep -Milliseconds 500;$effectProcess.Refresh()
 if((Test-Path -LiteralPath $effectLog)-and (Get-Item -LiteralPath $effectLog).LastWriteTime-ge $effectStarted){
  $effectLiveText=Get-Content -LiteralPath $effectLog -Raw -Encoding UTF8
  if($effectLiveText-match 'Angelscript: Error:|EFFECTS_TEST_FAIL|Fatal error:|TowerError'){
   if(!$effectProcess.HasExited){Stop-Process -Id $effectProcess.Id -Force}
   throw "Native effects verification failed: $effectLog"
  }
 }
}
if(!$effectProcess.HasExited){Stop-Process -Id $effectProcess.Id -Force;throw "Effects verification timed out: $effectLog"}
$effectProcess.WaitForExit();$effectText=Get-Content -LiteralPath $effectLog -Raw -Encoding UTF8
if($effectProcess.ExitCode-ne 0 -or $effectText-match 'Angelscript: Error:|EFFECTS_TEST_FAIL|Fatal error:|TowerError' -or $effectText-notmatch 'EFFECTS_TEST_PASS NativeHUD'){throw "Native effects verification failed: $effectLog"}
if($Render){$effectImage=Join-Path $effectProof 'player-effects.png';if(!(Test-Path -LiteralPath $effectImage)-or (Get-Item $effectImage).LastWriteTime-lt $effectStarted){throw 'Missing fresh native HUD capture'}}
Write-Output "Native effects verification passed: $effectLog"
