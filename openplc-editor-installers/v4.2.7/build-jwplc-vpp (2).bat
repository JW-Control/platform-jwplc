@echo off
setlocal EnableExtensions EnableDelayedExpansion

rem ============================================================================
rem JWPLC Basic OpenPLC VPP builder
rem Ubicacion recomendada:
rem   platform-jwplc\openplc-editor-installers\v4.2.7\build-jwplc-vpp.bat
rem
rem Uso:
rem   build-jwplc-vpp.bat
rem   build-jwplc-vpp.bat --clean-installed
rem ============================================================================

set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"

set "KEY_ID=jwcontrol-2026"
set "KEY_PATH=H:\Mi unidad\02.JW CONTROL\12.OpenPLC\keys\jwcontrol-2026-private.pem"

set "VPP_DIR=%ROOT%\vpp"
set "TOOLS_DIR=%ROOT%\tools"
set "OUT_DIR=%ROOT%\out"

set "UNSIGNED_VPP=%OUT_DIR%\jwplc-basic-openplc-2.1.0-alpha.3.unsigned.vpp"
set "SIGNED_VPP=%OUT_DIR%\jwplc-basic-openplc-2.1.0-alpha.3.jwcontrol-signed.vpp"
set "INSTALLED_PACKAGE=%APPDATA%\open-plc-editor-jwplc\packages\com.jwcontrol.jwplc-basic"

echo.
echo ============================================================
echo  JWPLC Basic OpenPLC VPP builder
echo ============================================================
echo Root:      "%ROOT%"
echo VPP dir:   "%VPP_DIR%"
echo Key ID:    "%KEY_ID%"
echo Key path:  "%KEY_PATH%"
echo.

if not exist "%VPP_DIR%\manifest.json" (
  echo [ERROR] No se encontro "%VPP_DIR%\manifest.json".
  echo Ejecuta este .bat desde openplc-editor-installers\v4.2.7.
  exit /b 1
)

if not exist "%TOOLS_DIR%\sign-vpp-package.mjs" (
  echo [ERROR] No se encontro "%TOOLS_DIR%\sign-vpp-package.mjs".
  exit /b 1
)

if not exist "%TOOLS_DIR%\build-unsigned-vpp.ps1" (
  echo [ERROR] No se encontro "%TOOLS_DIR%\build-unsigned-vpp.ps1".
  exit /b 1
)

where node >nul 2>nul
if errorlevel 1 (
  echo [ERROR] Node.js no esta disponible en PATH.
  exit /b 1
)

if not exist "%KEY_PATH%" (
  echo [ERROR] No se encontro la llave privada:
  echo "%KEY_PATH%"
  echo Edita KEY_PATH dentro de este .bat si la moviste.
  exit /b 1
)

echo [1/6] Version de Node:
node -v
if errorlevel 1 exit /b 1

echo.
echo [2/6] Limpiando firma y salida anterior...
if exist "%VPP_DIR%\signature.json" del /f /q "%VPP_DIR%\signature.json"
if exist "%OUT_DIR%" rmdir /s /q "%OUT_DIR%"

echo.
echo [3/6] Firmando paquete VPP con %KEY_ID%...
node "%TOOLS_DIR%\sign-vpp-package.mjs" "%VPP_DIR%" "%KEY_ID%" "%KEY_PATH%"
if errorlevel 1 (
  echo [ERROR] Fallo la firma del VPP.
  exit /b 1
)

if not exist "%VPP_DIR%\signature.json" (
  echo [ERROR] No se genero "%VPP_DIR%\signature.json".
  exit /b 1
)

echo.
echo [4/6] Construyendo VPP...
powershell -NoProfile -ExecutionPolicy Bypass -File "%TOOLS_DIR%\build-unsigned-vpp.ps1"
if errorlevel 1 (
  echo [ERROR] Fallo la construccion del VPP.
  exit /b 1
)

if not exist "%UNSIGNED_VPP%" (
  echo [ERROR] No se encontro el VPP generado:
  echo "%UNSIGNED_VPP%"
  exit /b 1
)

echo.
echo [5/6] Copiando salida final firmada...
copy /y "%UNSIGNED_VPP%" "%SIGNED_VPP%" >nul
if errorlevel 1 (
  echo [ERROR] No se pudo crear:
  echo "%SIGNED_VPP%"
  exit /b 1
)

echo.
echo [6/6] Verificando salida...
if not exist "%SIGNED_VPP%" (
  echo [ERROR] No existe el VPP firmado final.
  exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -Command "if (Select-String -Path '%VPP_DIR%\signature.json' -Pattern '%KEY_ID%' -Quiet) { Write-Host '[OK] signature.json contiene %KEY_ID%' } else { Write-Error 'signature.json no contiene %KEY_ID%'; exit 1 }"
if errorlevel 1 exit /b 1

if /I "%~1"=="--clean-installed" (
  echo.
  echo [EXTRA] Eliminando paquete instalado en JWPLC Edition...
  if exist "%INSTALLED_PACKAGE%" (
    rmdir /s /q "%INSTALLED_PACKAGE%"
    echo [OK] Eliminado: "%INSTALLED_PACKAGE%"
  ) else (
    echo [OK] No habia paquete instalado que eliminar.
  )
)

echo.
echo ============================================================
echo  VPP generado correctamente:
echo  "%SIGNED_VPP%"
echo ============================================================
echo.
echo Siguiente paso:
echo  1. Cierra OpenPLC Editor - JWPLC Edition.
echo  2. Importa el VPP firmado generado arriba.
echo.

echo.
echo Presiona ESC para cerrar esta ventana...
powershell -NoProfile -ExecutionPolicy Bypass -Command "do { $key = [Console]::ReadKey($true) } until ($key.Key -eq 'Escape')"

endlocal
exit /b 0
