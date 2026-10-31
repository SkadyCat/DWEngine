param([string]$Job='wbp-2bcc4ef5-37cc-4fae-9aac-506d7245801a',[switch]$Verify)
$ErrorActionPreference='Stop'
$inspectorRoot=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
if($Job -notmatch '^wbp-[a-zA-Z0-9_-]{1,90}$'){throw 'Invalid export job'}
$inspectorBundle=Join-Path $inspectorRoot "CustomTools/UICreatorAlpha/exports/$Job/bundle.json"
if(!(Test-Path -LiteralPath $inspectorBundle)){throw 'Frozen UICreator bundle is missing'}
$inspectorProof=Join-Path $inspectorRoot 'Saved/Verification/ItemHover'
New-Item -ItemType Directory -Path $inspectorProof -Force | Out-Null
$inspectorTask=if($Verify){'SvgCanvasVerify'}else{'SvgCanvasImport'}
$inspectorOutput=if($Verify){Join-Path $inspectorProof 'native'}else{Join-Path $inspectorProof 'native-import-receipt.json'}
$inspectorLog=Join-Path $inspectorProof ($inspectorTask+'.log')
$inspectorArgs=@(('"'+(Join-Path $inspectorRoot 'DungeonWorldOpus.uproject')+'"'),('-run='+$inspectorTask),('-Bundle="'+$inspectorBundle+'"'),('-Output="'+$inspectorOutput+'"'),'-SvgCanvasRoot=/Game/UICreatorHover','-Multiprocess','-unattended','-nosplash','-nosound','-as-exit-on-error',('-ABSLOG="'+$inspectorLog+'"'))
if($Verify){$inspectorArgs+=@('-AllowCommandletRendering','-RenderOffscreen')}else{$inspectorArgs+='-NullRHI'}
$inspectorProcess=Start-Process -FilePath 'G:/UnrealEngine-Angelscript-5.8.3/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $inspectorArgs -WorkingDirectory $inspectorRoot -WindowStyle Hidden -PassThru
$inspectorDeadline=(Get-Date).AddMinutes(5)
while(!$inspectorProcess.HasExited -and (Get-Date)-lt $inspectorDeadline){Start-Sleep -Milliseconds 500;$inspectorProcess.Refresh()}
if(!$inspectorProcess.HasExited){Stop-Process -Id $inspectorProcess.Id -Force;throw "Inspector commandlet timed out: $inspectorLog"}
$inspectorProcess.WaitForExit();$inspectorText=Get-Content -LiteralPath $inspectorLog -Raw -Encoding UTF8
if($inspectorProcess.ExitCode-ne 0 -or $inspectorText-match 'Angelscript: Error:|Fatal error:'){throw "Inspector commandlet failed: $inspectorLog"}
Write-Output "$inspectorTask passed: $inspectorOutput"
