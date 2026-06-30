@echo off
setlocal EnableExtensions EnableDelayedExpansion

rem ============================================================================
rem JWPLC Basic OpenPLC VPP builder - example version
rem Ubicacion recomendada:
rem   platform-jwplc\openplc-editor-installers\v4.2.7\build-jwplc-vpp.example.bat
rem
rem Uso recomendado:
rem   set "JWPLC_VPP_PRIVATE_KEY=H:\Mi unidad\02.JW CONTROL\12.OpenPLC\keys\jwcontrol-2026-private.pem"
rem   build-jwplc-vpp.example.bat
rem
rem Uso alternativo:
rem   build-jwplc-vpp.example.bat "H:\Mi unidad\02.JW CONTROL\12.OpenPLC\keys\jwcontrol-2026-private.pem"
rem   build-jwplc-vpp.example.bat "H:\Mi unidad\02.JW CONTROL\12.OpenPLC\keys\jwcontrol-2026-private.pem" --clean-installed
rem
rem Esta version NO contiene ruta privada fija. Para uso local puedes copiarla como:
rem   build-jwplc-vpp.bat
rem y ahi fijar KEY_PATH si lo prefieres. Ese archivo local debe quedar ignorado.
rem ============================================================================

set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"

set "KEY_ID=%JWPLC_VPP_KEY_ID%"
if "%KEY_ID%"=="" set "KEY_ID=jwcontrol-2026"

set "KEY_PATH=%JWPLC_VPP_PRIVATE_KEY%"
if not "%~1"=="" (
  if /I not "%~1"=="--clean-installed" set "KEY_PATH=%~1"
)

set "CLEAN_INSTALLED=0"
if /I "%~1"=="--clean-installed" set "CLEAN_INSTALLED=1"
if /I "%~2"=="--clean-installed" set "CLEAN_INSTALLED=1"

set "VPP_DIR=%ROOT%\vpp"
set "TOOLS_DIR=%ROOT%\tools"
set "OUT_DIR=%ROOT%\out"
set "INSTALLED_PACKAGE=%APPDATA%\open-plc-editor-jwplc\packages\com.jwcontrol.jwplc-basic"

echo.
echo ============================================================
echo  JWPLC Basic OpenPLC VPP builder
echo ============================================================
echo Root:      "%ROOT%"
echo VPP dir:   "%VPP_DIR%"
echo Key ID:    "%KEY_ID%"
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
  echo Abre una terminal donde node -v funcione.
  exit /b 1
)

if "%KEY_PATH%"=="" (
  echo [ERROR] No se indico llave privada.
  echo.
  echo Opcion A:
  echo   set "JWPLC_VPP_PRIVATE_KEY=H:\Mi unidad\02.JW CONTROL\12.OpenPLC\keys\jwcontrol-2026-private.pem"
  echo   build-jwplc-vpp.example.bat
  echo.
  echo Opcion B:
  echo   build-jwplc-vpp.example.bat "H:\Mi unidad\02.JW CONTROL\12.OpenPLC\keys\jwcontrol-2026-private.pem"
  exit /b 1
)

if not exist "%KEY_PATH%" (
  echo [ERROR] No se encontro la llave privada:
  echo "%KEY_PATH%"
  exit /b 1
)

for /f "usebackq delims=" %%V in (`powershell -NoProfile -ExecutionPolicy Bypass -Command "(Get-Content '%VPP_DIR%\manifest.json' -Raw | ConvertFrom-Json).package.version"`) do set "PKG_VERSION=%%V"
if "%PKG_VERSION%"=="" (
  echo [ERROR] No se pudo leer package.version desde manifest.json.
  exit /b 1
)

set "EXPECTED_UNSIGNED_VPP=%OUT_DIR%\jwplc-basic-openplc-%PKG_VERSION%.unsigned.vpp"

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

set "UNSIGNED_VPP=%EXPECTED_UNSIGNED_VPP%"
if not exist "%UNSIGNED_VPP%" (
  set "UNSIGNED_VPP="
  for /f "delims=" %%F in ('dir /b /a-d "%OUT_DIR%\*.unsigned.vpp" 2^>nul') do (
    if "!UNSIGNED_VPP!"=="" set "UNSIGNED_VPP=%OUT_DIR%\%%F"
  )
)

if "%UNSIGNED_VPP%"=="" (
  echo [ERROR] No se encontro ningun *.unsigned.vpp en:
  echo "%OUT_DIR%"
  exit /b 1
)

set "SIGNED_VPP=%UNSIGNED_VPP:.unsigned.vpp=.jwcontrol-signed.vpp%"

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

if "%CLEAN_INSTALLED%"=="1" (
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

endlocal
exit /b 0
