param([switch]$FullEvents)
$ErrorActionPreference='Stop'
$altarRoot=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$altarProof=Join-Path $altarRoot 'Saved/Verification/AltarBackpack'
New-Item -ItemType Directory -Force -Path $altarProof | Out-Null
$altarName=if($FullEvents){'all-events'}else{'drag-ui'}
$altarLog=Join-Path $altarProof ($altarName+'.log')
$altarProfile=Join-Path $altarRoot 'Saved/RuleTower/Profile.v2.json'
$altarBefore=(Get-FileHash -LiteralPath $altarProfile -Algorithm SHA256).Hash
$altarArgs=@(('"'+$altarRoot+'/DungeonWorldOpus.uproject"'),'/Engine/Maps/Entry?game=/Script/Angelscript.DWOTowerGameMode','-game','-DWOHeroArena','-DWOTransient','-DWOMapEventTest','-Multiprocess','-unattended','-nosplash','-nosound','-NoLoadingScreen','-as-exit-on-error','-DisablePlugins=UE_MCP_Bridge',('-ABSLOG="'+$altarLog+'"'))
if($FullEvents){$altarArgs+='-NullRHI'}else{$altarArgs+=@('-DWOEventCraftTest','-windowed','-RenderOffscreen','-ResX=1600','-ResY=900','-ForceRes','-SvgCanvasNoMotion',('-DWOEventCapture="'+$altarProof+'/ue-layout.png"'),'-ExecCmds="r.SetRes 1600x900w,t.MaxFPS 60,t.IdleWhenNotForeground 0,Slate.bAllowThrottling 0"')}
$altarProcess=Start-Process 'G:/UnrealEngine-Angelscript-5.8.3/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $altarArgs -WorkingDirectory $altarRoot -WindowStyle Hidden -PassThru
$altarDeadline=(Get-Date).AddMinutes(4)
while(!$altarProcess.HasExited -and (Get-Date)-lt $altarDeadline){Start-Sleep -Milliseconds 500;$altarProcess.Refresh()}
if(!$altarProcess.HasExited){Stop-Process -Id $altarProcess.Id;throw "Own altar verification timed out: $altarLog"}
$altarProcess.WaitForExit();$altarText=Get-Content -LiteralPath $altarLog -Raw -Encoding utf8
if($altarProcess.ExitCode-ne 0 -or $altarText-match 'EVENT_TEST_FAIL|Angelscript: Error:|Fatal error:' -or $altarText-notmatch 'EVENT_TEST_PASS'){throw "Altar verification failed: $altarLog"}
$altarAfter=(Get-FileHash -LiteralPath $altarProfile -Algorithm SHA256).Hash
@{test=$altarName;exit=$altarProcess.ExitCode;transient=$true;passed=([regex]::Matches($altarText,'EVENT_TEST_PASS')).Count;profileUnchanged=($altarBefore-eq $altarAfter);profileBefore=$altarBefore;profileAfter=$altarAfter;profileRestored=$false} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $altarProof ($altarName+'-receipt.json')) -Encoding utf8
Select-String -LiteralPath $altarLog -Pattern 'EVENT_TEST_' | ForEach-Object Line
