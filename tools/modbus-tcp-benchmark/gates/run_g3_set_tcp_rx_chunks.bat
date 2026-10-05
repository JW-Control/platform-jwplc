@echo off
setlocal

set MHZ=%~1
if "%MHZ%"=="" set MHZ=26

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0g3_set_tcp_rx_chunks.ps1" -ExpectedMHz %MHZ% -FromChunks 4 -ToChunks 8
exit /b %errorlevel%
