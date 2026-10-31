param([switch]$Visual)
$ErrorActionPreference='Stop'
$auraRoot=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$auraProof=Join-Path $auraRoot 'Saved/Verification/AuraClick'
New-Item -ItemType Directory -Force -Path $auraProof | Out-Null
$auraName=if($Visual){'visual'}else{'runtime'}
$auraLog=Join-Path $auraProof ($auraName+'.log')
if(Test-Path -LiteralPath $auraLog){Copy-Item -LiteralPath $auraLog -Destination (Join-Path $auraProof ($auraName+'-before-'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'.log'))}
$auraProfile=Join-Path $auraRoot 'Saved/RuleTower/Profile.v2.json'
$auraBefore=(Get-FileHash -LiteralPath $auraProfile -Algorithm SHA256).Hash
$auraArgs=@(('"'+$auraRoot+'/DungeonWorldOpus.uproject"'),'/Engine/Maps/Entry?game=/Script/Angelscript.DWOTowerGameMode','-game','-DWOHeroArena','-DWOTransient','-DWOAuraClickTest','-DWORecentPreview','-Multiprocess','-unattended','-nosplash','-nosound','-NoLoadingScreen','-as-exit-on-error','-DisablePlugins=UE_MCP_Bridge',('-ABSLOG="'+$auraLog+'"'))
if($Visual){$auraArgs+=@('-DWOAuraClickVisual','-windowed','-RenderOffscreen','-ResX=1600','-ResY=900','-ForceRes','-SvgCanvasNoMotion','-asyncassetcompilationmaxconcurrency=2','-asynctexturecompilationmaxconcurrency=1','-ExecCmds="r.SetRes 1600x900w,t.MaxFPS 60,t.IdleWhenNotForeground 0,Slate.bAllowThrottling 0"')}else{$auraArgs+='-NullRHI'}
$auraProcess=Start-Process 'G:/UnrealEngine-Angelscript-5.8.3/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $auraArgs -WorkingDirectory $auraRoot -WindowStyle Hidden -PassThru
$auraDeadline=(Get-Date).AddMinutes(3)
while(!$auraProcess.HasExited -and (Get-Date)-lt $auraDeadline){Start-Sleep -Milliseconds 500;$auraProcess.Refresh()}
if(!$auraProcess.HasExited){Stop-Process -Id $auraProcess.Id;throw "Own aura test timed out: $auraLog"}
$auraProcess.WaitForExit()
$auraText=Get-Content -LiteralPath $auraLog -Raw -Encoding utf8
if($auraProcess.ExitCode-ne 0 -or $auraText-match 'AURA_CLICK_FAIL|Angelscript: Error:|Fatal error:' -or $auraText-notmatch 'AURA_CLICK_DONE'){throw "Aura click test failed: $auraLog"}
$auraAfter=(Get-FileHash -LiteralPath $auraProfile -Algorithm SHA256).Hash
@{test=$auraName;passed=([regex]::Matches($auraText,'AURA_CLICK_PASS')).Count;processExit=$auraProcess.ExitCode;transient=$true;profileBefore=$auraBefore;profileAfter=$auraAfter;profileUnchanged=($auraBefore-eq $auraAfter);profileRestored=$false;verifiedAt=(Get-Date).ToString('o')} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $auraProof ($auraName+'-receipt.json')) -Encoding utf8
Select-String -LiteralPath $auraLog -Pattern 'AURA_CLICK_' | ForEach-Object Line
if($auraBefore-ne $auraAfter){Write-Output 'Formal profile changed during the running regular game; current user profile preserved.'}
