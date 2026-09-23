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
    if ($first -lt 0) { throw "A14_NB3F_PATCH_ANCHOR_MISSING=$Label" }

    $second = $Text.IndexOf($Old, $first + $Old.Length, [System.StringComparison]::Ordinal)
    if ($second -ge 0) { throw "A14_NB3F_PATCH_ANCHOR_NOT_UNIQUE=$Label" }

    return $Text.Substring(0, $first) + $New + $Text.Substring($first + $Old.Length)
}

function Get-CppFunctionBlock {
    param(
        [Parameter(Mandatory = $true)][string]$Text,
        [Parameter(Mandatory = $true)][string]$SignaturePrefix
    )

    $searchOffset = 0

    while ($true) {
        $candidate = $Text.IndexOf(
            $SignaturePrefix,
            $searchOffset,
            [System.StringComparison]::Ordinal)

        if ($candidate -lt 0) { break }

        $cursor = $candidate + $SignaturePrefix.Length
        $closeParen = $Text.IndexOf(")", $cursor, [System.StringComparison]::Ordinal)
        $braceStart = $Text.IndexOf("{", $cursor, [System.StringComparison]::Ordinal)
        $semicolon = $Text.IndexOf(";", $cursor, [System.StringComparison]::Ordinal)

        $looksLikeDefinition = (
            $closeParen -ge 0 -and
            $braceStart -gt $closeParen -and
            ($semicolon -lt 0 -or $braceStart -lt $semicolon)
        )

        if ($looksLikeDefinition) {
            $depth = 0

            for ($i = $braceStart; $i -lt $Text.Length; ++$i) {
                if ($Text[$i] -eq "{") {
                    ++$depth
                }
                elseif ($Text[$i] -eq "}") {
                    --$depth
                    if ($depth -eq 0) {
                        return $Text.Substring(
                            $candidate,
                            $i - $candidate + 1)
                    }
                }
            }

            throw "A14_NB3F_FUNCTION_END_NOT_FOUND=$SignaturePrefix"
        }

        $searchOffset = $candidate + $SignaturePrefix.Length
    }

    throw "A14_NB3F_FUNCTION_DEFINITION_NOT_FOUND=$SignaturePrefix"
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
    $used = ($text.IndexOf($ExpectedLibraryPath, [System.StringComparison]::OrdinalIgnoreCase) -ge 0)
    Write-Host "$($Label)_REPO_ETHERNET_LIBRARY_USED=$used"
    if (-not $used) { throw "A14_NB3F_WRONG_ETHERNET_LIBRARY=$Label" }
}

Write-Host "============================================================"
Write-Host " A14 NB3-F1 - LEGACY TCP SEND BOUNDS APPLY + COMPILE"
Write-Host "============================================================"

Assert-G2Branch

Write-Host "HEAD=$(Get-G2Head)"
Write-Host "EFFECTIVE_SPI_HZ=$(Get-G2SpiHz)"
Write-Host "UPLOAD=NO"

if ((Get-G2SpiHz) -ne 26000000) { throw "A14_NB3F_SPI_FREQUENCY_MISMATCH" }

$headerRelative = "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_W5x00_Ethernet.h"
$clientRelative = "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetClient.cpp"
$socketRelative = "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/socket.cpp"

$expectedDirty = @(
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.cpp",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.h",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetClient.cpp",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetUdp.cpp",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/jwplc_ethernet_async_tx.cpp",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/jwplc_ethernet_async_tx.h",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_W5x00_Ethernet.h",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/socket.cpp",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.cpp",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.h",
    "tools/modbus-tcp-benchmark/firmware/eth14_raw_transport_server/eth14_raw_transport_server.ino"
) | Sort-Object

$dirty = @(Get-G2TrackedDirtyPaths)
Write-Host "TRACKED_DIRTY_BEFORE=$($dirty.Count)"
$dirty | ForEach-Object { Write-Host "DIRTY_BEFORE=$_" }

if ($dirty.Count -ne $expectedDirty.Count) { throw "A14_NB3F_DIRTY_COUNT_INVALID=$($dirty.Count)" }
for ($i = 0; $i -lt $expectedDirty.Count; ++$i) {
    if ($dirty[$i] -ne $expectedDirty[$i]) { throw "A14_NB3F_DIRTY_PATH_INVALID=$($dirty[$i])" }
}

$staged = @(& git -C $script:G2RepoRoot diff --cached --name-only)
if ($LASTEXITCODE -ne 0 -or $staged.Count -ne 0) { throw "A14_NB3F_INDEX_NOT_CLEAN" }
Write-Host "STAGED_COUNT_BEFORE=0"

Assert-G2ProtectedArtifacts

$headerPath = Get-G2Path $headerRelative
$clientPath = Get-G2Path $clientRelative
$socketPath = Get-G2Path $socketRelative

$header = [System.IO.File]::ReadAllText($headerPath)
$client = [System.IO.File]::ReadAllText($clientPath)
$socketCpp = [System.IO.File]::ReadAllText($socketPath)

$preHeaderHash = Get-G2Sha256 $headerRelative
$preClientHash = Get-G2Sha256 $clientRelative
$preSocketHash = Get-G2Sha256 $socketRelative

Write-Host "HEADER_SHA256_BEFORE=$preHeaderHash"
Write-Host "CLIENT_CPP_SHA256_BEFORE=$preClientHash"
Write-Host "SOCKET_CPP_SHA256_BEFORE=$preSocketHash"

$oldHeader = @'
	static uint16_t socketSend(uint8_t s, const uint8_t * buf, uint16_t len);
'@

$newHeader = @'
	static uint16_t socketSend(
		uint8_t s,
		const uint8_t *buf,
		uint16_t len,
		uint32_t timeoutMs = 1000);
'@

$oldClient = @'
	if (_sockindex >= MAX_SOCK_NUM) return 0;
	if (Ethernet.socketSend(_sockindex, buf, size)) return size;
	setWriteError();
	return 0;
'@

$newClient = @'
	if (_sockindex >= MAX_SOCK_NUM) return 0;
	if (Ethernet.socketSend(_sockindex, buf, size, _timeout)) return size;
	setWriteError();
	return 0;
'@

$oldSocket = @'
uint16_t EthernetClass::socketSend(uint8_t s, const uint8_t * buf, uint16_t len)
{
	uint8_t status=0;
	uint16_t ret=0;
	uint16_t freesize=0;

	if (len > W5100.SSIZE) {
		ret = W5100.SSIZE; // check size not to exceed MAX size.
	} else {
		ret = len;
	}

	// if freebuf is available, start.
	do {
		SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
		freesize = getSnTX_FSR(s);
		status = W5100.readSnSR(s);
		SPI.endTransaction();
		if ((status != SnSR::ESTABLISHED) && (status != SnSR::CLOSE_WAIT)) {
			ret = 0;
			break;
		}
		yield();
	} while (freesize < ret);

	// copy data
	SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
	write_data(s, 0, (uint8_t *)buf, ret);
	W5100.execCmdSn(s, Sock_SEND);

	/* +2008.01 bj */
	while ( (W5100.readSnIR(s) & SnIR::SEND_OK) != SnIR::SEND_OK ) {
		/* m2008.01 [bj] : reduce code */
		if ( W5100.readSnSR(s) == SnSR::CLOSED ) {
			SPI.endTransaction();
			return 0;
		}
		SPI.endTransaction();
		yield();
		SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
	}
	/* +2008.01 bj */
	W5100.writeSnIR(s, SnIR::SEND_OK);
	SPI.endTransaction();
	return ret;
}
'@

$newSocket = @'
uint16_t EthernetClass::socketSend(
	uint8_t s,
	const uint8_t *buf,
	uint16_t len,
	uint32_t timeoutMs)
{
	uint8_t status = 0;
	uint16_t ret = 0;
	uint16_t freesize = 0;
	const uint32_t startedMs = millis();

	if (len > W5100.SSIZE) {
		ret = W5100.SSIZE;
	} else {
		ret = len;
	}

	do {
		SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
		freesize = getSnTX_FSR(s);
		status = W5100.readSnSR(s);
		SPI.endTransaction();

		if (status != SnSR::ESTABLISHED && status != SnSR::CLOSE_WAIT) {
			return 0;
		}

		if (freesize >= ret) {
			break;
		}

		yield();
	} while ((uint32_t)(millis() - startedMs) < timeoutMs);

	if (freesize < ret) {
		return 0;
	}

	SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
	W5100.writeSnIR(s, (uint8_t)(SnIR::SEND_OK | SnIR::TIMEOUT));
	write_data(s, 0, (uint8_t *)buf, ret);

	if (!W5100.execCmdSnChecked(s, Sock_SEND, 1000)) {
		SPI.endTransaction();
		return 0;
	}

	SPI.endTransaction();

	do {
		SPI.beginTransaction(SPI_ETHERNET_SETTINGS);

		const uint8_t interruptFlags = W5100.readSnIR(s);
		status = W5100.readSnSR(s);

		if ((interruptFlags & SnIR::SEND_OK) != 0) {
			W5100.writeSnIR(s, SnIR::SEND_OK);
			SPI.endTransaction();
			return ret;
		}

		if ((interruptFlags & SnIR::TIMEOUT) != 0) {
			W5100.writeSnIR(s, (uint8_t)(SnIR::SEND_OK | SnIR::TIMEOUT));
			SPI.endTransaction();
			return 0;
		}

		SPI.endTransaction();

		if (status != SnSR::ESTABLISHED && status != SnSR::CLOSE_WAIT) {
			return 0;
		}

		yield();
	} while ((uint32_t)(millis() - startedMs) < timeoutMs);

	return 0;
}
'@

$hasOldHeader = ($header.IndexOf($oldHeader, [System.StringComparison]::Ordinal) -ge 0)
$hasNewHeader = ($header.IndexOf($newHeader, [System.StringComparison]::Ordinal) -ge 0)
$hasOldClient = ($client.IndexOf($oldClient, [System.StringComparison]::Ordinal) -ge 0)
$hasNewClient = ($client.IndexOf($newClient, [System.StringComparison]::Ordinal) -ge 0)
$hasOldSocket = ($socketCpp.IndexOf($oldSocket, [System.StringComparison]::Ordinal) -ge 0)
$hasNewSocket = ($socketCpp.IndexOf($newSocket, [System.StringComparison]::Ordinal) -ge 0)

$freshState = ($hasOldHeader -and -not $hasNewHeader -and $hasOldClient -and -not $hasNewClient -and $hasOldSocket -and -not $hasNewSocket)
$resumeState = (-not $hasOldHeader -and $hasNewHeader -and -not $hasOldClient -and $hasNewClient -and -not $hasOldSocket -and $hasNewSocket)

Write-Host "NB3_F1_FRESH_STATE=$freshState"
Write-Host "NB3_F1_RESUME_STATE=$resumeState"

if ($freshState) {
    if ($preHeaderHash -ne "8CE510B0E3FED0E2CAFE962B623559AFBA02DCB88928D5C1A7C879263ED310A3") { throw "A14_NB3F_HEADER_PRE_HASH_MISMATCH=$preHeaderHash" }
    if ($preClientHash -ne "93B71C92E298053425FF03D750632A30102BE743E1C8B1FDAB800214EABED4B2") { throw "A14_NB3F_CLIENT_PRE_HASH_MISMATCH=$preClientHash" }
    if ($preSocketHash -ne "AE404D279294C0A4ABEA0BE51EE6267EB7106B289C27CF46B3991048A3C6A2C2") { throw "A14_NB3F_SOCKET_PRE_HASH_MISMATCH=$preSocketHash" }

    $header = Replace-ExactOnce -Text $header -Old $oldHeader -New $newHeader -Label "SOCKET_SEND_DECL"
    $client = Replace-ExactOnce -Text $client -Old $oldClient -New $newClient -Label "CLIENT_WRITE_CALL"
    $socketCpp = Replace-ExactOnce -Text $socketCpp -Old $oldSocket -New $newSocket -Label "SOCKET_SEND_BODY"

    $utf8NoBom = New-Object -TypeName System.Text.UTF8Encoding -ArgumentList $false
    [System.IO.File]::WriteAllText($headerPath, $header, $utf8NoBom)
    [System.IO.File]::WriteAllText($clientPath, $client, $utf8NoBom)
    [System.IO.File]::WriteAllText($socketPath, $socketCpp, $utf8NoBom)

    Write-Host "NB3_F1_PATCH_APPLICATION=APPLIED"
}
elseif ($resumeState) {
    Write-Host "NB3_F1_RESUME_PATCHED_STATE=YES"
    Write-Host "NB3_F1_PATCH_APPLICATION=SKIPPED_ALREADY_APPLIED"
}
else {
    throw "A14_NB3F_SOURCE_STATE_AMBIGUOUS"
}

$previousPreference = $ErrorActionPreference
try {
    $ErrorActionPreference = "Continue"
    $diffCheck = @(& git -C $script:G2RepoRoot diff --check 2>&1)
    $diffCheckExit = [int]$LASTEXITCODE
}
finally {
    $ErrorActionPreference = $previousPreference
}

Write-Host "GIT_DIFF_CHECK_EXIT=$diffCheckExit"
if ($diffCheckExit -ne 0) {
    $diffCheck | ForEach-Object { Write-Host $_ }
    throw "A14_NB3F_GIT_DIFF_CHECK_FAILED"
}
Write-Host "GIT_DIFF_CHECK=PASS"

$headerVerify = [System.IO.File]::ReadAllText($headerPath)
$clientVerify = [System.IO.File]::ReadAllText($clientPath)
$socketVerify = [System.IO.File]::ReadAllText($socketPath)
$socketSendBlock = Get-CppFunctionBlock -Text $socketVerify -SignaturePrefix "uint16_t EthernetClass::socketSend("

$oldFreeWaitCount = ([regex]::Matches($socketSendBlock, '}\s*while\s*\(\s*freesize\s*<\s*ret\s*\);')).Count
$oldSendOkWaitCount = ([regex]::Matches($socketSendBlock, 'while\s*\(\s*\(W5100\.readSnIR\(s\).*SEND_OK')).Count
$timeoutLoopCount = ([regex]::Matches($socketSendBlock, 'while\s*\(\s*\(uint32_t\)\(millis\(\) - startedMs\) < timeoutMs\s*\);')).Count
$checkedCommandCount = ([regex]::Matches($socketSendBlock, 'W5100\.execCmdSnChecked\(s, Sock_SEND, 1000\)')).Count
$hardwareTimeoutCount = ([regex]::Matches($socketSendBlock, 'interruptFlags\s*&\s*SnIR::TIMEOUT')).Count
$clientTimeoutPassCount = ([regex]::Matches($clientVerify, 'Ethernet\.socketSend\(_sockindex, buf, size, _timeout\)')).Count
$headerDefaultCount = ([regex]::Matches($headerVerify, 'uint32_t\s+timeoutMs\s*=\s*1000')).Count

Write-Host "TCP_LEGACY_FUNCTION_EXTRACTOR=MULTILINE_SIGNATURE_AWARE"\nWrite-Host "TCP_LEGACY_OLD_FREE_WAIT_COUNT=$oldFreeWaitCount"
Write-Host "TCP_LEGACY_OLD_SEND_OK_WAIT_COUNT=$oldSendOkWaitCount"
Write-Host "TCP_LEGACY_TIMEOUT_BOUNDED_LOOP_COUNT=$timeoutLoopCount"
Write-Host "TCP_LEGACY_CHECKED_SEND_COMMAND_COUNT=$checkedCommandCount"
Write-Host "TCP_LEGACY_HARDWARE_TIMEOUT_CHECK_COUNT=$hardwareTimeoutCount"
Write-Host "TCP_CLIENT_TIMEOUT_PASS_COUNT=$clientTimeoutPassCount"
Write-Host "TCP_SOCKET_DEFAULT_TIMEOUT_DECL_COUNT=$headerDefaultCount"

if ($oldFreeWaitCount -ne 0) { throw "A14_NB3F_OLD_FREE_WAIT_REMAINS" }
if ($oldSendOkWaitCount -ne 0) { throw "A14_NB3F_OLD_SEND_OK_WAIT_REMAINS" }
if ($timeoutLoopCount -ne 2) { throw "A14_NB3F_TIMEOUT_LOOP_COUNT_INVALID=$timeoutLoopCount" }
if ($checkedCommandCount -ne 1) { throw "A14_NB3F_CHECKED_COMMAND_COUNT_INVALID=$checkedCommandCount" }
if ($hardwareTimeoutCount -ne 1) { throw "A14_NB3F_HARDWARE_TIMEOUT_COUNT_INVALID=$hardwareTimeoutCount" }
if ($clientTimeoutPassCount -ne 1) { throw "A14_NB3F_CLIENT_TIMEOUT_PASS_INVALID=$clientTimeoutPassCount" }
if ($headerDefaultCount -ne 1) { throw "A14_NB3F_DEFAULT_TIMEOUT_DECL_INVALID=$headerDefaultCount" }

Write-Host "HEADER_SHA256_AFTER=$(Get-G2Sha256 $headerRelative)"
Write-Host "CLIENT_CPP_SHA256_AFTER=$(Get-G2Sha256 $clientRelative)"
Write-Host "SOCKET_CPP_SHA256_AFTER=$(Get-G2Sha256 $socketRelative)"

$dirtyAfter = @(Get-G2TrackedDirtyPaths)
$stagedAfter = @(& git -C $script:G2RepoRoot diff --cached --name-only)

Write-Host "TRACKED_DIRTY_AFTER_PATCH=$($dirtyAfter.Count)"
Write-Host "STAGED_COUNT_AFTER_PATCH=$($stagedAfter.Count)"

if ($dirtyAfter.Count -ne $expectedDirty.Count -or $stagedAfter.Count -ne 0) {
    throw "A14_NB3F_POST_PATCH_WORKTREE_INVALID"
}

$arduinoCli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"
if (-not (Test-Path -LiteralPath $arduinoCli)) { throw "A14_NB3F_ARDUINO_CLI_NOT_FOUND=$arduinoCli" }

$fqbn = "jwplc_local:esp32:jwplcbasic"
$librariesRoot = Get-G2Path "JWPLC/2.1.0/libraries"
$expectedEthernetLibrary = Join-Path $librariesRoot "JWPLC_Ethernet"
$rawFirmwarePath = Get-G2Path $script:G2RawFirmwareRelative
$rawSketchDir = Split-Path -Parent $rawFirmwarePath

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$tempRoot = Join-Path $env:TEMP ("jwplc_a14_nb3f1_tcp_send_{0}" -f $timestamp)
$rawBuildPath = Join-Path $tempRoot "raw_build"
$probeSketchDir = Join-Path $tempRoot "api_probe"
$probeBuildPath = Join-Path $tempRoot "probe_build"
$rawCompileLog = Join-Path $tempRoot "raw_compile.log"
$probeCompileLog = Join-Path $tempRoot "probe_compile.log"

New-Item -ItemType Directory -Force -Path $rawBuildPath | Out-Null
New-Item -ItemType Directory -Force -Path $probeSketchDir | Out-Null
New-Item -ItemType Directory -Force -Path $probeBuildPath | Out-Null

$probeSketchName = Split-Path -Leaf $probeSketchDir
$probeSketchPath = Join-Path $probeSketchDir ($probeSketchName + ".ino")
$utf8NoBom = New-Object -TypeName System.Text.UTF8Encoding -ArgumentList $false

$probeSketch = @'
#include <JWPLC_Ethernet.h>

volatile size_t nb3fSink = 0;

static void compileOnlyLegacyTcpApiProbe()
{
    EthernetClient client;
    client.setConnectionTimeout(250);

    uint8_t payload[8] = {};
    nb3fSink += client.write(payload, sizeof(payload));

    EthernetServer server(5001);
    nb3fSink += server.write(payload, sizeof(payload));
}

void setup()
{
    if (false)
    {
        compileOnlyLegacyTcpApiProbe();
    }
}

void loop()
{
}
'@

[System.IO.File]::WriteAllText($probeSketchPath, $probeSketch, $utf8NoBom)

Write-Host "API_PROBE_SKETCH_FOLDER=$probeSketchName"
Write-Host "API_PROBE_MAIN_BASENAME=$([System.IO.Path]::GetFileNameWithoutExtension($probeSketchPath))"
Write-Host "API_PROBE_SCOPE=PUBLIC_CLIENT_SERVER_WRITE_COMPATIBILITY"
Write-Host "UPLOAD=NO"

$rawArgs = @("compile","--fqbn",$fqbn,"--build-path",$rawBuildPath,"--libraries",$librariesRoot,$rawSketchDir)
$rawExit = Invoke-NB3NativeToLog -FilePath $arduinoCli -Arguments $rawArgs -LogPath $rawCompileLog

Write-Host "RAW_COMPILE_EXIT=$rawExit"
Write-Host "RAW_COMPILE_LOG=$rawCompileLog"

if ($rawExit -ne 0) {
    Invoke-G2CompileFinishedSound -Success $false
    Get-Content -LiteralPath $rawCompileLog -Tail 140 | ForEach-Object { Write-Host $_ }
    throw "A14_NB3F_RAW_COMPILE_FAILED"
}

Assert-NB3RepoEthernetUsed -LogPath $rawCompileLog -ExpectedLibraryPath $expectedEthernetLibrary -Label "RAW"

$probeArgs = @("compile","--fqbn",$fqbn,"--build-path",$probeBuildPath,"--libraries",$librariesRoot,$probeSketchDir)
$probeExit = Invoke-NB3NativeToLog -FilePath $arduinoCli -Arguments $probeArgs -LogPath $probeCompileLog

Write-Host "API_PROBE_COMPILE_EXIT=$probeExit"
Write-Host "API_PROBE_COMPILE_LOG=$probeCompileLog"

if ($probeExit -ne 0) {
    Invoke-G2CompileFinishedSound -Success $false
    Get-Content -LiteralPath $probeCompileLog -Tail 140 | ForEach-Object { Write-Host $_ }
    throw "A14_NB3F_API_PROBE_COMPILE_FAILED"
}

Assert-NB3RepoEthernetUsed -LogPath $probeCompileLog -ExpectedLibraryPath $expectedEthernetLibrary -Label "API_PROBE"

$rawBinCount = @(Get-ChildItem -LiteralPath $rawBuildPath -Recurse -File -Filter "*.bin").Count
$probeBinCount = @(Get-ChildItem -LiteralPath $probeBuildPath -Recurse -File -Filter "*.bin").Count

Write-Host "RAW_BIN_COUNT=$rawBinCount"
Write-Host "API_PROBE_BIN_COUNT=$probeBinCount"

if ($rawBinCount -lt 1 -or $probeBinCount -lt 1) {
    Invoke-G2CompileFinishedSound -Success $false
    throw "A14_NB3F_BIN_OUTPUT_MISSING"
}

Invoke-G2CompileFinishedSound -Success $true
Write-Host "COMPILE_FINISHED_SOUND=PLAYED"

Assert-G2ProtectedArtifacts

$dirtyFinal = @(Get-G2TrackedDirtyPaths)
$stagedFinal = @(& git -C $script:G2RepoRoot diff --cached --name-only)

Write-Host "TRACKED_DIRTY_COUNT_FINAL=$($dirtyFinal.Count)"
$dirtyFinal | ForEach-Object { Write-Host "DIRTY_FINAL=$_" }
Write-Host "STAGED_COUNT_FINAL=$($stagedFinal.Count)"

if ($dirtyFinal.Count -ne $expectedDirty.Count -or $stagedFinal.Count -ne 0) {
    throw "A14_NB3F_FINAL_WORKTREE_INVALID"
}

for ($i = 0; $i -lt $expectedDirty.Count; ++$i) {
    if ($dirtyFinal[$i] -ne $expectedDirty[$i]) { throw "A14_NB3F_FINAL_DIRTY_PATH_INVALID=$($dirtyFinal[$i])" }
}

Write-Host "NB3_TCP_LEGACY_TX_FREE_WAIT=BOUNDED_BY_TIMEOUT"
Write-Host "NB3_TCP_LEGACY_SEND_OK_WAIT=BOUNDED_BY_TIMEOUT"
Write-Host "NB3_TCP_LEGACY_SEND_COMMAND=CHECKED_1000US"
Write-Host "NB3_TCP_LEGACY_HARDWARE_TIMEOUT=OBSERVABLE"
Write-Host "NB3_TCP_CLIENT_WRITE_TIMEOUT=USES_CONNECTION_TIMEOUT"
Write-Host "NB3_TCP_SERVER_WRITE_TIMEOUT=DEFAULT_1000MS"
Write-Host "NB3_TCP_PUBLIC_WRITE_API=PRESERVED"
Write-Host "NB3_SPI_FREQUENCY_CHANGE=NO"
Write-Host "NB3_UPLOAD=NO"
Write-Host "A14_NB3_F1_LEGACY_TCP_SEND_BOUNDS_APPLY_COMPILE=PASS"
