@echo off
setlocal
title JWPLC HMI Designer - Instalador Alpha11

echo ================================================
echo  JWPLC HMI Designer - Instalador Alpha11
echo ================================================
echo.
echo Se instalara:
echo   - JWPLC HMI Designer en %%LOCALAPPDATA%%\JWPLC\HMI Designer
echo   - Extension JWPLC HMI para Arduino IDE 2 en %%USERPROFILE%%\.arduinoIDE\plugins
echo   - Accesos en Escritorio y Menu Inicio
echo.

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install-JWPLC-HMI-Designer.ps1"
if errorlevel 1 (
  echo.
  echo ERROR: la instalacion no finalizo correctamente.
  pause
  exit /b 1
)

echo.
echo Instalacion completada: aplicacion + extension Arduino IDE 2.
echo Cierra completamente Arduino IDE 2 y vuelve a abrirlo para cargar la extension.
echo.
pause
endlocal
