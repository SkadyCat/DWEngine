param([switch]$Render,[switch]$Closeup,[switch]$Dungeon)
$ErrorActionPreference='Stop'
$heroRoot=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$heroOut=Join-Path $heroRoot 'Saved\Verification\Hero'
New-Item -ItemType Directory -Path $heroOut -Force | Out-Null
$heroName=if($Render){if($Closeup){'visual-closeup'}else{'visual-gameplay'}}else{'integration'}
$heroLog=Join-Path $heroOut ($heroName+'.log')
$heroArgs=@('"'+(Join-Path $heroRoot 'DungeonWorldOpus.uproject')+'"','/Engine/Maps/Entry?game=/Script/Angelscript.DWOTowerGameMode','-game','-DWOTransient','-DWOHeroTest','-Multiprocess','-unattended','-nosplash','-nosound','-NoLoadingScreen','-as-exit-on-error','-ExecCmds="t.MaxFPS 60,t.IdleWhenNotForeground 0,Slate.bAllowThrottling 0,r.VSync 0,r.MotionBlurQuality 0,s.AsyncLoadingTimeLimit 8"','-DWOSmoke=130',"-ABSLOG=$heroLog")
$heroArgs+=@('-asyncassetcompilationmaxconcurrency=2','-asynctexturecompilationmaxconcurrency=1','-DWODebug')
if(!$Dungeon){$heroArgs+='-DWOHeroArena'}
if($Render){
 $heroImages=Join-Path $heroOut $heroName;New-Item -ItemType Directory -Path $heroImages -Force | Out-Null
 $heroArgs+=@('-windowed','-RenderOffscreen','-ResX=1600','-ResY=900','-SvgCanvasNoMotion',('-DWOHeroCaptures='+$heroImages.Replace('\','/')))
 if($Closeup){$heroArgs+='-DWOHeroCloseup'}
}else{$heroArgs+='-nullrhi'}
$heroProcess=Start-Process -FilePath 'G:\UnrealEngine-Angelscript-5.8.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' -ArgumentList $heroArgs -WorkingDirectory $heroRoot -WindowStyle Hidden -PassThru
$heroDeadline=(Get-Date).AddMinutes(4)
while(!$heroProcess.HasExited -and (Get-Date)-lt $heroDeadline){Start-Sleep -Milliseconds 500;$heroProcess.Refresh()}
if(!$heroProcess.HasExited){Stop-Process -Id $heroProcess.Id -Force;throw "Hero check timed out: $heroLog"}
$heroProcess.WaitForExit();$heroText=Get-Content -LiteralPath $heroLog -Raw -Encoding UTF8
if($heroProcess.ExitCode-ne 0 -or $heroText-match 'Angelscript: Error:|HERO_TEST_FAIL|HERO_VISUAL_ERROR|TowerError|Fatal error:'){throw "Hero check failed: $heroLog"}
foreach($heroMarker in @('RigAndIdle','ZeroCameraFollow','LocomotionAndReadOnly','CastPose','Pause','RootMotionEightDirections','CancelledWindupNoCast','ManualTargetDeathStillReleases','AuthoredCastRelease','HeldCastOnePerMotion','NativeCastAndDodge','NativeHitAndDeath','RetryAndCache','RuntimeIntegration')){
 if($heroText-notmatch ('HERO_TEST_PASS '+$heroMarker)){throw "Missing hero check: $heroMarker"}
}
Write-Host "Passed wizard hero: $heroName"
