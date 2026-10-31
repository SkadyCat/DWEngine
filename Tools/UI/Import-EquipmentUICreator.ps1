param([Parameter(Mandatory=$true)][string]$Job,[switch]$Verify)
$ErrorActionPreference='Stop'
$equipmentRoot=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
if($Job -notmatch '^wbp-[a-zA-Z0-9_-]{1,90}$'){throw 'Invalid export job'}
$equipmentJob=Join-Path $equipmentRoot "CustomTools/UICreatorAlpha/exports/$Job"
$equipmentBundle=Join-Path $equipmentJob 'bundle.json'
if(!(Test-Path -LiteralPath $equipmentBundle)){throw 'Frozen UICreator bundle is missing'}
$equipmentLab=Join-Path $equipmentRoot 'CustomTools/UICreatorAlpha/Unreal/SvgWbpLab/SvgWbpLab.uproject'
if(Get-CimInstance Win32_Process -Filter "Name='UnrealEditor.exe'" | Where-Object {$_.CommandLine -match 'SvgWbpLab.uproject'}){throw 'Close SvgWbpLab before import'}
$equipmentTask=if($Verify){'SvgCanvasVerify'}else{'SvgCanvasImport'}
$equipmentOutput=if($Verify){Join-Path $equipmentJob 'native'}else{Join-Path $equipmentJob 'import-receipt.json'}
$equipmentLog=Join-Path $equipmentJob ($equipmentTask+'.log')
$equipmentArgs=@(('"'+$equipmentLab+'"'),('-run='+$equipmentTask),('-Bundle="'+$equipmentBundle+'"'),('-Output="'+$equipmentOutput+'"'),'-Multiprocess','-unattended','-nosplash','-nosound','-UTF8Output',('-ABSLOG="'+$equipmentLog+'"'))
if($Verify){$equipmentArgs+=@('-AllowCommandletRendering','-RenderOffscreen')}else{$equipmentArgs+='-NullRHI'}
$equipmentProcess=Start-Process -FilePath 'G:/UnrealEngine-Angelscript-5.8.3/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $equipmentArgs -WorkingDirectory $equipmentRoot -WindowStyle Hidden -PassThru
$equipmentDeadline=(Get-Date).AddMinutes(8)
while(!$equipmentProcess.HasExited -and (Get-Date)-lt $equipmentDeadline){Start-Sleep -Milliseconds 500;$equipmentProcess.Refresh()}
if(!$equipmentProcess.HasExited){Stop-Process -Id $equipmentProcess.Id -Force;throw "Commandlet timed out: $equipmentLog"}
$equipmentProcess.WaitForExit()
if($equipmentProcess.ExitCode-ne 0){throw "Commandlet failed: $equipmentLog"}
Write-Output "$equipmentTask passed: $equipmentOutput"
