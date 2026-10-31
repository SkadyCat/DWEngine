param([string]$Job='wbp-recent-d1b8b241-9167-4aa6-abf8-4354c912f885',[switch]$Verify,[string]$ProofDirectory='Saved/Verification/UEIntegration/UICreator')
$ErrorActionPreference='Stop'
$recentRoot=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
if($Job -notmatch '^wbp-[a-zA-Z0-9_-]{1,90}$'){throw 'Invalid frozen export job'}
$recentBundle=Join-Path $recentRoot "CustomTools/UICreatorAlpha/exports/$Job/bundle.json"
if(!(Test-Path -LiteralPath $recentBundle)){throw 'Frozen UICreator bundle is missing'}
$recentProof=Join-Path $recentRoot $ProofDirectory
New-Item -ItemType Directory -Path $recentProof -Force | Out-Null
$recentTask=if($Verify){'SvgCanvasVerify'}else{'SvgCanvasImport'}
$recentOutput=if($Verify){Join-Path $recentProof 'native'}else{Join-Path $recentProof 'import-receipt.json'}
$recentLog=Join-Path $recentProof ($recentTask+'.log')
$recentArgs=@(('"'+(Join-Path $recentRoot 'DungeonWorldOpus.uproject')+'"'),('-run='+$recentTask),('-Bundle="'+$recentBundle+'"'),('-Output="'+$recentOutput+'"'),'-SvgCanvasRoot=/Game/UICreatorRecent','-Multiprocess','-unattended','-nosplash','-nosound','-as-exit-on-error',('-ABSLOG="'+$recentLog+'"'))
if($Verify){$recentArgs+=@('-AllowCommandletRendering','-RenderOffscreen')}else{$recentArgs+='-NullRHI'}
$recentProcess=Start-Process -FilePath 'G:\UnrealEngine-Angelscript-5.8.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' -ArgumentList $recentArgs -WorkingDirectory $recentRoot -WindowStyle Hidden -PassThru
$recentDeadline=(Get-Date).AddMinutes(5)
while(!$recentProcess.HasExited -and (Get-Date)-lt $recentDeadline){Start-Sleep -Milliseconds 500;$recentProcess.Refresh()}
if(!$recentProcess.HasExited){Stop-Process -Id $recentProcess.Id -Force;throw "Own UICreator commandlet timed out: $recentLog"}
$recentProcess.WaitForExit();$recentText=Get-Content -LiteralPath $recentLog -Raw -Encoding UTF8
if($recentProcess.ExitCode-ne 0 -or $recentText-match 'Angelscript: Error:|Fatal error:'){throw "UICreator commandlet failed: $recentLog"}
Write-Output "$recentTask passed: $recentOutput"
