@echo off
setlocal

set "GATE=%~dp0a14_p5f_rebuild_core_datalog_autoservice.ps1"
set "VALIDATOR=%~dp0assert_ps1_syntax.ps1"
set "REPO_ROOT=%~dp0..\..\.."
set "BUILD_CORE=%REPO_ROOT%\tools\build-speed-benchmark\Build-JWPLCPrecompiledCore.ps1"
set "VERIFY_CORE=%REPO_ROOT%\tools\build-speed-benchmark\Verify-JWPLCPrecompiledCore.ps1"

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%VALIDATOR%" -Path "%GATE%"
if errorlevel 1 exit /b %errorlevel%

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%VALIDATOR%" -Path "%BUILD_CORE%"
if errorlevel 1 exit /b %errorlevel%

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%VALIDATOR%" -Path "%VERIFY_CORE%"
if errorlevel 1 exit /b %errorlevel%

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%GATE%" %*
exit /b %ERRORLEVEL%
