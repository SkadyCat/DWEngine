param([switch]$SmokeOnly,[switch]$SkipSmoke,[switch]$SkipNative)
$ErrorActionPreference='Stop'
$opusRoot=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$opusEngine='G:\UnrealEngine-Angelscript-5.8.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$opusLogRoot=Join-Path $opusRoot 'Saved\Verification'
New-Item -ItemType Directory -Path $opusLogRoot -Force | Out-Null
$opusFixtureRoot=Join-Path $PSScriptRoot 'Verification\Fixtures'
$opusReferenceRoot=Join-Path $opusRoot 'Saved\RulesMigration'
New-Item -ItemType Directory -Path $opusReferenceRoot -Force | Out-Null
foreach($opusArchive in Get-ChildItem -LiteralPath $opusFixtureRoot -Filter '*.json.gz'){
 $opusFixture=Join-Path $opusReferenceRoot $opusArchive.BaseName
 if(!(Test-Path -LiteralPath $opusFixture)){
  $opusInput=[IO.File]::OpenRead($opusArchive.FullName)
  $opusGzip=[IO.Compression.GZipStream]::new($opusInput,[IO.Compression.CompressionMode]::Decompress)
  $opusOutput=[IO.File]::Create($opusFixture)
  try{$opusGzip.CopyTo($opusOutput)}finally{$opusOutput.Dispose();$opusGzip.Dispose();$opusInput.Dispose()}
 }
}
function Run-OpusCheck([string[]]$Arguments,[string]$Name,[int]$MaxSeconds=180){
 $opusLog=Join-Path $opusLogRoot ($Name+'.log')
 $opusArguments=@('"'+(Join-Path $opusRoot 'DungeonWorldOpus.uproject')+'"')+$Arguments+@('-nullrhi','-Multiprocess','-unattended','-nosplash','-nosound','-NoLoadingScreen',"-ABSLOG=$opusLog")
 $opusProcess=Start-Process -FilePath $opusEngine -ArgumentList $opusArguments -WindowStyle Hidden -WorkingDirectory $opusRoot -PassThru
 $opusDeadline=(Get-Date).AddSeconds($MaxSeconds)
 while(!$opusProcess.HasExited -and (Get-Date)-lt $opusDeadline){Start-Sleep -Milliseconds 500;$opusProcess.Refresh()}
 if(!$opusProcess.HasExited){Stop-Process -Id $opusProcess.Id -Force;throw "Timed out: $opusLog"}
 $opusProcess.WaitForExit()
 $opusText=Get-Content -LiteralPath $opusLog -Raw -Encoding UTF8
 if($opusProcess.ExitCode-ne 0 -or $opusText-match 'Angelscript: Error:|AS_TEST_FAIL|SVG_TEST_FAIL|TowerError|TowerCarryMismatch'){throw "Runtime failed: $opusLog"}
 Write-Host "Passed process: $Name"
 return $opusText
}
$opusGame='/Engine/Maps/Entry?game=/Script/Angelscript.DWOTowerGameMode'
$opusRuntime=@($opusGame,'-game','-DWOTransient')
if(!$SmokeOnly -and !$SkipNative){
 $opusNative=Run-OpusCheck @('/Engine/Maps/Entry','-ExecCmds="Automation RunTests DWRules+DWB.Rules"','-TestExit="Automation Test Queue Empty"',"-ReportExportPath=$opusLogRoot\native-report") 'native-final'
 $opusPasses=[regex]::Matches($opusNative,'Test Completed\. Result=\{Success\}').Count
 if($opusNative-match 'Result=\{Fail' -or $opusPasses-lt 35){throw "Native suite failed: $opusPasses successful tests"}
}
if(!$SkipSmoke){
 $opusFlow=Run-OpusCheck ($opusRuntime+@('-DWOFlowTest')) 'as-final-flow'
 if($opusFlow-notmatch 'AS_TEST_PASS TowerFlow'){throw 'AS flow check failed'}
 $opusBindings=Run-OpusCheck ($opusRuntime+@('-DWOContractTest')) 'svg-final-bindings'
 if($opusBindings-notmatch 'SVG_TEST_PASS RuntimeBindings'){throw 'SVG integration check failed'}
 $opusReady=Run-OpusCheck ($opusRuntime+@('-DWOSmoke=3')) 'tower-final-ready'
 if($opusReady-notmatch 'TowerSmokeComplete floor=1 phase=0 cleared=0' -or $opusReady-match 'TowerEncounter'){throw 'Default must remain in Ready until manual start'}
 $opusClear=Run-OpusCheck ($opusRuntime+@('-DWOStart','-DWOAuto','-DWOGod','-DWOAutoAdvance','-DWOAssertCarry','-DWOPreset=myriad_swords','-DWOSmoke=25')) 'tower-final-success'
 if($opusClear-notmatch 'TowerCleared floor=1 .*kills=20' -or $opusClear-notmatch 'TowerReady floor=2' -or $opusClear-notmatch 'TowerCarryPass'){throw 'Clear, travel or resource carry check failed'}
 $opusDeath=Run-OpusCheck ($opusRuntime+@('-DWODeathRetry','-DWOSmoke=45')) 'tower-final-death-retry'
 if($opusDeath-notmatch 'TowerFailed floor=1 reason=dead' -or $opusDeath-notmatch 'AS_TEST_PASS DeathRetryReady'){throw 'Death/retry/Ready check failed'}
 $opusDisk=Join-Path $opusLogRoot 'ProfileRoundtrip.json'
 if(Test-Path -LiteralPath $opusDisk){Remove-Item -LiteralPath $opusDisk}
 $null=Run-OpusCheck @($opusGame,'-game','-DWOProfileTest','-DWOSmoke=2') 'profile-final-write'
 if(!(Test-Path -LiteralPath $opusDisk)){throw 'Disk save missing'}
 $opusHash=(Get-FileHash -LiteralPath $opusDisk -Algorithm SHA256).Hash
 $null=Run-OpusCheck @($opusGame,'-game','-DWOProfileTest','-DWOSmoke=2') 'profile-final-reload'
 if((Get-FileHash -LiteralPath $opusDisk -Algorithm SHA256).Hash-ne $opusHash){throw 'Disk save/reload changed the inventory'}
}
Write-Host "Opus verification passed. Logs: $opusLogRoot"
