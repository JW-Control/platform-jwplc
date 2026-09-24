@echo off
setlocal

set "GATE=%~dp0a14_p5_final_full_runtime_combined.ps1"
set "VALIDATOR=%~dp0assert_ps1_syntax.ps1"

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%VALIDATOR%" -Path "%GATE%"
if errorlevel 1 exit /b %errorlevel%

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%GATE%" %*
exit /b %ERRORLEVEL%
