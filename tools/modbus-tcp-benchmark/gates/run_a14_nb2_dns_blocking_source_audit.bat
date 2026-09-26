@echo off
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0a14_nb2_dns_blocking_source_audit.ps1" %*
exit /b %ERRORLEVEL%
