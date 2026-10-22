@echo off
setlocal
cd /d "%~dp0\..\..\.."
if errorlevel 1 exit /b 2
rem Usar el ejecutable con extension: Windows puede resolver "python"
rem a C:\Windows\System32\python (sin extension) y abrir dialogo "Abrir con".
python.exe -B "tools\alpha13\gates\a13_g10_p1g_c1_native_prototype.py"
exit /b %ERRORLEVEL%
