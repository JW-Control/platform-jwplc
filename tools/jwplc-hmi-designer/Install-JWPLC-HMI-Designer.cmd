@echo off
setlocal
title JWPLC HMI Designer - Instalador Alpha11

echo ================================================
echo  JWPLC HMI Designer - Instalador Alpha11
echo ================================================
echo.
echo Se instalara en %%LOCALAPPDATA%%\JWPLC\HMI Designer
echo y se crearan accesos en Escritorio y Menu Inicio.
echo.

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install-JWPLC-HMI-Designer.ps1"
if errorlevel 1 (
  echo.
  echo ERROR: la instalacion no finalizo correctamente.
  pause
  exit /b 1
)

echo.
echo Instalacion principal completada.
echo.
set /p JWPLC_IDE="Instalar tambien el boton experimental en Arduino IDE 2? [S/N]: "
if /I "%JWPLC_IDE%"=="S" (
  powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install-ArduinoIDE-Launcher.ps1"
  if errorlevel 1 (
    echo.
    echo El Designer quedo instalado, pero el launcher de Arduino IDE fallo.
  )
)

echo.
echo Listo. Si instalaste el launcher Arduino IDE, reinicia completamente el IDE.
pause
endlocal
