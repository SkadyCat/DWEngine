param([string]$Job='wbp-events-590cb159-7df5-42f3-821e-1e4b22a60245',[switch]$Verify,[string]$ProofDirectory='Saved/Verification/MapEventsUICreator')
$ErrorActionPreference='Stop'
$eventRoot=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
if($Job -notmatch '^wbp-[a-zA-Z0-9_-]{1,90}$'){throw 'Invalid export job'}
$eventBundle=Join-Path $eventRoot "CustomTools/UICreatorAlpha/exports/$Job/bundle.json"
if(!(Test-Path -LiteralPath $eventBundle)){throw 'Frozen UICreator bundle is missing'}
$eventProof=Join-Path $eventRoot $ProofDirectory
New-Item -ItemType Directory -Path $eventProof -Force | Out-Null
$eventTask=if($Verify){'SvgCanvasVerify'}else{'SvgCanvasImport'}
$eventOutput=if($Verify){Join-Path $eventProof 'native'}else{Join-Path $eventProof 'import-receipt.json'}
$eventLog=Join-Path $eventProof ($eventTask+'.log')
$eventArgs=@(('"'+(Join-Path $eventRoot 'DungeonWorldOpus.uproject')+'"'),('-run='+$eventTask),('-Bundle="'+$eventBundle+'"'),('-Output="'+$eventOutput+'"'),'-SvgCanvasRoot=/Game/UICreatorEvents','-Multiprocess','-unattended','-nosplash','-nosound','-as-exit-on-error',('-ABSLOG="'+$eventLog+'"'))
if($Verify){$eventArgs+=@('-AllowCommandletRendering','-RenderOffscreen')}else{$eventArgs+='-NullRHI'}
$eventProcess=Start-Process -FilePath 'G:\UnrealEngine-Angelscript-5.8.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' -ArgumentList $eventArgs -WorkingDirectory $eventRoot -WindowStyle Hidden -PassThru
$eventDeadline=(Get-Date).AddMinutes(5)
while(!$eventProcess.HasExited -and (Get-Date)-lt $eventDeadline){Start-Sleep -Milliseconds 500;$eventProcess.Refresh()}
if(!$eventProcess.HasExited){Stop-Process -Id $eventProcess.Id -Force;throw "Scoped map-event commandlet timed out: $eventLog"}
$eventProcess.WaitForExit();$eventText=Get-Content -LiteralPath $eventLog -Raw -Encoding UTF8
if($eventProcess.ExitCode-ne 0 -or $eventText-match 'Angelscript: Error:|Fatal error:'){throw "Map-event commandlet failed: $eventLog"}
Write-Host "$eventTask passed: $eventOutput"
