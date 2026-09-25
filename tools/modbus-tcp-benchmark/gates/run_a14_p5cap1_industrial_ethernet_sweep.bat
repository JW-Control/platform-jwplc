@echo off
setlocal

set "GATE=%~dp0a14_p5cap1_industrial_ethernet_sweep.ps1"
set "VALIDATOR=%~dp0assert_ps1_syntax.ps1"
set "VERIFY_CORE=%~dp0..\..\..\tools\build-speed-benchmark\Verify-JWPLCPrecompiledCore.ps1"
set "P5B=%~dp0a14_p5b_physical_master_slave_combined.ps1"

for %%P in ("%GATE%" "%VALIDATOR%" "%VERIFY_CORE%" "%P5B%") do (
    if not exist "%%~fP" (
        echo P5CAP1_WRAPPER_REQUIRED_PATH_MISSING=%%~fP
        exit /b 91
    )
)

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%VALIDATOR%" -Path "%GATE%"
if errorlevel 1 exit /b %errorlevel%

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%VALIDATOR%" -Path "%VERIFY_CORE%"
if errorlevel 1 exit /b %errorlevel%

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%VALIDATOR%" -Path "%P5B%"
if errorlevel 1 exit /b %errorlevel%

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%GATE%" %*
exit /b %ERRORLEVEL%
