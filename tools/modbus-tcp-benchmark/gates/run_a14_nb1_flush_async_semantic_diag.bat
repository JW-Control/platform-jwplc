@echo off
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0a14_nb1_flush_async_semantic_diag.ps1" %*
exit /b %ERRORLEVEL%
