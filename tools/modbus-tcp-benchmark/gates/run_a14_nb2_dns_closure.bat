@echo off
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0a14_nb2_dns_closure.ps1" %*
exit /b %ERRORLEVEL%
