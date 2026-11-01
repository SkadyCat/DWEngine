param([ValidateSet(15,60)][int[]]$FPS=@(60,15))
$ErrorActionPreference='Stop'
$c3Root=Split-Path $PSScriptRoot -Parent
foreach($c3FPS in $FPS){
 $c3Tag="blocked-$c3FPS"
 $c3Log=Join-Path $c3Root ("Saved\$c3Tag.log")
 $c3Args=@('"'+(Join-Path $c3Root 'ThreeCLab.uproject')+'"','/Game/ThreeCLab/Maps/L_ThreeCArena?game=/Script/Angelscript.ThreeCGameMode','-game','-NullRHI','-Multiprocess','-unattended','-nosplash','-nosound','-as-exit-on-error','-C3BlockProbe=on',"-C3FPS=$c3FPS","-C3Tag=$c3Tag",'-ABSLOG="'+$c3Log+'"')
 $c3Process=Start-Process -FilePath 'G:\UnrealEngine-Angelscript-5.8.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' -ArgumentList $c3Args -WorkingDirectory $c3Root -WindowStyle Hidden -PassThru
 $c3Deadline=(Get-Date).AddMinutes(4)
 while(!$c3Process.HasExited -and (Get-Date)-lt $c3Deadline){Start-Sleep -Milliseconds 500;$c3Process.Refresh()}
 if(!$c3Process.HasExited){Stop-Process -Id $c3Process.Id -Force;throw "Own blockage probe timed out: $c3Tag"}
 $c3Process.WaitForExit();$c3Text=Get-Content -LiteralPath $c3Log -Raw
 if($c3Process.ExitCode-ne 0 -or $c3Text-notmatch 'C3_BLOCK_PROBE_PASS models=3 cases=12' -or $c3Text-match 'Angelscript: Error:|Fatal error:'){throw "Blockage probe failed: $c3Log"}
 Write-Host "Passed $c3Tag"
}
& python (Join-Path $PSScriptRoot 'analyze_blocks.py') @($FPS | ForEach-Object {"blocked-$_"})
if($LASTEXITCODE-ne 0){throw 'Native blockage analysis failed'}
