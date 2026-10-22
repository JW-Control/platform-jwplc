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
pwsh -NoProfile -ExecutionPolicy Bypass -Command "$f='%~dp0apply_a13_g2_p2_candidate.ps1'; $t=$null;$e=$null; $null=[System.Management.Automation.Language.Parser]::ParseFile($f,[ref]$t,[ref]$e); if($e.Count -ne 0){Write-Host 'SYNTAX_FAIL'; foreach($err in $e){Write-Host $err.Message}; exit 2}; Write-Host 'A13_APPLY_SYNTAX=PASS'"
if errorlevel 1 exit /b %errorlevel%

pwsh -NoProfile -ExecutionPolicy Bypass -File "%~dp0apply_a13_g2_p2_candidate.ps1"
exit /b %errorlevel%
