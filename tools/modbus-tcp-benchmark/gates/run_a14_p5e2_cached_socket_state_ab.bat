@echo off
setlocal

set "PREFLIGHT=%~dp0a14_p5e2_cached_socket_state_preflight.ps1"
set "P5E1=%~dp0run_a14_p5e1_unpaced_ceiling.bat"
set "VALIDATOR=%~dp0assert_ps1_syntax.ps1"

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%VALIDATOR%" -Path "%PREFLIGHT%"
if errorlevel 1 exit /b %errorlevel%

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%PREFLIGHT%"
if errorlevel 1 exit /b %errorlevel%

echo.
echo ============================================================
echo  A14 P5-E2 - RUN CANDIDATE AGAINST P5-E1 CHARACTERIZATION
echo ============================================================
call "%P5E1%" %*
if errorlevel 1 exit /b %errorlevel%

echo.
echo A14_P5E2_CACHED_SOCKET_STATE_CANDIDATE=RUN_COMPLETE
echo NEXT=RETURN_OUTPUT_TO_CHAT_FOR_GAIN_DECISION
exit /b 0
