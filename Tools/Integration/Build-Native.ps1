param([switch]$ForceHeaderGeneration)
$ErrorActionPreference='Stop'
$integrationRoot=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$integrationLog=Join-Path $integrationRoot 'Saved/Verification/UEIntegration/build-native.log'
New-Item -ItemType Directory -Force (Split-Path $integrationLog) | Out-Null
$integrationArgs=@('DungeonWorldOpusEditor','Win64','Development',('-Project='+ (Join-Path $integrationRoot 'DungeonWorldOpus.uproject')),'-Module=DWRulesCore','-Module=DWRules','-Module=DungeonWorldOpus','-Module=SvgCanvasRuntime','-Module=SvgCanvasEditor','-WaitMutex','-UsePrecompiled','-NoEngineChanges','-NoHotReloadFromIDE','-NoUBA','-MaxParallelActions=4',('-log='+$integrationLog))
if($ForceHeaderGeneration){$integrationArgs+='-ForceHeaderGeneration'}
& 'G:/UnrealEngine-Angelscript-5.8.3/Engine/Build/BatchFiles/Build.bat' @integrationArgs
if($LASTEXITCODE-ne 0){throw "Native integration build failed: $integrationLog"}
Write-Output "Native integration modules compiled: $integrationLog"
