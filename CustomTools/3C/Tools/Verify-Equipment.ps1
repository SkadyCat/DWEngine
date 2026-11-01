$ErrorActionPreference='Stop'
$c3Root=Split-Path $PSScriptRoot -Parent
$c3Log=Join-Path $c3Root 'Saved\equipment-probe.log'
$c3Args=@('"'+(Join-Path $c3Root 'ThreeCLab.uproject')+'"','/Game/ThreeCLab/Maps/L_ThreeCArena?game=/Script/Angelscript.ThreeCGameMode','-game','-NullRHI','-Multiprocess','-noshaderworker','-unattended','-nosplash','-nosound','-as-exit-on-error','-C3EquipmentProbe=on','-C3Tag=equipment-probe','-ABSLOG="'+$c3Log+'"')
$c3Process=Start-Process -FilePath 'G:\UnrealEngine-Angelscript-5.8.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' -ArgumentList $c3Args -WorkingDirectory $c3Root -WindowStyle Hidden -PassThru
$c3Deadline=(Get-Date).AddMinutes(4)
while(!$c3Process.HasExited -and (Get-Date)-lt $c3Deadline){Start-Sleep -Milliseconds 500;$c3Process.Refresh()}
if(!$c3Process.HasExited){Stop-Process -Id $c3Process.Id -Force;throw 'Own equipment probe timed out'}
$c3Process.WaitForExit();$c3Text=Get-Content -LiteralPath $c3Log -Raw
if($c3Process.ExitCode-ne 0 -or $c3Text-notmatch 'C3_EQUIPMENT_PROBE_PASS models=15 cases=75' -or $c3Text-match 'Angelscript: Error:|Fatal error:|C3_EQUIPMENT_.*FAILED'){throw "Equipment probe failed: $c3Log"}
& python (Join-Path $PSScriptRoot 'analyze_equipment.py')
if($LASTEXITCODE-ne 0){throw 'Native equipment attachment analysis failed'}
