@echo off
setlocal
set "GATES=%~dp0"

python "%GATES%a14_h3e5_existing_run_revalidation.py" %*
exit /b %errorlevel%
