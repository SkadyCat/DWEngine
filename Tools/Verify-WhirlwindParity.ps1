param([switch]$Before,[switch]$Visual,[switch]$Dungeon,[string]$ProofFolder='Saved/Verification/WhirlwindParity')
$ErrorActionPreference='Stop'
$swarmRoot=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$swarmProof=Join-Path $swarmRoot $ProofFolder
New-Item -ItemType Directory -Force -Path $swarmProof | Out-Null
$swarmPhase=if($Before){'before'}else{'after'}
$swarmLog=Join-Path $swarmProof ('native-'+$swarmPhase+$(if($Visual){'-visual'}else{''})+'.log')
$swarmArgs=@(('"'+$swarmRoot+'/DungeonWorldOpus.uproject"'),'/Engine/Maps/Entry?game=/Script/Angelscript.DWOTowerGameMode','-game','-DWOTransient','-DWOHeroArena','-DWOModernTest','-DWOWhirlwindParity','-NoLoadingScreen','-Multiprocess','-unattended','-nosplash','-nosound','-as-exit-on-error','-DisablePlugins=UE_MCP_Bridge',('-ABSLOG="'+$swarmLog+'"'),'-ExecCmds="t.MaxFPS 60,t.IdleWhenNotForeground 0,Slate.bAllowThrottling 0"')
if($ProofFolder-ne 'Saved/Verification/WhirlwindParity'){$swarmArgs+='-DWOWhirlwindGround'}
if($Dungeon){$swarmArgs[1]='/Game/ClaudeDungeon/Maps/L_OpusDungeon?game=/Script/Angelscript.DWOTowerGameMode';$swarmArgs=@($swarmArgs | Where-Object {$_-ne '-DWOHeroArena'});$swarmArgs+='-DWOWhirlwindDungeon'}
if($Before){$swarmArgs+='-DWOWhirlwindBefore'}else{$swarmArgs+='-DWOWhirlwindAssert'}
if($Visual){$swarmArgs+=@('-DWOWhirlwindVisual','-RenderOffscreen','-windowed','-ResX=1600','-ResY=900','-ForceRes')}else{$swarmArgs+='-NullRHI'}
$swarmProcess=Start-Process 'G:/UnrealEngine-Angelscript-5.8.3/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $swarmArgs -WorkingDirectory $swarmRoot -WindowStyle Hidden -PassThru
$swarmDeadline=(Get-Date).AddMinutes(3)
while(!$swarmProcess.HasExited -and (Get-Date)-lt $swarmDeadline){Start-Sleep -Milliseconds 500;$swarmProcess.Refresh()}
if(!$swarmProcess.HasExited){Stop-Process -Id $swarmProcess.Id;throw "Own swarm test timed out: $swarmLog"}
$swarmProcess.WaitForExit();$swarmText=Get-Content -LiteralPath $swarmLog -Raw
if($swarmProcess.ExitCode-ne 0 -or $swarmText-match 'WHIRLWIND_PARITY_FAIL|Angelscript: Error:|Fatal error:' -or $swarmText-notmatch 'WHIRLWIND_PARITY_DONE'){throw "Swarm verification failed: $swarmLog"}
Write-Output (($swarmText -split "`n"|Where-Object {$_-match 'WHIRLWIND_PARITY_DONE'}) -join "`n")
