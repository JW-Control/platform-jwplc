@echo off
setlocal

set "GATE=%~dp0a14_p5e2_core_autoservice_ab.ps1"
set "VALIDATOR=%~dp0assert_ps1_syntax.ps1"
set "BUILD_CORE=%~dp0..\..\..\tools\build-speed-benchmark\Build-JWPLCPrecompiledCore.ps1"
set "VERIFY_CORE=%~dp0..\..\..\tools\build-speed-benchmark\Verify-JWPLCPrecompiledCore.ps1"
set "P5B=%~dp0a14_p5b_physical_master_slave_combined.ps1"

for %%P in ("%GATE%" "%VALIDATOR%" "%BUILD_CORE%" "%VERIFY_CORE%" "%P5B%") do (
    if not exist "%%~fP" (
        echo P5E2_WRAPPER_REQUIRED_PATH_MISSING=%%~fP
        exit /b 91
    )
)

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%VALIDATOR%" -Path "%GATE%"
if errorlevel 1 exit /b %errorlevel%

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%VALIDATOR%" -Path "%BUILD_CORE%"
if errorlevel 1 exit /b %errorlevel%

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%VALIDATOR%" -Path "%VERIFY_CORE%"
if errorlevel 1 exit /b %errorlevel%

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%VALIDATOR%" -Path "%P5B%"
if errorlevel 1 exit /b %errorlevel%

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%GATE%" %*
exit /b %ERRORLEVEL%
