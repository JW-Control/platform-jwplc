@echo off
setlocal

if "%~1"=="" (
  echo Usage: %~nx0 ^<14^|20^|24^|26^|30^>
  exit /b 2
)

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0g2_set_spi_frequency.ps1" -MHz %~1
if errorlevel 1 exit /b %errorlevel%

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0g2_validate_frequency.ps1" -MHz %~1
exit /b %errorlevel%
