param([string]$EngineRoot='G:/UnrealEngine-Angelscript-5.8.3')
$ErrorActionPreference='Stop'
$incrementalRoot=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$incrementalProof=Join-Path $incrementalRoot 'Saved/Verification/IncrementalBuild'
New-Item -ItemType Directory -Path $incrementalProof -Force | Out-Null
$incrementalResults=@()
foreach($incrementalPass in 1..2){
 $incrementalLog=Join-Path $incrementalProof "unchanged-$incrementalPass.log"
 # Match the user's Development Editor / Win64 Visual Studio build flags.
 & (Join-Path $EngineRoot 'Engine/Build/BatchFiles/Build.bat') DungeonWorldOpusEditor Win64 Development "-Project=$incrementalRoot/DungeonWorldOpus.uproject" -UsePrecompiled -WaitMutex -FromMsBuild -2022 -architecture=x64 -NoEngineChanges -NoHotReloadFromIDE -NoUBA -MaxParallelActions=8 "-log=$incrementalLog"
 if($LASTEXITCODE-ne 0){throw "Incremental build failed: $incrementalLog"}
 $incrementalText=Get-Content -LiteralPath $incrementalLog -Raw -Encoding UTF8
 if($incrementalText-notmatch 'Target is up to date' -or $incrementalText-match '(?m)^\[\d+/\d+\]|There is no partition covering'){
  throw "Unchanged build queued work or lost dependency tracking: $incrementalLog"
 }
 $incrementalSeconds=[regex]::Match($incrementalText,'Total execution time: ([\d.]+) seconds').Groups[1].Value
 $incrementalResults+=@{pass=$incrementalPass;actions=0;targetUpToDate=$true;seconds=[double]$incrementalSeconds;log=$incrementalLog}
}
$incrementalResults | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $incrementalProof 'results.json') -Encoding UTF8
Write-Host 'Passed: two consecutive unchanged builds, zero compile/link actions.'
