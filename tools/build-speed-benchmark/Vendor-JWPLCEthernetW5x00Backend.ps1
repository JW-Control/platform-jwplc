[CmdletBinding()]
param(
    [string]$Version = "2.0.2",
    [switch]$Force
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = [System.IO.Path]::GetFullPath((Join-Path $ScriptRoot "..\.."))
$PlatformRoot = Join-Path $RepoRoot "JWPLC\2.1.0"
$LibrariesRoot = Join-Path $PlatformRoot "libraries"
$TargetRoot = Join-Path $LibrariesRoot "JWPLC_Ethernet_W5x00_Backend"
$JwEthernetRoot = Join-Path $LibrariesRoot "JWPLC_Ethernet"
$JwEthernetHeader = Join-Path $JwEthernetRoot "src\JWPLC_Ethernet.h"
$JwEthernetProperties = Join-Path $JwEthernetRoot "library.properties"

if ($Version -ne "2.0.2")
{
    throw "Este vendorizador esta fijado y validado para Arduino Ethernet 2.0.2."
}

function Get-GitBlobSha1
{
    param([string]$Path)

    $bytes = [System.IO.File]::ReadAllBytes($Path)
    $prefix = [System.Text.Encoding]::UTF8.GetBytes(("blob {0}`0" -f $bytes.Length))
    $payload = New-Object byte[] ($prefix.Length + $bytes.Length)
    [System.Array]::Copy($prefix, 0, $payload, 0, $prefix.Length)
    [System.Array]::Copy($bytes, 0, $payload, $prefix.Length, $bytes.Length)

    $sha1 = [System.Security.Cryptography.SHA1]::Create()
    try
    {
        return (($sha1.ComputeHash($payload) | ForEach-Object { $_.ToString("x2") }) -join "")
    }
    finally
    {
        $sha1.Dispose()
    }
}

function Assert-UpstreamFile
{
    param(
        [string]$Root,
        [string]$RelativePath,
        [string]$ExpectedSha
    )

    $path = Join-Path $Root ($RelativePath -replace '/', '\')
    if (-not (Test-Path -LiteralPath $path))
    {
        throw "Falta archivo upstream: $RelativePath"
    }

    $actual = Get-GitBlobSha1 -Path $path
    if ($actual -ne $ExpectedSha)
    {
        throw ("SHA Git incorrecto para {0}. Esperado={1}, actual={2}" -f $RelativePath, $ExpectedSha, $actual)
    }

    Write-Host ("  OK {0} {1}" -f $ExpectedSha.Substring(0, 8), $RelativePath) -ForegroundColor DarkGreen
}

$expected = [ordered]@{
    "library.properties"      = "f3f361662b929523d9271bf5a45fed3e83736494"
    "AUTHORS"                 = "1faeec415934af16e085f837ff9755f15460b3f0"
    "src/Dhcp.cpp"            = "2bfd584bb29e3a59ebeb5a713cca1da6b7caee3d"
    "src/Dhcp.h"              = "43ec4f8531f3f4e5fcf3cd27577780e90e5e9b92"
    "src/Dns.cpp"             = "dca7ce42375d347e1b100d591b0648f414ad7d68"
    "src/Dns.h"               = "58f9d2c5384fc0b3a5005a1b3c3c1672aa749d6e"
    "src/Ethernet.cpp"        = "8d9ce7fd8c3ffe1d539743350f2bad976e3d06d8"
    "src/Ethernet.h"          = "0045de8815731a11bc1973c4ee099048a47a1a68"
    "src/EthernetClient.cpp"  = "5a20c74808ad419b9ef5e9bf140e25cfbc296313"
    "src/EthernetClient.h"    = "b5aef9602e457402bfcff9a98fdd08877bc64ac2"
    "src/EthernetServer.cpp"  = "ddebd154a487d5831e13f1d8a24251aacfc478a6"
    "src/EthernetServer.h"    = "b5aef9602e457402bfcff9a98fdd08877bc64ac2"
    "src/EthernetUdp.cpp"     = "e28791f6aa119b351cfbd374ccbc97b85fbccb95"
    "src/EthernetUdp.h"       = "16bb062e5c16032ac5b195167addefbfc7005ae8"
    "src/socket.cpp"          = "7dc83feb83ae0eed9f779ca85d77702e7cce14d6"
    "src/utility/w5100.cpp"   = "6e7dbd2033f18978614565464bb3c2fbc2e5b35d"
    "src/utility/w5100.h"     = "b2e8ec8378590e2b3f3fb39c41f1bd03d34b7490"
}

if (-not (Test-Path -LiteralPath $JwEthernetHeader)) { throw "No existe $JwEthernetHeader" }
if (-not (Test-Path -LiteralPath $JwEthernetProperties)) { throw "No existe $JwEthernetProperties" }

if (Test-Path -LiteralPath $TargetRoot)
{
    if (-not $Force)
    {
        throw "Ya existe $TargetRoot. Usa -Force solo si deseas regenerarlo desde upstream."
    }
    Remove-Item -LiteralPath $TargetRoot -Recurse -Force
}

$tempRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("jwplc-ethernet-vendor-" + [Guid]::NewGuid().ToString("N"))
$zipPath = Join-Path $tempRoot ("Ethernet-{0}.zip" -f $Version)
$extractRoot = Join-Path $tempRoot "extract"
New-Item -ItemType Directory -Path $tempRoot -Force | Out-Null

try
{
    $url = "https://github.com/arduino-libraries/Ethernet/archive/refs/tags/$Version.zip"
    Write-Host "JWPLC - vendor Arduino Ethernet W5x00 backend" -ForegroundColor Cyan
    Write-Host ("Upstream: arduino-libraries/Ethernet tag {0}" -f $Version)
    Write-Host ("Descargando: {0}" -f $url)

    Invoke-WebRequest -UseBasicParsing -Uri $url -OutFile $zipPath
    Expand-Archive -LiteralPath $zipPath -DestinationPath $extractRoot -Force

    $sourceRoot = Join-Path $extractRoot ("Ethernet-{0}" -f $Version)
    if (-not (Test-Path -LiteralPath $sourceRoot))
    {
        $sourceRoot = Get-ChildItem -LiteralPath $extractRoot -Directory | Select-Object -First 1 -ExpandProperty FullName
    }
    if (-not (Test-Path -LiteralPath $sourceRoot)) { throw "No se encontro el source root del ZIP upstream." }

    Write-Host ""
    Write-Host "[1/4] Verificando fuentes exactos del tag oficial..." -ForegroundColor Cyan
    foreach ($item in $expected.GetEnumerator())
    {
        Assert-UpstreamFile -Root $sourceRoot -RelativePath $item.Key -ExpectedSha $item.Value
    }

    Write-Host "[2/4] Creando backend vendorizado dentro del package..." -ForegroundColor Cyan
    New-Item -ItemType Directory -Path $TargetRoot -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $sourceRoot "src") -Destination $TargetRoot -Recurse -Force
    Copy-Item -LiteralPath (Join-Path $sourceRoot "AUTHORS") -Destination (Join-Path $TargetRoot "AUTHORS") -Force

    $properties = @"
name=JWPLC Ethernet W5x00 Backend
version=2.0.2
author=Various (see AUTHORS file for details)
maintainer=JW Control <jw.control.peru@gmail.com>
sentence=Backend W5x00 reproducible para JWPLC basado en Arduino Ethernet 2.0.2.
paragraph=Copia vendorizada y verificada del tag oficial arduino-libraries/Ethernet 2.0.2 para evitar que librerias Ethernet del sketchbook alteren el runtime JWPLC.
category=Communication
url=https://github.com/arduino-libraries/Ethernet
architectures=*
includes=Ethernet.h
"@
    $properties | Set-Content -LiteralPath (Join-Path $TargetRoot "library.properties") -Encoding ascii

    $marker = @"
#ifndef JWPLC_BUNDLED_ETHERNET_W5X00_H
#define JWPLC_BUNDLED_ETHERNET_W5X00_H
// Marker exclusivo del package JWPLC. Su inclusion fuerza a Arduino Builder
// a importar este backend antes de resolver Ethernet.h.
#endif
"@
    $marker | Set-Content -LiteralPath (Join-Path $TargetRoot "src\JWPLC_Bundled_Ethernet_W5x00.h") -Encoding ascii

    $upstreamDoc = New-Object System.Collections.Generic.List[string]
    $upstreamDoc.Add("# Upstream - Arduino Ethernet 2.0.2")
    $upstreamDoc.Add("")
    $upstreamDoc.Add("Fuente: https://github.com/arduino-libraries/Ethernet")
    $upstreamDoc.Add("Tag: 2.0.2")
    $upstreamDoc.Add("")
    $upstreamDoc.Add("Los archivos bajo src/ y AUTHORS se copiaron sin modificaciones desde el tag oficial y se verificaron por Git blob SHA-1 antes de vendorizarlos.")
    $upstreamDoc.Add("")
    $upstreamDoc.Add("library.properties fue adaptado unicamente para dar identidad unica al backend dentro del package JWPLC.")
    $upstreamDoc.Add("JWPLC_Bundled_Ethernet_W5x00.h es un marcador propio de JWPLC y no forma parte del upstream.")
    $upstreamDoc.Add("")
    $upstreamDoc.Add("## Git blob SHA-1 verificados")
    $upstreamDoc.Add("")
    foreach ($item in $expected.GetEnumerator())
    {
        if ($item.Key -eq "library.properties") { continue }
        $upstreamDoc.Add(('- `{0}`: `{1}`' -f $item.Key, $item.Value))
    }
    $upstreamDoc | Set-Content -LiteralPath (Join-Path $TargetRoot "UPSTREAM.md") -Encoding utf8

    Write-Host "[3/4] Fijando JWPLC_Ethernet al backend vendorizado..." -ForegroundColor Cyan
    $headerText = Get-Content -LiteralPath $JwEthernetHeader -Raw
    if ($headerText -notmatch '#include\s*<JWPLC_Bundled_Ethernet_W5x00\.h>')
    {
        $needle = '#include <IPAddress.h>'
        if ($headerText.IndexOf($needle, [System.StringComparison]::Ordinal) -lt 0)
        {
            throw "No se encontro el punto de insercion esperado en JWPLC_Ethernet.h"
        }
        $replacement = $needle + "`r`n#include <JWPLC_Bundled_Ethernet_W5x00.h>"
        $headerText = $headerText.Replace($needle, $replacement)
        Set-Content -LiteralPath $JwEthernetHeader -Value $headerText -Encoding utf8
    }

    $propsText = Get-Content -LiteralPath $JwEthernetProperties -Raw
    if ($propsText -match '(?m)^depends=Ethernet\s*$')
    {
        $propsText = [regex]::Replace($propsText, '(?m)^depends=Ethernet\s*$', 'depends=JWPLC Ethernet W5x00 Backend')
        Set-Content -LiteralPath $JwEthernetProperties -Value $propsText -Encoding utf8
    }
    elseif ($propsText -notmatch '(?m)^depends=JWPLC Ethernet W5x00 Backend\s*$')
    {
        throw "depends inesperado en JWPLC_Ethernet/library.properties"
    }

    Write-Host "[4/4] Verificando copia vendorizada..." -ForegroundColor Cyan
    foreach ($item in $expected.GetEnumerator())
    {
        if ($item.Key -eq "library.properties") { continue }
        Assert-UpstreamFile -Root $TargetRoot -RelativePath $item.Key -ExpectedSha $item.Value
    }

    Write-Host ""
    Write-Host "VENDORIZADO ETHERNET W5x00 2.0.2: OK" -ForegroundColor Green
    Write-Host ("Destino: {0}" -f $TargetRoot)
    Write-Host ""
    Write-Host "Cambios locales listos para revisar/commit:" -ForegroundColor Yellow
    & git -C $RepoRoot status --short
}
finally
{
    if (Test-Path -LiteralPath $tempRoot)
    {
        Remove-Item -LiteralPath $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}
