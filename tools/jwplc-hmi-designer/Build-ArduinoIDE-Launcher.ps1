param(
    [string]$OutputDirectory = (Join-Path $PSScriptRoot 'dist')
)

$ErrorActionPreference = 'Stop'
$source = Join-Path $PSScriptRoot 'arduino-ide-launcher'
$packageJson = Join-Path $source 'package.json'
$extensionJs = Join-Path $source 'extension.js'

foreach ($required in @($packageJson, $extensionJs)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "Falta un archivo del launcher Arduino IDE: $required"
    }
}

$package = Get-Content -LiteralPath $packageJson -Raw | ConvertFrom-Json
$version = [string]$package.version
$name = [string]$package.name
$publisher = [string]$package.publisher
$fileName = "$name-$version.vsix"

New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$output = Join-Path $OutputDirectory $fileName
$tempRoot = Join-Path ([IO.Path]::GetTempPath()) ("jwplc-hmi-vsix-" + [guid]::NewGuid().ToString('N'))
$extensionRoot = Join-Path $tempRoot 'extension'

try {
    New-Item -ItemType Directory -Force -Path $extensionRoot | Out-Null
    Copy-Item -LiteralPath $packageJson -Destination (Join-Path $extensionRoot 'package.json') -Force
    Copy-Item -LiteralPath $extensionJs -Destination (Join-Path $extensionRoot 'extension.js') -Force

    $media = Join-Path $source 'media'
    if (Test-Path -LiteralPath $media -PathType Container) {
        Copy-Item -LiteralPath $media -Destination (Join-Path $extensionRoot 'media') -Recurse -Force
    }

    $manifest = @"
<?xml version="1.0" encoding="utf-8"?>
<PackageManifest Version="2.0.0" xmlns="http://schemas.microsoft.com/developer/vsx-schema/2011">
  <Metadata>
    <Identity Language="en-US" Id="$name" Version="$version" Publisher="$publisher" />
    <DisplayName>JWPLC HMI Designer Launcher</DisplayName>
    <Description xml:space="preserve">Abre JWPLC HMI Designer desde Arduino IDE 2.</Description>
    <Properties>
      <Property Id="Microsoft.VisualStudio.Code.Engine" Value="^1.70.0" />
    </Properties>
  </Metadata>
  <Installation>
    <InstallationTarget Id="Microsoft.VisualStudio.Code" />
  </Installation>
  <Dependencies />
  <Assets>
    <Asset Type="Microsoft.VisualStudio.Code.Manifest" Path="extension/package.json" Addressable="true" />
  </Assets>
</PackageManifest>
"@
    Set-Content -LiteralPath (Join-Path $tempRoot 'extension.vsixmanifest') -Value $manifest -Encoding UTF8

    $contentTypes = @"
<?xml version="1.0" encoding="utf-8"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
  <Default Extension="json" ContentType="application/json" />
  <Default Extension="js" ContentType="application/javascript" />
  <Default Extension="svg" ContentType="image/svg+xml" />
  <Override PartName="/extension.vsixmanifest" ContentType="text/xml" />
</Types>
"@
    Set-Content -LiteralPath (Join-Path $tempRoot '[Content_Types].xml') -Value $contentTypes -Encoding UTF8

    $zip = [IO.Path]::ChangeExtension($output, '.zip')
    Remove-Item -LiteralPath $zip -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $output -Force -ErrorAction SilentlyContinue
    Compress-Archive -Path (Join-Path $tempRoot '*') -DestinationPath $zip -Force
    Move-Item -LiteralPath $zip -Destination $output -Force

    Write-Host 'VSIX generado.' -ForegroundColor Green
    Write-Host "  $output"
    Write-Output $output
}
finally {
    Remove-Item -LiteralPath $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
}
