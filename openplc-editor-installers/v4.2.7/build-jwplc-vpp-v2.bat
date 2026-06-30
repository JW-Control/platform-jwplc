@echo off
setlocal EnableExtensions EnableDelayedExpansion

rem ============================================================================
rem JWPLC Basic OpenPLC VPP builder - LOCAL
rem Ubicacion:
rem   platform-jwplc\openplc-editor-installers\v4.2.7\build-jwplc-vpp.bat
rem
rem Uso normal:
rem   build-jwplc-vpp.bat
rem
rem Por defecto:
rem   - Limpia vpp\signature.json
rem   - Limpia out\
rem   - Firma el VPP con jwcontrol-2026
rem   - Genera out\*.jwcontrol-signed.vpp
rem   - Limpia el paquete VPP instalado en rutas conocidas de AppData
rem   - Espera ESC antes de cerrar, incluso si hay error
rem
rem Opciones:
rem   build-jwplc-vpp.bat --no-clean-installed
rem ============================================================================

set "EXIT_CODE=0"

set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"

set "KEY_ID=jwcontrol-2026"
set "KEY_PATH=H:\Mi unidad\02.JW CONTROL\12.OpenPLC\keys\jwcontrol-2026-private.pem"

set "CLEAN_INSTALLED=1"
if /I "%~1"=="--no-clean-installed" set "CLEAN_INSTALLED=0"
if /I "%~2"=="--no-clean-installed" set "CLEAN_INSTALLED=0"

set "VPP_DIR=%ROOT%\vpp"
set "TOOLS_DIR=%ROOT%\tools"
set "OUT_DIR=%ROOT%\out"
set "PACKAGE_ID=com.jwcontrol.jwplc-basic"

echo.
echo ============================================================
echo  JWPLC Basic OpenPLC VPP builder
echo ============================================================
echo Root:       "%ROOT%"
echo VPP dir:    "%VPP_DIR%"
echo Key ID:     "%KEY_ID%"
echo Clean app:  "%CLEAN_INSTALLED%"
echo.

if not exist "%VPP_DIR%\manifest.json" (
  echo [ERROR] No se encontro "%VPP_DIR%\manifest.json".
  echo Ejecuta este .bat desde openplc-editor-installers\v4.2.7.
  set "EXIT_CODE=1"
  goto finish
)

if not exist "%TOOLS_DIR%\sign-vpp-package.mjs" (
  echo [ERROR] No se encontro "%TOOLS_DIR%\sign-vpp-package.mjs".
  set "EXIT_CODE=1"
  goto finish
)

if not exist "%TOOLS_DIR%\build-unsigned-vpp.ps1" (
  echo [ERROR] No se encontro "%TOOLS_DIR%\build-unsigned-vpp.ps1".
  set "EXIT_CODE=1"
  goto finish
)

where node >nul 2>nul
if errorlevel 1 (
  echo [ERROR] Node.js no esta disponible en PATH.
  echo Abre una terminal donde node -v funcione.
  set "EXIT_CODE=1"
  goto finish
)

if not exist "%KEY_PATH%" (
  echo [ERROR] No se encontro la llave privada:
  echo "%KEY_PATH%"
  echo.
  echo Edita KEY_PATH dentro de este .bat si moviste la llave.
  set "EXIT_CODE=1"
  goto finish
)

for /f "usebackq delims=" %%V in (`powershell -NoProfile -ExecutionPolicy Bypass -Command "(Get-Content '%VPP_DIR%\manifest.json' -Raw | ConvertFrom-Json).package.version"`) do set "PKG_VERSION=%%V"
if "%PKG_VERSION%"=="" (
  echo [ERROR] No se pudo leer package.version desde manifest.json.
  set "EXIT_CODE=1"
  goto finish
)

set "EXPECTED_UNSIGNED_VPP=%OUT_DIR%\jwplc-basic-openplc-%PKG_VERSION%.unsigned.vpp"

echo [1/7] Version de Node:
node -v
if errorlevel 1 (
  set "EXIT_CODE=1"
  goto finish
)

echo.
echo [2/7] Limpiando firma y salida anterior...
if exist "%VPP_DIR%\signature.json" del /f /q "%VPP_DIR%\signature.json"
if exist "%OUT_DIR%" rmdir /s /q "%OUT_DIR%"

echo.
echo [3/7] Firmando paquete VPP con %KEY_ID%...
node "%TOOLS_DIR%\sign-vpp-package.mjs" "%VPP_DIR%" "%KEY_ID%" "%KEY_PATH%"
if errorlevel 1 (
  echo [ERROR] Fallo la firma del VPP.
  set "EXIT_CODE=1"
  goto finish
)

if not exist "%VPP_DIR%\signature.json" (
  echo [ERROR] No se genero "%VPP_DIR%\signature.json".
  set "EXIT_CODE=1"
  goto finish
)

echo.
echo [4/7] Construyendo VPP...
powershell -NoProfile -ExecutionPolicy Bypass -File "%TOOLS_DIR%\build-unsigned-vpp.ps1"
if errorlevel 1 (
  echo [ERROR] Fallo la construccion del VPP.
  set "EXIT_CODE=1"
  goto finish
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
  set "EXIT_CODE=1"
  goto finish
)

set "SIGNED_VPP=%UNSIGNED_VPP:.unsigned.vpp=.jwcontrol-signed.vpp%"

echo.
echo [5/7] Copiando salida final firmada...
copy /y "%UNSIGNED_VPP%" "%SIGNED_VPP%" >nul
if errorlevel 1 (
  echo [ERROR] No se pudo crear:
  echo "%SIGNED_VPP%"
  set "EXIT_CODE=1"
  goto finish
)

echo.
echo [6/7] Verificando salida...
if not exist "%SIGNED_VPP%" (
  echo [ERROR] No existe el VPP firmado final.
  set "EXIT_CODE=1"
  goto finish
)

powershell -NoProfile -ExecutionPolicy Bypass -Command "if (Select-String -Path '%VPP_DIR%\signature.json' -Pattern '%KEY_ID%' -Quiet) { Write-Host '[OK] signature.json contiene %KEY_ID%' } else { Write-Error 'signature.json no contiene %KEY_ID%'; exit 1 }"
if errorlevel 1 (
  set "EXIT_CODE=1"
  goto finish
)

echo.
echo [7/7] Limpieza de paquetes instalados...
if "%CLEAN_INSTALLED%"=="1" (
  call :cleanKnownPackagePaths
) else (
  echo [INFO] Limpieza omitida por --no-clean-installed.
)

goto finish

:cleanKnownPackagePaths
call :deletePackage "%APPDATA%\open-plc-editor-jwplc\packages\%PACKAGE_ID%"
call :deletePackage "%APPDATA%\open-plc-editor\packages\%PACKAGE_ID%"
call :deletePackage "%APPDATA%\OpenPLC Editor\packages\%PACKAGE_ID%"
call :deletePackage "%APPDATA%\OpenPLC Editor - JWPLC Edition\packages\%PACKAGE_ID%"
call :deletePackage "%LOCALAPPDATA%\open-plc-editor-jwplc\packages\%PACKAGE_ID%"
call :deletePackage "%LOCALAPPDATA%\open-plc-editor\packages\%PACKAGE_ID%"
call :deletePackage "%LOCALAPPDATA%\OpenPLC Editor\packages\%PACKAGE_ID%"
call :deletePackage "%LOCALAPPDATA%\OpenPLC Editor - JWPLC Edition\packages\%PACKAGE_ID%"
exit /b 0

:deletePackage
set "TARGET=%~1"
if exist "%TARGET%" (
  rmdir /s /q "%TARGET%"
  if exist "%TARGET%" (
    echo [WARN] No se pudo eliminar: "%TARGET%"
  ) else (
    echo [OK] Eliminado: "%TARGET%"
  )
) else (
  echo [OK] No existe: "%TARGET%"
)
exit /b 0

:finish
echo.
echo ============================================================
if "%EXIT_CODE%"=="0" (
  echo  VPP generado correctamente
  if defined SIGNED_VPP echo  "%SIGNED_VPP%"
) else (
  echo  VPP finalizado con errores
)
echo ============================================================
echo.
echo Presiona ESC para cerrar esta ventana...

call :waitEsc

endlocal & exit /b %EXIT_CODE%

:waitEsc
powershell -NoProfile -ExecutionPolicy Bypass -Command "$Host.UI.RawUI.FlushInputBuffer(); do { $key = $Host.UI.RawUI.ReadKey('NoEcho,IncludeKeyDown') } until ($key.VirtualKeyCode -eq 27)"
if errorlevel 1 (
  echo.
  echo No se pudo capturar ESC con PowerShell. Presiona cualquier tecla para cerrar...
  pause >nul
)
exit /b 0
