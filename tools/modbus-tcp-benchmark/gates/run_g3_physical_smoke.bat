@echo off
setlocal

if "%~1"=="" (
  echo Usage: %~nx0 ^<14^|20^|24^|26^|30^> [chunks]
  exit /b 2
)

set CHUNKS=%~2
if "%CHUNKS%"=="" set CHUNKS=8

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0g3_physical_smoke.ps1" -MHz %~1 -ExpectedChunks %CHUNKS%
exit /b %errorlevel%
