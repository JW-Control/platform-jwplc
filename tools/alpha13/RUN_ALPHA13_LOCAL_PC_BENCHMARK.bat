@echo off
setlocal
cd /d "%~dp0\..\.."
if errorlevel 1 (
  echo [REVIEW_STOP] No se pudo llegar al repositorio JWPLC.
  exit /b 2
)

echo ==============================================================
echo  JWPLC Alpha13 - Benchmark local de compilacion Arduino CLI
echo  Sin USB, COM, flash, upload ni modificaciones del package
echo ==============================================================
echo.

where arduino-cli >nul 2>&1
if errorlevel 1 (
  echo [REVIEW_STOP] arduino-cli no se encontro en PATH.
  echo Revisa tu instalacion de Arduino CLI 1.5.1.
  exit /b 2
)

where py >nul 2>&1
if not errorlevel 1 (
  set "PY=py -3"
) else (
  where python >nul 2>&1
  if errorlevel 1 (
    echo [REVIEW_STOP] Python 3 no esta disponible.
    exit /b 2
  )
  set "PY=python"
)

%PY% -B tools\alpha13\a13_local_pc_benchmark.py --check
if errorlevel 1 (
  echo [REVIEW_STOP] Preflight no superado. NO se lanzo benchmark.
  exit /b 2
)

echo.
echo [INFO] Inicio de los 3 pares por perfil, source y archive.
echo [INFO] Solo se trabaja sobre copias temporales del package.
echo.
%PY% -B tools\alpha13\a13_local_pc_benchmark.py
if errorlevel 1 (
  echo [REVIEW_STOP] Revisa RESULT_ROOT y MANIFEST.json para diagnostico.
  exit /b 2
)
echo.
echo [PASS] Comparacion local terminada. Conserva MANIFEST.json.
exit /b 0
