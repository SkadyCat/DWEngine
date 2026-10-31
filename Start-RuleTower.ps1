param([switch]$Fresh,[switch]$NoHUD,[switch]$AutoStart,[switch]$LowMemory)
$ErrorActionPreference='Stop'
$opusRoot=$PSScriptRoot
$opusEditor='G:\UnrealEngine-Angelscript-5.8.3\Engine\Binaries\Win64\UnrealEditor.exe'
& (Join-Path $opusRoot 'CustomTools/TradeBackend/scripts/Start.ps1') -NoBuild
if($Fresh){
 $opusProfile=Join-Path $opusRoot 'Saved\RuleTower\Profile.v2.json'
 if(Test-Path -LiteralPath $opusProfile){Move-Item -LiteralPath $opusProfile -Destination ($opusProfile+'.'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'.bak')}
}
$opusArgs=@('"'+(Join-Path $opusRoot 'DungeonWorldOpus.uproject')+'"','/Game/ClaudeDungeon/Maps/L_OpusDungeon?game=/Script/Angelscript.DWOTowerGameMode','-game','-windowed','-ResX=1600','-ResY=900','-NoSplash','-NoLoadingScreen')
if($NoHUD){$opusArgs+='-DWONoHUD'}
if($AutoStart){$opusArgs+='-DWOStart'}
if($LowMemory){$opusArgs+='-ForceDPCVars=r.Streaming.PoolSize=256,r.Nanite.Streaming.StreamingPoolSize=64,r.Shadow.Virtual.MaxPhysicalPages=512,r.Lumen.ScreenProbeGather.RadianceCache.ProbeResolution=16,r.RayTracing=0,r.ScreenPercentage=85,s.AsyncLoadingTimeLimit=50,s.AsyncLoadingUseFullTimeLimit=1,t.IdleWhenNotForeground=0'}
Start-Process -FilePath $opusEditor -ArgumentList $opusArgs -WorkingDirectory $opusRoot -WindowStyle Normal
