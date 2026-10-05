@echo off
setlocal

set MHZ=%~1
if "%MHZ%"=="" set MHZ=26

set CHUNKS=%~2
if "%CHUNKS%"=="" set CHUNKS=8

set PAIRS=%~3
if "%PAIRS%"=="" set PAIRS=12

set TIMEOUT_MS=%~4
if "%TIMEOUT_MS%"=="" set TIMEOUT_MS=200

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0g3_stop_timeout_ab_diag.ps1" -MHz %MHZ% -ExpectedChunks %CHUNKS% -Pairs %PAIRS% -TimeoutMs %TIMEOUT_MS%
exit /b %errorlevel%
