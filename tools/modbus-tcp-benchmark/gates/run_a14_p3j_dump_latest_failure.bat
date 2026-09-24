@echo off
setlocal

set "GATE=%~dp0a14_p3j_dump_latest_failure.ps1"
set "VALIDATOR=%~dp0assert_ps1_syntax.ps1"

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%VALIDATOR%" -Path "%GATE%"
if errorlevel 1 exit /b %errorlevel%

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%GATE%"
exit /b %ERRORLEVEL%
