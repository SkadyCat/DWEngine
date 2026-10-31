param([string]$Label='after')
$ErrorActionPreference='Stop'
$dragRoot=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$dragProof=Join-Path $dragRoot 'Saved/Verification/InventoryDrag'
New-Item -ItemType Directory -Force -Path $dragProof | Out-Null
$dragLog=Join-Path $dragProof ($Label+'.log')
$dragProfile=Join-Path $dragRoot 'Saved/RuleTower/Profile.v2.json'
$dragBefore=if(Test-Path -LiteralPath $dragProfile){(Get-FileHash -LiteralPath $dragProfile).Hash}else{''}
$dragArgs=@(('"'+$dragRoot+'/DungeonWorldOpus.uproject"'),'/Engine/Maps/Entry?game=/Script/Angelscript.DWOTowerGameMode','-game','-DWOTransient','-DWOHeroArena','-DWOInventoryDragTest','-RenderOffscreen','-windowed','-ResX=1600','-ResY=900','-ForceRes','-Multiprocess','-unattended','-nosplash','-nosound','-NoLoadingScreen','-as-exit-on-error','-DisablePlugins=UE_MCP_Bridge',('-ABSLOG="'+$dragLog+'"'),'-ExecCmds="t.MaxFPS 60,t.IdleWhenNotForeground 0,Slate.bAllowThrottling 0"')
$dragProcess=Start-Process -FilePath 'G:/UnrealEngine-Angelscript-5.8.3/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $dragArgs -WorkingDirectory $dragRoot -WindowStyle Hidden -PassThru
$dragDeadline=(Get-Date).AddMinutes(3)
while(!$dragProcess.HasExited -and (Get-Date)-lt $dragDeadline){Start-Sleep -Milliseconds 500;$dragProcess.Refresh()}
if(!$dragProcess.HasExited){Stop-Process -Id $dragProcess.Id;throw "Own transient drag verification timed out: $dragLog"}
$dragProcess.WaitForExit();$dragText=Get-Content -LiteralPath $dragLog -Raw -Encoding utf8
$dragAfter=if(Test-Path -LiteralPath $dragProfile){(Get-FileHash -LiteralPath $dragProfile).Hash}else{''}
if($dragBefore-ne$dragAfter){throw 'Formal profile changed during transient verification'}
$dragOk=$dragProcess.ExitCode-eq 0 -and $dragText-notmatch 'DRAG_FAIL|Angelscript: Error:|Fatal error:' -and $dragText-match 'DRAG_PASS Complete'
[ordered]@{ok=$dragOk;passed=([regex]::Matches($dragText,'DRAG_PASS')).Count;rendered=$true;profileUnchanged=$true;profileBefore=$dragBefore;profileAfter=$dragAfter;exit=$dragProcess.ExitCode;log=$dragLog}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $dragProof ($Label+'-receipt.json')) -Encoding utf8
Select-String -LiteralPath $dragLog -Pattern 'DRAG_|Angelscript: Error:'|ForEach-Object Line
if(!$dragOk){throw "Inventory drag verification failed: $dragLog"}
