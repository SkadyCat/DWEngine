@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Open-OpusEditor.ps1" %*
set "OPUS_LAUNCH_RESULT=%errorlevel%"
if not "%OPUS_LAUNCH_RESULT%"=="0" pause
exit /b %OPUS_LAUNCH_RESULT%
