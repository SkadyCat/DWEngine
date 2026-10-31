$ErrorActionPreference='Stop'
$inspectorRoot=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$inspectorProof=Join-Path $inspectorRoot 'Saved/Verification/ItemHover'
$inspectorLog=Join-Path $inspectorProof 'projection-tests.log'
$inspectorArgs=@(('"'+$inspectorRoot+'/DungeonWorldOpus.uproject"'),'-Multiprocess','-unattended','-nosplash','-nosound','-as-exit-on-error','-nullrhi','-ExecCmds="Automation RunTests Opus.UI.Inspector"','-TestExit="Automation Test Queue Empty"',('-ReportExportPath="'+$inspectorProof+'/projection-tests"'),('-ABSLOG="'+$inspectorLog+'"'))
$inspectorProcess=Start-Process -FilePath 'G:/UnrealEngine-Angelscript-5.8.3/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $inspectorArgs -WorkingDirectory $inspectorRoot -WindowStyle Hidden -PassThru
$inspectorDeadline=(Get-Date).AddMinutes(3)
while(!$inspectorProcess.HasExited -and (Get-Date)-lt $inspectorDeadline){Start-Sleep -Milliseconds 500;$inspectorProcess.Refresh()}
if(!$inspectorProcess.HasExited){Stop-Process -Id $inspectorProcess.Id -Force;throw 'Inspector projection tests timed out'}
$inspectorProcess.WaitForExit();$inspectorText=Get-Content -LiteralPath $inspectorLog -Raw -Encoding UTF8
if($inspectorProcess.ExitCode-ne 0 -or $inspectorText-match 'Angelscript: Error:|Fatal error:|Test Completed. Result=\{Fail'){throw "Inspector tests failed: $inspectorLog"}
foreach($inspectorCase in @('ConcreteParameters','MovedInteractionBrushes')){if($inspectorText-notmatch ('Test Completed. Result=\{Success\}.*'+$inspectorCase)){throw "Inspector case did not pass: $inspectorCase"}}
Write-Output 'Passed inspector parameter and interaction-brush regressions'
