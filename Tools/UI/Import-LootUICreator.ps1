param([string]$Job='wbp-6f2cace1-eec6-430f-b1f8-fa5fd0448236',[switch]$Verify)
$ErrorActionPreference='Stop'
$lootRoot=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
if($Job -notmatch '^wbp-[a-zA-Z0-9_-]{1,90}$'){throw 'Invalid export job'}
$lootBundle=Join-Path $lootRoot "CustomTools/UICreatorAlpha/exports/$Job/bundle.json"
if(!(Test-Path -LiteralPath $lootBundle)){throw 'Frozen UICreator bundle is missing'}
$lootProof=Join-Path $lootRoot 'Saved/Verification/LootUICreator'
New-Item -ItemType Directory -Path $lootProof -Force | Out-Null
$lootTask=if($Verify){'SvgCanvasVerify'}else{'SvgCanvasImport'}
$lootOutput=if($Verify){Join-Path $lootProof 'native'}else{Join-Path $lootProof 'import-receipt.json'}
$lootLog=Join-Path $lootProof ($lootTask+'.log')
$lootArgs=@(('"'+(Join-Path $lootRoot 'DungeonWorldOpus.uproject')+'"'),('-run='+$lootTask),('-Bundle="'+$lootBundle+'"'),('-Output="'+$lootOutput+'"'),'-SvgCanvasRoot=/Game/UICreatorLoot','-Multiprocess','-unattended','-nosplash','-nosound','-as-exit-on-error',('-ABSLOG="'+$lootLog+'"'))
if($Verify){$lootArgs+=@('-AllowCommandletRendering','-RenderOffscreen')}else{$lootArgs+='-NullRHI'}
$lootProcess=Start-Process -FilePath 'G:\UnrealEngine-Angelscript-5.8.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' -ArgumentList $lootArgs -WorkingDirectory $lootRoot -WindowStyle Hidden -PassThru
$lootDeadline=(Get-Date).AddMinutes(5)
while(!$lootProcess.HasExited -and (Get-Date)-lt $lootDeadline){Start-Sleep -Milliseconds 500;$lootProcess.Refresh()}
if(!$lootProcess.HasExited){Stop-Process -Id $lootProcess.Id -Force;throw "Scoped loot commandlet timed out: $lootLog"}
$lootProcess.WaitForExit();$lootText=Get-Content -LiteralPath $lootLog -Raw -Encoding UTF8
if($lootProcess.ExitCode-ne 0 -or $lootText-match 'Angelscript: Error:|Fatal error:'){throw "Loot commandlet failed: $lootLog"}
Write-Host "$lootTask passed: $lootOutput"
