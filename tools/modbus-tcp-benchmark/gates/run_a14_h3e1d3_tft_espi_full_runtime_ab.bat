@echo off
setlocal
set "GATES=%~dp0"

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%GATES%assert_ps1_syntax.ps1" -Path "%GATES%a14_h3e1b1_display_linkage_setup.ps1"
if errorlevel 1 exit /b %errorlevel%

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%GATES%assert_ps1_syntax.ps1" -Path "%GATES%a14_h3e1b1_display_linkage_leg.ps1"
if errorlevel 1 exit /b %errorlevel%

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%GATES%assert_ps1_syntax.ps1" -Path "%GATES%a14_h3e1d3_tft_espi_full_runtime_ab.ps1"
if errorlevel 1 exit /b %errorlevel%

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%GATES%a14_h3e1d3_tft_espi_full_runtime_ab.ps1" %*
exit /b %errorlevel%
