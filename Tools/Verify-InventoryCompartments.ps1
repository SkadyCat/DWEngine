$ErrorActionPreference='Stop'
$contextRoot=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$contextProof=Join-Path $contextRoot 'Saved/Verification/InventoryCompartments'
New-Item -ItemType Directory -Force -Path $contextProof | Out-Null
$contextLog=Join-Path $contextProof 'game.log'
$contextProfile=Join-Path $contextRoot 'Saved/RuleTower/Profile.v2.json'
$contextBefore=(Get-FileHash -LiteralPath $contextProfile -Algorithm SHA256).Hash
$contextArgs=@(('"'+$contextRoot+'/DungeonWorldOpus.uproject"'),'/Engine/Maps/Entry?game=/Script/Angelscript.DWOTowerGameMode','-game','-DWOTransient','-DWOHeroArena','-DWOInventorySplitTest','-RenderOffscreen','-windowed','-ResX=1600','-ResY=900','-ForceRes','-Multiprocess','-unattended','-nosplash','-nosound','-NoLoadingScreen','-as-exit-on-error','-DisablePlugins=UE_MCP_Bridge',('-ABSLOG="'+$contextLog+'"'),'-ExecCmds="t.MaxFPS 60,t.IdleWhenNotForeground 0,Slate.bAllowThrottling 0,r.MotionBlurQuality 0"')
$contextProcess=Start-Process 'G:/UnrealEngine-Angelscript-5.8.3/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $contextArgs -WorkingDirectory $contextRoot -WindowStyle Hidden -PassThru
$contextDeadline=(Get-Date).AddMinutes(4)
while(!$contextProcess.HasExited -and (Get-Date)-lt $contextDeadline){Start-Sleep -Milliseconds 500;$contextProcess.Refresh()}
if(!$contextProcess.HasExited){Stop-Process -Id $contextProcess.Id;throw 'Own context menu verification timed out'}
$contextProcess.WaitForExit();$contextText=Get-Content -LiteralPath $contextLog -Raw -Encoding utf8
if($contextProcess.ExitCode-ne 0 -or $contextText-match 'CONTEXT_FAIL|Angelscript: Error:|Fatal error:' -or $contextText-notmatch 'CONTEXT_PASS Complete'){throw "Context menu verification failed: $contextLog"}
$contextAfter=(Get-FileHash -LiteralPath $contextProfile -Algorithm SHA256).Hash
if($contextBefore-ne $contextAfter){throw 'Formal profile changed during transient verification'}
@{passed=([regex]::Matches($contextText,'CONTEXT_PASS')).Count;nativeSlatePointer=$true;profileUnchanged=$true;profileBefore=$contextBefore;profileAfter=$contextAfter;profileRestored=$false;exit=$contextProcess.ExitCode}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $contextProof 'receipt.json') -Encoding utf8
Select-String -LiteralPath $contextLog -Pattern 'CONTEXT_'|ForEach-Object Line
