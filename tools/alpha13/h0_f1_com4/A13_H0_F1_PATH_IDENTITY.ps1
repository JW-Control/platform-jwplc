# Alpha13 H0-F1: Win32 API para comprobar la identidad REAL de un path existente.
# Expande alias cortos NTFS 8.3 como RUNNER~1. No reescribe ni crea archivos.
if ($null -eq ('JWPLC_H0F1_WinPath' -as [type])) {
  Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
using System.Text;
public static class JWPLC_H0F1_WinPath {
  [DllImport("kernel32.dll", EntryPoint="GetLongPathNameW", CharSet=CharSet.Unicode, SetLastError=true)]
  public static extern uint GetLongPathName(string source, StringBuilder output, uint capacity);
  [DllImport("kernel32.dll", EntryPoint="GetShortPathNameW", CharSet=CharSet.Unicode, SetLastError=true)]
  public static extern uint GetShortPathName(string source, StringBuilder output, uint capacity);
}
'@ -ErrorAction Stop
}
function Get-H0F1CanonicalPath([string]$path) {
  if ([string]::IsNullOrWhiteSpace($path)) { throw 'H0F1_PATH_EMPTY' }
  $full = (Resolve-Path -LiteralPath $path -ErrorAction Stop).ProviderPath
  $buf = New-Object System.Text.StringBuilder(32768)
  $n = [JWPLC_H0F1_WinPath]::GetLongPathName($full, $buf, [uint32]$buf.Capacity)
  if ($n -eq 0 -or $n -ge $buf.Capacity) {
    throw ('H0F1_WIN32_LONG_PATH_UNAVAILABLE:'+ $path)
  }
  return [IO.Path]::GetFullPath($buf.ToString()).TrimEnd([char[]]@('\','/'))
}
function Assert-H0F1SamePath([string]$selected, [string]$expected) {
  $a = Get-H0F1CanonicalPath $selected
  $b = Get-H0F1CanonicalPath $expected
  return [string]::Equals($a,$b,[StringComparison]::OrdinalIgnoreCase)
}
function Test-H0F1PathIdentity {
  $full = (Resolve-Path -LiteralPath $env:USERPROFILE -ErrorAction Stop).ProviderPath
  $buf = New-Object System.Text.StringBuilder(32768)
  $n = [JWPLC_H0F1_WinPath]::GetShortPathName($full,$buf,[uint32]$buf.Capacity)
  if ($n -eq 0 -or $n -ge $buf.Capacity) {
    throw 'H0F1_8DOT3_TEST_SHORT_PATH_UNAVAILABLE'
  }
  $short = $buf.ToString()
  if (!(Assert-H0F1SamePath $short $full)) {
    throw 'H0F1_8DOT3_ALIAS_EQUIVALENCE_FAILED'
  }
  if (!(Assert-H0F1SamePath ($full.ToUpperInvariant()) $full)) {
    throw 'H0F1_CASE_EQUIVALENCE_FAILED'
  }
  $different = Join-Path $env:USERPROFILE 'AppData'
  if (!(Test-Path -LiteralPath $different -PathType Container)) {
    throw 'H0F1_NEGATIVE_FIXTURE_MISSING'
  }
  if (Assert-H0F1SamePath $different $full) {
    throw 'H0F1_DISTINCT_DIR_ACCEPTED'
  }
  Write-Host 'H0F1_PATH_N0=PASS_8DOT3_ALIAS_AND_NEGATIVE_DIFFERENT_DIRECTORY'
}
