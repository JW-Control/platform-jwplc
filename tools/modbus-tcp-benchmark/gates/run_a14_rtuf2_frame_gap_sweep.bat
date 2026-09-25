@echo off
setlocal

set "GATE=%~dp0a14_rtuf2_frame_gap_sweep.ps1"
set "VALIDATOR=%~dp0assert_ps1_syntax.ps1"
set "P5B=%~dp0a14_p5b_physical_master_slave_combined.ps1"

for %%P in ("%GATE%" "%VALIDATOR%" "%P5B%") do (
    if not exist "%%~fP" (
        echo RTUF2_WRAPPER_REQUIRED_PATH_MISSING=%%~fP
        exit /b 91
    )
)

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%VALIDATOR%" -Path "%GATE%"
if errorlevel 1 exit /b %errorlevel%

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%VALIDATOR%" -Path "%P5B%"
if errorlevel 1 exit /b %errorlevel%

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%GATE%" %*
exit /b %ERRORLEVEL%
