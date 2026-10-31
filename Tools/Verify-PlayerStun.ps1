param([switch]$Visuals)
$ErrorActionPreference='Stop'
$stunRoot=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$stunProof=Join-Path $stunRoot 'Saved/Verification/PlayerStun'
New-Item -ItemType Directory -Path $stunProof -Force | Out-Null
$stunName=if($Visuals){'runtime-visual'}else{'runtime'}
$stunArgs=@('"'+(Join-Path $stunRoot 'DungeonWorldOpus.uproject')+'"','/Engine/Maps/Entry?game=/Script/Angelscript.DWOTowerGameMode','-game','-DWOTransient','-DWOHeroArena','-DWOStunTest','-Multiprocess','-as-exit-on-error','-unattended','-nosplash','-nosound','-NoLoadingScreen',('-ABSLOG="'+(Join-Path $stunProof ($stunName+'.log'))+'"'))
if($Visuals){$stunArgs+=@('-DWOStunVisual','-windowed','-ResX=1600','-ResY=900','-ForceRes','-RenderOffscreen','-ExecCmds="r.SetRes 1600x900w,t.MaxFPS 60,t.IdleWhenNotForeground 0,Slate.bAllowThrottling 0,r.MotionBlurQuality 0"')}else{$stunArgs+='-NullRHI'}
$stunArgs+='-DisablePlugins=UE_MCP_Bridge'
$stunProcess=Start-Process -FilePath 'G:\UnrealEngine-Angelscript-5.8.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' -ArgumentList $stunArgs -WorkingDirectory $stunRoot -WindowStyle Hidden -PassThru
$stunDeadline=(Get-Date).AddMinutes(3)
while(!$stunProcess.HasExited -and (Get-Date)-lt $stunDeadline){Start-Sleep -Milliseconds 500;$stunProcess.Refresh()}
if(!$stunProcess.HasExited){throw "Player stun verification timed out (PID $($stunProcess.Id)): $stunProof"}
$stunProcess.WaitForExit()
$stunText=Get-Content -LiteralPath (Join-Path $stunProof ($stunName+'.log')) -Raw -Encoding UTF8
if($stunProcess.ExitCode-ne 0 -or $stunText-match 'Angelscript: Error:|STUN_TEST_FAIL|TowerError|Fatal error:' -or ([regex]::Matches($stunText,'STUN_TEST_PASS')).Count-ne 5){throw "Player stun verification failed: $stunProof"}
Write-Host "Passed player equipment buildup, interrupt, stun, decay and saved HUD: $stunProof"
