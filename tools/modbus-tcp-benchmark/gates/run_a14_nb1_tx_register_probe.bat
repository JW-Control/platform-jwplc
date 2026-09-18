@echo off
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0a14_nb1_tx_register_probe.ps1" %*
exit /b %ERRORLEVEL%
