@echo off
setlocal

call "%~dp0run_a13_g2_p2_apply_candidate.bat"
if errorlevel 1 exit /b %errorlevel%

call "%~dp0run_a13_g2_p2_tca_startup_candidate.bat"
exit /b %errorlevel%
