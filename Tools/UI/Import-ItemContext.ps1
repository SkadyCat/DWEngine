param([string]$Job='wbp-context-8605178c-a5a2-40ad-b2e6-6a3223d35e3d',[switch]$Verify)
$ErrorActionPreference='Stop'
$contextRoot=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$contextProof=Join-Path $contextRoot 'Saved/Verification/ItemContext'
New-Item -ItemType Directory -Force -Path $contextProof | Out-Null
$contextTask=if($Verify){'SvgCanvasVerify'}else{'SvgCanvasImport'}
$contextOutput=if($Verify){Join-Path $contextProof 'native'}else{Join-Path $contextProof 'import-receipt.json'}
$contextBundle=Join-Path $contextRoot "CustomTools/UICreatorAlpha/exports/$Job/bundle.json"
$contextArgs=@(('"'+$contextRoot+'/DungeonWorldOpus.uproject"'),('-run='+$contextTask),('-Bundle="'+$contextBundle+'"'),('-Output="'+$contextOutput+'"'),'-SvgCanvasRoot=/Game/UICreatorContext','-Multiprocess','-unattended','-nosplash','-nosound','-as-exit-on-error',('-ABSLOG="'+$contextProof+'/'+$contextTask+'.log"'))
if($Verify){$contextArgs+=@('-AllowCommandletRendering','-RenderOffscreen')}else{$contextArgs+='-NullRHI'}
$contextProcess=Start-Process 'G:/UnrealEngine-Angelscript-5.8.3/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $contextArgs -WorkingDirectory $contextRoot -WindowStyle Hidden -PassThru
$contextDeadline=(Get-Date).AddMinutes(5)
while(!$contextProcess.HasExited -and (Get-Date)-lt $contextDeadline){Start-Sleep -Milliseconds 500;$contextProcess.Refresh()}
if(!$contextProcess.HasExited){Stop-Process -Id $contextProcess.Id;throw 'Own menu import timed out'}
$contextProcess.WaitForExit();if($contextProcess.ExitCode-ne 0){throw "Menu $contextTask failed"}
Write-Output "$contextTask passed: $contextOutput"
