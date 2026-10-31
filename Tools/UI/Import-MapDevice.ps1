param([string]$Job='wbp-28c80d7b-24ea-4f6b-8706-806da72f133c',[switch]$Verify)
$ErrorActionPreference='Stop'
$mapRoot=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
if(Get-CimInstance Win32_Process -Filter "Name='UnrealEditor.exe'" | Where-Object {$_.CommandLine -match '[\\/]DungeonWorldOpus.uproject' -and $_.CommandLine -notmatch '(?:^|\s)-game(?:\s|$)'}){throw 'Close the Opus editor and PIE before importing generated assets'}
if($Job -notmatch '^wbp-[a-zA-Z0-9_-]{1,90}$'){throw 'Invalid frozen job'}
$mapBundle=Join-Path $mapRoot "CustomTools/UICreatorAlpha/exports/$Job/bundle.json"
if(!(Test-Path -LiteralPath $mapBundle)){throw 'Missing frozen map bundle'}
$mapProof=Join-Path $mapRoot 'Saved/Verification/MapDevice';New-Item -ItemType Directory -Force $mapProof | Out-Null
$mapDestination=Join-Path $mapRoot 'Content/UICreatorMaps'
if(!$Verify -and (Test-Path -LiteralPath $mapDestination)){
 $mapBackup=Join-Path $mapProof ('before-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
 Copy-Item -LiteralPath $mapDestination -Destination $mapBackup -Recurse
}
$mapTask=if($Verify){'SvgCanvasVerify'}else{'SvgCanvasImport'}
$mapOutput=Join-Path $mapProof $(if($Verify){'native'}else{'import.json'})
$mapLog=Join-Path $mapProof ($mapTask+'.log')
$mapArgs=@(('"'+(Join-Path $mapRoot 'DungeonWorldOpus.uproject')+'"'),('-run='+$mapTask),('-Bundle="'+$mapBundle+'"'),('-Output="'+$mapOutput+'"'),'-SvgCanvasRoot=/Game/UICreatorMaps','-Multiprocess','-unattended','-nosplash','-nosound','-as-exit-on-error','-DisablePlugins=UE_MCP_Bridge',('-ABSLOG="'+$mapLog+'"'))
if($Verify){$mapArgs+=@('-AllowCommandletRendering','-RenderOffscreen')}else{$mapArgs+='-NullRHI'}
$mapProcess=Start-Process -FilePath 'G:/UnrealEngine-Angelscript-5.8.3/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $mapArgs -WorkingDirectory $mapRoot -WindowStyle Hidden -PassThru
$mapDeadline=(Get-Date).AddMinutes(8)
while(!$mapProcess.HasExited -and (Get-Date)-lt $mapDeadline){Start-Sleep -Milliseconds 500;$mapProcess.Refresh()}
if(!$mapProcess.HasExited){Stop-Process -Id $mapProcess.Id -Force;throw "Map import timed out: $mapLog"}
$mapProcess.WaitForExit();$mapText=Get-Content -LiteralPath $mapLog -Raw -Encoding UTF8
if($mapProcess.ExitCode-ne 0 -or $mapText-match 'Angelscript: Error:|Fatal error:'){throw "Map import failed: $mapLog"}
if(!$Verify){& node (Join-Path $PSScriptRoot 'Stage-MapDevice.mjs') $Job;if($LASTEXITCODE-ne 0){throw 'Map staging failed'}}
Write-Output "$mapTask passed: $Job"
