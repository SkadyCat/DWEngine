param()
$ErrorActionPreference='Stop'
$attributeRoot=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$attributeProof=Join-Path $attributeRoot 'Saved/Verification/AttributeScroll'
New-Item -ItemType Directory -Force $attributeProof | Out-Null
$attributeLog=Join-Path $attributeProof 'runtime.log'
$attributeArgs=@(('"'+$attributeRoot+'/DungeonWorldOpus.uproject"'),'/Game/ClaudeDungeon/Maps/L_OpusDungeon?game=/Script/Angelscript.DWOTowerGameMode','-game','-DWOTransient','-DWOMapItemReloadTest','-DWOView=character','-DWOAttributeScrollTest',('-DWOAttributeScrollProof='+$attributeProof.Replace('\','/')),'-DWOSmoke=25','-Multiprocess','-as-exit-on-error','-DisablePlugins=UE_MCP_Bridge','-unattended','-nosplash','-nosound','-NoLoadingScreen','-windowed','-RenderOffscreen','-ResX=1600','-ResY=900','-ForceRes','-SvgCanvasNoMotion','-ExecCmds="t.MaxFPS 60,t.IdleWhenNotForeground 0,Slate.bAllowThrottling 0,r.MotionBlurQuality 0"',('-ABSLOG="'+$attributeLog+'"'))
$attributeStarted=Get-Date
$attributeProcess=Start-Process -FilePath 'G:/UnrealEngine-Angelscript-5.8.3/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $attributeArgs -WorkingDirectory $attributeRoot -WindowStyle Hidden -PassThru
$attributeDeadline=(Get-Date).AddMinutes(3)
while(!$attributeProcess.HasExited -and (Get-Date)-lt $attributeDeadline){Start-Sleep -Milliseconds 500;$attributeProcess.Refresh()}
if(!$attributeProcess.HasExited){Stop-Process -Id $attributeProcess.Id -Force;throw 'Own attribute scroll verification process timed out'}
$attributeProcess.WaitForExit();$attributeText=Get-Content -LiteralPath $attributeLog -Raw -Encoding UTF8
if($attributeProcess.ExitCode-ne 0 -or $attributeText-match 'Angelscript: Error:|Fatal error:|ATTRIBUTE_SCROLL_FAIL' -or $attributeText-notmatch 'ATTRIBUTE_SCROLL_PASS NativeViewport'){throw "Attribute scrolling verification failed: $attributeLog"}
foreach($attributeName in @('top','bottom')){
 $attributeImage=Join-Path $attributeProof ($attributeName+'.png')
 if(!(Test-Path -LiteralPath $attributeImage)-or (Get-Item -LiteralPath $attributeImage).LastWriteTime-lt $attributeStarted){throw "Missing or stale native capture: $attributeName"}
}
Select-String -LiteralPath $attributeLog -Pattern 'ATTRIBUTE_SCROLL_PASS' | ForEach-Object {$_.Line}
Write-Output "Attribute scroll verified: $attributeProof"
