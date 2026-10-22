@echo off
setlocal
python -B "%~dp0a13_g3_p0_tca_rmw_baseline.py" %*
exit /b %errorlevel%
