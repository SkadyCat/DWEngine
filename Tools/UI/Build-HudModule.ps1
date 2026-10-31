param([switch]$ForceHeaderGeneration)
$ErrorActionPreference='Stop'
$hudBuildRoot=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$hudBuildLog=Join-Path $hudBuildRoot 'Saved/Verification/FullHUD/build-verified-source.log'
New-Item -ItemType Directory -Force (Split-Path $hudBuildLog) | Out-Null
$hudBuildArgs=@('DungeonWorldOpusEditor','Win64','Development',('-Project='+ (Join-Path $hudBuildRoot 'DungeonWorldOpus.uproject')),'-Module=DWRulesCore','-Module=DWRules','-Module=DungeonWorldOpus','-Module=SvgCanvasRuntime','-Module=SvgCanvasEditor','-WaitMutex','-UsePrecompiled','-NoEngineChanges','-NoHotReloadFromIDE','-NoUBA','-MaxParallelActions=4',('-log='+$hudBuildLog))
if($ForceHeaderGeneration){$hudBuildArgs+='-ForceHeaderGeneration'}
& 'G:/UnrealEngine-Angelscript-5.8.3/Engine/Build/BatchFiles/Build.bat' @hudBuildArgs
if($LASTEXITCODE-ne 0){throw "HUD module build failed: $hudBuildLog"}
# A -Module build skips UBT's metadata action and can delete the old manifests.
# Regenerate the real target receipt with UBT, using this installed engine's ID.
$hudEngine='G:/UnrealEngine-Angelscript-5.8.3/Engine'
$hudMetadata=Get-Content -LiteralPath (Join-Path $hudBuildRoot 'Intermediate/Build/Win64/x64/DungeonWorldOpusEditor/Development/TargetMetadata.json') -Raw | ConvertFrom-Json
$hudEngineId=(Get-Content -LiteralPath (Join-Path $hudEngine 'Binaries/Win64/UnrealEditor.modules') -Raw | ConvertFrom-Json).BuildId
$hudMetadata.Receipt.Version.BuildId=$hudEngineId
$hudMetadata.Version=$null
$hudMetadata.VersionFile=$null
$hudProjectPrefix=$hudBuildRoot.TrimEnd('/','\')+[IO.Path]::DirectorySeparatorChar
foreach($hudOutput in @($hudMetadata.ReceiptFile)+@($hudMetadata.FileToManifest.PSObject.Properties.Name)){
    if(![IO.Path]::GetFullPath($hudOutput).StartsWith($hudProjectPrefix,[StringComparison]::OrdinalIgnoreCase)){throw "Metadata output outside project: $hudOutput"}
}
foreach($hudManifest in $hudMetadata.FileToManifest.PSObject.Properties){
    foreach($hudDll in $hudManifest.Value.ModuleNameToFileName.PSObject.Properties.Value){
        if(!(Test-Path -LiteralPath (Join-Path (Split-Path $hudManifest.Name) $hudDll))){throw "Missing module for metadata: $hudDll"}
    }
}
$hudMetadataPath=Join-Path $hudBuildRoot 'Saved/Verification/FullHUD/TargetMetadata.project.json'
$hudMetadata | ConvertTo-Json -Depth 100 | Set-Content -LiteralPath $hudMetadataPath -Encoding utf8
$hudMetadataSource=Get-Content -LiteralPath (Join-Path $hudEngine 'Source/Programs/UnrealBuildTool/Modes/WriteMetadataMode.cs') -Raw
$hudMetadataVersion=[regex]::Match($hudMetadataSource,'CurrentVersionNumber\s*=\s*(\d+)').Groups[1].Value
if(!$hudMetadataVersion){throw 'Cannot determine UBT metadata version'}
& (Join-Path $hudEngine 'Binaries/ThirdParty/DotNet/10.0/win-x64/dotnet.exe') (Join-Path $hudEngine 'Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.dll') '-Mode=WriteMetadata' ('-Input='+$hudMetadataPath) ('-Version='+$hudMetadataVersion)
if($LASTEXITCODE-ne 0){throw 'UBT metadata generation failed'}
Write-Output "HUD modules compiled through UnrealBuildTool: $hudBuildLog"
