param([switch]$Render)
$ErrorActionPreference='Stop'
$hotbarRoot=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$hotbarProof=Join-Path $hotbarRoot 'Saved/Verification/HotbarPickerUE'
New-Item -ItemType Directory -Force -Path $hotbarProof | Out-Null
$hotbarName=if($Render){'render'}else{'runtime'}
$hotbarLog=Join-Path $hotbarProof ($hotbarName+'.log')
$hotbarProfile=Join-Path $hotbarRoot 'Saved/RuleTower/Profile.v2.json'
$hotbarBefore=if(Test-Path -LiteralPath $hotbarProfile){(Get-FileHash -LiteralPath $hotbarProfile).Hash}else{''}
$hotbarMap=if($Render){'/Game/ClaudeDungeon/Maps/L_OpusDungeon?game=/Script/Angelscript.DWOTowerGameMode'}else{'/Engine/Maps/Entry?game=/Script/Angelscript.DWOTowerGameMode'}
$hotbarArgs=@(('"'+$hotbarRoot+'/DungeonWorldOpus.uproject"'),$hotbarMap,'-game','-DWOTransient','-DWOHotbarTest','-DWORecentPreview','-Multiprocess','-unattended','-nosplash','-nosound','-NoLoadingScreen','-as-exit-on-error','-DisablePlugins=UE_MCP_Bridge',('-ABSLOG="'+$hotbarLog+'"'))
if(!$Render){$hotbarArgs+='-DWOHeroArena'}
if($Render){$hotbarArgs+=@('-DWOHotbarRender','-windowed','-RenderOffscreen','-ResX=1600','-ResY=900','-ForceRes','-ExecCmds="t.MaxFPS 60,t.IdleWhenNotForeground 0,Slate.bAllowThrottling 0"')}else{$hotbarArgs+='-NullRHI'}
$hotbarProcess=Start-Process 'G:/UnrealEngine-Angelscript-5.8.3/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $hotbarArgs -WorkingDirectory $hotbarRoot -WindowStyle Hidden -PassThru
$hotbarDeadline=(Get-Date).AddMinutes(4)
while(!$hotbarProcess.HasExited -and (Get-Date)-lt$hotbarDeadline){Start-Sleep -Milliseconds 500;$hotbarProcess.Refresh()}
if(!$hotbarProcess.HasExited){Stop-Process -Id $hotbarProcess.Id;throw "Own hotbar test timed out: $hotbarLog"}
$hotbarProcess.WaitForExit();$hotbarText=Get-Content -LiteralPath $hotbarLog -Raw -Encoding utf8
if($hotbarProcess.ExitCode-ne 0 -or $hotbarText-match 'HOTBAR_FAIL|Angelscript: Error:|Fatal error:' -or $hotbarText-notmatch 'HOTBAR_DONE'){throw "Hotbar test failed: $hotbarLog"}
$hotbarAfter=if(Test-Path -LiteralPath $hotbarProfile){(Get-FileHash -LiteralPath $hotbarProfile).Hash}else{''}
if($hotbarBefore-ne$hotbarAfter){throw 'Formal profile changed during isolated test'}
$hotbarChecks=@([regex]::Matches($hotbarText,'HOTBAR_PASS ([^\r\n]+)')|ForEach-Object {$_.Groups[1].Value})
@{ok=$true;render=[bool]$Render;checks=$hotbarChecks;profileUnchanged=$true;formalProfileWrittenByTest=$false;log=$hotbarLog}|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $hotbarProof ($hotbarName+'-receipt.json')) -Encoding utf8
Write-Output "Hotbar passed $($hotbarChecks.Count) checks: $hotbarLog"
