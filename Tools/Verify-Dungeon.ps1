param([switch]$Visuals,[switch]$Reset,[switch]$SpawnSafetyOnly)
$ErrorActionPreference='Stop'
$dungeonRoot=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$dungeonProof=Join-Path $dungeonRoot 'Saved/Verification/DungeonExtraction'
New-Item -ItemType Directory -Path $dungeonProof -Force | Out-Null
$dungeonName=if($SpawnSafetyOnly){'spawn-safety'}elseif($Reset){'reset'}elseif($Visuals){'rendered'}else{'integration'}
$dungeonLog=Join-Path $dungeonProof ($dungeonName+'.log')
$dungeonArgs=@('"'+(Join-Path $dungeonRoot 'DungeonWorldOpus.uproject')+'"','/Game/ClaudeDungeon/Maps/L_OpusDungeon?game=/Script/Angelscript.DWOTowerGameMode','-game','-DWOTransient','-DWOGod','-DWOSmoke=150','-Multiprocess','-unattended','-nosplash','-nosound','-NoLoadingScreen',"-ABSLOG=$dungeonLog")
if($Reset){$dungeonArgs+='-DWOResetTest'}else{$dungeonArgs+='-DWODungeonTest'}
if($SpawnSafetyOnly){$dungeonArgs+='-DWOSpawnSafetyOnly'}
if($Visuals){
 $dungeonCapture=(Join-Path $dungeonProof 'opus-dungeon.png').Replace('\','/')
 $dungeonArgs+=@('-windowed','-ResX=1600','-ResY=900','-SvgCanvasNoMotion',"-DWODungeonCapture=$dungeonCapture",'-ForceDPCVars=r.Streaming.PoolSize=256,r.Nanite.Streaming.StreamingPoolSize=64,r.Shadow.Virtual.MaxPhysicalPages=512,r.Lumen.ScreenProbeGather.RadianceCache.ProbeResolution=16,r.RayTracing=0,r.ScreenPercentage=85,s.AsyncLoadingTimeLimit=50,s.AsyncLoadingUseFullTimeLimit=1,t.IdleWhenNotForeground=0')
}else{$dungeonArgs+='-nullrhi'}
$dungeonStarted=Get-Date
$dungeonProcess=Start-Process -FilePath 'G:/UnrealEngine-Angelscript-5.8.3/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $dungeonArgs -WorkingDirectory $dungeonRoot -WindowStyle Hidden -PassThru
"Dungeon verification PID=$($dungeonProcess.Id) log=$dungeonLog"
$dungeonDeadline=(Get-Date).AddMinutes(5)
while(!$dungeonProcess.HasExited -and (Get-Date)-lt $dungeonDeadline){Start-Sleep -Milliseconds 500;$dungeonProcess.Refresh()}
if(!$dungeonProcess.HasExited){Stop-Process -Id $dungeonProcess.Id -Force;throw "Dungeon verification timed out: $dungeonLog"}
$dungeonProcess.WaitForExit()
$dungeonText=Get-Content -LiteralPath $dungeonLog -Raw -Encoding UTF8
if($dungeonProcess.ExitCode-ne 0 -or $dungeonText-match 'Angelscript: Error:|DUNGEON_TEST_FAIL|SVG_TEST_FAIL|TowerError|Fatal error:'){throw "Dungeon verification failed: $dungeonLog"}
$dungeonChecks=if($SpawnSafetyOnly){@('ZeroGeometry','ManualReady','ConnectedNavigation','CollisionAndProjectileWalls','SpawnExclusionConfig','SpawnExclusionBoundary','HighDensitySpawns','SpawnExclusionAllMonsters','SpawnExclusionFallbackKeepsOriginalStart','SpawnExclusionEventGuards','SpawnSafety')|ForEach-Object{'DUNGEON_TEST_PASS '+$_}}elseif($Reset){@('SVG_TEST_PASS ResetStartsCurrentFloor')}else{@('ZeroGeometry','ManualReady','ConnectedNavigation','CollisionAndProjectileWalls','HighDensitySpawns','SpawnExclusionAllMonsters','SpawnExclusionFallbackKeepsOriginalStart','SpawnExclusionEventGuards','PlayerWallsBeyondOldBounds','NativeDungeonSpawns','MonstersRespectAggroRange','NativeCombatAndLoot','FreshLevelTravel','Integration')|ForEach-Object{'DUNGEON_TEST_PASS '+$_}}
foreach($dungeonCheck in $dungeonChecks){if(!$dungeonText.Contains($dungeonCheck)){throw "Missing dungeon verification: $dungeonCheck"}}
if($Visuals){if((Get-Item -LiteralPath $dungeonCapture).LastWriteTime-lt $dungeonStarted){throw 'Stale dungeon capture'}}
"Dungeon verification passed: $dungeonLog"
