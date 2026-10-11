@echo off
setlocal
python -B "%~dp0a13_g7c_adopt_local.py" %*
exit /b %ERRORLEVEL%
