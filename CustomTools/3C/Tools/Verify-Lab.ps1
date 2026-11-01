$ErrorActionPreference='Stop'
$c3Root=Split-Path $PSScriptRoot -Parent
$c3Engine='G:\UnrealEngine-Angelscript-5.8.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
foreach($c3Mode in @('opus','controlled')){
 $c3Log=Join-Path $c3Root ("Saved\verify-$c3Mode.log")
 $c3Args=@('"'+(Join-Path $c3Root 'ThreeCLab.uproject')+'"','/Game/ThreeCLab/Maps/L_ThreeCArena?game=/Script/Angelscript.ThreeCGameMode','-game','-NullRHI','-Multiprocess','-unattended','-nosplash','-nosound','-as-exit-on-error','-C3Seconds=40',"-C3Mode=$c3Mode","-C3Tag=$c3Mode","-ABSLOG=$c3Log")
 $c3Process=Start-Process -FilePath $c3Engine -ArgumentList $c3Args -WorkingDirectory $c3Root -WindowStyle Hidden -PassThru
 $c3Deadline=(Get-Date).AddMinutes(5)
 while(!$c3Process.HasExited -and (Get-Date)-lt $c3Deadline){Start-Sleep -Milliseconds 500;$c3Process.Refresh()}
 if(!$c3Process.HasExited){Stop-Process -Id $c3Process.Id -Force;throw "Lab timed out: $c3Mode"}
 $c3Process.WaitForExit();$c3Text=Get-Content -LiteralPath $c3Log -Raw
 if($c3Process.ExitCode-ne 0 -or $c3Text-notmatch 'C3_READY count=100 factions=34/33/33 models=15' -or $c3Text-notmatch 'C3_RUN_PASS count=100' -or $c3Text-match 'Angelscript: Error:|Fatal error:'){throw "Lab verification failed: $c3Log"}
 Write-Host "Passed $c3Mode (100 units / 3 factions)"
}
& python (Join-Path $PSScriptRoot 'analyze.py')
if($LASTEXITCODE-ne 0){throw '3C diagnostic analysis failed'}
