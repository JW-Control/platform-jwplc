@echo off
setlocal
set "GATES=%~dp0"

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%GATES%assert_ps1_syntax.ps1" -Path "%GATES%a14_h3e3a_display_source_migration.ps1"
if errorlevel 1 exit /b %errorlevel%

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%GATES%a14_h3e3a_display_source_migration.ps1" %*
exit /b %errorlevel%
