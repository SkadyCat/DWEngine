param([switch]$Trace)
$ErrorActionPreference='Stop'
$opusRoot=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$opusOut=Join-Path $opusRoot 'Saved\Verification\FrameSpreading'
New-Item -ItemType Directory -Path $opusOut -Force | Out-Null
function Run-FrameCheck([string]$Name,[string[]]$Flags,[bool]$Render=$false){
 $opusLog=Join-Path $opusOut ($Name+'.log')
 $opusArgs=@('"'+(Join-Path $opusRoot 'DungeonWorldOpus.uproject')+'"','/Engine/Maps/Entry?game=/Script/Angelscript.DWOTowerGameMode','-game','-DWOTransient','-Multiprocess','-unattended','-nosplash','-nosound','-NoLoadingScreen',"-ABSLOG=$opusLog")+$Flags
 if($Render){$opusArgs+=@('-windowed','-ResX=1600','-ResY=900',"-trace=cpu,frame,bookmark,loadtime",('-tracefile='+($opusOut.Replace('\','/')+'/'+$Name+'.utrace')))}else{$opusArgs+='-nullrhi'}
 $opusProcess=Start-Process -FilePath 'G:\UnrealEngine-Angelscript-5.8.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' -ArgumentList $opusArgs -WorkingDirectory $opusRoot -WindowStyle Hidden -PassThru
 $opusDeadline=(Get-Date).AddMinutes(4)
 while(!$opusProcess.HasExited -and (Get-Date)-lt $opusDeadline){Start-Sleep -Milliseconds 500;$opusProcess.Refresh()}
 if(!$opusProcess.HasExited){Stop-Process -Id $opusProcess.Id -Force;throw "Frame check timed out: $Name"}
 $opusProcess.WaitForExit();$opusText=Get-Content -LiteralPath $opusLog -Raw -Encoding UTF8
 if($opusProcess.ExitCode-ne 0 -or $opusText-match 'Angelscript: Error:|FRAME_TEST_FAIL|TowerError|Fatal error:'){throw "Frame check failed: $opusLog"}
 Write-Host "Passed $Name"
 return $opusText
}
foreach($opusDensity in @(20,300)){
 $opusText=Run-FrameCheck "schedule-$opusDensity" @('-DWOFrameTest',"-DWODensity=$opusDensity",'-DWOGod','-DWOSmoke=120')
 if($opusText-notmatch 'FRAME_TEST_PASS RetryAndAssetCache' -or [regex]::Matches($opusText,'FRAME_TEST_PASS StagedEncounter').Count-ne 2){throw "Incomplete frame check at density $opusDensity"}
 if([regex]::Matches($opusText,"FRAME_TEST_PASS AllMonstersWithBoss count=$opusDensity bosses=1").Count-ne 2){throw "Entire dungeon was not published together at density $opusDensity"}
}
if($Trace){
 $opusText=Run-FrameCheck 'schedule-rendered' @('-DWOFrameTest','-DWOGod','-DWOSmoke=120') $true
 if($opusText-notmatch 'FRAME_TEST_PASS RetryAndAssetCache'){throw 'Rendered frame check incomplete'}
}
Write-Host "Frame spreading verified: $opusOut"
