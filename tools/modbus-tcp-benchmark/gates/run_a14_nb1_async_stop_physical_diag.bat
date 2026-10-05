@echo off
setlocal

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0a14_nb1_async_stop_physical_diag.ps1" -MHz 26 -ExpectedChunks 8 -Pairs 20
exit /b %errorlevel%
