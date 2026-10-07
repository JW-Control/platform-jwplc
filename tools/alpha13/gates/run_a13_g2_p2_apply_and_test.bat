@echo off
setlocal

where pwsh >nul 2>nul
if errorlevel 1 (
  echo A13_PWSH_REQUIRED=YES
  echo REQUIRED=PowerShell Core 7+
  exit /b 2
)

for /f "delims=" %%V in ('pwsh -NoProfile -Command "$PSVersionTable.PSVersion.ToString()"') do set "A13_PWSH_VERSION=%%V"
echo A13_PWSH=pwsh
echo A13_PWSH_VERSION=%A13_PWSH_VERSION%
pwsh -NoProfile -ExecutionPolicy Bypass -Command "$files=@('%~dp0common.ps1','%~dp0apply_a13_g2_p2_candidate.ps1','%~dp0a13_g2_p2_tca_startup_candidate.ps1'); foreach($f in $files){$t=$null;$e=$null; $null=[System.Management.Automation.Language.Parser]::ParseFile($f,[ref]$t,[ref]$e); if($e.Count -ne 0){Write-Host ('SYNTAX_FAIL=' + $f); foreach($err in $e){Write-Host $err.Message}; exit 2}}; Write-Host 'A13_COMBINED_SYNTAX=PASS'"
if errorlevel 1 exit /b %errorlevel%

call "%~dp0run_a13_g2_p2_apply_candidate.bat"
if errorlevel 1 exit /b %errorlevel%

call "%~dp0run_a13_g2_p2_tca_startup_candidate.bat"
exit /b %errorlevel%
