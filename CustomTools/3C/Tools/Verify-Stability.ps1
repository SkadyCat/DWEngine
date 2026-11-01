param([ValidateSet('baseline-60','stable-60','stable-15')][string[]]$Tag=@('baseline-60','stable-60','stable-15'))
$ErrorActionPreference='Stop'
$c3Root=Split-Path $PSScriptRoot -Parent
$c3Engine='G:\UnrealEngine-Angelscript-5.8.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
foreach($c3Tag in $Tag){
 $c3Mode=if($c3Tag -eq 'baseline-60'){'opus'}else{'controlled'}
 $c3FPS=if($c3Tag -eq 'stable-15'){15}else{60}
 $c3Log=Join-Path $c3Root ("Saved\stability-$c3Tag.log")
 $c3Args=@('"'+(Join-Path $c3Root 'ThreeCLab.uproject')+'"','/Game/ThreeCLab/Maps/L_ThreeCArena?game=/Script/Angelscript.ThreeCGameMode','-game','-NullRHI','-Multiprocess','-unattended','-nosplash','-nosound','-as-exit-on-error','-C3Seconds=40',"-C3Mode=$c3Mode","-C3FPS=$c3FPS","-C3Tag=$c3Tag",'-ABSLOG="'+$c3Log+'"')
 $c3Process=Start-Process -FilePath $c3Engine -ArgumentList $c3Args -WorkingDirectory $c3Root -WindowStyle Hidden -PassThru
 $c3Deadline=(Get-Date).AddMinutes(3)
 while(!$c3Process.HasExited -and (Get-Date)-lt $c3Deadline){Start-Sleep -Milliseconds 500;$c3Process.Refresh()}
 if(!$c3Process.HasExited){Stop-Process -Id $c3Process.Id -Force;throw "Own stability test timed out: $c3Tag"}
 $c3Process.WaitForExit();$c3Text=Get-Content -LiteralPath $c3Log -Raw
 if($c3Process.ExitCode-ne 0 -or $c3Text-notmatch 'C3_READY count=100 factions=34/33/33 models=15' -or $c3Text-notmatch 'C3_RUN_PASS count=100' -or $c3Text-match 'Angelscript: Error:|Fatal error:'){throw "Stability test failed: $c3Log"}
 Write-Host "Passed $c3Tag (100 units / 3 factions)"
}
& python (Join-Path $PSScriptRoot 'analyze_stability.py') baseline-60 stable-60 stable-15
if($LASTEXITCODE-ne 0){throw '3C stability analysis failed'}
