@echo off
setlocal

set "GATE=%~dp0a14_p5e2_core_autoservice_ab.ps1"
set "VALIDATOR=%~dp0assert_ps1_syntax.ps1"
set "REPO_ROOT=%~dp0......"
set "BUILD_CORE=%REPO_ROOT%	oolsuild-speed-benchmarkBuild-JWPLCPrecompiledCore.ps1"
set "VERIFY_CORE=%REPO_ROOT%	oolsuild-speed-benchmarkVerify-JWPLCPrecompiledCore.ps1"
set "P5B=%~dp0a14_p5b_physical_master_slave_combined.ps1"

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
