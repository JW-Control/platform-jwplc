@echo off
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0a14_nb3_apply_compile_bounded_socket_primitives.ps1" %*
exit /b %ERRORLEVEL%
