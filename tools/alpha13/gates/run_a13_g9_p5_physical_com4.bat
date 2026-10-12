@echo off
setlocal
cd /d "%~dp0\..\..\.."
if errorlevel 1 exit /b 2
python -B "tools\alpha13\gates\a13_g9_p5_physical_com4.py"
exit /b %ERRORLEVEL%
