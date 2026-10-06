@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0a14_nb1_apply_raw_async_stop.ps1"
exit /b %ERRORLEVEL%
