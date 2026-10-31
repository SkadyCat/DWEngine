param([Parameter(Mandatory=$true)][string]$Job,[switch]$Verify)
$ErrorActionPreference='Stop'
$combatRoot=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
if($Job -notmatch '^wbp-[a-zA-Z0-9_-]{1,90}$'){throw 'Invalid export job'}
$combatJob=Join-Path $combatRoot "CustomTools/UICreatorAlpha/exports/$Job"
$combatBundle=Join-Path $combatJob 'bundle.json'
if(!(Test-Path -LiteralPath $combatBundle)){throw 'Frozen UICreator bundle is missing'}
if(Get-CimInstance Win32_Process -Filter "Name='UnrealEditor.exe'" | Where-Object {$_.CommandLine -match 'DungeonWorldOpus.uproject' -and $_.CommandLine -notmatch '(?:^|\s)-game(?:\s|$)'}){throw 'Close the main editor and PIE before importing its generated UI assets'}
$combatProof=Join-Path $combatRoot 'Saved/Verification/CombatLayout'
New-Item -ItemType Directory -Path $combatProof -Force | Out-Null
$combatTask=if($Verify){'SvgCanvasVerify'}else{'SvgCanvasImport'}
$combatOutput=if($Verify){Join-Path $combatProof 'native'}else{Join-Path $combatProof 'import-receipt.json'}
$combatLog=Join-Path $combatProof ($combatTask+'.log')
if(!$Verify){
 $combatBackup=Join-Path $combatProof ('before-'+$Job)
 New-Item -ItemType Directory -Path $combatBackup -Force | Out-Null
 foreach($combatAsset in @('WBP_CurrentCanvas.uasset','DA_CurrentCanvas.uasset')){
  if(!(Test-Path (Join-Path $combatBackup $combatAsset))){Copy-Item -LiteralPath (Join-Path $combatRoot ('Content/UICreator/'+$combatAsset)) -Destination $combatBackup}
 }
}
$combatArgs=@(('"'+(Join-Path $combatRoot 'DungeonWorldOpus.uproject')+'"'),('-run='+$combatTask),('-Bundle="'+$combatBundle+'"'),('-Output="'+$combatOutput+'"'),'-SvgCanvasRoot=/Game/UICreator','-Multiprocess','-unattended','-nosplash','-nosound','-as-exit-on-error',('-ABSLOG="'+$combatLog+'"'))
if($Verify){$combatArgs+=@('-AllowCommandletRendering','-RenderOffscreen')}else{$combatArgs+='-NullRHI'}
$combatProcess=Start-Process -FilePath 'G:/UnrealEngine-Angelscript-5.8.3/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $combatArgs -WorkingDirectory $combatRoot -WindowStyle Hidden -PassThru
$combatDeadline=(Get-Date).AddMinutes(6)
while(!$combatProcess.HasExited -and (Get-Date)-lt $combatDeadline){Start-Sleep -Milliseconds 500;$combatProcess.Refresh()}
if(!$combatProcess.HasExited){Stop-Process -Id $combatProcess.Id -Force;throw "Combat HUD commandlet timed out: $combatLog"}
$combatProcess.WaitForExit();$combatText=Get-Content -LiteralPath $combatLog -Raw -Encoding UTF8
if($combatProcess.ExitCode-ne 0 -or $combatText-match 'Angelscript: Error:|Fatal error:'){throw "Combat HUD commandlet failed: $combatLog"}
Write-Output "$combatTask passed: $combatOutput"
