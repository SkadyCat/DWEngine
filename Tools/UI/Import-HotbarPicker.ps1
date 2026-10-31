param([string]$Job='wbp-hotbar-d888a20d-3ddc-45d3-81fc-c194b55c70a2',[switch]$Update)
$ErrorActionPreference='Stop'
$hotbarRoot=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
if($Job -notmatch '^wbp-hotbar-[a-zA-Z0-9-]+$'){throw 'Invalid hotbar job'}
$hotbarProof=Join-Path $hotbarRoot 'Saved/Verification/HotbarPickerUE'
$hotbarOutput=Join-Path $hotbarRoot 'Content/UICreatorHotbar'
if(Test-Path -LiteralPath $hotbarOutput){
 if(!$Update){throw 'Hotbar destination already exists; use -Update for the existing hotbar project'}
 $hotbarSource=Get-Content -LiteralPath (Join-Path $hotbarOutput 'source.uialpha.json') -Raw -Encoding utf8|ConvertFrom-Json
 if($hotbarSource.id-ne 'opus-hotbar-picker-20261028'){throw 'Refusing to replace a different UI project'}
 if(@(Get-CimInstance Win32_Process -Filter "Name = 'UnrealEditor.exe'"|Where-Object {$_.CommandLine-match 'DungeonWorldOpus.uproject'}).Count){throw 'Close the Opus editor before replacing loaded HUD assets'}
}
$hotbarLog=Join-Path $hotbarProof 'import.log'
$hotbarArgs=@(('"'+$hotbarRoot+'/DungeonWorldOpus.uproject"'),'-run=SvgCanvasImport',('-Bundle="'+$hotbarRoot+'/CustomTools/UICreatorAlpha/exports/'+$Job+'/bundle.json"'),('-Output="'+$hotbarProof+'/import.json"'),'-SvgCanvasRoot=/Game/UICreatorHotbar','-NullRHI','-Multiprocess','-unattended','-nosplash','-nosound','-as-exit-on-error','-DisablePlugins=UE_MCP_Bridge',('-ABSLOG="'+$hotbarLog+'"'))
$hotbarProcess=Start-Process 'G:/UnrealEngine-Angelscript-5.8.3/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $hotbarArgs -WorkingDirectory $hotbarRoot -WindowStyle Hidden -PassThru
$hotbarDeadline=(Get-Date).AddMinutes(5)
while(!$hotbarProcess.HasExited -and (Get-Date)-lt$hotbarDeadline){Start-Sleep -Milliseconds 500;$hotbarProcess.Refresh()}
if(!$hotbarProcess.HasExited){Stop-Process -Id $hotbarProcess.Id;throw 'Own hotbar import timed out'}
$hotbarProcess.WaitForExit()
$hotbarText=Get-Content -LiteralPath $hotbarLog -Raw -Encoding utf8
if($hotbarProcess.ExitCode-ne 0 -or $hotbarText-match 'Angelscript: Error:|Fatal error:'){throw "Hotbar import failed: $hotbarLog"}
& node (Join-Path $PSScriptRoot 'Stage-HotbarPicker.mjs') $Job
if($LASTEXITCODE-ne 0){throw 'Hotbar source staging failed'}
Write-Output "Hotbar imported: $hotbarOutput"
