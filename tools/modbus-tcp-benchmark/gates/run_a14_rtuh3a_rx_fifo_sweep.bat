@echo off
setlocal

set "GATE=%~dp0a14_rtuh3a_rx_fifo_sweep.ps1"
set "VALIDATOR=%~dp0assert_ps1_syntax.ps1"

for %%P in ("%GATE%" "%VALIDATOR%") do (
    if not exist "%%~fP" (
        echo RTUH3A_WRAPPER_REQUIRED_PATH_MISSING=%%~fP
        exit /b 91
    )
)

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%VALIDATOR%" -Path "%GATE%"
if errorlevel 1 exit /b %errorlevel%

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%GATE%" %*
exit /b %ERRORLEVEL%
