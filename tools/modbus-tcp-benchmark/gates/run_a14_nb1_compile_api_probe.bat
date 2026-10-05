@echo off
setlocal
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0a14_nb1_compile_api_probe.ps1"
set "RC=%ERRORLEVEL%"
endlocal & exit /b %RC%
