@echo off
setlocal
set "GATES=%~dp0"
set "SCRIPT=%GATES%a14_h4a0_raw_ethernet_baseline.ps1"

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%GATES%assert_ps1_syntax.ps1" -Path "%SCRIPT%"
if errorlevel 1 exit /b %errorlevel%

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT%" %*
exit /b %errorlevel%
