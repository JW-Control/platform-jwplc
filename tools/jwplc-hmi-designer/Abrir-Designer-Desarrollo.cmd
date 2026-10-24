@echo off
setlocal
title JWPLC HMI Designer - Desarrollo
cd /d "%~dp0electron"

rem Abre la app de escritorio directo desde el repositorio, sin compilar ni
rem instalar: sirve ..\poc y recarga la ventana al guardar un archivo del
rem Designer (conserva el diseno). F5 empieza limpio, F12 abre DevTools.
rem Si cambias electron\main.js o preload.cjs, cierra la app y vuelve a abrir
rem este archivo. La app instalada no se toca y puede estar abierta a la vez.

where npm.cmd >nul 2>nul
if errorlevel 1 (
  echo ERROR: falta Node.js.
  echo   Instalalo desde https://nodejs.org ^(version LTS^)
  echo   o con:  winget install OpenJS.NodeJS.LTS
  echo.
  pause
  exit /b 1
)

if not exist "node_modules\.bin\electron.cmd" (
  echo Primera vez: instalando Electron ^(unos minutos^)...
  call npm.cmd ci --no-audit --no-fund
  if errorlevel 1 (
    echo.
    echo ERROR: no se pudo instalar Electron. Revisa los mensajes de arriba.
    pause
    exit /b 1
  )
)

echo Abriendo JWPLC HMI Designer [desarrollo] desde:
echo   %~dp0poc
echo Cierra esta ventana o la app para terminar.
call "node_modules\.bin\electron.cmd" . %*
endlocal
