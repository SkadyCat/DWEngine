param([int]$FPS=60,[switch]$Render,[switch]$Crowd)
$ErrorActionPreference='Stop'
$threeCRoot=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$threeCProof=Join-Path $threeCRoot 'Saved/Verification/ThreeC'
New-Item -ItemType Directory -Force -Path $threeCProof | Out-Null
$threeCTag=if($Crowd){'crowd-300'}elseif($Render){'rendered'}else{'native-'+$FPS}
$threeCLog=Join-Path $threeCProof ($threeCTag+'.log')
$threeCProfile=Join-Path $threeCRoot 'Saved/RuleTower/Profile.v2.json'
$threeCBefore=(Get-FileHash -LiteralPath $threeCProfile -Algorithm SHA256).Hash
$threeCArgs=@(('"'+$threeCRoot+'/DungeonWorldOpus.uproject"'),'/Engine/Maps/Entry?game=/Script/Angelscript.DWOTowerGameMode','-game','-DWOHeroArena','-DWOTransient','-DWO3CTest',('-DWO3CFPS='+$FPS),'-Multiprocess','-unattended','-nosplash','-nosound','-NoLoadingScreen','-as-exit-on-error','-DisablePlugins=UE_MCP_Bridge',('-ABSLOG="'+$threeCLog+'"'),'-ExecCmds="t.MaxFPS 60,t.IdleWhenNotForeground 0,Slate.bAllowThrottling 0,r.MotionBlurQuality 0"')
if($Crowd){$threeCArgs=@($threeCArgs | Where-Object {$_-ne '-DWOHeroArena' -and $_-ne '-DWO3CTest'})+@('-DWOFrameTest','-DWODensity=300','-DWOGod','-DWOSmoke=150')}
if($Render){$threeCArgs+=@('-windowed','-RenderOffscreen','-ResX=1600','-ResY=900','-ForceRes','-DWO3CCapture')}else{$threeCArgs+='-NullRHI'}
$threeCProcess=Start-Process 'G:/UnrealEngine-Angelscript-5.8.3/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $threeCArgs -WorkingDirectory $threeCRoot -WindowStyle Hidden -PassThru
Write-Output "3C check PID=$($threeCProcess.Id) log=$threeCLog"
$threeCDeadline=(Get-Date).AddMinutes(5)
while(!$threeCProcess.HasExited -and (Get-Date)-lt $threeCDeadline){Start-Sleep -Milliseconds 500;$threeCProcess.Refresh()}
if(!$threeCProcess.HasExited){Stop-Process -Id $threeCProcess.Id;throw "Own 3C verification timed out: $threeCLog"}
$threeCProcess.WaitForExit();$threeCText=Get-Content -LiteralPath $threeCLog -Raw -Encoding utf8
$threeCComplete=if($Crowd){'FRAME_TEST_PASS RetryAndAssetCache'}else{'THREEC_PASS Complete'}
if($threeCProcess.ExitCode-ne 0 -or $threeCText-match 'THREEC_FAIL|FRAME_TEST_FAIL|Angelscript: Error:|TowerError|Fatal error:' -or $threeCText-notmatch $threeCComplete){throw "3C verification failed: $threeCLog"}
if($Crowd -and [regex]::Matches($threeCText,'FRAME_TEST_PASS AllMonstersWithBoss count=300 bosses=1').Count-ne 2){throw 'Both 300-monster publications must include the boss'}
$threeCAfter=(Get-FileHash -LiteralPath $threeCProfile -Algorithm SHA256).Hash
if($threeCBefore-ne $threeCAfter){throw 'Formal profile changed during transient 3C verification'}
@{test=$threeCTag;fpsCap=$FPS;exit=$threeCProcess.ExitCode;transient=$true;profileUnchanged=$true;profileBefore=$threeCBefore;profileAfter=$threeCAfter;profileRestored=$false;passed=([regex]::Matches($threeCText,'THREEC_PASS|FRAME_TEST_PASS')).Count} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $threeCProof ($threeCTag+'-receipt.json')) -Encoding utf8
Select-String -LiteralPath $threeCLog -Pattern 'THREEC_|FRAME_TEST_' | ForEach-Object Line
