param([switch]$Render)
$ErrorActionPreference='Stop'
$studioRoot=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$studioProof=Join-Path $studioRoot 'Saved/Verification/StudioSync';New-Item -ItemType Directory -Force $studioProof | Out-Null
$studioProfile=Join-Path $studioRoot 'Saved/RuleTower/Profile.v2.json'
$studioHashBefore=if(Test-Path -LiteralPath $studioProfile){(Get-FileHash -LiteralPath $studioProfile).Hash}else{''}
$studioName=if($Render){'render'}else{'integration'}
$studioLog=Join-Path $studioProof ($studioName+'.log')
$studioArgs=@(('"'+$studioRoot+'/DungeonWorldOpus.uproject"'),'/Game/ClaudeDungeon/Maps/L_OpusDungeon?game=/Script/Angelscript.DWOTowerGameMode','-game','-DWOTransient','-DWOStudioSyncTest','-DWOSmoke=150','-Multiprocess','-unattended','-nosplash','-nosound','-NoLoadingScreen','-as-exit-on-error','-DisablePlugins=UE_MCP_Bridge',('-ABSLOG="'+$studioLog+'"'),'-ExecCmds="t.MaxFPS 60,t.IdleWhenNotForeground 0,Slate.bAllowThrottling 0"')
if($Render){$studioArgs+=@('-DWOStudioRender','-RenderOffscreen','-windowed','-ForceRes','-ResX=1600','-ResY=900')}else{$studioArgs+='-NullRHI'}
$studioProcess=Start-Process -FilePath 'G:/UnrealEngine-Angelscript-5.8.3/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $studioArgs -WorkingDirectory $studioRoot -WindowStyle Hidden -PassThru
$studioDeadline=(Get-Date).AddMinutes(4)
while(!$studioProcess.HasExited -and (Get-Date)-lt$studioDeadline){Start-Sleep -Milliseconds 500;$studioProcess.Refresh()}
if(!$studioProcess.HasExited){Stop-Process -Id $studioProcess.Id -Force;throw "Own transient test timed out: $studioLog"}
$studioProcess.WaitForExit();$studioText=Get-Content -LiteralPath $studioLog -Raw -Encoding utf8
if($studioProcess.ExitCode-ne 0 -or $studioText-match 'Angelscript: Error:|STUDIO_SYNC_FAIL|TowerError|Fatal error:' -or $studioText-notmatch 'STUDIO_SYNC_PASS Complete'){throw "Studio native integration failed: $studioLog"}
$studioHashAfter=if(Test-Path -LiteralPath $studioProfile){(Get-FileHash -LiteralPath $studioProfile).Hash}else{''}
if($studioHashBefore-ne$studioHashAfter){throw 'Formal profile changed during transient test'}
$studioChecks=@([regex]::Matches($studioText,'STUDIO_SYNC_PASS ([^\r\n]+)') | ForEach-Object {$_.Groups[1].Value})
[ordered]@{ok=$true;timestamp=(Get-Date).ToString('o');execution='native Unreal transient world';render=[bool]$Render;checks=$studioChecks;profileUnchanged=$true;formalProfileWrittenByTest=$false;rulesEngineChanged=$false;log=$studioLog} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $studioProof ($studioName+'-receipt.json')) -Encoding utf8
Write-Output "Studio sync passed $($studioChecks.Count) checks: $studioLog"
