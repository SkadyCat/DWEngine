param([switch]$Headless)
$ErrorActionPreference='Stop'
$castRoot=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$castOut=Join-Path $castRoot 'Saved/Verification/FreeCast';New-Item -ItemType Directory -Path $castOut -Force | Out-Null
$castLog=Join-Path $castOut 'native.log'
$castArgs=@(('"'+$castRoot+'/DungeonWorldOpus.uproject"'),'/Engine/Maps/Entry?game=/Script/Angelscript.DWOTowerGameMode','-game','-DWOTransient','-DWOHeroArena','-DWOFreeCastTest','-DWOSmoke=100','-Multiprocess','-unattended','-nosplash','-nosound','-NoLoadingScreen','-as-exit-on-error','-DisablePlugins=UE_MCP_Bridge',('-ABSLOG="'+$castLog+'"'),'-ExecCmds="t.MaxFPS 60,t.IdleWhenNotForeground 0,r.MotionBlurQuality 0,s.AsyncLoadingTimeLimit 8"')
if($Headless){$castArgs+='-NullRHI'}else{$castArgs+=@('-DWOFreeCastCapture','-RenderOffscreen','-windowed','-ResX=1600','-ResY=900','-ForceRes')}
$castStarted=Get-Date
$castProcess=Start-Process -FilePath 'G:/UnrealEngine-Angelscript-5.8.3/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $castArgs -WorkingDirectory $castRoot -WindowStyle Hidden -PassThru
$castDeadline=(Get-Date).AddMinutes(3)
while(!$castProcess.HasExited -and (Get-Date)-lt $castDeadline){Start-Sleep -Milliseconds 500;$castProcess.Refresh()}
if(!$castProcess.HasExited){Stop-Process -Id $castProcess.Id -Force;throw 'Own free cast verification process timed out'}
$castProcess.WaitForExit();$castText=Get-Content -LiteralPath $castLog -Raw -Encoding utf8
if($castProcess.ExitCode-ne 0 -or $castText-match 'Angelscript: Error:|Fatal error:|FREE_CAST_FAIL|HERO_VISUAL_ERROR' -or $castText-notmatch 'FREE_CAST_PASS Complete'){throw "Free cast verification failed: $castLog"}
if(!$Headless){foreach($castView in @('ready-empty-casting','cleared-empty-casting')){if((Get-Item -LiteralPath (Join-Path $castOut ($castView+'.png'))).LastWriteTime-lt $castStarted){throw "Stale $castView capture"}}}
Write-Output 'Free cast passed: native input, animation release, held input, costs, pause, auto aim, Ready and Cleared'
