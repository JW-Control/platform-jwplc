@echo off
setlocal
title JWPLC HMI Designer - Generar e instalar la app de escritorio
cd /d "%~dp0"

echo ==========================================================
echo  JWPLC HMI Designer - Generar e instalar app de escritorio
echo ==========================================================
echo.
echo Este paso hace todo de una vez:
echo   1. Compila la app de escritorio (Electron) con la version
echo      actual del repositorio.
echo   2. La instala en %%LOCALAPPDATA%%\JWPLC\HMI Designer, con
echo      accesos en el Escritorio y el Menu Inicio, y la
echo      extension para Arduino IDE 2.
echo.
echo La primera vez descarga Electron (unos minutos).
echo.

where npm.cmd >nul 2>nul
if errorlevel 1 (
  echo ERROR: falta Node.js.
  echo   Instalalo desde https://nodejs.org ^(version LTS^)
  echo   o con:  winget install OpenJS.NodeJS.LTS
  echo   Luego cierra esta ventana y vuelve a ejecutar este archivo.
  echo.
  pause
  exit /b 1
)

echo [1/2] Compilando la app de escritorio...
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Build-JWPLC-HMI-Designer-Electron.ps1"
if errorlevel 1 (
  echo.
  echo ERROR: no se pudo compilar la app de escritorio. Revisa los mensajes de arriba.
  pause
  exit /b 1
)

echo.
echo [2/2] Instalando...
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install-JWPLC-HMI-Designer.ps1" -RequireElectron
if errorlevel 1 (
  echo.
  echo ERROR: la instalacion no finalizo correctamente. Revisa los mensajes de arriba.
  pause
  exit /b 1
)

echo.
echo Listo. Abre "JWPLC HMI Designer" desde el Escritorio o el Menu Inicio.
echo Si Arduino IDE 2 estaba abierto, cierralo y vuelve a abrirlo.
echo.
pause
endlocal
