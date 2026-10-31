$ErrorActionPreference='Stop'
$quickRoot=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$quickProof=Join-Path $quickRoot 'Saved/Verification/HotbarPickerUE'
$quickLog=Join-Path $quickProof 'quick-slots-native.log'
$quickArgs=@(('"'+$quickRoot+'/DungeonWorldOpus.uproject"'),'-NullRHI','-Multiprocess','-unattended','-nosplash','-nosound','-as-exit-on-error','-DisablePlugins=UE_MCP_Bridge','-ExecCmds="Automation RunTests DWRules.UI.QuickSlots"','-TestExit="Automation Test Queue Empty"',('-ABSLOG="'+$quickLog+'"'))
$quickProcess=Start-Process 'G:/UnrealEngine-Angelscript-5.8.3/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $quickArgs -WorkingDirectory $quickRoot -WindowStyle Hidden -PassThru
$quickDeadline=(Get-Date).AddMinutes(4)
while(!$quickProcess.HasExited -and (Get-Date)-lt$quickDeadline){Start-Sleep -Milliseconds 500;$quickProcess.Refresh()}
if(!$quickProcess.HasExited){Stop-Process -Id $quickProcess.Id;throw 'Own quick-slot automation timed out'}
$quickProcess.WaitForExit();$quickText=Get-Content -LiteralPath $quickLog -Raw -Encoding utf8
if($quickProcess.ExitCode-ne 0 -or $quickText-match 'Angelscript: Error:|Fatal error:|Test Completed. Result=\{Fail' -or $quickText-notmatch 'Test Completed. Result=\{Success\}.*QuickSlots'){throw "Quick-slot automation failed: $quickLog"}
@{ok=$true;suite='DWRules.UI.QuickSlots';log=$quickLog;checks=@('fixed Q/E/T/G indices','sparse save/reload','clear preserves neighbours','duplicate moves to target','depletion preserves neighbours','dropping last item preserves neighbours')}|ConvertTo-Json -Depth 4|Set-Content -LiteralPath (Join-Path $quickProof 'quick-slots-native.json') -Encoding utf8
Write-Output 'DWRules.UI.QuickSlots passed'
