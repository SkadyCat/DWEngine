$ErrorActionPreference='Stop'
$c3Root=Split-Path $PSScriptRoot -Parent
$c3Log=Join-Path $c3Root 'Saved\gait-probe.log'
$c3Args=@('"'+(Join-Path $c3Root 'ThreeCLab.uproject')+'"','/Game/ThreeCLab/Maps/L_ThreeCArena?game=/Script/Angelscript.ThreeCGameMode','-game','-NullRHI','-Multiprocess','-unattended','-nosplash','-nosound','-as-exit-on-error','-C3GaitProbe=on','-C3Mode=controlled','-C3Tag=gait-probe-battle','-ABSLOG="'+$c3Log+'"')
$c3Process=Start-Process -FilePath 'G:\UnrealEngine-Angelscript-5.8.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' -ArgumentList $c3Args -WorkingDirectory $c3Root -WindowStyle Hidden -PassThru
$c3Deadline=(Get-Date).AddMinutes(5)
while(!$c3Process.HasExited -and (Get-Date)-lt $c3Deadline){Start-Sleep -Milliseconds 500;$c3Process.Refresh()}
if(!$c3Process.HasExited){Stop-Process -Id $c3Process.Id -Force;throw 'Own gait probe timed out'}
$c3Process.WaitForExit();$c3Text=Get-Content -LiteralPath $c3Log -Raw
if($c3Process.ExitCode-ne 0 -or $c3Text-notmatch 'C3_GAIT_PROBE_PASS variants=25 cases=50' -or $c3Text-match 'Angelscript: Error:|Fatal error:'){throw "Gait probe failed: $c3Log"}
& python (Join-Path $PSScriptRoot 'analyze_gaits.py')
if($LASTEXITCODE-ne 0){throw 'Native gait analysis failed'}
