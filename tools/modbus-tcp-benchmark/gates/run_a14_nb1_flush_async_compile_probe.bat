@echo off
setlocal
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0a14_nb1_flush_async_compile_probe.ps1"
exit /b %errorlevel%
