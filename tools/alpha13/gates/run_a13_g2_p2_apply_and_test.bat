@echo off
setlocal

powershell -NoProfile -ExecutionPolicy Bypass -Command "$files=@('%~dp0common.ps1','%~dp0apply_a13_g2_p2_candidate.ps1','%~dp0a13_g2_p2_tca_startup_candidate.ps1'); foreach($f in $files){$t=$null;$e=$null; $null=[System.Management.Automation.Language.Parser]::ParseFile($f,[ref]$t,[ref]$e); if($e.Count -ne 0){Write-Host ('SYNTAX_FAIL=' + $f); foreach($err in $e){Write-Host $err.Message}; exit 2}}; Write-Host 'A13_COMBINED_SYNTAX=PASS'"
if errorlevel 1 exit /b %errorlevel%

call "%~dp0run_a13_g2_p2_apply_candidate.bat"
if errorlevel 1 exit /b %errorlevel%

call "%~dp0run_a13_g2_p2_tca_startup_candidate.bat"
exit /b %errorlevel%
