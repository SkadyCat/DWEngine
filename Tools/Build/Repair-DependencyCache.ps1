param([string]$EngineRoot='G:/UnrealEngine-Angelscript-5.8.3')
$ErrorActionPreference='Stop'
$dependencyPatch=Join-Path $PSScriptRoot 'Patches/CppDependencyCache-project-pch.patch'
$dependencySource=Join-Path $EngineRoot 'Engine/Source/Programs/UnrealBuildTool/System/CppDependencyCache.cs'
if(!(Test-Path -LiteralPath $dependencySource)){throw "Missing UnrealBuildTool source: $dependencySource"}
$dependencyMarker='Mount(TargetDescriptor.ProjectFile, TargetDescriptor.Name, TargetDescriptor.Platform, TargetDescriptor.Configuration, TargetDescriptor.Architectures, TargetDescriptor.IntermediateEnvironment, Logger);'
if((Get-Content -LiteralPath $dependencySource -Raw).Contains($dependencyMarker)){Write-Host 'Project PCH cache fix is already applied.'}else{
 & git -C $EngineRoot apply --check $dependencyPatch
 if($LASTEXITCODE-ne 0){throw 'The engine source differs from this patch; inspect it before applying.'}
 & git -C $EngineRoot apply $dependencyPatch
 if($LASTEXITCODE-ne 0){throw 'Could not apply the project PCH cache fix.'}
}
& (Join-Path $EngineRoot 'Engine/Build/BatchFiles/BuildUBT.bat')
if($LASTEXITCODE-ne 0){throw 'UnrealBuildTool rebuild failed.'}
Write-Host 'UnrealBuildTool project PCH dependency cache repaired.'
