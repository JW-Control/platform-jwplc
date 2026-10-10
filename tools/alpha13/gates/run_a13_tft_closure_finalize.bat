@echo off
setlocal
cd /d "%~dp0\..\..\.." || exit /b 1
where python >nul 2>nul || (
  echo ERROR=PYTHON_NOT_FOUND
  exit /b 2
)
python -B "%~dp0a13_tft_closure_finalize.py" %*
exit /b %ERRORLEVEL%
