param([ValidateSet('Native','Actors')][string]$Phase='Native')
$ErrorActionPreference='Stop'
$seekRoot=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$seekProof=Join-Path $seekRoot 'Saved/Verification/SeekRange'
New-Item -ItemType Directory -Force -Path $seekProof | Out-Null
$seekProfile=Join-Path $seekRoot 'Saved/RuleTower/Profile.v2.json'
$seekBefore=if(Test-Path -LiteralPath $seekProfile){(Get-FileHash -LiteralPath $seekProfile).Hash}else{''}
$seekLog=Join-Path $seekProof ($Phase.ToLower()+'.log')
$seekArgs=@(('"'+$seekRoot+'/DungeonWorldOpus.uproject"'),'/Engine/Maps/Entry','-NullRHI','-Multiprocess','-unattended','-nosplash','-nosound','-NoLoadingScreen','-DisablePlugins=UE_MCP_Bridge','-as-exit-on-error',('-ABSLOG="'+$seekLog+'"'))
if($Phase-eq'Native'){
 $seekArgs+=@('-ExecCmds="Automation RunTests DWO.AI.SeekRangeTraits"','-TestExit="Automation Test Queue Empty"',('-ReportExportPath="'+$seekProof+'/native-report"'))
}else{
 $seekArgs[1]='/Engine/Maps/Entry?game=/Script/Angelscript.DWOTowerGameMode'
 $seekArgs+=@('-game','-DWOTransient','-DWOHeroArena','-DWOAggroTest','-DWOGod','-RenderOffscreen')
}
$seekProcess=Start-Process -FilePath 'G:/UnrealEngine-Angelscript-5.8.3/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $seekArgs -WorkingDirectory $seekRoot -WindowStyle Hidden -PassThru
$seekDeadline=(Get-Date).AddMinutes(3)
while(!$seekProcess.HasExited-and(Get-Date)-lt$seekDeadline){Start-Sleep -Milliseconds 500;$seekProcess.Refresh()}
if(!$seekProcess.HasExited){$seekProcess.Kill();throw 'Own seek range test process timed out'}
$seekProcess.WaitForExit()
$seekText=Get-Content -LiteralPath $seekLog -Raw -Encoding utf8
$seekAfter=if(Test-Path -LiteralPath $seekProfile){(Get-FileHash -LiteralPath $seekProfile).Hash}else{''}
$seekPass=if($Phase-eq'Native'){$seekText-match'Test Completed\. Result=\{Success\}.*SeekRangeTraits'}else{$seekText-match'AGGRO_TEST_PASS Complete'}
$seekOk=$seekProcess.ExitCode-eq 0-and$seekPass-and$seekText-notmatch'AGGRO_TEST_FAIL|Result=\{Fail|Angelscript: Error:|Fatal error:'-and$seekBefore-eq$seekAfter
[ordered]@{ok=$seekOk;phase=$Phase;exitCode=$seekProcess.ExitCode;profileHashBefore=$seekBefore;profileHashAfter=$seekAfter;profileUnchanged=($seekBefore-eq$seekAfter);log=$seekLog} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $seekProof ($Phase.ToLower()+'-receipt.json')) -Encoding utf8
if(!$seekOk){throw "Seek range $Phase verification failed: $seekLog"}
"Seek range $Phase verification passed"
