param([switch]$Visual)
$ErrorActionPreference='Stop'
$talentRoot=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$talentProof=Join-Path $talentRoot 'Saved/Verification/Talents'
New-Item -ItemType Directory -Path $talentProof -Force | Out-Null
$talentLog=Join-Path $talentProof 'native-final.log'
$talentArgs=@(('"'+(Join-Path $talentRoot 'DungeonWorldOpus.uproject')+'"'),'/Game/ClaudeDungeon/Maps/L_OpusDungeon?game=/Script/Angelscript.DWOTowerGameMode','-game','-DWOTransient','-DWOTalentTest','-DWOTalentTravelTest','-Multiprocess','-as-exit-on-error','-unattended','-nosplash','-nosound','-DisablePlugins=UE_MCP_Bridge',('-ABSLOG="'+$talentLog+'"'))
if($Visual){$talentArgs+=@('-DWOTalentVisual','-windowed','-ResX=1600','-ResY=900','-ForceRes','-RenderOffscreen')}else{$talentArgs+='-NullRHI'}
$talentProcess=Start-Process -FilePath 'G:/UnrealEngine-Angelscript-5.8.3/Engine/Binaries/Win64/UnrealEditor.exe' -ArgumentList $talentArgs -WorkingDirectory $talentRoot -WindowStyle Hidden -PassThru
if(!$talentProcess.WaitForExit(240000)){Stop-Process -Id $talentProcess.Id;throw "Talent test timed out: $talentLog"}
$talentText=Get-Content -LiteralPath $talentLog -Raw -Encoding UTF8
if($talentProcess.ExitCode-ne 0 -or $talentText-match 'TALENT_TEST_FAIL|Angelscript: Error:|Fatal error:' -or $talentText-notmatch 'TALENT_TEST_PASS TravelComplete'){throw "Talent native verification failed: $talentLog"}
$talentChecks=([regex]::Matches($talentText,'TALENT_TEST_PASS ')).Count
Write-Output "Passed $talentChecks native checks, including a level reload. Known rule gaps remain explicit in the log."
