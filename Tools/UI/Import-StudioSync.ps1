param([string]$Job='wbp-708f07d1-192f-42a4-ba4a-a837e89b46d3',[switch]$Verify,[switch]$NewDirectoryOnly)
$ErrorActionPreference='Stop'
$syncRoot=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
if($Job -notmatch '^wbp-[a-zA-Z0-9_-]{1,90}$'){throw 'Invalid frozen job'}
$syncEditor=@(Get-CimInstance Win32_Process -Filter "Name='UnrealEditor.exe'" | Where-Object {$_.CommandLine -match 'DungeonWorldOpus.uproject' -and $_.CommandLine -notmatch '(?:^|\s)-game(?:\s|$)'})
if($syncEditor.Count -and !$Verify){
 if(!$NewDirectoryOnly){throw 'Save and close the Opus editor before replacing generated UI'}
 if(Get-ChildItem -LiteralPath (Join-Path $syncRoot 'Content/UICreatorStudio') -Filter '*.uasset' -Recurse -ErrorAction SilentlyContinue){throw 'NewDirectoryOnly cannot replace existing UE packages'}
 $syncEditorLog=Get-Content -LiteralPath (Join-Path $syncRoot 'Saved/Logs/DungeonWorldOpus.log') -Raw -Encoding utf8
 if($syncEditorLog -match 'LogPlayLevel:|PIE:'){throw 'NewDirectoryOnly requires an editor session with no PIE activity'}
}
$syncProof=Join-Path $syncRoot 'Saved/Verification/StudioSync';New-Item -ItemType Directory -Force $syncProof | Out-Null
$syncTask=if($Verify){'SvgCanvasVerify'}else{'SvgCanvasImport'}
$syncOutput=if($Verify){Join-Path $syncProof 'native'}else{Join-Path $syncProof 'import.json'}
$syncLog=Join-Path $syncProof ($syncTask+'.log')
if(!$Verify){
 $syncBackup=Join-Path $syncProof ('before-'+$Job);New-Item -ItemType Directory -Force $syncBackup | Out-Null
 foreach($syncFile in @('WBP_CurrentCanvas.uasset','DA_CurrentCanvas.uasset','contract.json','source.uialpha.json')){
  $syncOld=Join-Path $syncRoot ('Content/UICreatorStudio/'+$syncFile)
  if((Test-Path -LiteralPath $syncOld)-and !(Test-Path -LiteralPath (Join-Path $syncBackup $syncFile))){Copy-Item -LiteralPath $syncOld -Destination $syncBackup}
 }
}
$syncArgs=@(('"'+$syncRoot+'/DungeonWorldOpus.uproject"'),('-run='+$syncTask),('-Bundle="'+$syncRoot+'/CustomTools/UICreatorAlpha/exports/'+$Job+'/bundle.json"'),('-Output="'+$syncOutput+'"'),'-SvgCanvasRoot=/Game/UICreatorStudio','-Multiprocess','-unattended','-nosplash','-nosound','-as-exit-on-error','-DisablePlugins=UE_MCP_Bridge',('-ABSLOG="'+$syncLog+'"'))
if($Verify){$syncArgs+=@('-AllowCommandletRendering','-RenderOffscreen')}else{$syncArgs+='-NullRHI'}
$syncProcess=Start-Process -FilePath 'G:/UnrealEngine-Angelscript-5.8.3/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $syncArgs -WorkingDirectory $syncRoot -WindowStyle Hidden -PassThru
$syncDeadline=(Get-Date).AddMinutes(6)
while(!$syncProcess.HasExited -and (Get-Date)-lt$syncDeadline){Start-Sleep -Milliseconds 500;$syncProcess.Refresh()}
if(!$syncProcess.HasExited){Stop-Process -Id $syncProcess.Id -Force;throw "Own UI commandlet timed out: $syncLog"}
$syncProcess.WaitForExit();$syncText=Get-Content -LiteralPath $syncLog -Raw -Encoding utf8
if($syncProcess.ExitCode-ne 0 -or $syncText-match 'Angelscript: Error:|Fatal error:'){throw "UI commandlet failed: $syncLog"}
if(!$Verify){& node (Join-Path $PSScriptRoot 'Stage-StudioSync.mjs') $Job;if($LASTEXITCODE-ne 0){throw 'Source staging failed'}}
Write-Output "$syncTask passed: $syncOutput"
