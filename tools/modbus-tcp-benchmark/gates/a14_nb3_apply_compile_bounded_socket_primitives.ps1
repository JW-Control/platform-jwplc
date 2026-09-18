param()

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")

function Replace-ExactOnce {
    param(
        [Parameter(Mandatory = $true)][string]$Text,
        [Parameter(Mandatory = $true)][string]$Old,
        [Parameter(Mandatory = $true)][string]$New,
        [Parameter(Mandatory = $true)][string]$Label
    )

    $first = $Text.IndexOf($Old, [System.StringComparison]::Ordinal)
    if ($first -lt 0) {
        throw "A14_NB3B_ANCHOR_NOT_FOUND=$Label"
    }

    $second = $Text.IndexOf(
        $Old,
        $first + $Old.Length,
        [System.StringComparison]::Ordinal
    )
    if ($second -ge 0) {
        throw "A14_NB3B_ANCHOR_NOT_UNIQUE=$Label"
    }

    return $Text.Substring(0, $first) + $New + $Text.Substring($first + $Old.Length)
}

function Invoke-NB3NativeToLog {
    param(
        [Parameter(Mandatory = $true)][string]$FilePath,
        [Parameter(Mandatory = $true)][string[]]$Arguments,
        [Parameter(Mandatory = $true)][string]$LogPath
    )

    $previousPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        & $FilePath @Arguments *> $LogPath
        return [int]$LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $previousPreference
    }
}

function Assert-NB3RepoEthernetUsed {
    param(
        [Parameter(Mandatory = $true)][string]$LogPath,
        [Parameter(Mandatory = $true)][string]$ExpectedLibraryPath,
        [Parameter(Mandatory = $true)][string]$Label
    )

    $text = [System.IO.File]::ReadAllText($LogPath)
    $used = $text.IndexOf(
        $ExpectedLibraryPath,
        [System.StringComparison]::OrdinalIgnoreCase
    ) -ge 0

    Write-Host ("{0}_REPO_ETHERNET_LIBRARY_USED={1}" -f $Label, $used)
    if (-not $used) {
        throw ("A14_NB3B_{0}_WRONG_ETHERNET_LIBRARY" -f $Label)
    }
}

Write-Host "============================================================"
Write-Host " A14 NB3-B - APPLY + COMPILE BOUNDED SOCKET PRIMITIVES"
Write-Host "============================================================"

Assert-G2Branch

$w5100HeaderRelative = "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.h"
$w5100CppRelative = "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.cpp"
$socketRelative = "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/socket.cpp"
$asyncHeaderRelative = "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/jwplc_ethernet_async_tx.h"
$asyncCppRelative = "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/jwplc_ethernet_async_tx.cpp"

$expectedDirtyBefore = @(
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.cpp",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.h",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetClient.cpp",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_W5x00_Ethernet.h",
    $w5100HeaderRelative,
    $script:G2RawFirmwareRelative
) | Sort-Object

$expectedDirtyAfter = @(
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.cpp",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.h",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetClient.cpp",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_W5x00_Ethernet.h",
    $w5100HeaderRelative,
    $w5100CppRelative,
    $socketRelative,
    $asyncHeaderRelative,
    $asyncCppRelative,
    $script:G2RawFirmwareRelative
) | Sort-Object

$dirtyBefore = @(Get-G2TrackedDirtyPaths)

Write-Host "HEAD=$(Get-G2Head)"
Write-Host "EFFECTIVE_SPI_HZ=$(Get-G2SpiHz)"
Write-Host "STABLE_REGISTER_MAX_COMPARISONS=8"
Write-Host "SOCKET_COMMAND_TIMEOUT_US=1000"
Write-Host "TRACKED_DIRTY_BEFORE=$($dirtyBefore.Count)"
$dirtyBefore | ForEach-Object { Write-Host "DIRTY_BEFORE=$_" }

if ((Get-G2SpiHz) -ne 26000000) {
    throw "A14_NB3B_SPI_FREQUENCY_MISMATCH"
}

$resumePatched = $false

if ($dirtyBefore.Count -eq $expectedDirtyBefore.Count) {
    for ($i = 0; $i -lt $expectedDirtyBefore.Count; ++$i) {
        if ($dirtyBefore[$i] -ne $expectedDirtyBefore[$i]) {
            throw "A14_NB3B_DIRTY_PATH_BEFORE_INVALID=$($dirtyBefore[$i])"
        }
    }
}
elseif ($dirtyBefore.Count -eq $expectedDirtyAfter.Count) {
    for ($i = 0; $i -lt $expectedDirtyAfter.Count; ++$i) {
        if ($dirtyBefore[$i] -ne $expectedDirtyAfter[$i]) {
            throw "A14_NB3B_RESUME_DIRTY_PATH_INVALID=$($dirtyBefore[$i])"
        }
    }

    $resumePatched = $true
    Write-Host "NB3_B_RESUME_PATCHED_STATE=YES"
}
else {
    throw "A14_NB3B_DIRTY_COUNT_BEFORE_INVALID=$($dirtyBefore.Count)"
}

$stagedBefore = @(& git -C $script:G2RepoRoot diff --cached --name-only)
if ($LASTEXITCODE -ne 0 -or $stagedBefore.Count -ne 0) {
    throw "A14_NB3B_INDEX_NOT_CLEAN_BEFORE"
}
Write-Host "STAGED_COUNT_BEFORE=0"

if (-not $resumePatched) {
    foreach ($relative in @(
        $w5100CppRelative,
        $socketRelative,
        $asyncHeaderRelative,
        $asyncCppRelative
    )) {
        $existingDiff = @(& git -C $script:G2RepoRoot diff --name-only -- $relative)

        if ($LASTEXITCODE -ne 0) {
            throw "A14_NB3B_DIFF_CHECK_FAILED=$relative"
        }
        if ($existingDiff.Count -ne 0) {
            throw "A14_NB3B_SOURCE_ALREADY_DIRTY=$relative"
        }
    }

    Write-Host "NB3_NEW_SOURCES_TRACKED_CLEAN_BEFORE=YES"
}
else {
    Write-Host "NB3_NEW_SOURCES_TRACKED_CLEAN_BEFORE=RESUME_NOT_APPLICABLE"
}

Assert-G2ProtectedArtifacts

$w5100HeaderPath = Get-G2Path $w5100HeaderRelative
$w5100CppPath = Get-G2Path $w5100CppRelative
$socketPath = Get-G2Path $socketRelative
$asyncHeaderPath = Get-G2Path $asyncHeaderRelative
$asyncCppPath = Get-G2Path $asyncCppRelative

$w5100Header = [System.IO.File]::ReadAllText($w5100HeaderPath)
$w5100Cpp = [System.IO.File]::ReadAllText($w5100CppPath)
$socketCpp = [System.IO.File]::ReadAllText($socketPath)
$asyncHeader = [System.IO.File]::ReadAllText($asyncHeaderPath)
$asyncCpp = [System.IO.File]::ReadAllText($asyncCppPath)

# Shared state required by both fresh-apply and resume paths.
$utf8NoBom = New-Object -TypeName System.Text.UTF8Encoding -ArgumentList $false
if ($null -eq $utf8NoBom) {
    throw "A14_NB3B_SHARED_RUNTIME_INIT_FAILED=UTF8_NO_BOM"
}
Write-Host "NB3_B_SHARED_RUNTIME_INIT=READY"

if ($resumePatched) {
    Write-Host "NB3_B_PATCH_APPLICATION=SKIPPED_ALREADY_APPLIED"
}
else {
foreach ($marker in @(
    "execCmdSnChecked",
    "readSnTX_FSRStable",
    "readSnRX_RSRStable"
)) {
    if ($w5100Header.Contains($marker) -or $w5100Cpp.Contains($marker)) {
        throw "A14_NB3B_ALREADY_PATCHED=$marker"
    }
}

$oldHeaderDecl = @'
  static void execCmdSn(SOCKET s, SockCMD _cmd);
'@

$newHeaderDecl = @'
  // JWPLC bounded W5x00 primitives.
  // Existing execCmdSn() remains source-compatible. New cooperative code can
  // use execCmdSnChecked() to observe a command-register timeout.
  static bool execCmdSnChecked(
      SOCKET s,
      SockCMD _cmd,
      uint32_t timeoutUs = 1000);
  static void execCmdSn(SOCKET s, SockCMD _cmd);

  // Stable 16-bit socket-register reads with an explicit comparison bound.
  // On false, value contains the latest complete register sample.
  static bool readSnTX_FSRStable(
      SOCKET s,
      uint16_t &value,
      uint8_t maxComparisons = 8);
  static bool readSnRX_RSRStable(
      SOCKET s,
      uint16_t &value,
      uint8_t maxComparisons = 8);
'@

$w5100Header = Replace-ExactOnce -Text $w5100Header -Old $oldHeaderDecl -New $newHeaderDecl -Label "W5100_HEADER_DECL"

$oldExec = @'
void W5100Class::execCmdSn(SOCKET s, SockCMD _cmd)
{
	// Send command to socket
	writeSnCR(s, _cmd);
	// Wait for command to complete
	while (readSnCR(s)) ;
}
'@

$newExec = @'
bool W5100Class::readSnTX_FSRStable(
	SOCKET s,
	uint16_t &value,
	uint8_t maxComparisons)
{
	uint16_t previous = readSnTX_FSR(s);

	for (uint8_t i = 0; i < maxComparisons; ++i) {
		const uint16_t current = readSnTX_FSR(s);
		if (current == previous) {
			value = current;
			return true;
		}
		previous = current;
	}

	value = previous;
	return false;
}

bool W5100Class::readSnRX_RSRStable(
	SOCKET s,
	uint16_t &value,
	uint8_t maxComparisons)
{
	uint16_t previous = readSnRX_RSR(s);

	for (uint8_t i = 0; i < maxComparisons; ++i) {
		const uint16_t current = readSnRX_RSR(s);
		if (current == previous) {
			value = current;
			return true;
		}
		previous = current;
	}

	value = previous;
	return false;
}

bool W5100Class::execCmdSnChecked(
	SOCKET s,
	SockCMD _cmd,
	uint32_t timeoutUs)
{
	writeSnCR(s, _cmd);

	const uint32_t startedUs = micros();

	do {
		if (readSnCR(s) == 0) {
			return true;
		}
	} while ((uint32_t)(micros() - startedUs) < timeoutUs);

	return false;
}

void W5100Class::execCmdSn(SOCKET s, SockCMD _cmd)
{
	(void)execCmdSnChecked(s, _cmd, 1000);
}
'@

$w5100Cpp = Replace-ExactOnce -Text $w5100Cpp -Old $oldExec -New $newExec -Label "W5100_EXEC_CMD"

$oldRxStable = @'
static uint16_t getSnRX_RSR(uint8_t s)
{
#if 1
        uint16_t val, prev;

        prev = W5100.readSnRX_RSR(s);
        while (1) {
                val = W5100.readSnRX_RSR(s);
                if (val == prev) {
			return val;
		}
                prev = val;
        }
#else
	uint16_t val = W5100.readSnRX_RSR(s);
	return val;
#endif
}
'@

$newRxStable = @'
static uint16_t getSnRX_RSR(uint8_t s)
{
	uint16_t value = 0;
	(void)W5100.readSnRX_RSRStable(s, value);
	return value;
}
'@

$socketCpp = Replace-ExactOnce -Text $socketCpp -Old $oldRxStable -New $newRxStable -Label "SOCKET_RX_STABLE"

$oldTxStable = @'
static uint16_t getSnTX_FSR(uint8_t s)
{
        uint16_t val, prev;

        prev = W5100.readSnTX_FSR(s);
        while (1) {
                val = W5100.readSnTX_FSR(s);
                if (val == prev) {
			state[s].TX_FSR = val;
			return val;
		}
                prev = val;
        }
}
'@

$newTxStable = @'
static uint16_t getSnTX_FSR(uint8_t s)
{
	uint16_t value = 0;
	(void)W5100.readSnTX_FSRStable(s, value);
	state[s].TX_FSR = value;
	return value;
}
'@

$socketCpp = Replace-ExactOnce -Text $socketCpp -Old $oldTxStable -New $newTxStable -Label "SOCKET_TX_STABLE"

$oldAsyncHeader = @'
    static uint16_t readTxFreeStable(uint8_t socket);
'@

$newAsyncHeader = @'
    static bool readTxFreeStable(uint8_t socket, uint16_t &value);
'@

$asyncHeader = Replace-ExactOnce -Text $asyncHeader -Old $oldAsyncHeader -New $newAsyncHeader -Label "ASYNC_TX_HEADER"

$oldAsyncStable = @'
uint16_t JWPLC_EthernetAsyncTx::readTxFreeStable(uint8_t socket)
{
    uint16_t previous = W5100.readSnTX_FSR(socket);

    while (true)
    {
        const uint16_t current = W5100.readSnTX_FSR(socket);
        if (current == previous)
        {
            return current;
        }
        previous = current;
    }
}
'@

$newAsyncStable = @'
bool JWPLC_EthernetAsyncTx::readTxFreeStable(
    uint8_t socket,
    uint16_t &value)
{
    return W5100.readSnTX_FSRStable(socket, value);
}
'@

$asyncCpp = Replace-ExactOnce -Text $asyncCpp -Old $oldAsyncStable -New $newAsyncStable -Label "ASYNC_TX_STABLE"

$oldAsyncUse = @'
    const uint16_t freeBytes = readTxFreeStable(socket);
    if (freeBytes < length)
    {
        SPI.endTransaction();
        return 0;
    }

    // Elimina flags de un SEND anterior antes de disparar uno nuevo.
    W5100.writeSnIR(socket, (uint8_t)(SnIR::SEND_OK | SnIR::TIMEOUT));

    writeTxData(socket, data, length);
    W5100.execCmdSn(socket, Sock_SEND);

    SPI.endTransaction();

    _socket = socket;
    _pending = true;
    return 0;
'@

$newAsyncUse = @'
    uint16_t freeBytes = 0;
    if (!readTxFreeStable(socket, freeBytes))
    {
        SPI.endTransaction();
        reset();
        return -1;
    }

    if (freeBytes < length)
    {
        SPI.endTransaction();
        return 0;
    }

    // Elimina flags de un SEND anterior antes de disparar uno nuevo.
    W5100.writeSnIR(socket, (uint8_t)(SnIR::SEND_OK | SnIR::TIMEOUT));

    writeTxData(socket, data, length);

    if (!W5100.execCmdSnChecked(socket, Sock_SEND))
    {
        SPI.endTransaction();
        reset();
        return -1;
    }

    SPI.endTransaction();

    _socket = socket;
    _pending = true;
    return 0;
'@

$asyncCpp = Replace-ExactOnce -Text $asyncCpp -Old $oldAsyncUse -New $newAsyncUse -Label "ASYNC_TX_BEGIN_USE"

[System.IO.File]::WriteAllText($w5100HeaderPath, $w5100Header, $utf8NoBom)
[System.IO.File]::WriteAllText($w5100CppPath, $w5100Cpp, $utf8NoBom)
[System.IO.File]::WriteAllText($socketPath, $socketCpp, $utf8NoBom)
[System.IO.File]::WriteAllText($asyncHeaderPath, $asyncHeader, $utf8NoBom)
[System.IO.File]::WriteAllText($asyncCppPath, $asyncCpp, $utf8NoBom)
}


& git -C $script:G2RepoRoot diff --check
if ($LASTEXITCODE -ne 0) {
    throw "A14_NB3B_DIFF_CHECK_AFTER_PATCH_FAILED"
}

$dirtyAfterPatch = @(Get-G2TrackedDirtyPaths)

Write-Host "TRACKED_DIRTY_AFTER_PATCH=$($dirtyAfterPatch.Count)"
$dirtyAfterPatch | ForEach-Object { Write-Host "DIRTY_AFTER_PATCH=$_" }

if ($dirtyAfterPatch.Count -ne $expectedDirtyAfter.Count) {
    throw "A14_NB3B_DIRTY_COUNT_AFTER_PATCH_INVALID=$($dirtyAfterPatch.Count)"
}
for ($i = 0; $i -lt $expectedDirtyAfter.Count; ++$i) {
    if ($dirtyAfterPatch[$i] -ne $expectedDirtyAfter[$i]) {
        throw "A14_NB3B_DIRTY_PATH_AFTER_PATCH_INVALID=$($dirtyAfterPatch[$i])"
    }
}

$stagedAfterPatch = @(& git -C $script:G2RepoRoot diff --cached --name-only)
if ($LASTEXITCODE -ne 0 -or $stagedAfterPatch.Count -ne 0) {
    throw "A14_NB3B_INDEX_NOT_CLEAN_AFTER_PATCH"
}
Write-Host "STAGED_COUNT_AFTER_PATCH=0"

$w5100HeaderVerify = [System.IO.File]::ReadAllText($w5100HeaderPath)
$w5100CppVerify = [System.IO.File]::ReadAllText($w5100CppPath)
$socketVerify = [System.IO.File]::ReadAllText($socketPath)
$asyncCppVerify = [System.IO.File]::ReadAllText($asyncCppPath)

$legacyExecWhileCount = ([regex]::Matches(
    $w5100CppVerify,
    'while\s*\(\s*readSnCR\(s\)\s*\)'
)).Count

$socketWhileOneCount = ([regex]::Matches(
    $socketVerify,
    'while\s*\(\s*1\s*\)'
)).Count

$asyncWhileTrueCount = ([regex]::Matches(
    $asyncCppVerify,
    'while\s*\(\s*true\s*\)'
)).Count

Write-Host "W5100_LEGACY_EXEC_CMD_UNBOUNDED_LOOP_COUNT=$legacyExecWhileCount"
Write-Host "SOCKET_STABLE_WHILE1_COUNT=$socketWhileOneCount"
Write-Host "ASYNC_TX_STABLE_WHILE_TRUE_COUNT=$asyncWhileTrueCount"

if ($legacyExecWhileCount -ne 0) {
    throw "A14_NB3B_EXEC_CMD_UNBOUNDED_LOOP_REMAINS"
}
if ($socketWhileOneCount -ne 0) {
    throw "A14_NB3B_SOCKET_STABLE_UNBOUNDED_LOOP_REMAINS"
}
if ($asyncWhileTrueCount -ne 0) {
    throw "A14_NB3B_ASYNC_STABLE_UNBOUNDED_LOOP_REMAINS"
}

foreach ($marker in @(
    "execCmdSnChecked",
    "readSnTX_FSRStable",
    "readSnRX_RSRStable"
)) {
    $headerPattern = '(?m)^[ \t]*static[ \t]+bool[ \t]+' +
        [regex]::Escape($marker) + '[ \t]*\('

    $cppPattern = '(?m)^[ \t]*bool[ \t]+W5100Class::' +
        [regex]::Escape($marker) + '[ \t]*\('

    $headerCount = ([regex]::Matches(
        $w5100HeaderVerify,
        $headerPattern
    )).Count

    $cppCount = ([regex]::Matches(
        $w5100CppVerify,
        $cppPattern
    )).Count

    Write-Host "W5100_API=$marker HEADER_DECL_COUNT=$headerCount CPP_DEF_COUNT=$cppCount"

    if ($headerCount -ne 1 -or $cppCount -ne 1) {
        throw "A14_NB3B_W5100_API_SIGNATURE_INVALID=$marker"
    }
}

Write-Host "W5100_HEADER_SHA256=$(Get-G2Sha256 $w5100HeaderRelative)"
Write-Host "W5100_CPP_SHA256=$(Get-G2Sha256 $w5100CppRelative)"
Write-Host "SOCKET_CPP_SHA256=$(Get-G2Sha256 $socketRelative)"
Write-Host "ASYNC_TX_HEADER_SHA256=$(Get-G2Sha256 $asyncHeaderRelative)"
Write-Host "ASYNC_TX_CPP_SHA256=$(Get-G2Sha256 $asyncCppRelative)"

$arduinoCli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"
if (-not (Test-Path -LiteralPath $arduinoCli)) {
    throw "A14_NB3B_ARDUINO_CLI_NOT_FOUND=$arduinoCli"
}

$fqbn = "jwplc_local:esp32:jwplcbasic"
$librariesRoot = Get-G2Path "JWPLC/2.1.0/libraries"
$expectedEthernetLibrary = Join-Path $librariesRoot "JWPLC_Ethernet"
$rawFirmwarePath = Get-G2Path $script:G2RawFirmwareRelative
$rawSketchDir = Split-Path -Parent $rawFirmwarePath

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$tempRoot = Join-Path $env:TEMP ("jwplc_a14_nb3b_primitives_{0}" -f $timestamp)
$rawBuildPath = Join-Path $tempRoot "raw_build"
$probeSketchDir = Join-Path $tempRoot "api_probe"
$probeBuildPath = Join-Path $tempRoot "probe_build"
$rawCompileLog = Join-Path $tempRoot "raw_compile.log"
$probeCompileLog = Join-Path $tempRoot "probe_compile.log"

New-Item -ItemType Directory -Force -Path $rawBuildPath | Out-Null
New-Item -ItemType Directory -Force -Path $probeSketchDir | Out-Null
New-Item -ItemType Directory -Force -Path $probeBuildPath | Out-Null

$probeSketchPath = Join-Path $probeSketchDir "nb3_bounded_primitives_probe.ino"

$probeSketch = @'
#include <JWPLC_Ethernet.h>
#include <utility/w5100.h>

volatile int nb3Sink = 0;

static void compileOnlyBoundedPrimitiveProbe()
{
    uint16_t value = 0;

    nb3Sink += W5100.readSnTX_FSRStable(0, value, 8) ? 1 : 0;
    nb3Sink += value;

    nb3Sink += W5100.readSnRX_RSRStable(0, value, 8) ? 1 : 0;
    nb3Sink += value;

    nb3Sink += W5100.execCmdSnChecked(
        0,
        Sock_OPEN,
        1000) ? 1 : 0;
}

void setup()
{
    if (false)
    {
        compileOnlyBoundedPrimitiveProbe();
    }
}

void loop()
{
}
'@

[System.IO.File]::WriteAllText(
    $probeSketchPath,
    $probeSketch,
    $utf8NoBom)

Write-Host "ARDUINO_CLI=$arduinoCli"
Write-Host "FQBN=$fqbn"
Write-Host "TEMP_ROOT=$tempRoot"
Write-Host "SOURCE_MUTATION=W5100_SOCKET_ASYNC_TX"
Write-Host "UPLOAD=NO"

Write-Host ""
Write-Host "=== COMPILE RAW CANDIDATE WITH BOUNDED PRIMITIVES ==="

$rawCompileArgs = @(
    "compile",
    "--fqbn", $fqbn,
    "--build-path", $rawBuildPath,
    "--libraries", $librariesRoot,
    $rawSketchDir
)

$rawCompileExit = Invoke-NB3NativeToLog -FilePath $arduinoCli -Arguments $rawCompileArgs -LogPath $rawCompileLog

Write-Host "RAW_COMPILE_EXIT=$rawCompileExit"
Write-Host "RAW_COMPILE_LOG=$rawCompileLog"

if ($rawCompileExit -ne 0) {
    Invoke-G2CompileFinishedSound -Success $false
    Get-Content -LiteralPath $rawCompileLog -Tail 140 | ForEach-Object { Write-Host $_ }
    throw "A14_NB3B_RAW_COMPILE_FAILED"
}

Assert-NB3RepoEthernetUsed -LogPath $rawCompileLog -ExpectedLibraryPath $expectedEthernetLibrary -Label "RAW"

Write-Host ""
Write-Host "=== COMPILE BOUNDED PRIMITIVES API PROBE ==="

$probeCompileArgs = @(
    "compile",
    "--fqbn", $fqbn,
    "--build-path", $probeBuildPath,
    "--libraries", $librariesRoot,
    $probeSketchDir
)

$probeCompileExit = Invoke-NB3NativeToLog -FilePath $arduinoCli -Arguments $probeCompileArgs -LogPath $probeCompileLog

Write-Host "API_PROBE_COMPILE_EXIT=$probeCompileExit"
Write-Host "API_PROBE_COMPILE_LOG=$probeCompileLog"

if ($probeCompileExit -ne 0) {
    Invoke-G2CompileFinishedSound -Success $false
    Get-Content -LiteralPath $probeCompileLog -Tail 140 | ForEach-Object { Write-Host $_ }
    throw "A14_NB3B_API_PROBE_COMPILE_FAILED"
}

Assert-NB3RepoEthernetUsed -LogPath $probeCompileLog -ExpectedLibraryPath $expectedEthernetLibrary -Label "API_PROBE"

$rawBinCount = @(
    Get-ChildItem -LiteralPath $rawBuildPath -Recurse -File -Filter "*.bin"
).Count
$probeBinCount = @(
    Get-ChildItem -LiteralPath $probeBuildPath -Recurse -File -Filter "*.bin"
).Count

Write-Host "RAW_BIN_COUNT=$rawBinCount"
Write-Host "API_PROBE_BIN_COUNT=$probeBinCount"

if ($rawBinCount -lt 1 -or $probeBinCount -lt 1) {
    Invoke-G2CompileFinishedSound -Success $false
    throw "A14_NB3B_BIN_OUTPUT_MISSING"
}

Invoke-G2CompileFinishedSound -Success $true
Write-Host "COMPILE_FINISHED_SOUND=PLAYED"

Assert-G2ProtectedArtifacts

$dirtyFinal = @(Get-G2TrackedDirtyPaths)
$stagedFinal = @(& git -C $script:G2RepoRoot diff --cached --name-only)

Write-Host "TRACKED_DIRTY_COUNT_FINAL=$($dirtyFinal.Count)"
$dirtyFinal | ForEach-Object { Write-Host "DIRTY_FINAL=$_" }
Write-Host "STAGED_COUNT_FINAL=$($stagedFinal.Count)"

if ($dirtyFinal.Count -ne $expectedDirtyAfter.Count -or $stagedFinal.Count -ne 0) {
    throw "A14_NB3B_FINAL_WORKTREE_INVALID"
}
for ($i = 0; $i -lt $expectedDirtyAfter.Count; ++$i) {
    if ($dirtyFinal[$i] -ne $expectedDirtyAfter[$i]) {
        throw "A14_NB3B_FINAL_DIRTY_PATH_INVALID=$($dirtyFinal[$i])"
    }
}

Write-Host "NB3_W5100_COMMAND_WAIT=BOUNDED_1000US"
Write-Host "NB3_W5100_STABLE_REGISTER_READS=BOUNDED_8_COMPARISONS"
Write-Host "NB3_SOCKET_STABLE_READ_LOOPS=REMOVED"
Write-Host "NB3_ASYNC_TX_STABLE_READ_LOOP=REMOVED"
Write-Host "NB3_ASYNC_TX_COMMAND_FAILURE=OBSERVABLE"
Write-Host "NB3_LEGACY_EXEC_CMD_API=PRESERVED"
Write-Host "NB3_API_SIGNATURE_CHECK=EXACT_DECLARATION_DEFINITION"
Write-Host "NB3_RESUME_SHARED_INIT=UNCONDITIONAL"
Write-Host "NB3_UPLOAD=NO"
Write-Host "A14_NB3_BOUNDED_SOCKET_PRIMITIVES_COMPILE=PASS"
