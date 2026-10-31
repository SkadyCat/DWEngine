param([Parameter(Mandatory)][ValidateSet('NativeTests','RecentHost','ModernCombat','Gallery','Talents','Taunt','PlayerEffects','VFXImport','VFXCold')][string]$Stage)
$ErrorActionPreference='Stop'
$stageRoot=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$stageProof=Join-Path $stageRoot 'Saved/Verification/UEIntegration'
$stageLog=Join-Path $stageProof ($Stage+'.log')
$stageArgs=@(('"'+(Join-Path $stageRoot 'DungeonWorldOpus.uproject')+'"'),'-Multiprocess','-unattended','-nosplash','-nosound','-as-exit-on-error','-DisablePlugins=UE_MCP_Bridge',('-ABSLOG="'+$stageLog+'"'))
switch($Stage){
 'NativeTests' {$stageArgs+=@('-NullRHI','-ExecCmds="Automation RunTests DungeonWorldOpus.Recent"','-TestExit="Automation Test Queue Empty"',('-ReportExportPath="'+(Join-Path $stageProof 'NativeAutomation')+'"'))}
 'RecentHost' {$stageArgs+=@('/Engine/Maps/Entry?game=/Script/Angelscript.DWOTowerGameMode','-game','-DWOTransient','-DWOHeroArena','-DWORecentTest','-NoLoadingScreen','-RenderOffscreen','-windowed','-ResX=1600','-ResY=900','-ForceRes')}
 'ModernCombat' {$stageArgs+=@('/Engine/Maps/Entry?game=/Script/Angelscript.DWOTowerGameMode','-game','-DWOTransient','-DWOHeroArena','-DWOModernTest','-DWORecentPreview','-NoLoadingScreen','-RenderOffscreen','-windowed','-ResX=1600','-ResY=900','-ForceRes','-ExecCmds="t.MaxFPS 60,t.IdleWhenNotForeground 0,Slate.bAllowThrottling 0"')}
 'Gallery' {$stageArgs+=@('/Engine/Maps/Entry?game=/Script/Angelscript.DWORecentVFXGalleryMode','-game','-DWOTransient','-NoLoadingScreen','-RenderOffscreen','-windowed','-ResX=1600','-ResY=900','-ForceRes')}
 'Talents' {$stageArgs+=@('/Game/ClaudeDungeon/Maps/L_OpusDungeon?game=/Script/Angelscript.DWOTowerGameMode','-game','-DWOTransient','-DWOTalentTest','-DWOTalentTravelTest','-NullRHI')}
 'Taunt' {$stageArgs+=@('/Game/ClaudeDungeon/Maps/L_OpusDungeon?game=/Script/Angelscript.DWOTowerGameMode','-game','-DWOTransient','-DWOTauntTest','-NullRHI')}
 'PlayerEffects' {$stageArgs+=@('/Game/ClaudeDungeon/Maps/L_OpusDungeon?game=/Script/Angelscript.DWOTowerGameMode','-game','-DWOTransient','-DWOStunTest','-NullRHI')}
 'VFXImport' {$stageArgs+=@('-run=pythonscript',('-script="'+(Join-Path $stageRoot 'Tools/WebVFX/Rebuild.py')+'"'),'-AllowCommandletRendering','-RenderOffscreen')}
 'VFXCold' {$stageArgs+=@('-run=pythonscript',('-script="'+(Join-Path $stageRoot 'Tools/WebVFX/Verify.py')+'"'),'-AllowCommandletRendering','-RenderOffscreen')}
}
$stageProcess=Start-Process -FilePath 'G:/UnrealEngine-Angelscript-5.8.3/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $stageArgs -WorkingDirectory $stageRoot -WindowStyle Hidden -PassThru
$stageDeadline=(Get-Date).AddMinutes($(if($Stage-eq 'VFXImport'){12}else{4}))
while(!$stageProcess.HasExited -and (Get-Date)-lt $stageDeadline){Start-Sleep -Milliseconds 500;$stageProcess.Refresh()}
if(!$stageProcess.HasExited){Stop-Process -Id $stageProcess.Id -Force;throw "Own verification process timed out: $stageLog"}
$stageProcess.WaitForExit();$stageText=Get-Content -LiteralPath $stageLog -Raw -Encoding utf8
if($stageProcess.ExitCode-ne 0 -or $stageText-match 'Angelscript: Error:|Fatal error:|RECENT_TEST_FAIL|MODERN_TEST_FAIL|GALLERY_TEST_FAIL|TALENT_TEST_FAIL|TAUNT_TEST_FAIL|STUN_TEST_FAIL|LogPython: Error:|Test Completed. Result=\{Fail'){throw "Stage failed: $stageLog"}
$stageRequired=switch($Stage){'NativeTests' {'Test Completed. Result=\{Success\}.*TreasuryFinisher'} 'RecentHost' {'RECENT_TEST_PASS'} 'ModernCombat' {'MODERN_TEST_PASS Complete'} 'Gallery' {'GALLERY_TEST_PASS'} 'Talents' {'TALENT_TEST_PASS TravelComplete'} 'Taunt' {'TAUNT_TEST_PASS IndependentCooldownSwitchPauseAndRecast'} 'PlayerEffects' {'STUN_TEST_PASS'} 'VFXImport' {'OPUS_VFX_REBUILT'} 'VFXCold' {'OPUS_VFX_COLD_PASS'}}
if($stageText-notmatch $stageRequired){throw "Stage did not produce its success receipt: $stageLog"}
Write-Output "$Stage passed: $stageLog"
