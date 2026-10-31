param([string]$Job='wbp-0ccc0be2-719b-41f5-8f79-aad5280ad3ce',[switch]$Verify)
$ErrorActionPreference='Stop'
$talentRoot=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
if($Job -notmatch '^wbp-[a-zA-Z0-9_-]{1,90}$'){throw 'Invalid job'}
$talentBundle=Join-Path $talentRoot "CustomTools/UICreatorAlpha/exports/$Job/bundle.json"
if(!(Test-Path -LiteralPath $talentBundle)){throw 'Frozen UICreator bundle missing'}
$talentProof=Join-Path $talentRoot 'Saved/Verification/Talents'
New-Item -ItemType Directory -Path $talentProof -Force | Out-Null
if(!$Verify){
 & python (Join-Path $PSScriptRoot 'Install-Talents.py') $Job
 if($LASTEXITCODE-ne 0){throw 'Contract installation failed'}
}
$talentTask=if($Verify){'SvgCanvasVerify'}else{'SvgCanvasImport'}
$talentOutput=if($Verify){Join-Path $talentProof 'native-readback'}else{Join-Path $talentProof 'native-import-receipt.json'}
$talentLog=Join-Path $talentProof ($talentTask+'.log')
$talentArgs=@(('"'+(Join-Path $talentRoot 'DungeonWorldOpus.uproject')+'"'),('-run='+$talentTask),('-Bundle="'+$talentBundle+'"'),('-Output="'+$talentOutput+'"'),'-SvgCanvasRoot=/Game/UICreatorTalents','-Multiprocess','-unattended','-nosplash','-nosound','-DisablePlugins=UE_MCP_Bridge','-as-exit-on-error',('-ABSLOG="'+$talentLog+'"'))
if($Verify){$talentArgs+=@('-AllowCommandletRendering','-RenderOffscreen')}else{$talentArgs+='-NullRHI'}
$talentProcess=Start-Process -FilePath 'G:/UnrealEngine-Angelscript-5.8.3/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $talentArgs -WorkingDirectory $talentRoot -WindowStyle Hidden -PassThru
$talentProcess.WaitForExit()
$talentText=Get-Content -LiteralPath $talentLog -Raw -Encoding UTF8
if($talentProcess.ExitCode-ne 0 -or $talentText-match 'Angelscript: Error:|Fatal error:'){throw "Native $talentTask failed: $talentLog"}
Write-Output "$talentTask passed: $talentOutput"
