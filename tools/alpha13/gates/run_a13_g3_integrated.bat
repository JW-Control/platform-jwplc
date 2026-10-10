@echo off
setlocal
python -B "%~dp0a13_g3_integrated.py" %*
exit /b %errorlevel%
