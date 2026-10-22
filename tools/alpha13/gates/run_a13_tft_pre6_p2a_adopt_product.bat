@echo off
setlocal

where pwsh >nul 2>nul
if errorlevel 1 (
  echo A13_PWSH_REQUIRED=YES
  exit /b 2
)

where python >nul 2>nul
if errorlevel 1 (
  echo A13_PYTHON_REQUIRED=YES
  exit /b 3
)

for /f "delims=" %%V in ('pwsh -NoProfile -Command "$PSVersionTable.PSVersion.ToString()"') do set "A13_PWSH_VERSION=%%V"
echo A13_PWSH=pwsh
echo A13_PWSH_VERSION=%A13_PWSH_VERSION%

pwsh -NoProfile -ExecutionPolicy Bypass -Command "$files=@('%~dp0a13_tft_pre6_p2a_adopt_product.ps1'); foreach($f in $files){$t=$null;$e=$null;$null=[System.Management.Automation.Language.Parser]::ParseFile($f,[ref]$t,[ref]$e);if($e.Count -ne 0){Write-Host ('A13_TFT_PRE6_P2A_PWSH_SYNTAX=FAIL FILE=' + $f);foreach($err in $e){Write-Host $err.Message};exit 2}};Write-Host 'A13_TFT_PRE6_P2A_PWSH_SYNTAX=PASS'"
if errorlevel 1 exit /b %errorlevel%

python -B -c "import ast,pathlib,sys; ast.parse(pathlib.Path(sys.argv[1]).read_text(encoding='utf-8')); print('A13_TFT_PRE6_P2A_PY_SYNTAX=PASS')" "%~dp0a13_tft_pre6_p2a_adopt_product.py"
if errorlevel 1 exit /b %errorlevel%

pwsh -NoProfile -ExecutionPolicy Bypass -File "%~dp0a13_tft_pre6_p2a_adopt_product.ps1" %*
exit /b %errorlevel%
