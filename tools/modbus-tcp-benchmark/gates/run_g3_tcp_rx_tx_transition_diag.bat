@echo off
setlocal

set MHZ=%~1
if "%MHZ%"=="" set MHZ=26

set CHUNKS=%~2
if "%CHUNKS%"=="" set CHUNKS=8

set PAIRS=%~3
if "%PAIRS%"=="" set PAIRS=12

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0g3_tcp_rx_tx_transition_diag.ps1" -MHz %MHZ% -ExpectedChunks %CHUNKS% -Pairs %PAIRS%
exit /b %errorlevel%
