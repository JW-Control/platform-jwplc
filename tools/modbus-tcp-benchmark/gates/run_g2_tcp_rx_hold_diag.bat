@echo off
setlocal

if "%~1"=="" (
  echo Usage: %~nx0 ^<14^|20^|24^|26^|30^>
  exit /b 2
)

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0g2_tcp_rx_hold_diag.ps1" -MHz %~1
exit /b %errorlevel%
