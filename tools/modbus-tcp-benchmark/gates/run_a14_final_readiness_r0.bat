@echo off
setlocal
set "GATES=%~dp0"
set "SCRIPT=%GATES%a14_final_readiness_r0.py"

python -c "import ast,pathlib,sys; p=pathlib.Path(sys.argv[1]); ast.parse(p.read_text(encoding='utf-8'), filename=str(p))" "%SCRIPT%"
if errorlevel 1 (
    echo PYTHON_SYNTAX=FAIL
    exit /b %errorlevel%
)
echo PYTHON_SYNTAX=PASS

python "%SCRIPT%"
exit /b %errorlevel%
