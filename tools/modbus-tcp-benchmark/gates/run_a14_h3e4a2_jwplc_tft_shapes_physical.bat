@echo off
setlocal
set "GATES=%~dp0"

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%GATES%assert_ps1_syntax.ps1" -Path "%GATES%a14_h3e4a1_jwplc_tft_shapes_candidate.ps1"
if errorlevel 1 exit /b %errorlevel%

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%GATES%assert_ps1_syntax.ps1" -Path "%GATES%a14_h3e4a2_jwplc_tft_shapes_physical.ps1"
if errorlevel 1 exit /b %errorlevel%

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%GATES%a14_h3e4a2_jwplc_tft_shapes_physical.ps1" %*
exit /b %errorlevel%
