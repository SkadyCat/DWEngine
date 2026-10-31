param([int]$TimeoutSeconds=180)
$ErrorActionPreference='Stop'
$integrationRoot=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$integrationProof=Join-Path $integrationRoot 'Saved/Verification/UEIntegration'
New-Item -ItemType Directory -Path $integrationProof -Force | Out-Null
$integrationLog=Join-Path $integrationProof ('content-audit-'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'.log')
$integrationEngine='G:\UnrealEngine-Angelscript-5.8.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$integrationArgs=@('"'+(Join-Path $integrationRoot 'DungeonWorldOpus.uproject')+'"','/Engine/Maps/Entry?game=/Script/Angelscript.DWOContentAuditMode','-game','-DWOTransient','-nullrhi','-Multiprocess','-unattended','-nosplash','-nosound','-NoLoadingScreen','-as-exit-on-error',"-ABSLOG=$integrationLog")
$integrationProcess=Start-Process -FilePath $integrationEngine -ArgumentList $integrationArgs -WorkingDirectory $integrationRoot -WindowStyle Hidden -PassThru
$integrationDeadline=(Get-Date).AddSeconds($TimeoutSeconds)
while(!$integrationProcess.HasExited -and (Get-Date)-lt $integrationDeadline){Start-Sleep -Milliseconds 500;$integrationProcess.Refresh()}
if(!$integrationProcess.HasExited){Stop-Process -Id $integrationProcess.Id -Force;throw "Content audit timed out: $integrationLog"}
$integrationProcess.WaitForExit()
$integrationText=Get-Content -LiteralPath $integrationLog -Raw -Encoding UTF8
if($integrationProcess.ExitCode-ne 0 -or $integrationText-match 'CONTENT_TEST_FAIL|Angelscript: Error:|Fatal error:' -or $integrationText-notmatch 'CONTENT_TEST_PASS Catalog '){throw "Content audit failed: $integrationLog"}
$integrationText -split '\r?\n' | Where-Object {$_-match 'CONTENT_TEST_PASS'} | ForEach-Object {Write-Host $_}
Write-Host "Content audit passed: $integrationLog"
