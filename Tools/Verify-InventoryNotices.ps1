param([switch]$Render)
$ErrorActionPreference='Stop'
$noticeRoot=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$noticeProof=Join-Path $noticeRoot 'Saved/Verification/InventoryNotices'
New-Item -ItemType Directory -Force $noticeProof | Out-Null
$noticeName=if($Render){'inventory-visual'}else{'inventory-integration'}
$noticeLog=Join-Path $noticeProof ($noticeName+'.log')
$noticeArgs=@(('"'+$noticeRoot+'/DungeonWorldOpus.uproject"'),'/Engine/Maps/Entry?game=/Script/Angelscript.DWOTowerGameMode','-game','-DWOTransient','-DWOInventoryNoticeTest','-Multiprocess','-as-exit-on-error','-unattended','-nosplash','-nosound','-NoLoadingScreen','-DWOSmoke=12','-DisablePlugins=UE_MCP_Bridge',('-ABSLOG="'+$noticeLog+'"'))
if($Render){$noticeArgs+=@('-DWOInventoryNoticeVisual','-windowed','-RenderOffscreen','-ForceRes','-ResX=1600','-ResY=900',('-DWOCapture='+$noticeProof.Replace('\','/')+'/'+$noticeName+'.png'),'-ExecCmds="t.MaxFPS 60,t.IdleWhenNotForeground 0,Slate.bAllowThrottling 0"')}else{$noticeArgs+='-NullRHI'}
$noticeStarted=Get-Date
$noticeProcess=Start-Process -FilePath 'G:/UnrealEngine-Angelscript-5.8.3/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $noticeArgs -WorkingDirectory $noticeRoot -WindowStyle Hidden -PassThru
Write-Output "Started $noticeName PID $($noticeProcess.Id)"
$noticeDeadline=(Get-Date).AddMinutes(5)
while(!$noticeProcess.HasExited -and (Get-Date)-lt $noticeDeadline){Start-Sleep -Milliseconds 500;$noticeProcess.Refresh()}
if(!$noticeProcess.HasExited){Stop-Process -Id $noticeProcess.Id -Force;throw "Inventory notice check timed out: $noticeLog"}
$noticeProcess.WaitForExit();$noticeText=Get-Content -LiteralPath $noticeLog -Raw -Encoding UTF8
if($noticeProcess.ExitCode-ne 0 -or $noticeText-match 'Angelscript: Error:|INVENTORY_NOTICE_FAIL|TowerError|Fatal error:'){throw "Inventory notice check failed: $noticeLog"}
if($noticeText-notmatch 'INVENTORY_NOTICE_PASS Acquisition SharedAsset HoverConsumed Character StackIncrease'){throw "Inventory notice check did not complete: $noticeLog"}
if($Render){$noticeImage=Get-Item -LiteralPath (Join-Path $noticeProof ($noticeName+'.png'));if($noticeImage.LastWriteTime-lt $noticeStarted){throw 'Stale inventory notice screenshot'}}
Write-Output "Passed $noticeName"
