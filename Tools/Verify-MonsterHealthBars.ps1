param([switch]$Visuals)
$ErrorActionPreference='Stop'
$opusRoot=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$opusProof=Join-Path $opusRoot 'Saved\Verification\MonsterHealthBars'
New-Item -ItemType Directory -Path $opusProof -Force | Out-Null
function Run-HealthCheck([string]$Name,[string[]]$Extra,[string[]]$Captures=@()){
 $opusLog=Join-Path $opusProof ($Name+'.log')
 $opusArgs=@('"'+(Join-Path $opusRoot 'DungeonWorldOpus.uproject')+'"','/Engine/Maps/Entry?game=/Script/Angelscript.DWOTowerGameMode','-game','-DWOTransient','-unattended','-nosplash','-nosound','-NoLoadingScreen',"-ABSLOG=$opusLog")+$Extra
 if($Captures.Count){$opusArgs+=@('-windowed','-ResX=1600','-ResY=900','-SvgCanvasNoMotion')}else{$opusArgs+='-nullrhi'}
 $opusStarted=Get-Date
 $opusProcess=Start-Process -FilePath 'G:\UnrealEngine-Angelscript-5.8.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' -ArgumentList $opusArgs -WorkingDirectory $opusRoot -WindowStyle Hidden -PassThru
 $opusDeadline=(Get-Date).AddMinutes(3)
 while(!$opusProcess.HasExited -and (Get-Date)-lt $opusDeadline){Start-Sleep -Milliseconds 500;$opusProcess.Refresh()}
 if(!$opusProcess.HasExited){Stop-Process -Id $opusProcess.Id -Force;throw "Health-bar check timed out: $Name"}
 $opusProcess.WaitForExit();$opusText=Get-Content -LiteralPath $opusLog -Raw -Encoding UTF8
 if($opusProcess.ExitCode-ne 0 -or $opusText-match 'Angelscript: Error:|HEALTH_TEST_FAIL|TowerError|Fatal error:'){throw "Health-bar check failed: $opusLog"}
 foreach($opusCapture in $Captures){$opusImage=Get-Item -LiteralPath (Join-Path $opusProof ($opusCapture+'.png'));if($opusImage.LastWriteTime-lt $opusStarted){throw "Stale health-bar image: $opusCapture"}}
 Write-Host "Passed $Name"
 return $opusText
}
$opusIntegration=Run-HealthCheck 'integration-final' @('-DWOHealthbarTest','-DWOGod','-DWOSmoke=30')
foreach($opusContract in @('SpawnAndInput shielded=[1-9]','NativeDamage','Pause','TrailCatchUp','DeathAndBoss','RuntimeIntegration')){
 if($opusIntegration-notmatch ('HEALTH_TEST_PASS '+$opusContract)){throw "Missing integration result: $opusContract"}
}
if($Visuals){
 $opusCaptureRoot=$opusProof.Replace('\','/')
 $null=Run-HealthCheck 'visual-battle' @('-DWOStart','-DWOGod','-DWOSmoke=7',"-DWOCapture=$opusCaptureRoot/battle-final.png") @('battle-final')
 $null=Run-HealthCheck 'visual-damage-boss' @('-DWOHealthbarTest','-DWOGod','-DWOSmoke=30',"-DWOCaptureDamage=$opusCaptureRoot/damage-final.png","-DWOCapture=$opusCaptureRoot/boss-final.png") @('damage-final','boss-final')
}
Write-Host "Monster health bars verified: $opusProof"
