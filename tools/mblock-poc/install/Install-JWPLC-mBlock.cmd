@echo off
rem JWPLC Basic para mBlock 5: instala el JWPLC mBlock Bridge (ver Install-JWPLC-mBlock.ps1).
rem Opciones: -Extension (también los bloques), -NoAutostart, -Uninstall
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install-JWPLC-mBlock.ps1" %*
pause
