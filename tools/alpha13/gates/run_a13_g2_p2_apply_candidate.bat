@echo off
setlocal

powershell -NoProfile -ExecutionPolicy Bypass -Command "$f='%~dp0apply_a13_g2_p2_candidate.ps1'; $t=$null;$e=$null; $null=[System.Management.Automation.Language.Parser]::ParseFile($f,[ref]$t,[ref]$e); if($e.Count -ne 0){Write-Host 'SYNTAX_FAIL'; foreach($err in $e){Write-Host $err.Message}; exit 2}; Write-Host 'A13_APPLY_SYNTAX=PASS'"
if errorlevel 1 exit /b %errorlevel%

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0apply_a13_g2_p2_candidate.ps1"
exit /b %errorlevel%
