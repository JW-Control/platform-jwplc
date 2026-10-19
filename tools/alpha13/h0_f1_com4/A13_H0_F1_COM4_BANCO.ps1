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
$FQBN = 'jwplc_h0f1:esp32:jwplcbasic'  # ID exclusivo de la copia temporal; board/firmware JWPLC reales
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
$pathIdentity = Join-Path $scriptDir 'A13_H0_F1_PATH_IDENTITY.ps1'
if (!(Test-Path -LiteralPath $pathIdentity -PathType Leaf)) { Fail 'F132_PATH_IDENTITY_HELPER_MISSING' }
. $pathIdentity
$ino = Join-Path $scriptDir 'A13_H0_F1_SMOKE.ino'
if (!(Test-Path -LiteralPath $ino)) { Fail 'SMOKE_SKETCH_MISSING' }
$null = Run-Checked $Cli @('version')
$stage = Join-Path $env:TEMP ('jwplc_a13_h0f1_'+$head.Substring(0,12)+'_'+(Get-Date -Format 'yyyyMMddHHmmss'))
if (Test-Path -LiteralPath $stage) { Fail 'TEMP_COLLISION' }
$book = Join-Path $stage 'sketchbook'
$hw = Join-Path $book 'hardware\jwplc_h0f1\esp32'
$cfgDir = Join-Path $stage 'config'
$build = Join-Path $stage 'build'
$sketch = Join-Path $stage 'A13_H0_F1_SMOKE'
New-Item -ItemType Directory -Path @((Split-Path -Parent $hw),$cfgDir,$build,$sketch) -Force | Out-Null
Copy-Item -LiteralPath $source -Destination $hw -Recurse -Force
Copy-Item -LiteralPath $ino -Destination $sketch -Force
# F132: Boards Manager eclipsa un platform manual con el mismo vendor:architecture.
# Se monta la MISMA copia del package con vendor temporal exclusivo; no se edita
# boards.txt, platform.txt, las librerias, ni la instalacion global Arduino15.
# El FQBN se usa solo en build/upload del smoke, no es un board distribuible.
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
$expectedCanonical = Get-H0F1CanonicalPath $platform
$selectedCanonical = Get-H0F1CanonicalPath $selected
if (!([string]::Equals($selectedCanonical,$expectedCanonical,
                       [StringComparison]::OrdinalIgnoreCase))) {
  Write-Host ('H0F1_PLATFORM_SELECTED='+$selectedCanonical)
  Write-Host ('H0F1_PLATFORM_EXPECTED='+$expectedCanonical)
  Fail 'WRONG_PLATFORM_SELECTED'
}
# La plataforma de placa, core y variant debe venir del staging, no de Arduino15.
foreach ($property in @('build.board.platform.path','build.core.platform.path',
                        'build.variant.platform.path')) {
  $matched = @($props -split '\r?\n' | Where-Object {
    $_.Trim() -match ('^'+[regex]::Escape($property)+'\s*=')
  })
  if ($matched.Count -gt 1) { Fail ('DUPLICATED_PROPERTY_'+$property) }
  if ($matched.Count -eq 1) {
    $raw = ($matched[0] -split '=', 2)[1].Trim().Trim('"')
    if (!$raw) { Fail ('EMPTY_PROPERTY_'+$property) }
    try {
      $p = (Resolve-Path -LiteralPath ($raw.Replace('/','\')) -ErrorAction Stop).ProviderPath
      $canon = Get-H0F1CanonicalPath $p
    } catch { Fail ('UNRESOLVED_PROPERTY_'+$property) }
    if (!([string]::Equals($canon,$expectedCanonical,[StringComparison]::OrdinalIgnoreCase))) {
      Write-Host ('H0F1_WRONG_PROPERTY='+$property+' VALUE='+$canon)
      Fail 'NON_ISOLATED_PLATFORM_DEPENDENCY'
    }
  }
}
Write-Host 'H0F1_PLATFORM=PASS_ISOLATED_PACKAGE_PATH_CANONICAL'
Write-Host 'H0F1_STAGING_VENDOR=jwplc_h0f1_ALIAS_JWPLC_SOURCE_UNMODIFIED'
# Prioridad máxima documentada por Arduino CLI: --library <ruta>.
# No confiar solo en la prioridad de librerías bundled si el vendor de
# staging cambia, aunque runtime.platform.path ya sea el esperado.
$stagedEthernet = Join-Path $hw 'libraries\JWPLC_Ethernet'
if (!(Test-Path -LiteralPath (Join-Path $stagedEthernet 'src\JWPLC_Ethernet.h') -PathType Leaf)) {
  Fail 'STAGED_ETHERNET_LIBRARY_INCOMPLETE'
}
$compile = Run-Checked $Cli @('--config-file',$cfg,'compile','--fqbn',$FQBN,
                              '--warnings','default','--library',$stagedEthernet,
                              '--build-path',$build,'-v',$sketch) (Join-Path $stage 'COMPILE.log')
$found = @($compile -split '\r?\n' | Where-Object { $_ -match '^Using library JWPLC_Ethernet\b' })
if ($found.Count -ne 1) { Fail 'ETHERNET_LIBRARY_AMBIGUOUS' }
# Arduino CLI puede usar el alias NTFS 8.3 (RUNNER~1) para una misma
# libreria cuyo log muestra el nombre largo. No comparar con Contains().
$selectedLibLine = ($found[0] -replace '^.+? in folder:\s*','').Trim().Trim('"')
$selectedLibCanonical = $null
try {
  $selectedLibCanonical = Get-H0F1CanonicalPath $selectedLibLine
} catch {
  Write-Host ('H0F1_ETHERNET_SELECTED_RAW='+$selectedLibLine)
  Fail 'ETHERNET_LIBRARY_PATH_MISSING_OR_INVALID'
}
$wantedLibCanonical = Get-H0F1CanonicalPath $stagedEthernet
if (!([string]::Equals($selectedLibCanonical,$wantedLibCanonical,[StringComparison]::OrdinalIgnoreCase))) {
  Write-Host ('H0F1_ETHERNET_SELECTED='+$selectedLibCanonical)
  Write-Host ('H0F1_ETHERNET_EXPECTED='+$wantedLibCanonical)
  Fail 'WRONG_ETHERNET_LIBRARY'
}
Write-Host 'H0F1_ETHERNET=PASS_CANONICAL_SAME_STAGED_LIBRARY'
$objs = @(Get-ChildItem -LiteralPath $build -Recurse -File -Filter 'w5100.cpp.o')
if ($objs.Count -ne 1) { Fail 'W5100_SOURCE_OBJECT_MISSING_OR_MULTIPLE' }
# gcc -MMD puede generar w5100.cpp.d o w5100.cpp.o.d; exigir evidencia
# GCC real con referencia al fuente staged y rechazar .o heredados.
$deps = @(
  (Join-Path $objs[0].DirectoryName 'w5100.cpp.d'),
  (Join-Path $objs[0].DirectoryName 'w5100.cpp.o.d')
) | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf }
if ($deps.Count -ne 1) { Fail 'W5100_DEP_MISSING_OR_AMBIGUOUS' }
$depText = [IO.File]::ReadAllText($deps[0])
# GCC Makefile .d usa escapes para los espacios, los dos puntos de Windows,
# barras duplicadas y continuaciones de linea. Igual regla que F129.
$depNorm = $depText.Replace("`r`n","`n").Replace("\`n"," ").Replace('\ ',' ').Replace('\:',':').Replace('\\','\').Replace('\','/').ToLowerInvariant()
$sourceFile = Join-Path $stagedEthernet 'src\utility\w5100.cpp'
$sourceAlias = $sourceFile.Replace('\','/').ToLowerInvariant()
$sourceLong = (Get-H0F1CanonicalPath $sourceFile).Replace('\','/').ToLowerInvariant()
if (!($depNorm.Contains($sourceAlias) -or $depNorm.Contains($sourceLong))) {
  Fail 'W5100_SOURCE_DEPENDENCY_NOT_FROM_STAGING'
}
Write-Host 'H0F1_W5100=PASS_GCC_OBJECT_DEPENDENCY_FROM_STAGING'
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
