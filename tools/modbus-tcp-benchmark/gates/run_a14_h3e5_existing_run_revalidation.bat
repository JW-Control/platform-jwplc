@echo off
setlocal
set "GATES=%~dp0"
set "SCRIPT=%GATES%a14_h3e5_existing_run_revalidation.py"

python -m py_compile "%SCRIPT%"
if errorlevel 1 (
    echo PYTHON_SYNTAX=FAIL
    exit /b %errorlevel%
)
echo PYTHON_SYNTAX=PASS

python "%SCRIPT%" %*
exit /b %errorlevel%
