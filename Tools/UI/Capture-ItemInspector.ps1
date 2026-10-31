param([string]$Name='inspector-3d-idle',[string]$Currency='',[string]$Item='',[switch]$Probe)
$ErrorActionPreference='Stop'
$inspectorRoot=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$inspectorOut=Join-Path $inspectorRoot 'Saved/Verification/ItemHover'
$inspectorLog=Join-Path $inspectorOut ($Name+'.log')
$inspectorImage=Join-Path $inspectorOut ($Name+'.png')
$inspectorContract=Get-Content (Join-Path $inspectorRoot 'Content/UICreatorHover/contract.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$inspectorArguments=@(('"'+(Join-Path $inspectorRoot 'DungeonWorldOpus.uproject')+'"'),'/Engine/Maps/Entry?game=/Script/Angelscript.DWOTowerGameMode','-game','-DWOTransient','-DWOHoverView=reference','-DWOHoverPinned','-Multiprocess','-as-exit-on-error','-unattended','-nosplash','-nosound','-NoLoadingScreen','-windowed','-RenderOffscreen','-ResX=1600','-ResY=900','-ForceRes','-SvgCanvasNoMotion','-DWOSmoke=8','-asyncassetcompilationmaxconcurrency=2','-asynctexturecompilationmaxconcurrency=1','-ExecCmds="r.SetRes 1600x900w,t.MaxFPS 60,t.IdleWhenNotForeground 0,Slate.bAllowThrottling 0,r.MotionBlurQuality 0"',('-DWOExpectedHoverHash='+$inspectorContract.sourceHash),('-DWOCapture='+$inspectorImage.Replace('\','/')),('-ABSLOG="'+$inspectorLog+'"'))
if($Currency){$inspectorArguments+=('-DWOHoverCurrency='+$Currency)}
if($Item){$inspectorArguments+=('-DWOHoverItem='+$Item)}
if($Probe){$inspectorArguments+='-DWOHoverProbe'}
$inspectorStarted=Get-Date
$inspectorProcess=Start-Process -FilePath 'G:/UnrealEngine-Angelscript-5.8.3/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $inspectorArguments -WorkingDirectory $inspectorRoot -WindowStyle Hidden -PassThru
$inspectorDeadline=(Get-Date).AddMinutes(3)
while(!$inspectorProcess.HasExited -and (Get-Date)-lt $inspectorDeadline){Start-Sleep -Milliseconds 500;$inspectorProcess.Refresh()}
if(!$inspectorProcess.HasExited){Stop-Process -Id $inspectorProcess.Id -Force;throw 'Inspector capture timed out'}
$inspectorProcess.WaitForExit();$inspectorText=Get-Content $inspectorLog -Raw -Encoding UTF8
if($inspectorProcess.ExitCode-ne 0 -or $inspectorText-match 'Angelscript: Error:|HOVER_TEST_FAIL|Fatal error:|TowerError'){throw "Inspector capture failed: $inspectorLog"}
if(!(Test-Path $inspectorImage)-or (Get-Item $inspectorImage).LastWriteTime-lt $inspectorStarted){throw 'Missing or stale inspector capture'}
Write-Output "Passed inspector capture: $inspectorImage"
