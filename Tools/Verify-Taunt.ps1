param([switch]$Visuals)
$ErrorActionPreference='Stop'
$tauntRoot=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$tauntProof=Join-Path $tauntRoot 'Saved/Verification/Taunt'
New-Item -ItemType Directory -Path $tauntProof -Force | Out-Null
$tauntName=if($Visuals){'runtime-visual'}else{'runtime'}
$tauntLog=Join-Path $tauntProof ($tauntName+'.log')
if(Test-Path -LiteralPath $tauntLog){Copy-Item -LiteralPath $tauntLog -Destination (Join-Path $tauntProof ($tauntName+'-before-'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'.log'))}
$tauntArgs=@('"'+(Join-Path $tauntRoot 'DungeonWorldOpus.uproject')+'"','/Engine/Maps/Entry?game=/Script/Angelscript.DWOTowerGameMode','-game','-DWOTransient','-DWOHeroArena','-DWOTauntTest','-DWOView=selected-2','-Multiprocess','-as-exit-on-error','-unattended','-nosplash','-nosound','-NoLoadingScreen','-DisablePlugins=UE_MCP_Bridge',('-ABSLOG="'+$tauntLog+'"'))
if($Visuals){$tauntArgs+=@('-DWOTauntVisual','-windowed','-ResX=1600','-ResY=900','-ForceRes','-RenderOffscreen','-ExecCmds="r.SetRes 1600x900w,t.MaxFPS 60,t.IdleWhenNotForeground 0,Slate.bAllowThrottling 0,r.MotionBlurQuality 0"')}else{$tauntArgs+='-NullRHI'}
$tauntProcess=Start-Process -FilePath 'G:/UnrealEngine-Angelscript-5.8.3/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $tauntArgs -WorkingDirectory $tauntRoot -WindowStyle Hidden -PassThru
$tauntDeadline=(Get-Date).AddMinutes(3)
while(!$tauntProcess.HasExited -and (Get-Date)-lt $tauntDeadline){Start-Sleep -Milliseconds 500;$tauntProcess.Refresh()}
if(!$tauntProcess.HasExited){throw "Taunt verification timed out PID $($tauntProcess.Id): $tauntLog"}
$tauntProcess.WaitForExit()
$tauntText=Get-Content -LiteralPath $tauntLog -Raw -Encoding UTF8
if($tauntProcess.ExitCode-ne 0 -or $tauntText-match 'Angelscript: Error:|TAUNT_TEST_FAIL|TowerError|Fatal error:' -or ([regex]::Matches($tauntText,'TAUNT_TEST_PASS')).Count-ne 4){throw "Taunt verification failed: $tauntLog"}
Write-Output "Taunt native verification passed: $tauntLog"
