@echo off
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0a14_nb1_tcp_lifecycle_closure.ps1" %*
exit /b %ERRORLEVEL%
