@echo off
setlocal

set "GATE=%~dp0a14_nb3_h_source_snapshot_commit.ps1"
set "VALIDATOR=%~dp0assert_ps1_syntax.ps1"

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%VALIDATOR%" -Path "%GATE%"
if errorlevel 1 exit /b %errorlevel%

if "%~1"=="" goto snapshot
if /I "%~1"=="commit" goto commit

echo USAGE:
echo   %~nx0
echo   %~nx0 commit
exit /b 64

:snapshot
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%GATE%"
exit /b %ERRORLEVEL%

:commit
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%GATE%" -Commit
exit /b %ERRORLEVEL%
