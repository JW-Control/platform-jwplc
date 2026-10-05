@echo off
setlocal

set "GATE=%~dp0a14_rtuh2b_500k_tcp500_confirmation.ps1"
set "VALIDATOR=%~dp0assert_ps1_syntax.ps1"

for %%P in ("%GATE%" "%VALIDATOR%") do (
    if not exist "%%~fP" (
        echo RTUH2B_WRAPPER_REQUIRED_PATH_MISSING=%%~fP
        exit /b 91
    )
)

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%VALIDATOR%" -Path "%GATE%"
if errorlevel 1 exit /b %errorlevel%

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%GATE%" %*
exit /b %ERRORLEVEL%
