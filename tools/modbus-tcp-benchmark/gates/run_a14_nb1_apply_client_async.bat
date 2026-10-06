@echo off
setlocal

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0a14_nb1_apply_client_async.ps1"
exit /b %errorlevel%
