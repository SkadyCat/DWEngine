param([ValidateSet('Build','Rebuild','Clean')][string]$Mode='Build')
$ErrorActionPreference='Stop'
$opusRoot=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$opusProof=Join-Path $opusRoot 'Saved/Verification/OpusVSBuild'
New-Item -ItemType Directory -Path $opusProof -Force | Out-Null
$opusLog=Join-Path $opusProof ('native-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff')+'.log')
Write-Host "Opus editor $Mode using the precompiled engine. Full diagnostics: $opusLog"
# The root entry can recover missing precompiled SDK import libraries. Let its
# final exit code decide success, so a recovered LNK1181 does not poison MSBuild.
try {
 $ErrorActionPreference='Continue'
 & powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $opusRoot 'Build-Opus.ps1') -Mode $Mode *> $opusLog
 $opusExit=$LASTEXITCODE
} finally {$ErrorActionPreference='Stop'}
if($opusExit-ne 0){
 Get-Content -LiteralPath $opusLog
 Write-Host "Opus editor $Mode failed. Full diagnostics: $opusLog"
 exit $opusExit
}
Get-Content -LiteralPath $opusLog | Where-Object {
 $_ -match 'Target is up to date|Total execution time:|^RESTORED SDK import|^LINKED |^Receipt written|^exit='
} | ForEach-Object {Write-Host $_}
Write-Host "Opus editor $Mode succeeded. Full diagnostics: $opusLog"
exit 0
