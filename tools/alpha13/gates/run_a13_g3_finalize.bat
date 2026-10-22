@echo off
setlocal
python -B -c "import ast,sys;ast.parse(open(sys.argv[1],encoding='utf-8').read());print('G3_FINALIZE_PY_SYNTAX=PASS')" "%~dp0a13_g3_finalize.py"
if errorlevel 1 exit /b 2
python -B "%~dp0a13_g3_finalize.py"
exit /b %errorlevel%
