$ErrorActionPreference='Stop'
$merchantRoot=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$merchantProof=Join-Path $merchantRoot 'Saved/Verification/MerchantWidget'
New-Item -ItemType Directory -Force -Path $merchantProof|Out-Null
$merchantLog=Join-Path $merchantProof 'render.log'
$merchantProfile=Join-Path $merchantRoot 'Saved/RuleTower/Profile.v2.json'
$merchantBefore=if(Test-Path -LiteralPath $merchantProfile){(Get-FileHash -LiteralPath $merchantProfile).Hash}else{''}
$merchantArgs=@(('"'+$merchantRoot+'/DungeonWorldOpus.uproject"'),'/Game/ClaudeDungeon/Maps/L_OpusDungeon?game=/Script/Angelscript.DWOTowerGameMode','-game','-DWOTransient','-DWOMerchantWidgetTest','-Multiprocess','-unattended','-nosplash','-nosound','-NoLoadingScreen','-as-exit-on-error','-DisablePlugins=UE_MCP_Bridge','-RenderOffscreen','-windowed','-ForceRes','-ResX=1600','-ResY=900','-ExecCmds="t.MaxFPS 60,t.IdleWhenNotForeground 0,Slate.bAllowThrottling 0"',('-ABSLOG="'+$merchantLog+'"'))
$merchantProcess=Start-Process 'G:/UnrealEngine-Angelscript-5.8.3/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $merchantArgs -WorkingDirectory $merchantRoot -WindowStyle Hidden -PassThru
$merchantDeadline=(Get-Date).AddMinutes(4)
while(!$merchantProcess.HasExited -and (Get-Date)-lt$merchantDeadline){Start-Sleep -Milliseconds 500;$merchantProcess.Refresh()}
if(!$merchantProcess.HasExited){Stop-Process -Id $merchantProcess.Id;throw 'Own merchant widget test timed out'}
$merchantProcess.WaitForExit();$merchantText=Get-Content -LiteralPath $merchantLog -Raw -Encoding utf8
if($merchantProcess.ExitCode-ne 0 -or $merchantText-match 'MERCHANT_WIDGET_FAIL|Angelscript: Error:|Fatal error:' -or $merchantText-notmatch 'MERCHANT_WIDGET_DONE'){throw "Merchant WidgetComponent test failed: $merchantLog"}
$merchantAfter=if(Test-Path -LiteralPath $merchantProfile){(Get-FileHash -LiteralPath $merchantProfile).Hash}else{''}
if($merchantBefore-ne$merchantAfter){throw 'Formal profile changed during isolated test'}
$merchantChecks=@([regex]::Matches($merchantText,'MERCHANT_WIDGET_PASS ([^\r\n]+)')|ForEach-Object {$_.Groups[1].Value})
@{ok=$true;checks=$merchantChecks;profileUnchanged=$true;host='ADWOOutpostMerchant.Bubble';widgetSpace='Screen';viewportProjectionRemoved=$true;log=$merchantLog}|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $merchantProof 'receipt.json') -Encoding utf8
Write-Output "Merchant WidgetComponent passed $($merchantChecks.Count) checks"
