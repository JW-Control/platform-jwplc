@echo off
setlocal
cd /d "%~dp0\..\..\.."
if errorlevel 1 exit /b 2
python.exe -B "tools\alpha13\gates\a13_g10_p1g_c2_s2b2b_outbound_connect.py"
exit /b %ERRORLEVEL%
