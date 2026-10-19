#Requires -Version 5.1
<#
JWPLC Alpha13 H0-F1: kit versionado. Ejecutar desde la raiz platform-jwplc.
Predeterminado: BUILD SIN HARDWARE. -UploadCOM4: solo COM4, con MAC y consentimiento.
#>
[CmdletBinding()]
param(
  [Parameter(Mandatory=$true)][ValidatePattern('^[a-fA-F0-9]{40}$')][string]$ExpectedHead,
  [switch]$UploadCOM4,
  [string]$ExpectedMac = '',
  [string]$Cli = 'arduino-cli'
)
$ErrorActionPreference = 'Stop'
$FQBN = 'jwplc_local:esp32:jwplcbasic'
$PORT = 'COM4'
$CONSENT = 'H0F1 AUTORIZO SOLO COM4 PACKAGE CORREGIDO SIN BACKUP'
$F1_MERGE = 'f2c6821566fa62df744c3af8d4d70a440994e64f'
function Fail([string]$why) { throw ('H0F1_REVIEW_STOP=' + $why) }
function Run-Checked([string]$exe,[string[]]$argv,[string]$log='') {
  $old = $ErrorActionPreference
  $ErrorActionPreference = 'Continue'
  try {
    $out = (& $exe @argv 2>&1 | Out-String)
    $rc = $LASTEXITCODE
  } finally { $ErrorActionPreference = $old }
  if ($log) { [IO.File]::WriteAllText($log,$out,[Text.Encoding]::UTF8) }
  if ($rc -ne 0) { Fail ('COMMAND_FAILED_'+$exe+' '+($argv -join ' ')+' EXIT='+$rc+' '+$out.Substring([Math]::Max(0,$out.Length-1200))) }
  return $out
}
if ($env:OS -ne 'Windows_NT') { Fail 'WINDOWS_ONLY' }
$root = (Run-Checked 'git' @('rev-parse','--show-toplevel')).Trim()
$branch = (Run-Checked 'git' @('-C',$root,'branch','--show-current')).Trim()
if ($branch -cne 'v2.1.0-alpha.13/feature/cleanup-robustness') {
  # CI usa checkout de SHA (detached HEAD), permitido SOLO para compilacion sin hardware.
  if (!(($branch -eq '') -and (!$UploadCOM4))) { Fail 'WRONG_ALPHA13_BRANCH' }
}
$head = (Run-Checked 'git' @('-C',$root,'rev-parse','HEAD')).Trim()
if ($head -cne $ExpectedHead) { Fail 'HEAD_MISMATCH' }
$null = Run-Checked 'git' @('-C',$root,'merge-base','--is-ancestor',$F1_MERGE,'HEAD')
if ((Run-Checked 'git' @('-C',$root,'status','--porcelain')).Trim()) { Fail 'REPO_DIRTY' }
$source = Join-Path $root 'JWPLC\2.1.0'
$w5100 = Join-Path $source 'libraries\JWPLC_Ethernet\src\utility\w5100.cpp'
$body = [IO.File]::ReadAllText($w5100)
if (!$body.Contains('#define PIN_SPI_SS_ETHERNET_LIB JWPLC_ETH_CS') -or
    !$body.Contains('#error "SS_PIN_DEFAULT apunta a GPIO6-11')) { Fail 'PACKAGE_F1_NOT_PRESENT' }
if ($UploadCOM4 -and $ExpectedMac -notmatch '^([a-fA-F0-9]{2}:){5}[a-fA-F0-9]{2}$') { Fail 'EXPECTED_MAC_REQUIRED' }
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ino = Join-Path $scriptDir 'A13_H0_F1_SMOKE.ino'
if (!(Test-Path -LiteralPath $ino)) { Fail 'SMOKE_SKETCH_MISSING' }
$null = Run-Checked $Cli @('version')
$stage = Join-Path $env:TEMP ('jwplc_a13_h0f1_'+$head.Substring(0,12)+'_'+(Get-Date -Format 'yyyyMMddHHmmss'))
if (Test-Path -LiteralPath $stage) { Fail 'TEMP_COLLISION' }
$book = Join-Path $stage 'sketchbook'
$hw = Join-Path $book 'hardware\jwplc_local\esp32'
$cfgDir = Join-Path $stage 'config'
$build = Join-Path $stage 'build'
$sketch = Join-Path $stage 'A13_H0_F1_SMOKE'
New-Item -ItemType Directory -Path @((Split-Path -Parent $hw),$cfgDir,$build,$sketch) -Force | Out-Null
Copy-Item -LiteralPath $source -Destination $hw -Recurse -Force
Copy-Item -LiteralPath $ino -Destination $sketch -Force
$cfg = Join-Path $cfgDir 'arduino-cli.yaml'
$null = Run-Checked $Cli @('config','init','--dest-dir',$cfgDir,'--overwrite')
$null = Run-Checked $Cli @('--config-file',$cfg,'config','set','directories.user',$book)
# Comprobar la plataforma seleccionada sin depender de la forma textual de Windows
# (C:/ frente a C:\, comillas, slash final o diferencias de mayusculas).
# Fail-closed: se exige una unica propiedad runtime.platform.path, ruta valida
# y equivalencia canonica exacta con la copia aislada del package.
foreach ($essential in @('boards.txt','platform.txt',
    'libraries\JWPLC_Ethernet\src\utility\w5100.cpp')) {
  if (!(Test-Path -LiteralPath (Join-Path $hw $essential) -PathType Leaf)) {
    Fail ('ISOLATED_PACKAGE_COPY_INCOMPLETE: '+$essential)
  }
}
$props = Run-Checked $Cli @('--config-file',$cfg,'compile','--fqbn',$FQBN,'--show-properties',$sketch) (Join-Path $stage 'SHOW_PROPERTIES_LOCAL.log')
$platform = (Resolve-Path -LiteralPath $hw).ProviderPath
$platformLines = @($props -split '\r?\n' | Where-Object {
  $_.Trim() -match '^runtime\.platform\.path\s*='
})
if ($platformLines.Count -ne 1) {
  Write-Host ('H0F1_PLATFORM_EXPECTED='+$platform)
  Write-Host ('H0F1_PLATFORM_PROPERTY_COUNT='+$platformLines.Count)
  Fail 'RUNTIME_PLATFORM_PROPERTY_MISSING_OR_DUPLICATED'
}
$platformValue = ($platformLines[0] -split '=', 2)[1].Trim().Trim('"')
try {
  $selected = (Resolve-Path -LiteralPath ($platformValue.Replace('/','\')) -ErrorAction Stop).ProviderPath
} catch {
  Write-Host ('H0F1_PLATFORM_SELECTED_RAW='+$platformValue)
  Write-Host ('H0F1_PLATFORM_EXPECTED='+$platform)
  Fail 'RUNTIME_PLATFORM_SELECTED_PATH_NOT_FOUND'
}
$expectedCanonical = [IO.Path]::GetFullPath($platform).TrimEnd([char[]]@('\','/'))
$selectedCanonical = [IO.Path]::GetFullPath($selected).TrimEnd([char[]]@('\','/'))
if (!([string]::Equals($selectedCanonical,$expectedCanonical,
                       [StringComparison]::OrdinalIgnoreCase))) {
  Write-Host ('H0F1_PLATFORM_SELECTED='+$selectedCanonical)
  Write-Host ('H0F1_PLATFORM_EXPECTED='+$expectedCanonical)
  Fail 'WRONG_PLATFORM_SELECTED'
}
Write-Host 'H0F1_PLATFORM=PASS_ISOLATED_PACKAGE_PATH_CANONICAL'
$compile = Run-Checked $Cli @('--config-file',$cfg,'compile','--fqbn',$FQBN,
                              '--warnings','default','--build-path',$build,'-v',$sketch) (Join-Path $stage 'COMPILE.log')
$found = @($compile -split '\r?\n' | Where-Object { $_ -match '^Using library JWPLC_Ethernet\b' })
if ($found.Count -ne 1) { Fail 'ETHERNET_LIBRARY_AMBIGUOUS' }
$norm = ($found[0] -replace '\\','/').ToLowerInvariant()
$wanted = ((Join-Path $hw 'libraries\JWPLC_Ethernet') -replace '\\','/').ToLowerInvariant()
if (!$norm.Contains($wanted)) { Fail 'WRONG_ETHERNET_LIBRARY' }
$objs = @(Get-ChildItem -LiteralPath $build -Recurse -File -Filter 'w5100.cpp.o')
if ($objs.Count -ne 1) { Fail 'W5100_SOURCE_OBJECT_MISSING_OR_MULTIPLE' }
$dep = Join-Path $objs[0].DirectoryName 'w5100.cpp.d'
if (!(Test-Path -LiteralPath $dep)) { Fail 'W5100_DEP_MISSING' }
if (!([IO.File]::ReadAllText($dep)).Contains('w5100.cpp')) { Fail 'W5100_DEP_INVALID' }
$elfs = @(Get-ChildItem -LiteralPath $build -File -Filter '*.elf')
if ($elfs.Count -ne 1) { Fail 'ELF_MISSING_OR_MULTIPLE' }
$sourceSha = (Get-FileHash $w5100 -Algorithm SHA256).Hash
$stagedSha = (Get-FileHash (Join-Path $hw 'libraries\JWPLC_Ethernet\src\utility\w5100.cpp') -Algorithm SHA256).Hash
if ($sourceSha -cne $stagedSha) { Fail 'STAGED_SOURCE_HASH_MISMATCH' }
$manifest = [ordered]@{
  status = 'PASS_SOURCE_FIRST_BUILD_ONLY'
  head = $head
  branch = $branch
  port = 'COM4'
  fqbn = $FQBN
  source_sha256 = $sourceSha
  object_sha256 = (Get-FileHash $objs[0].FullName -Algorithm SHA256).Hash
  elf_sha256 = (Get-FileHash $elfs[0].FullName -Algorithm SHA256).Hash
  upload = 'NOT_RUN'
  hardware = 'NOT_TOUCHED'
}
$manifestPath = Join-Path $stage 'MANIFEST.json'
($manifest | ConvertTo-Json -Depth 5) | Set-Content -LiteralPath $manifestPath -Encoding UTF8
Write-Host "H0F1_BUILD=PASS_SOURCE_FIRST HEAD=$head" -ForegroundColor Green
Write-Host "H0F1_EVIDENCE=$manifestPath"
if (!$UploadCOM4) { Write-Host 'H0F1_UPLOAD=NOT_RUN COM14=NOT_TOUCHED'; return }
# Frontera fisica, solo COM4.
Write-Host 'CERRAR TODO MONITOR SERIAL antes de verificar COM4.'
$devices = @(Get-CimInstance Win32_PnPEntity | Where-Object {
  $_.Name -match '\(COM4\)' -and $_.PNPDeviceID -match 'VID_1A86[&]PID_7523'
})
if ($devices.Count -ne 1) { Fail 'COM4_CH340_VID_PID_UNVERIFIED' }
$macText = Run-Checked 'py' @('-m','esptool','--chip','esp32','--port',$PORT,
                             '--baud','115200','read_mac') (Join-Path $stage 'MAC_LOCAL.log')
if (!($macText -match '(?i)MAC:\s*([a-f0-9]{2}(?::[a-f0-9]{2}){5})')) { Fail 'MAC_NOT_READ' }
if ($Matches[1].ToLowerInvariant() -cne $ExpectedMac.ToLowerInvariant()) { Fail 'MAC_MISMATCH' }
$flashText = Run-Checked 'py' @('-m','esptool','--chip','esp32','--port',$PORT,
                               '--baud','115200','flash_id') (Join-Path $stage 'FLASH_LOCAL.log')
if ($flashText -notmatch '(?i)Detected flash size:\s*4MB\b') { Fail 'FLASH_NOT_4MB' }
Write-Host 'CONFIRMAR: JWPLC propio de banco, sin cargas ni tensiones externas en salidas Q, no instalado en maquinaria.'
Write-Host 'NO se guarda backup automatico. El firmware previo de COM4 puede perderse. COM14 NO se abre.'
$answer = Read-Host "Escribe exactamente: $CONSENT"
if ($answer.Trim() -cne $CONSENT) { Fail 'CONSENT_NOT_GIVEN' }
if ((Run-Checked 'git' @('-C',$root,'rev-parse','HEAD')).Trim() -cne $head) { Fail 'HEAD_CHANGED' }
if ((Run-Checked 'git' @('-C',$root,'status','--porcelain')).Trim()) { Fail 'REPO_CHANGED' }
$null = Run-Checked $Cli @('--config-file',$cfg,'upload','--fqbn',$FQBN,
                           '--port',$PORT,'--input-dir',$build) (Join-Path $stage 'UPLOAD_COM4.log')
$manifest.upload = 'UPLOAD_COM4_COMMAND_PASS_PHYSICAL_NOT_YET_VALIDATED'
$manifest.hardware = 'COM4_ONLY'
($manifest | ConvertTo-Json -Depth 5) | Set-Content -LiteralPath $manifestPath -Encoding UTF8
Write-Host 'H0F1_UPLOAD=PASS_COM4_ONLY COM14=NOT_TOUCHED' -ForegroundColor Green
Write-Host 'H0F1_NEXT=COM4_UART_115200_EARLY_LINK_PHY'
Write-Host "H0F1_EVIDENCE=$manifestPath"
