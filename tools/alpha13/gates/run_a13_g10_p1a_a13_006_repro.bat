@echo off
setlocal
cd /d "%~dp0\..\..\.."
if errorlevel 1 exit /b 2
python -B "tools\alpha13\gates\a13_g10_p1a_a13_006_repro.py" --source-only
exit /b %ERRORLEVEL%
