@echo off
setlocal

set "GATE_DIR=%~dp0"
set "ASSERT_PS1=%GATE_DIR%assert_ps1_syntax.ps1"
set "SETUP_PS1=%GATE_DIR%a14_h3e1b1_display_linkage_setup.ps1"
set "LEG_PS1=%GATE_DIR%a14_h3e1b1_display_linkage_leg.ps1"
set "WRAPPER_PS1=%GATE_DIR%a14_h3e1b2b_candidate_archive_qualification.ps1"

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%ASSERT_PS1%" -Path "%SETUP_PS1%"
if errorlevel 1 exit /b %errorlevel%

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%ASSERT_PS1%" -Path "%LEG_PS1%"
if errorlevel 1 exit /b %errorlevel%

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%ASSERT_PS1%" -Path "%WRAPPER_PS1%"
if errorlevel 1 exit /b %errorlevel%

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%WRAPPER_PS1%" %*
exit /b %errorlevel%
