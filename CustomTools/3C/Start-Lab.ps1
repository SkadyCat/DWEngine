param([ValidateSet('opus','controlled')][string]$Mode='controlled',[switch]$Editor,[switch]$CameraTour)
$ErrorActionPreference='Stop'
$c3Root=$PSScriptRoot
$c3Engine='G:\UnrealEngine-Angelscript-5.8.3\Engine\Binaries\Win64\UnrealEditor.exe'
$c3Args=@('"'+(Join-Path $c3Root 'ThreeCLab.uproject')+'"','-NoSplash','-NoLoadingScreen','-as-exit-on-error','-as-development-mode')
if(!$Editor){$c3Args+=@('/Game/ThreeCLab/Maps/L_ThreeCArena?game=/Script/Angelscript.ThreeCGameMode','-game','-windowed','-ResX=1600','-ResY=1000',"-C3Mode=$Mode")}
if($CameraTour){$c3Args+='-C3Camera=cycle'}
Start-Process -FilePath $c3Engine -ArgumentList $c3Args -WorkingDirectory $c3Root -WindowStyle Normal
