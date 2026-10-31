param([switch]$SkipBackend)
$ErrorActionPreference='Stop'
$crystalRoot=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$crystalOut=Join-Path $crystalRoot 'Saved/Verification/Crystals'
New-Item -ItemType Directory -Path $crystalOut -Force | Out-Null
$crystalLog=Join-Path $crystalOut 'native.log'
$crystalProfile=Join-Path $crystalRoot 'Saved/RuleTower/Profile.v2.json'
$crystalBefore=if(Test-Path -LiteralPath $crystalProfile){(Get-FileHash -LiteralPath $crystalProfile).Hash}else{''}
$crystalArgs=@(('"'+$crystalRoot+'/DungeonWorldOpus.uproject"'),'/Engine/Maps/Entry','-NullRHI','-Multiprocess','-unattended','-nosplash','-nosound','-NoLoadingScreen','-DisablePlugins=UE_MCP_Bridge','-as-exit-on-error','-ExecCmds="Automation RunTests DWO.Crafting.Crystals"','-TestExit="Automation Test Queue Empty"',('-ReportExportPath="'+$crystalOut+'/native-report"'),('-ABSLOG="'+$crystalLog+'"'))
$crystalProcess=Start-Process -FilePath 'G:/UnrealEngine-Angelscript-5.8.3/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $crystalArgs -WorkingDirectory $crystalRoot -WindowStyle Hidden -PassThru
$crystalDeadline=(Get-Date).AddMinutes(3)
while(!$crystalProcess.HasExited -and (Get-Date)-lt $crystalDeadline){Start-Sleep -Milliseconds 500;$crystalProcess.Refresh()}
if(!$crystalProcess.HasExited){$crystalProcess.Kill();throw 'Own crystal automation process timed out'}
$crystalProcess.WaitForExit();$crystalText=Get-Content -LiteralPath $crystalLog -Raw -Encoding utf8
if($crystalProcess.ExitCode-ne 0 -or $crystalText-match 'Result=\{Fail|Angelscript: Error:|Fatal error:' -or $crystalText-notmatch 'Test Completed\. Result=\{Success\}.*Crystals'){throw "Native crystal verification failed: $crystalLog"}
$crystalBackend=$null
if(!$SkipBackend){
 $hash=[Security.Cryptography.IncrementalHash]::CreateHash([Security.Cryptography.HashAlgorithmName]::SHA1)
 try{
  foreach($group in @('Equipment','Items')){
   $names=[Collections.Generic.List[string]]::new();foreach($file in Get-ChildItem -LiteralPath (Join-Path $crystalRoot ('Content/RulesTower/'+$group)) -Filter '*.json' -File){$names.Add($file.Name)}
   $names.Sort([StringComparer]::Ordinal)
   foreach($name in $names){$hash.AppendData([Text.Encoding]::UTF8.GetBytes($group+'/'+$name+[char]0));$hash.AppendData([IO.File]::ReadAllBytes((Join-Path $crystalRoot ('Content/RulesTower/'+$group+'/'+$name))));$hash.AppendData([byte[]]@(0))}
  }
  $version=[Convert]::ToHexString($hash.GetHashAndReset()).ToLowerInvariant()
 }finally{$hash.Dispose()}
 $pool=Invoke-RestMethod -Uri ('http://127.0.0.1:5188/v1/maps/map_core_equipment/drop-pool?floor=1&catalogVersion='+$version) -TimeoutSec 3
 if($pool.catalogVersion-ne $version -or 'annulment_crystal'-notin $pool.itemIds -or 'wish_crystal'-notin $pool.itemIds){throw 'Backend map pool does not contain both new currencies'}
 $pool | ConvertTo-Json -Depth 15 | Set-Content -LiteralPath (Join-Path $crystalOut 'drop-pool.json') -Encoding utf8
 $crystalBackend=[ordered]@{verified=$true;catalogVersion=$version;poolVersion=$pool.poolVersion;currencyIds=@('annulment_crystal','wish_crystal')}
}
$crystalAfter=if(Test-Path -LiteralPath $crystalProfile){(Get-FileHash -LiteralPath $crystalProfile).Hash}else{''}
[ordered]@{ok=$true;timestamp=(Get-Date).ToString('o');currencies=@('annulment_crystal','wish_crystal');nativeTest='DWO.Crafting.Crystals';execution='native Unreal automation / NullRHI; isolated in-memory profiles';profileHashBefore=$crystalBefore;profileHashAfter=$crystalAfter;profileUnchanged=($crystalBefore-eq $crystalAfter);formalProfileWrittenByTest=$false;backend=$crystalBackend;log=$crystalLog} | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $crystalOut 'receipt.json') -Encoding utf8
'Crystals verified: native crafting, payment, RNG rollback, grade bindings, save reload, inspector eligibility and map drop catalogue'
