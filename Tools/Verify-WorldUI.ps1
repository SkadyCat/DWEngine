param([switch]$Render,[switch]$Arena,[switch]$Online)
$ErrorActionPreference='Stop'
$opusRoot=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$opusProof=Join-Path $opusRoot 'Saved\Verification\WorldUI'
New-Item -ItemType Directory -Path $opusProof -Force | Out-Null
$opusName=if($Render){if($Arena){'loot-preview-arena'}else{'loot-preview'}}else{'loot-integration'}
if($Online){$opusName+='-online'}
$opusLog=Join-Path $opusProof ($opusName+'.log')
$opusArgs=@('"'+(Join-Path $opusRoot 'DungeonWorldOpus.uproject')+'"','/Engine/Maps/Entry?game=/Script/Angelscript.DWOTowerGameMode','-game','-DWOTransient','-DWOGod','-unattended','-nosplash','-nosound','-NoLoadingScreen',"-ABSLOG=$opusLog")
$opusContract=Get-Content -LiteralPath (Join-Path $opusRoot 'Content\UICreatorLoot\contract.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$opusArgs+=@('-Multiprocess','-as-exit-on-error',('-DWOExpectedWorldUIHash='+$opusContract.sourceHash))
if($Online){$opusArgs+='-DWODropsOnline'}
if($Arena){if(!$Render){throw 'Arena is a visual-only capture fixture'};$opusArgs+='-DWOHeroArena'}
if($Render){$opusArgs+=@('-DWOLootPreview','-windowed','-RenderOffscreen','-ForceRes','-ResX=1600','-ResY=900','-SvgCanvasNoMotion','-ExecCmds="sg.ShadowQuality 0,sg.GlobalIlluminationQuality 0,sg.ReflectionQuality 0,sg.EffectsQuality 0,sg.PostProcessQuality 0,r.Streaming.PoolSize 256,r.MotionBlurQuality 0,t.MaxFPS 60"','-DWOSmoke=120',('-DWOLootCapture='+$opusProof.Replace('\','/')+'/'+$opusName+'.png'))}else{$opusArgs+=@('-DWOLootTest','-nullrhi','-DWOSmoke=120')}
$opusLaunchTime=Get-Date
$opusProcess=Start-Process -FilePath 'G:\UnrealEngine-Angelscript-5.8.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' -ArgumentList $opusArgs -WorkingDirectory $opusRoot -WindowStyle Hidden -PassThru
$opusDeadline=(Get-Date).AddMinutes(5)
while(!$opusProcess.HasExited -and (Get-Date)-lt $opusDeadline){Start-Sleep -Milliseconds 500;$opusProcess.Refresh()}
if(!$opusProcess.HasExited){Stop-Process -Id $opusProcess.Id -Force;throw 'World UI verification timed out'}
$opusProcess.WaitForExit();$opusText=Get-Content -LiteralPath $opusLog -Raw -Encoding UTF8
if($opusProcess.ExitCode-ne 0 -or $opusText-match 'Angelscript: Error:|LOOT_TEST_FAIL|AS_TEST_FAIL|TowerError|Fatal error:'){throw "Failed: $opusLog"}
if(!$Render-and $opusText-notmatch 'LOOT_TEST_PASS RuntimeIntegration'){throw 'Native pickup contract did not complete'}
if($Render-and $opusText-notmatch 'LOOT_TEST_PASS VisualPreviewReady'){throw 'Loot UI was not ready when the preview was captured'}
if($Render-and (!(Test-Path -LiteralPath (Join-Path $opusProof ($opusName+'.png'))) -or (Get-Item -LiteralPath (Join-Path $opusProof ($opusName+'.png'))).LastWriteTime-lt $opusLaunchTime)){throw 'Missing or stale World UI screenshot'}
Write-Host "Passed $opusName : $opusLog"
