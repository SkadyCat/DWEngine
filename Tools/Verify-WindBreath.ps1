param([switch]$Visuals,[switch]$Units)
$ErrorActionPreference='Stop'
$windRoot=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$windProof=Join-Path $windRoot 'Saved/Verification/WindBreath'
New-Item -ItemType Directory -Path $windProof -Force | Out-Null
$windName=if($Units){'native-unit'}elseif($Visuals){'runtime-visual'}else{'runtime'}
$windLog=Join-Path $windProof ($windName+'.log')
if(Test-Path -LiteralPath $windLog){Copy-Item -LiteralPath $windLog -Destination (Join-Path $windProof ($windName+'-before-'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'.log'))}
$windProfile=Join-Path $windRoot 'Saved/RuleTower/Profile.v2.json'
$windHash=if(Test-Path -LiteralPath $windProfile){(Get-FileHash -LiteralPath $windProfile -Algorithm SHA256).Hash}else{''}
$windOwners=@(Get-CimInstance Win32_Process -Filter "Name = 'UnrealEditor.exe' OR Name = 'UnrealEditor-Cmd.exe'" | Where-Object {$_.CommandLine -match 'DungeonWorldOpus' -and $_.CommandLine -notmatch 'DWOTransient'} | Select-Object ProcessId,CreationDate)
$windArgs=@(('"'+$windRoot+'/DungeonWorldOpus.uproject"'),'-DWOTransient','-Multiprocess','-unattended','-nosplash','-nosound','-NoLoadingScreen','-as-exit-on-error','-DisablePlugins=UE_MCP_Bridge',('-ABSLOG="'+$windLog+'"'))
if($Units){
 $windArgs+=@('-NullRHI','-ExecCmds="Automation RunTests DungeonWorldOpus.WindBreath"','-TestExit="Automation Test Queue Empty"',('-ReportExportPath="'+$windProof+'/automation"'))
}else{
 $windArgs+=@('/Engine/Maps/Entry?game=/Script/Angelscript.DWOTowerGameMode','-game','-DWOHeroArena','-DWOWindBreathTest','-DWORecentPreview')
 if($Visuals){$windArgs+=@('-DWOWindBreathVisual','-windowed','-RenderOffscreen','-ResX=1600','-ResY=900','-ForceRes','-SvgCanvasNoMotion','-asyncassetcompilationmaxconcurrency=2','-asynctexturecompilationmaxconcurrency=1','-ExecCmds="r.SetRes 1600x900w,t.MaxFPS 60,t.IdleWhenNotForeground 0,Slate.bAllowThrottling 0,r.MotionBlurQuality 0"')}else{$windArgs+='-NullRHI'}
}
$windProcess=Start-Process -FilePath 'G:/UnrealEngine-Angelscript-5.8.3/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $windArgs -WorkingDirectory $windRoot -WindowStyle Hidden -PassThru
$windDeadline=(Get-Date).AddMinutes(3)
while(!$windProcess.HasExited -and (Get-Date)-lt $windDeadline){Start-Sleep -Milliseconds 500;$windProcess.Refresh()}
if(!$windProcess.HasExited){Stop-Process -Id $windProcess.Id -Force;throw "Own wind verification process timed out: $windLog"}
$windProcess.WaitForExit();$windText=Get-Content -LiteralPath $windLog -Raw -Encoding UTF8
if($windProcess.ExitCode-ne 0 -or $windText-match 'Angelscript: Error:|WIND_BREATH_FAIL|Fatal error:|TowerError|Result=\{Fail\}') {throw "Wind verification failed: $windLog"}
if($Units){if(([regex]::Matches($windText,'Result=\{Success\} Name=\{')).Count-ne 2){throw "Native wind unit tests did not complete: $windLog"}}
elseif(([regex]::Matches($windText,'WIND_BREATH_PASS')).Count-ne 3){throw "Wind runtime groups did not complete: $windLog"}
$windAfter=if(Test-Path -LiteralPath $windProfile){(Get-FileHash -LiteralPath $windProfile -Algorithm SHA256).Hash}else{''}
$windOwnersAfter=@(Get-CimInstance Win32_Process -Filter "Name = 'UnrealEditor.exe' OR Name = 'UnrealEditor-Cmd.exe'" | Where-Object {$_.CommandLine -match 'DungeonWorldOpus' -and $_.CommandLine -notmatch 'DWOTransient'} | Select-Object ProcessId,CreationDate)
@{test=$windName;processExit=$windProcess.ExitCode;transient=$true;profileBefore=$windHash;profileAfter=$windAfter;profileUnchanged=($windHash-eq $windAfter);regularOwnersBefore=$windOwners;regularOwnersAfter=$windOwnersAfter;profileRestored=$false} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $windProof ($windName+'-receipt.json')) -Encoding utf8
if($windHash-ne $windAfter){if($windOwners.Count-or $windOwnersAfter.Count){Write-Output 'Formal profile changed while a regular game was active; current profile preserved without restoration.'}else{throw 'Formal profile changed without a known regular owner; preserve current profile and inspect ownership'}}
Write-Output "Wind verification passed: $windLog"
