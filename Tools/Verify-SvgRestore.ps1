param([switch]$Visuals,[switch]$ChecksOnly)
$ErrorActionPreference='Stop'
$opusRoot=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$opusEngine='G:\UnrealEngine-Angelscript-5.8.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$opusProof=Join-Path $opusRoot 'Saved\Verification\SvgRestore'
New-Item -ItemType Directory -Path $opusProof -Force | Out-Null
function Run-Svg([string]$Name,[string[]]$Extra,[bool]$Render=$false){
 $opusLog=Join-Path $opusProof ($Name+'.log')
 $opusArgs=@('"'+(Join-Path $opusRoot 'DungeonWorldOpus.uproject')+'"','/Engine/Maps/Entry?game=/Script/Angelscript.DWOTowerGameMode','-game','-DWOTransient','-unattended','-nosplash','-nosound','-NoLoadingScreen',"-ABSLOG=$opusLog")+$Extra
 if($Render){$opusArgs+=@('-windowed','-ResX=1600','-ResY=900','-SvgCanvasNoMotion','-DWOSmoke=7',('-DWOCapture='+($opusProof.Replace('\','/'))+'/'+$Name+'.png'))}else{$opusArgs+='-nullrhi'}
 $opusLaunchTime=Get-Date
 $opusProcess=Start-Process -FilePath $opusEngine -ArgumentList $opusArgs -WorkingDirectory $opusRoot -WindowStyle Hidden -PassThru
 $opusDeadline=(Get-Date).AddMinutes(3)
 while(!$opusProcess.HasExited -and (Get-Date)-lt $opusDeadline){Start-Sleep -Milliseconds 500;$opusProcess.Refresh()}
 if(!$opusProcess.HasExited){Stop-Process -Id $opusProcess.Id -Force;throw "Timed out: $Name"}
 $opusProcess.WaitForExit();$opusText=Get-Content -LiteralPath $opusLog -Raw -Encoding UTF8
 if($opusProcess.ExitCode-ne 0 -or $opusText-match 'Angelscript: Error:|SVG_TEST_FAIL|AS_TEST_FAIL|TowerError|Fatal error:'){throw "Failed: $opusLog"}
 if($Render-and !(Test-Path -LiteralPath (Join-Path $opusProof ($Name+'.png')))){throw "Screenshot missing: $Name"}
 if($Render-and (Get-Item -LiteralPath (Join-Path $opusProof ($Name+'.png'))).LastWriteTime-lt $opusLaunchTime){throw "Screenshot is stale: $Name"}
 Write-Host "Passed $Name"
 return $opusText
}
if(!$Visuals){
 $opusBindings=Run-Svg 'bindings' @('-DWOContractTest')
 if($opusBindings-notmatch 'SVG_TEST_PASS RuntimeBindings'){throw 'Binding contract did not complete'}
 $opusReset=Run-Svg 'reset' @('-DWOResetTest','-DWOFloor=3','-DWOSmoke=20')
 if($opusReset-notmatch 'SVG_TEST_PASS ResetStartsCurrentFloor' -or ([regex]::Matches($opusReset,'TowerReady floor=3')).Count-lt 2){throw 'Reset did not restart the same floor'}
 $opusReady=Run-Svg 'ready' @('-DWOSmoke=2')
 if($opusReady-notmatch 'phase=0' -or $opusReady-match 'TowerEncounter'){throw 'First launch must remain Ready'}
}
if(!$ChecksOnly){
 foreach($opusView in @('picker-2','inventory','builds','shop','character','craft')){$null=Run-Svg $opusView @('-DWOView='+$opusView) $true}
 $null=Run-Svg 'source-picker' @('-DWOSvgReference','-DWOView=picker-2') $true
}
Write-Host "SVG restore evidence: $opusProof"
