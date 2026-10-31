param([ValidateSet('before','after')][string]$Capture='after')
$ErrorActionPreference='Stop'
$trailRoot=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$trailProof=Join-Path $trailRoot 'Saved/Verification/TreasuryTrail'
$trailLog=Join-Path $trailProof ($Capture+'-native.log')
$trailArgs=@(('"'+$trailRoot+'/DungeonWorldOpus.uproject"'),'/Engine/Maps/Entry?game=/Script/Angelscript.DWOTreasuryTrailProofMode','-game','-DWOTransient',('-DWOTrailCapture='+$Capture),'-Multiprocess','-unattended','-nosplash','-nosound','-NoLoadingScreen','-RenderOffscreen','-windowed','-ResX=1600','-ResY=900','-ForceRes','-as-exit-on-error','-DisablePlugins=UE_MCP_Bridge',('-ABSLOG="'+$trailLog+'"'),'-ExecCmds="t.MaxFPS 60,t.IdleWhenNotForeground 0,r.MotionBlurQuality 0"')
$trailStart=Get-Date
$trailProcess=Start-Process -FilePath 'G:/UnrealEngine-Angelscript-5.8.3/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $trailArgs -WorkingDirectory $trailRoot -WindowStyle Hidden -PassThru
$trailDeadline=(Get-Date).AddMinutes(3)
while(!$trailProcess.HasExited -and (Get-Date)-lt $trailDeadline){Start-Sleep -Milliseconds 500;$trailProcess.Refresh()}
if(!$trailProcess.HasExited){Stop-Process -Id $trailProcess.Id -Force;throw 'Own trail verification process timed out'}
$trailProcess.WaitForExit();$trailText=Get-Content -LiteralPath $trailLog -Raw -Encoding utf8
if($trailProcess.ExitCode-ne 0 -or $trailText-match 'Angelscript: Error:|Fatal error:|TREASURY_TRAIL_FAIL' -or $trailText-notmatch 'TREASURY_TRAIL_PASS ThreeSavedStylesRendered'){throw "Native trail verification failed: $trailLog"}
foreach($trailView in @('top','side')){if((Get-Item -LiteralPath (Join-Path $trailProof ($Capture+'-'+$trailView+'.png'))).LastWriteTime-lt $trailStart){throw "Stale $trailView capture"}}
Write-Output "Native saved-asset trail captures passed: $Capture"
