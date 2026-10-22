@echo off
setlocal
cd /d "%~dp0\..\..\.."
if errorlevel 1 exit /b 2
python -B "tools\alpha13\gates\a13_g8_p4_dual_plc_physical.py"
exit /b %ERRORLEVEL%
