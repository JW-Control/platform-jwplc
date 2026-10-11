@echo off
setlocal
set "PYTHON_EXE=%LOCALAPPDATA%\Programs\Python\Python311\python.exe"
if not exist "%PYTHON_EXE%" (
  echo G6_PYTHON311_MISSING=YES
  exit /b 2
)
"%PYTHON_EXE%" -B -c "import ast,sys;ast.parse(open(sys.argv[1],encoding='utf-8').read());print('G6_PYTHON_SYNTAX=PASS')" "%~dp0a13_g6_integrated.py"
if errorlevel 1 exit /b 2
"%PYTHON_EXE%" -B "%~dp0a13_g6_integrated.py" %*
exit /b %errorlevel%
