param([switch]$Render)
$ErrorActionPreference='Stop'
$clipRoot=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$clipProof=Join-Path $clipRoot 'Saved\Verification\SkillIconClip'
New-Item -ItemType Directory -Path $clipProof -Force | Out-Null
$clipName=if($Render){'runtime'}else{'binding'}
$clipLog=Join-Path $clipProof ($clipName+'.log')
$clipArgs=@(('"'+$clipRoot+'\DungeonWorldOpus.uproject"'),'-Multiprocess','-unattended','-nosplash','-nosound',('-ABSLOG='+$clipLog))
if($Render){
 $clipArgs+=@('/Engine/Maps/Entry?game=/Script/Angelscript.DWOTowerGameMode','-game','-DWOTransient','-windowed','-ResX=1600','-ResY=900','-NoLoadingScreen','-SvgCanvasNoMotion','-DWOView=picker-2','-DWOSmoke=8',('-DWOCapture='+$clipProof.Replace('\','/')+'/runtime.png'))
}else{
 $clipArgs+=@('-nullrhi','-ExecCmds="Automation RunTests DungeonWorldOpus.UI.DynamicImageClip"','-TestExit="Automation Test Queue Empty"',('-ReportExportPath='+$clipProof+'\automation'))
}
$clipStart=Get-Date
$clipProcess=Start-Process -FilePath 'G:\UnrealEngine-Angelscript-5.8.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' -ArgumentList $clipArgs -WorkingDirectory $clipRoot -WindowStyle Hidden -PassThru
$clipDeadline=(Get-Date).AddMinutes(3)
while(!$clipProcess.HasExited -and (Get-Date)-lt $clipDeadline){Start-Sleep -Milliseconds 500;$clipProcess.Refresh()}
if(!$clipProcess.HasExited){Stop-Process -Id $clipProcess.Id -Force;throw 'Skill icon verification timed out'}
$clipProcess.WaitForExit();$clipText=Get-Content -LiteralPath $clipLog -Raw -Encoding UTF8
if($clipProcess.ExitCode-ne 0 -or $clipText-match 'Angelscript: Error:|Fatal error:|Test Completed. Result=\{Fail'){throw "Skill icon verification failed: $clipLog"}
if($Render){
 $clipImage=Join-Path $clipProof 'runtime.png'
 if(!(Test-Path -LiteralPath $clipImage) -or (Get-Item -LiteralPath $clipImage).LastWriteTime-lt $clipStart){throw 'Missing or stale skill icon screenshot'}
}elseif($clipText-notmatch 'Test Completed. Result=\{Success\}.*DynamicImageClip'){throw 'Dynamic image clipping test did not pass'}
Write-Host "Passed $clipName : $clipLog"
