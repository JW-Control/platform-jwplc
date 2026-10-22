@echo off
setlocal
cd /d "%~dp0\..\..\.."
if errorlevel 1 exit /b 2
python -B "tools\alpha13\gates\a13_g9_git_close.py" --confirm A13_G9_COMMIT
exit /b %ERRORLEVEL%
