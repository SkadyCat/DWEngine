param([switch]$Visuals,[switch]$IdentificationVisuals)
$ErrorActionPreference='Stop'
$opusRoot=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$opusProof=Join-Path $opusRoot 'Saved\Verification\ItemHover'
New-Item -ItemType Directory -Path $opusProof -Force | Out-Null
function Run-Hover([string]$Name,[string[]]$Extra,[bool]$Render=$false){
 $opusLog=Join-Path $opusProof ($Name+'.log')
 $opusContract=Get-Content -LiteralPath (Join-Path $opusRoot 'Content/UICreatorHover/contract.json') -Raw -Encoding UTF8 | ConvertFrom-Json
 $opusArgs=@('"'+(Join-Path $opusRoot 'DungeonWorldOpus.uproject')+'"','/Engine/Maps/Entry?game=/Script/Angelscript.DWOTowerGameMode','-game','-DWOTransient','-Multiprocess','-as-exit-on-error','-unattended','-nosplash','-nosound','-NoLoadingScreen',('-DWOExpectedHoverHash='+$opusContract.sourceHash),"-ABSLOG=$opusLog")+$Extra
 if($Render){$opusArgs+=@('-windowed','-RenderOffscreen','-ResX=1600','-ResY=900','-SvgCanvasNoMotion','-DWOSmoke=7',('-DWOCapture='+$opusProof.Replace('\','/')+'/'+$Name+'.png'))}else{$opusArgs+=@('-nullrhi','-DWOSmoke=20')}
 $opusStarted=Get-Date
 $opusProcess=Start-Process -FilePath 'G:\UnrealEngine-Angelscript-5.8.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' -ArgumentList $opusArgs -WorkingDirectory $opusRoot -WindowStyle Hidden -PassThru
 $opusDeadline=(Get-Date).AddMinutes(3)
 while(!$opusProcess.HasExited -and (Get-Date)-lt $opusDeadline){Start-Sleep -Milliseconds 500;$opusProcess.Refresh()}
 if(!$opusProcess.HasExited){Stop-Process -Id $opusProcess.Id -Force;throw "Hover check timed out: $Name"}
 $opusProcess.WaitForExit();$opusText=Get-Content -LiteralPath $opusLog -Raw -Encoding UTF8
 if($opusProcess.ExitCode-ne 0 -or $opusText-match 'Angelscript: Error:|HOVER_TEST_FAIL|TowerError|Fatal error:'){throw "Hover check failed: $opusLog"}
 if($Render){$opusImage=Get-Item -LiteralPath (Join-Path $opusProof ($Name+'.png'));if($opusImage.LastWriteTime-lt $opusStarted){throw "Stale hover image: $Name"}}
 Write-Host "Passed $Name"
 return $opusText
}
$opusIntegration=Run-Hover 'integration' @('-DWOHoverTest')
if($opusIntegration-notmatch 'HOVER_TEST_PASS RuntimeInspector'){throw 'Hover integration did not complete'}
if($opusIntegration-notmatch 'HOVER_TEST_PASS IdentifyInPlace'){throw 'Hover identification did not update in place'}
foreach($opusMarker in @('InlineGradeCraft','WholeCraftInPlace')){if($opusIntegration-notmatch ('HOVER_TEST_PASS '+$opusMarker)){throw "Missing inline inspector check: $opusMarker"}}
if($Visuals){
 $null=Run-Hover 'hover-affixed' @('-DWOHoverView=affixed') $true
 $null=Run-Hover 'hover-pinned' @('-DWOHoverView=affixed','-DWOHoverPinned') $true
 $null=Run-Hover 'hover-unidentified' @('-DWOHoverView=unidentified') $true
}
if($Visuals-or $IdentificationVisuals){
 $null=Run-Hover 'hover-identify-before' @('-DWOHoverView=unidentified','-DWOHoverPinned') $true
 $opusIdentified=Run-Hover 'hover-identify-after' @('-DWOHoverView=identified') $true
 if($opusIdentified-notmatch 'HOVER_TEST_PASS IdentifyInPlace'){throw 'Visual identification did not update the pinned hover'}
}
Write-Host "Item hover evidence: $opusProof"
