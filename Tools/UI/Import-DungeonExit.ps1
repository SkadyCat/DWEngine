param([switch]$Verify)
$ErrorActionPreference='Stop'
$portalRoot=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$portalProof=Join-Path $portalRoot 'Saved/Verification/DungeonExit'
$portalExport=Get-Content -LiteralPath (Join-Path $portalProof 'export.json') -Raw | ConvertFrom-Json
$portalTask=if($Verify){'SvgCanvasVerify'}else{'SvgCanvasImport'}
$portalOutput=if($Verify){Join-Path $portalProof 'native'}else{Join-Path $portalProof 'import-receipt.json'}
$portalLog=Join-Path $portalProof ($portalTask+'.log')
$portalArgs=@(('"'+$portalRoot+'/DungeonWorldOpus.uproject"'),('-run='+$portalTask),('-Bundle="'+$portalExport.bundle+'"'),('-Output="'+$portalOutput+'"'),'-SvgCanvasRoot=/Game/UICreatorPortal','-Multiprocess','-unattended','-nosplash','-nosound','-as-exit-on-error','-DisablePlugins=UE_MCP_Bridge',('-ABSLOG="'+$portalLog+'"'))
if($Verify){$portalArgs+=@('-AllowCommandletRendering','-RenderOffscreen')}else{$portalArgs+='-NullRHI'}
$portalProcess=Start-Process 'G:/UnrealEngine-Angelscript-5.8.3/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $portalArgs -WorkingDirectory $portalRoot -WindowStyle Hidden -PassThru
$portalDeadline=(Get-Date).AddMinutes(4)
while(!$portalProcess.HasExited -and (Get-Date)-lt $portalDeadline){Start-Sleep -Milliseconds 500;$portalProcess.Refresh()}
if(!$portalProcess.HasExited){Stop-Process -Id $portalProcess.Id;throw "Own portal commandlet timed out: $portalLog"}
$portalProcess.WaitForExit();$portalText=Get-Content -LiteralPath $portalLog -Raw
if($portalProcess.ExitCode-ne 0 -or $portalText-match 'Angelscript: Error:|Fatal error:'){throw "Portal $portalTask failed: $portalLog"}
Write-Output "$portalTask passed: $portalOutput"
