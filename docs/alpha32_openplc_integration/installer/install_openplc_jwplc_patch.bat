@echo off
setlocal

set SCRIPT_DIR=%~dp0
set PS_SCRIPT=%SCRIPT_DIR%install_openplc_jwplc_patch.ps1

if not exist "%PS_SCRIPT%" (
    echo No se encontro el script PowerShell:
    echo %PS_SCRIPT%
    pause
    exit /b 1
)

echo ==============================================
echo  JWPLC Basic - OpenPLC Editor v4 Patch Installer
echo ==============================================
echo.

powershell -NoProfile -ExecutionPolicy Bypass -File "%PS_SCRIPT%"

endlocal
