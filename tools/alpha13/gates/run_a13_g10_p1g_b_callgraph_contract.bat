@echo off
setlocal
cd /d "%~dp0\..\..\.."
if errorlevel 1 exit /b 2
python -B "tools\alpha13\gates\a13_g10_p1g_b_callgraph_contract.py" --selftest
if errorlevel 1 exit /b 2
python -B "tools\alpha13\gates\a13_g10_p1g_b_callgraph_contract.py"
exit /b %ERRORLEVEL%
