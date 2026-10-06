@echo off
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0a14_nb2_apply_compile_dns_async.ps1" %*
exit /b %ERRORLEVEL%
