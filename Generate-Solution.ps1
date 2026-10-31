param([switch]$Open)
$ErrorActionPreference='Stop'
$opusRoot=$PSScriptRoot
$opusEngine='G:\UnrealEngine-Angelscript-5.8.3'
$opusProject=Join-Path $opusRoot 'DungeonWorldOpus.uproject'
$opusSolution=Join-Path $opusRoot 'DungeonWorldOpus.sln'
$opusLog=Join-Path $opusRoot 'Saved\Build\generate-solution.log'
New-Item -ItemType Directory -Path (Split-Path $opusLog) -Force | Out-Null
$env:DOTNET_CLI_HOME='G:\UE-Angelscript-Build\DotNetHome'
$env:NUGET_PACKAGES='G:\UE-Angelscript-Build\NuGetPackages'
$env:NUGET_HTTP_CACHE_PATH='G:\UE-Angelscript-Build\NuGetHttpCache'
$env:DOTNET_CLI_TELEMETRY_OPTOUT='1'
$env:DOTNET_NOLOGO='1'
& "$opusEngine\Engine\Build\BatchFiles\GenerateProjectFiles.bat" "-Project=$opusProject" -Game -2022 -UsePrecompiled *> $opusLog
if($LASTEXITCODE-ne 0){throw "Solution generation failed: $opusLog"}

# Retain the normal Unreal projects and build the game against the existing
# engine binaries, with the same build flags used by Build-Opus.ps1.
$opusVcx=Join-Path $opusRoot 'Intermediate\ProjectFiles\DungeonWorldOpus.vcxproj'
$opusXml=[xml](Get-Content -LiteralPath $opusVcx -Raw)
$opusNs=[System.Xml.XmlNamespaceManager]::new($opusXml.NameTable)
$opusNs.AddNamespace('m','http://schemas.microsoft.com/developer/msbuild/2003')
foreach($opusCommand in $opusXml.SelectNodes('//m:NMakeBuildCommandLine|//m:NMakeReBuildCommandLine',$opusNs)){
 foreach($opusFlag in @('-UsePrecompiled','-NoEngineChanges','-NoHotReloadFromIDE','-NoUBA','-MaxParallelActions=8')){
  if(!$opusCommand.InnerText.Contains($opusFlag)){$opusCommand.InnerText+=' '+$opusFlag}
 }
}
$opusXml.Save($opusVcx)
& (Join-Path $opusRoot 'Tools/Build/Configure-OpusSolution.ps1')

# F5 runs the current tower in a game window, using the Development Editor
# executable and the native modules Visual Studio has just built.
$opusUserFile=$opusVcx+'.user'
$opusUser=[xml](Get-Content -LiteralPath $opusUserFile -Raw)
$opusUserNs=[System.Xml.XmlNamespaceManager]::new($opusUser.NameTable)
$opusUserNs.AddNamespace('m','http://schemas.microsoft.com/developer/msbuild/2003')
$opusCondition="'`$(Configuration)|`$(Platform)'=='Development_Editor|x64'"
$opusGroup=$opusUser.SelectNodes('//m:PropertyGroup',$opusUserNs) | Where-Object {$_.Condition-eq $opusCondition} | Select-Object -First 1
if(!$opusGroup){throw 'Generated Development Editor configuration is missing'}
$opusDebug=@{
 LocalDebuggerCommand="$opusEngine\Engine\Binaries\Win64\UnrealEditor.exe"
 LocalDebuggerCommandArguments='"$(SolutionDir)DungeonWorldOpus.uproject" /Game/ClaudeDungeon/Maps/L_OpusDungeon?game=/Script/Angelscript.DWOTowerGameMode -game -windowed -ResX=1600 -ResY=900 -NoSplash -NoLoadingScreen'
 LocalDebuggerWorkingDirectory='$(SolutionDir)'
 DebuggerFlavor='WindowsLocalDebugger'
}
foreach($opusPair in $opusDebug.GetEnumerator()){
 $opusNode=$opusGroup.SelectSingleNode('m:'+$opusPair.Key,$opusUserNs)
 if(!$opusNode){$opusNode=$opusUser.CreateElement($opusPair.Key,$opusUser.DocumentElement.NamespaceURI);$null=$opusGroup.AppendChild($opusNode)}
 $opusNode.InnerText=$opusPair.Value
}
$opusUser.Save($opusUserFile)
Write-Host "Solution ready: $opusSolution"
Write-Host 'Development Editor | Win64; startup project DungeonWorldOpus; F5 starts the tower.'
if($Open){
 $opusVS='G:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\devenv.exe'
 Start-Process -FilePath $opusVS -ArgumentList @('"'+$opusSolution+'"') -WorkingDirectory $opusRoot -WindowStyle Normal
}
