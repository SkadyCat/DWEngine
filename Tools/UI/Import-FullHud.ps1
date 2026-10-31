param([string]$HudJob='wbp-b68d7041-2a8c-4150-a7ce-dc25c286c909',[string]$EffectsJob='wbp-44c19b09-37d1-47a4-a45b-d35816a4ab51',[switch]$Verify)
$ErrorActionPreference='Stop'
$hudRoot=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
if(Get-CimInstance Win32_Process -Filter "Name='UnrealEditor.exe'" | Where-Object {$_.CommandLine -match 'DungeonWorldOpus.uproject' -and $_.CommandLine -notmatch '(?:^|\s)-game(?:\s|$)'}){throw 'Close the Opus editor and PIE before importing generated assets'}
$hudProof=Join-Path $hudRoot 'Saved/Verification/FullHUD';New-Item -ItemType Directory -Force $hudProof | Out-Null
foreach($hudPair in @(@($EffectsJob,'UICreatorEffects'),@($HudJob,'UICreator'))){
 $hudJobId=$hudPair[0];$hudDirectory=$hudPair[1]
 if($hudJobId -notmatch '^wbp-[a-zA-Z0-9_-]{1,90}$'){throw 'Invalid job'}
 $hudBundle=Join-Path $hudRoot "CustomTools/UICreatorAlpha/exports/$hudJobId/bundle.json"
 if(!(Test-Path -LiteralPath $hudBundle)){throw "Missing frozen bundle $hudJobId"}
 $hudTask=if($Verify){'SvgCanvasVerify'}else{'SvgCanvasImport'}
 $hudOutput=if($Verify){Join-Path $hudProof "$hudDirectory-native"}else{Join-Path $hudProof "$hudDirectory-import.json"}
 $hudLog=Join-Path $hudProof "$hudDirectory-$hudTask.log"
 if(!$Verify){
  $hudBackup=Join-Path $hudProof "before-$hudJobId/$hudDirectory";New-Item -ItemType Directory -Force $hudBackup | Out-Null
  foreach($hudFile in @('WBP_CurrentCanvas.uasset','DA_CurrentCanvas.uasset','contract.json','source.uialpha.json')){
   $hudExisting=Join-Path $hudRoot "Content/$hudDirectory/$hudFile"
   if((Test-Path -LiteralPath $hudExisting)-and !(Test-Path -LiteralPath (Join-Path $hudBackup $hudFile))){Copy-Item -LiteralPath $hudExisting -Destination $hudBackup}
  }
 }
 $hudArgs=@(('"'+(Join-Path $hudRoot 'DungeonWorldOpus.uproject')+'"'),('-run='+$hudTask),('-Bundle="'+$hudBundle+'"'),('-Output="'+$hudOutput+'"'),('-SvgCanvasRoot=/Game/'+$hudDirectory),'-Multiprocess','-unattended','-nosplash','-nosound','-as-exit-on-error',('-ABSLOG="'+$hudLog+'"'))
 $hudArgs+='-DisablePlugins=UE_MCP_Bridge'
 if($Verify){$hudArgs+=@('-AllowCommandletRendering','-RenderOffscreen')}else{$hudArgs+='-NullRHI'}
 $hudProcess=Start-Process -FilePath 'G:/UnrealEngine-Angelscript-5.8.3/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $hudArgs -WorkingDirectory $hudRoot -WindowStyle Hidden -PassThru
 $hudDeadline=(Get-Date).AddMinutes(8)
 while(!$hudProcess.HasExited -and (Get-Date)-lt $hudDeadline){Start-Sleep -Milliseconds 500;$hudProcess.Refresh()}
 if(!$hudProcess.HasExited){Stop-Process -Id $hudProcess.Id -Force;throw "Timed out $hudLog"}
 $hudProcess.WaitForExit();$hudText=Get-Content -LiteralPath $hudLog -Raw -Encoding UTF8
 if($hudProcess.ExitCode-ne 0 -or $hudText-match 'Angelscript: Error:|Fatal error:'){throw "Failed $hudLog"}
 Write-Output "$hudDirectory $hudTask passed"
 if(!$Verify -and $hudDirectory-eq 'UICreatorEffects'){& node (Join-Path $PSScriptRoot 'Stage-PlayerEffects.mjs') $EffectsJob;if($LASTEXITCODE-ne 0){throw 'Status controls staging failed'}}
}
