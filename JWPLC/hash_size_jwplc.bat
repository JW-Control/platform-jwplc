@echo off
setlocal

rem Carpeta donde está este .bat
set "BASE=%~dp0"

rem Nombre del archivo: por argumento o por teclado
if "%~1"=="" (
    set /p "FILE=Ingresa el nombre del archivo ZIP: "
) else (
    set "FILE=%~1"
)

rem Si existe tal cual, úsalo. Si no, búscalo en la carpeta del .bat
if exist "%FILE%" (
    set "FULL=%FILE%"
) else (
    set "FULL=%BASE%%FILE%"
)

if not exist "%FULL%" (
    echo.
    echo [ERROR] No se encontro el archivo:
    echo %FULL%
    echo.
    pause
    exit /b 1
)

rem Nombre y tamaño
for %%A in ("%FULL%") do (
    set "NAME=%%~nxA"
    set "SIZE=%%~zA"
)

rem Obtener SHA256 con certutil
set "SHA="
for /f "skip=1 delims=" %%H in ('certutil -hashfile "%FULL%" SHA256 ^| findstr /v /c:"CertUtil"') do (
    if not defined SHA set "SHA=%%H"
)

rem Quitar espacios del hash
set "SHA=%SHA: =%"

rem Validacion
if not defined SHA (
    echo.
    echo [ERROR] No se pudo obtener el SHA256.
    echo Verifica que certutil este disponible y que el archivo no este dañado.
    echo.
    pause
    exit /b 1
)

echo.
echo           "archiveFileName": "%NAME%",
echo           "checksum": "SHA-256:%SHA%",
echo           "size": "%SIZE%",
echo.

pause