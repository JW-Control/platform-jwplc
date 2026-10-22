@echo off
setlocal
cd /d "%~dp0\..\..\.."
if errorlevel 1 exit /b 2
python.exe -B "tools\alpha13\gates\a13_g10_p1g_c2_s2a_socket_begin_lease.py"
exit /b %ERRORLEVEL%
