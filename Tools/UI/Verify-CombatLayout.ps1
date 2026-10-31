param([switch]$RenderOnly,[switch]$BindingsOnly)
$ErrorActionPreference='Stop'
$combatRoot=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$combatProof=Join-Path $combatRoot 'Saved/Verification/CombatLayout'
New-Item -ItemType Directory -Path $combatProof -Force | Out-Null
function Test-CombatLayout([string]$Name,[string[]]$Extra,[bool]$Render=$false){
 $combatLog=Join-Path $combatProof ($Name+'.log')
 $combatImage=Join-Path $combatProof ($Name+'.png')
 $combatArgs=@(('"'+(Join-Path $combatRoot 'DungeonWorldOpus.uproject')+'"'),'/Game/ClaudeDungeon/Maps/L_OpusDungeon?game=/Script/Angelscript.DWOTowerGameMode','-game','-DWOTransient','-Multiprocess','-as-exit-on-error','-unattended','-nosplash','-nosound','-NoLoadingScreen',('-ABSLOG="'+$combatLog+'"'))+$Extra
 if($Render){$combatArgs+=@('-windowed','-RenderOffscreen','-ResX=1600','-ResY=900','-ForceRes','-SvgCanvasNoMotion','-DWOSmoke=8','-asyncassetcompilationmaxconcurrency=2','-asynctexturecompilationmaxconcurrency=1','-ExecCmds="r.SetRes 1600x900w,t.MaxFPS 60,t.IdleWhenNotForeground 0,Slate.bAllowThrottling 0,r.MotionBlurQuality 0"',('-DWOCapture='+$combatImage.Replace('\','/')))}else{$combatArgs+='-NullRHI'}
 $combatArgs+='-DisablePlugins=UE_MCP_Bridge'
 $combatStarted=Get-Date
 $combatProcess=Start-Process -FilePath 'G:/UnrealEngine-Angelscript-5.8.3/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $combatArgs -WorkingDirectory $combatRoot -WindowStyle Hidden -PassThru
 $combatDeadline=(Get-Date).AddMinutes(3)
 while(!$combatProcess.HasExited -and (Get-Date)-lt $combatDeadline){Start-Sleep -Milliseconds 500;$combatProcess.Refresh()}
 if(!$combatProcess.HasExited){Stop-Process -Id $combatProcess.Id -Force;throw "Timed out: $Name"}
 $combatProcess.WaitForExit();$combatText=Get-Content $combatLog -Raw -Encoding UTF8
 if($combatProcess.ExitCode-ne 0 -or $combatText-match 'Angelscript: Error:|SVG_TEST_FAIL|Fatal error:|TowerError'){throw "Failed: $combatLog"}
 if($Render-and (!(Test-Path $combatImage)-or (Get-Item $combatImage).LastWriteTime-lt $combatStarted)){throw 'Missing or stale capture'}
 Write-Host "Passed $Name"
 return $combatText
}
if(!$RenderOnly){
 $combatBindings=Test-CombatLayout 'bindings' @('-DWOContractTest')
 if($combatBindings-notmatch 'SVG_TEST_PASS RuntimeBindings'){throw 'Native navigation contract did not complete'}
 if($BindingsOnly){Write-Output "Native HUD bindings passed: $combatProof";return}
 $combatReset=Test-CombatLayout 'reset' @('-DWOResetTest','-DWOFloor=3','-DWOSmoke=20')
 if($combatReset-notmatch 'SVG_TEST_PASS ResetStartsCurrentFloor' -or ([regex]::Matches($combatReset,'TowerReady floor=3')).Count-lt 2){throw 'Reset did not restart current floor'}
 $combatReady=Test-CombatLayout 'ready' @('-DWOSmoke=2')
 if($combatReady-notmatch 'phase=0' -or $combatReady-match 'TowerEncounter'){throw 'First launch must remain Ready'}
}
foreach($combatView in @('selected-1','inventory')){$null=Test-CombatLayout $combatView @('-DWOView='+$combatView) $true}
Write-Output "Combat HUD evidence: $combatProof"
