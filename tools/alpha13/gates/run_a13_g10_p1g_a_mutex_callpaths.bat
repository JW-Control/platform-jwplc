@echo off
setlocal
cd /d "%~dp0\..\..\.."
if errorlevel 1 exit /b 2
python -B "tools\alpha13\gates\a13_g10_p1g_a_mutex_callpaths.py" --selftest
if errorlevel 1 exit /b 2
python -B "tools\alpha13\gates\a13_g10_p1g_a_mutex_callpaths.py"
exit /b %ERRORLEVEL%
