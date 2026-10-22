@echo off
setlocal
cd /d "%~dp0\..\..\.."
if errorlevel 1 exit /b 2
rem Fase inicial: compose y detecta bloqueos. No Arduino, flash ni producto.
python.exe -B "tools\alpha13\gates\a13_g10_integrated.py"
exit /b %ERRORLEVEL%
