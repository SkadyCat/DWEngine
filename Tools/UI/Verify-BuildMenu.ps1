param([string]$Job='wbp-build-menu-afb9f6b7-572d-4eb8-8640-cc18d684aaeb',[switch]$ImportOnly,[switch]$SkipImport)
$ErrorActionPreference='Stop'
$bdRoot=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$bdProof=Join-Path $bdRoot 'Saved/Verification/BuildMenu'
New-Item -ItemType Directory -Path $bdProof -Force | Out-Null
if($Job -notmatch '^wbp-[a-zA-Z0-9_-]{1,90}$'){throw 'Invalid frozen build menu job'}
function Invoke-BuildMenuStage([string]$Name,[string[]]$Extra){
 $bdLog=Join-Path $bdProof ($Name+'.log')
 $bdArgs=@(('"'+$bdRoot+'/DungeonWorldOpus.uproject"'),'-Multiprocess','-unattended','-nosplash','-nosound','-as-exit-on-error','-DisablePlugins=UE_MCP_Bridge',('-ABSLOG="'+$bdLog+'"'))+$Extra
 $bdProcess=Start-Process -FilePath 'G:/UnrealEngine-Angelscript-5.8.3/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $bdArgs -WorkingDirectory $bdRoot -WindowStyle Hidden -PassThru
 $bdDeadline=(Get-Date).AddMinutes(3)
 while(!$bdProcess.HasExited -and (Get-Date)-lt $bdDeadline){Start-Sleep -Milliseconds 500;$bdProcess.Refresh()}
 if(!$bdProcess.HasExited){Stop-Process -Id $bdProcess.Id -Force;throw "Own build menu process timed out: $Name"}
 $bdProcess.WaitForExit();$bdText=Get-Content -LiteralPath $bdLog -Raw -Encoding utf8
 if($bdProcess.ExitCode-ne 0 -or $bdText-match 'Angelscript: Error:|Fatal error:|BUILD_MENU_FAIL'){throw "Build menu $Name failed: $bdLog"}
 Write-Host "Passed build menu $Name";return $bdText
}
if(!$SkipImport){$null=Invoke-BuildMenuStage 'import' @('-run=SvgCanvasImport',('-Bundle="'+$bdRoot+'/CustomTools/UICreatorAlpha/exports/'+$Job+'/bundle.json"'),('-Output="'+$bdProof+'/import-receipt.json"'),'-SvgCanvasRoot=/Game/UICreatorBuilds','-NullRHI')}
if($ImportOnly){return}
$bdProfile=Join-Path $bdRoot 'Saved/RuleTower/Profile.v2.json'
$bdBefore=(Get-FileHash -LiteralPath $bdProfile -Algorithm SHA256).Hash
$bdStarted=Get-Date
$bdText=Invoke-BuildMenuStage 'runtime' @('/Game/ClaudeDungeon/Maps/L_OpusDungeon?game=/Script/Angelscript.DWOTowerGameMode','-game','-DWOTransient','-DWOBuildMenuTest',('-DWOBuildMenuProof='+$bdProof.Replace('\','/')),'-NoLoadingScreen','-windowed','-RenderOffscreen','-ResX=1600','-ResY=900','-ForceRes','-SvgCanvasNoMotion','-DWOSmoke=60','-asyncassetcompilationmaxconcurrency=2','-asynctexturecompilationmaxconcurrency=1','-ExecCmds="r.SetRes 1600x900w,t.MaxFPS 60,t.IdleWhenNotForeground 0,Slate.bAllowThrottling 0,r.MotionBlurQuality 0"')
if($bdText-notmatch 'BUILD_MENU_PASS families=23 presets=27 grades=5'){throw 'Native build menu checks did not finish'}
foreach($bdName in @('families','grades-details','applied-fifth')){ $bdImage=Join-Path $bdProof ($bdName+'.png');if(!(Test-Path -LiteralPath $bdImage)-or (Get-Item -LiteralPath $bdImage).LastWriteTime-lt $bdStarted){throw "Missing or stale native build menu screenshot: $bdName"} }
if((Get-FileHash -LiteralPath $bdProfile -Algorithm SHA256).Hash-ne $bdBefore){throw 'Formal profile changed during transient build menu test'}
Write-Host 'Build menu native input, all families/grades, actual apply and profile preservation passed'
