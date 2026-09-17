@echo off
setlocal

if "%~1"=="" (
  echo Usage: %~nx0 ^<from_mhz^> ^<to_mhz^>
  exit /b 2
)

if "%~2"=="" (
  echo Usage: %~nx0 ^<from_mhz^> ^<to_mhz^>
  exit /b 2
)

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0g2_advance_frequency.ps1" -FromMHz %~1 -ToMHz %~2
exit /b %errorlevel%
