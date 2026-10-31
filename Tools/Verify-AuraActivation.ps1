$ErrorActionPreference='Stop'
$flowRoot=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$flowProof=Join-Path $flowRoot 'Saved/Verification/AuraActivation'
New-Item -ItemType Directory -Force -Path $flowProof | Out-Null
$flowLog=Join-Path $flowProof 'runtime.log'
$flowProfile=Join-Path $flowRoot 'Saved/RuleTower/Profile.v2.json'
$flowBefore=(Get-FileHash -LiteralPath $flowProfile -Algorithm SHA256).Hash
$flowArgs=@(('"'+$flowRoot+'/DungeonWorldOpus.uproject"'),'/Engine/Maps/Entry?game=/Script/Angelscript.DWOTowerGameMode','-game','-DWOHeroArena','-DWOTransient','-DWOAuraActivationTest','-DWORecentPreview','-Multiprocess','-unattended','-nosplash','-nosound','-NoLoadingScreen','-as-exit-on-error','-DisablePlugins=UE_MCP_Bridge',('-ABSLOG="'+$flowLog+'"'),'-windowed','-RenderOffscreen','-ResX=1600','-ResY=900','-ForceRes','-asyncassetcompilationmaxconcurrency=2','-asynctexturecompilationmaxconcurrency=1','-ExecCmds="r.SetRes 1600x900w,t.MaxFPS 60,t.IdleWhenNotForeground 0,Slate.bAllowThrottling 0,r.MotionBlurQuality 0"')
# Motion stays enabled: this is an animation verification, not a frozen pixel comparison.
$flowProcess=Start-Process 'G:/UnrealEngine-Angelscript-5.8.3/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $flowArgs -WorkingDirectory $flowRoot -WindowStyle Hidden -PassThru
$flowDeadline=(Get-Date).AddMinutes(3)
while(!$flowProcess.HasExited -and (Get-Date)-lt $flowDeadline){Start-Sleep -Milliseconds 500;$flowProcess.Refresh()}
if(!$flowProcess.HasExited){Stop-Process -Id $flowProcess.Id;throw "Own aura flow test timed out: $flowLog"}
$flowProcess.WaitForExit()
$flowText=Get-Content -LiteralPath $flowLog -Raw -Encoding utf8
if($flowProcess.ExitCode-ne 0 -or $flowText-match 'AURA_FLOW_FAIL|Angelscript: Error:|Fatal error:' -or $flowText-notmatch 'AURA_FLOW_DONE'){throw "Aura flow verification failed: $flowLog"}
$flowAfter=(Get-FileHash -LiteralPath $flowProfile -Algorithm SHA256).Hash
@{passed=([regex]::Matches($flowText,'AURA_FLOW_PASS')).Count;processExit=$flowProcess.ExitCode;transient=$true;profileBefore=$flowBefore;profileAfter=$flowAfter;profileUnchanged=($flowBefore-eq $flowAfter);profileRestored=$false;verifiedAt=(Get-Date).ToString('o')} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $flowProof 'runtime-receipt.json') -Encoding utf8
Select-String -LiteralPath $flowLog -Pattern 'AURA_FLOW_' | ForEach-Object Line
