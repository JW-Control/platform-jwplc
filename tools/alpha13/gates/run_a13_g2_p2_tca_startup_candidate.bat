@echo off
setlocal

powershell -NoProfile -ExecutionPolicy Bypass -Command "$files=@('%~dp0common.ps1','%~dp0a13_g2_p2_tca_startup_candidate.ps1'); foreach($f in $files){$t=$null;$e=$null; $null=[System.Management.Automation.Language.Parser]::ParseFile($f,[ref]$t,[ref]$e); if($e.Count -ne 0){Write-Host ('SYNTAX_FAIL=' + $f); foreach($err in $e){Write-Host $err.Message}; exit 2}}; Write-Host 'A13_GATE_SYNTAX=PASS'"
if errorlevel 1 exit /b %errorlevel%

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0a13_g2_p2_tca_startup_candidate.ps1" %*
exit /b %errorlevel%
