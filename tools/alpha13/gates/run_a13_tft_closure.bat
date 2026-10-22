@echo off
setlocal
where pwsh >nul 2>nul
if errorlevel 1 (
 echo A13_PWSH7_REQUIRED=YES
 exit /b 2
)
where python >nul 2>nul
if errorlevel 1 (
 echo A13_PYTHON_REQUIRED=YES
 exit /b 3
)
for /f "delims=" %%V in ('pwsh -NoProfile -Command "$PSVersionTable.PSVersion.Major"') do set "MAJOR=%%V"
if not "%MAJOR%"=="7" (
 echo A13_PWSH7_REQUIRED=YES
 exit /b 4
)
python -B -c "import ast,pathlib,sys; ast.parse(pathlib.Path(sys.argv[1]).read_text(encoding='utf-8'));print('A13_TFT_CLOSURE_PY_SYNTAX=PASS')" "%~dp0a13_tft_closure.py"
if errorlevel 1 exit /b %errorlevel%
python -B "%~dp0a13_tft_closure.py" %*
exit /b %errorlevel%
