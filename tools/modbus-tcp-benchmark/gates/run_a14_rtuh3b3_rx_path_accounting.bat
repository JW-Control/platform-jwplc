@echo off
setlocal

set "GATE=%~dp0a14_rtuh3b3_rx_path_accounting.ps1"
set "VALIDATOR=%~dp0assert_ps1_syntax.ps1"

for %%P in ("%GATE%" "%VALIDATOR%") do (
    if not exist "%%~fP" (
        echo RTUH3B3_WRAPPER_REQUIRED_PATH_MISSING=%%~fP
        exit /b 91
    )
)

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%VALIDATOR%" -Path "%GATE%"
if errorlevel 1 exit /b %errorlevel%

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%GATE%" %*
exit /b %ERRORLEVEL%
