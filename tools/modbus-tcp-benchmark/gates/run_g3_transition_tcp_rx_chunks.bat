@echo off
setlocal

set MHZ=%~1
if "%MHZ%"=="" set MHZ=26

set FROM_CHUNKS=%~2
if "%FROM_CHUNKS%"=="" set FROM_CHUNKS=8

set TO_CHUNKS=%~3
if "%TO_CHUNKS%"=="" set TO_CHUNKS=6

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0g3_transition_tcp_rx_chunks.ps1" -ExpectedMHz %MHZ% -FromChunks %FROM_CHUNKS% -ToChunks %TO_CHUNKS%
exit /b %errorlevel%
