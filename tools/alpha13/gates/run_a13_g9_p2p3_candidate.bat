@echo off
setlocal
cd /d "%~dp0\..\..\.."
if errorlevel 1 exit /b 2
python -B "tools\alpha13\gates\a13_g9_p2p3_candidate.py"
exit /b %ERRORLEVEL%
