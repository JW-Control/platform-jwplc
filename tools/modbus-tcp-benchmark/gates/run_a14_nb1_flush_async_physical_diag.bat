@echo off
setlocal
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0a14_nb1_flush_async_physical_diag.ps1" %*
exit /b %ERRORLEVEL%
