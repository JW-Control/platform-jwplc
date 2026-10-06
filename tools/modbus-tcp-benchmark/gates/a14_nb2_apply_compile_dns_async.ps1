param()

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")

Write-Host "============================================================"
Write-Host " A14 NB2-B - APPLY + COMPILE COOPERATIVE DNS ENGINE"
Write-Host "============================================================"

Assert-G2Branch

$dnsHeaderRelative = "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.h"
$dnsCppRelative = "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.cpp"
$clientHeaderRelative = "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_W5x00_Ethernet.h"
$clientCppRelative = "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetClient.cpp"

$dnsHeaderPath = Get-G2Path $dnsHeaderRelative
$dnsCppPath = Get-G2Path $dnsCppRelative

$expectedDirtyBefore = @(
    $clientHeaderRelative,
    $clientCppRelative,
    $script:G2SpiHeaderRelative,
    $script:G2RawFirmwareRelative
) | Sort-Object

$dirtyBefore = @(Get-G2TrackedDirtyPaths)
Write-Host "HEAD=$(Get-G2Head)"
Write-Host "EFFECTIVE_SPI_HZ=$(Get-G2SpiHz)"
Write-Host "TRACKED_DIRTY_BEFORE=$($dirtyBefore.Count)"
$dirtyBefore | ForEach-Object { Write-Host "DIRTY_BEFORE=$_" }

if ($dirtyBefore.Count -ne $expectedDirtyBefore.Count) {
    throw "A14_NB2B_DIRTY_COUNT_BEFORE_INVALID=$($dirtyBefore.Count)"
}
for ($i = 0; $i -lt $expectedDirtyBefore.Count; ++$i) {
    if ($dirtyBefore[$i] -ne $expectedDirtyBefore[$i]) {
        throw "A14_NB2B_DIRTY_PATH_BEFORE_INVALID=$($dirtyBefore[$i])"
    }
}

$stagedBefore = @(& git -C $script:G2RepoRoot diff --cached --name-only)
if ($LASTEXITCODE -ne 0 -or $stagedBefore.Count -ne 0) {
    throw "A14_NB2B_INDEX_NOT_CLEAN_BEFORE"
}
Write-Host "STAGED_COUNT_BEFORE=0"

foreach ($relative in @($dnsHeaderRelative, $dnsCppRelative)) {
    $existingDiff = @(& git -C $script:G2RepoRoot diff --name-only -- $relative)
    if ($LASTEXITCODE -ne 0) {
        throw "A14_NB2B_DIFF_CHECK_SOURCE_FAILED=$relative"
    }
    if ($existingDiff.Count -ne 0) {
        throw "A14_NB2B_DNS_SOURCE_ALREADY_DIRTY=$relative"
    }
}
Write-Host "DNS_SOURCE_CLEAN_BEFORE=YES"

Assert-G2ProtectedArtifacts

$dnsHeaderText = [System.IO.File]::ReadAllText($dnsHeaderPath)
$dnsCppText = [System.IO.File]::ReadAllText($dnsCppPath)

if ($dnsHeaderText.Contains("beginResolveAsync") -or
    $dnsCppText.Contains("DNSClient::beginResolveAsync")) {
    throw "A14_NB2B_DNS_ALREADY_PATCHED"
}

# ---- Dns.h public API ---------------------------------------------------

$publicAnchorPattern = '(?m)^([ \t]*int getHostByName\(const char\* aHostname, IPAddress& aResult, uint16_t timeout=5000\);[ \t]*)\r?$'
$publicAnchorRegex = [regex]::new($publicAnchorPattern)
$publicMatches = @($publicAnchorRegex.Matches($dnsHeaderText))
Write-Host "DNS_HEADER_PUBLIC_ANCHOR_COUNT=$($publicMatches.Count)"
if ($publicMatches.Count -ne 1) {
    throw "A14_NB2B_DNS_HEADER_PUBLIC_ANCHOR_INVALID=$($publicMatches.Count)"
}

$publicReplacement = @'
	int getHostByName(const char* aHostname, IPAddress& aResult, uint16_t timeout=5000);

	// JWPLC cooperative DNS extension.
	// begin/poll: negative = failed, 0 = pending, 1 = resolved.
	// The result object must remain alive while the request is pending.
	int beginResolveAsync(const char* aHostname,
	                     IPAddress& aResult,
	                     uint16_t timeout=5000);
	int pollResolveAsync();
	bool resolveAsyncInProgress() const;
	void cancelResolveAsync();
'@
$dnsHeaderText = $publicAnchorRegex.Replace($dnsHeaderText, $publicReplacement, 1)

$protectedAnchorPattern = '(?m)^([ \t]*uint16_t ProcessResponse\(uint16_t aTimeout, IPAddress& aAddress\);[ \t]*)\r?$'
$protectedAnchorRegex = [regex]::new($protectedAnchorPattern)
$protectedMatches = @($protectedAnchorRegex.Matches($dnsHeaderText))
Write-Host "DNS_HEADER_PROTECTED_ANCHOR_COUNT=$($protectedMatches.Count)"
if ($protectedMatches.Count -ne 1) {
    throw "A14_NB2B_DNS_HEADER_PROTECTED_ANCHOR_INVALID=$($protectedMatches.Count)"
}

$protectedReplacement = @'
	uint16_t ProcessResponse(uint16_t aTimeout, IPAddress& aAddress);
	int ProcessResponsePacket(IPAddress& aAddress);
	void finishResolveAsync(int result);
'@
$dnsHeaderText = $protectedAnchorRegex.Replace($dnsHeaderText, $protectedReplacement, 1)

$stateAnchorPattern = '(?m)^([ \t]*EthernetUDP iUdp;[ \t]*)\r?$'
$stateAnchorRegex = [regex]::new($stateAnchorPattern)
$stateMatches = @($stateAnchorRegex.Matches($dnsHeaderText))
Write-Host "DNS_HEADER_STATE_ANCHOR_COUNT=$($stateMatches.Count)"
if ($stateMatches.Count -ne 1) {
    throw "A14_NB2B_DNS_HEADER_STATE_ANCHOR_INVALID=$($stateMatches.Count)"
}

$stateReplacement = @'
	EthernetUDP iUdp;

	// Cooperative resolution state. The query is sent once, matching the
	// legacy implementation; its three wait windows are preserved as polls.
	IPAddress* iAsyncResult = nullptr;
	uint16_t iAsyncTimeout = 0;
	uint32_t iAsyncWaitStartMs = 0;
	uint8_t iAsyncWaitAttempt = 0;
	int iAsyncStatus = -4;
	bool iAsyncActive = false;
'@
$dnsHeaderText = $stateAnchorRegex.Replace($dnsHeaderText, $stateReplacement, 1)

# ---- Dns.cpp getHostByName + async engine ------------------------------

$getHostPattern = '(?ms)^int DNSClient::getHostByName\(const char\* aHostname, IPAddress& aResult, uint16_t timeout\)\r?\n\{.*?^\}\r?\n\r?\nuint16_t DNSClient::BuildRequest'
$getHostRegex = [regex]::new($getHostPattern)
$getHostMatches = @($getHostRegex.Matches($dnsCppText))
Write-Host "DNS_CPP_GETHOST_ANCHOR_COUNT=$($getHostMatches.Count)"
if ($getHostMatches.Count -ne 1) {
    throw "A14_NB2B_DNS_CPP_GETHOST_ANCHOR_INVALID=$($getHostMatches.Count)"
}

$getHostReplacement = @'
int DNSClient::getHostByName(const char* aHostname, IPAddress& aResult, uint16_t timeout)
{
	int state = beginResolveAsync(aHostname, aResult, timeout);

	// Preserve historical 0 for socket/transport setup failure.
	if (state == -11) {
		return 0;
	}

	while (state == 0) {
		delay(1);
		state = pollResolveAsync();
	}

	if (state == -11) {
		return 0;
	}

	return state;
}

int DNSClient::beginResolveAsync(
	const char* aHostname,
	IPAddress& aResult,
	uint16_t timeout)
{
	cancelResolveAsync();

	if (aHostname == nullptr) {
		iAsyncStatus = INVALID_RESPONSE;
		return iAsyncStatus;
	}

	if (inet_aton(aHostname, aResult)) {
		iAsyncStatus = SUCCESS;
		return iAsyncStatus;
	}

	if (iDNSServer == INADDR_NONE) {
		iAsyncStatus = INVALID_SERVER;
		return iAsyncStatus;
	}

	if (iUdp.begin(1024 + (millis() & 0xF)) != 1) {
		iAsyncStatus = -11;
		return iAsyncStatus;
	}

	int ret = iUdp.beginPacket(iDNSServer, DNS_PORT);
	if (ret == 0) {
		finishResolveAsync(-11);
		return -11;
	}

	ret = BuildRequest(aHostname);
	if (ret == 0) {
		finishResolveAsync(-11);
		return -11;
	}

	// The long DNS-response wait becomes cooperative here.
	// UDP endPacket/socketSendUDP remains synchronous until NB3.
	ret = iUdp.endPacket();
	if (ret == 0) {
		finishResolveAsync(-11);
		return -11;
	}

	iAsyncResult = &aResult;
	iAsyncTimeout = timeout;
	iAsyncWaitStartMs = millis();
	iAsyncWaitAttempt = 1;
	iAsyncStatus = 0;
	iAsyncActive = true;
	return 0;
}

int DNSClient::pollResolveAsync()
{
	if (!iAsyncActive) {
		return iAsyncStatus;
	}

	if (iAsyncResult == nullptr) {
		finishResolveAsync(INVALID_RESPONSE);
		return INVALID_RESPONSE;
	}

	const int packetSize = iUdp.parsePacket();
	if (packetSize > 0) {
		const int result = ProcessResponsePacket(*iAsyncResult);
		finishResolveAsync(result);
		return result;
	}

	if ((uint32_t)(millis() - iAsyncWaitStartMs) > iAsyncTimeout) {
		if (iAsyncWaitAttempt < 3) {
			++iAsyncWaitAttempt;
			iAsyncWaitStartMs = millis();
			return 0;
		}

		finishResolveAsync(TIMED_OUT);
		return TIMED_OUT;
	}

	return 0;
}

bool DNSClient::resolveAsyncInProgress() const
{
	return iAsyncActive;
}

void DNSClient::cancelResolveAsync()
{
	iUdp.stop();
	iAsyncResult = nullptr;
	iAsyncTimeout = 0;
	iAsyncWaitStartMs = 0;
	iAsyncWaitAttempt = 0;
	iAsyncStatus = INVALID_RESPONSE;
	iAsyncActive = false;
}

void DNSClient::finishResolveAsync(int result)
{
	iUdp.stop();
	iAsyncResult = nullptr;
	iAsyncTimeout = 0;
	iAsyncWaitStartMs = 0;
	iAsyncWaitAttempt = 0;
	iAsyncStatus = result;
	iAsyncActive = false;
}

uint16_t DNSClient::BuildRequest
'@
$dnsCppText = $getHostRegex.Replace($dnsCppText, $getHostReplacement, 1)

# ---- Split ProcessResponse wait from packet parser ---------------------

$processPattern = '(?ms)^uint16_t DNSClient::ProcessResponse\(uint16_t aTimeout, IPAddress& aAddress\)\r?\n\{\r?\n[ \t]*uint32_t startTime = millis\(\);\r?\n\r?\n[ \t]*// Wait for a response packet\r?\n[ \t]*while \(iUdp\.parsePacket\(\) <= 0\) \{\r?\n[ \t]*if \(\(millis\(\) - startTime\) > aTimeout\) \{\r?\n[ \t]*return TIMED_OUT;\r?\n[ \t]*\}\r?\n[ \t]*delay\(50\);\r?\n[ \t]*\}\r?\n\r?\n(?<body>[ \t]*// We''ve had a reply!.*)^\}'
$processRegex = [regex]::new($processPattern)
$processMatches = @($processRegex.Matches($dnsCppText))
Write-Host "DNS_CPP_PROCESS_RESPONSE_ANCHOR_COUNT=$($processMatches.Count)"
if ($processMatches.Count -ne 1) {
    throw "A14_NB2B_DNS_CPP_PROCESS_RESPONSE_ANCHOR_INVALID=$($processMatches.Count)"
}

$packetBody = $processMatches[0].Groups["body"].Value
$processReplacement = @'
uint16_t DNSClient::ProcessResponse(uint16_t aTimeout, IPAddress& aAddress)
{
	uint32_t startTime = millis();

	while (iUdp.parsePacket() <= 0) {
		if ((millis() - startTime) > aTimeout) {
			return TIMED_OUT;
		}
		delay(50);
	}

	return (uint16_t)ProcessResponsePacket(aAddress);
}

int DNSClient::ProcessResponsePacket(IPAddress& aAddress)
{
'@ + $packetBody + @'
}
'@

$dnsCppText = $processRegex.Replace(
    $dnsCppText,
    [System.Text.RegularExpressions.MatchEvaluator]{
        param($match)
        return $processReplacement
    },
    1)

$utf8NoBom = New-Object -TypeName System.Text.UTF8Encoding -ArgumentList $false
[System.IO.File]::WriteAllText($dnsHeaderPath, $dnsHeaderText, $utf8NoBom)
[System.IO.File]::WriteAllText($dnsCppPath, $dnsCppText, $utf8NoBom)

& git -C $script:G2RepoRoot diff --check
if ($LASTEXITCODE -ne 0) {
    throw "A14_NB2B_DIFF_CHECK_FAILED"
}

# ---- Static post-patch checks -----------------------------------------

$dnsHeaderVerify = [System.IO.File]::ReadAllText($dnsHeaderPath)
$dnsCppVerify = [System.IO.File]::ReadAllText($dnsCppPath)

foreach ($marker in @(
    "beginResolveAsync",
    "pollResolveAsync",
    "resolveAsyncInProgress",
    "cancelResolveAsync"
)) {
    $headerCount = ([regex]::Matches(
        $dnsHeaderVerify,
        [regex]::Escape($marker)
    )).Count
    $cppCount = ([regex]::Matches(
        $dnsCppVerify,
        [regex]::Escape("DNSClient::$marker")
    )).Count

    Write-Host "DNS_API=$marker HEADER_COUNT=$headerCount CPP_COUNT=$cppCount"

    if ($headerCount -ne 1 -or $cppCount -ne 1) {
        throw "A14_NB2B_DNS_API_MARKER_INVALID=$marker"
    }
}

$legacyGetHostCount = ([regex]::Matches(
    $dnsCppVerify,
    'int\s+DNSClient::getHostByName\s*\('
)).Count
$legacyProcessDelay50Count = ([regex]::Matches(
    $dnsCppVerify,
    'delay\s*\(\s*50\s*\)'
)).Count
$asyncPollBlockMatch = [regex]::Match(
    $dnsCppVerify,
    '(?ms)^int DNSClient::pollResolveAsync\(\)\r?\n\{.*?^\}')
$asyncPollDelayCount = 0
if ($asyncPollBlockMatch.Success) {
    $asyncPollDelayCount = ([regex]::Matches(
        $asyncPollBlockMatch.Value,
        'delay\s*\('
    )).Count
}

Write-Host "DNS_LEGACY_GETHOST_DEFINITION_COUNT=$legacyGetHostCount"
Write-Host "DNS_LEGACY_PROCESS_DELAY50_COUNT=$legacyProcessDelay50Count"
Write-Host "DNS_ASYNC_POLL_DELAY_COUNT=$asyncPollDelayCount"

if ($legacyGetHostCount -ne 1) {
    throw "A14_NB2B_LEGACY_GETHOST_INVALID"
}
if ($legacyProcessDelay50Count -ne 1) {
    throw "A14_NB2B_LEGACY_PROCESS_DELAY50_INVALID=$legacyProcessDelay50Count"
}
if (-not $asyncPollBlockMatch.Success -or $asyncPollDelayCount -ne 0) {
    throw "A14_NB2B_ASYNC_POLL_DELAY_CHECK_FAILED"
}

$expectedDirtyAfter = @(
    $clientHeaderRelative,
    $clientCppRelative,
    $dnsHeaderRelative,
    $dnsCppRelative,
    $script:G2SpiHeaderRelative,
    $script:G2RawFirmwareRelative
) | Sort-Object

$dirtyAfterPatch = @(Get-G2TrackedDirtyPaths)
Write-Host "TRACKED_DIRTY_AFTER_PATCH=$($dirtyAfterPatch.Count)"
$dirtyAfterPatch | ForEach-Object { Write-Host "DIRTY_AFTER_PATCH=$_" }

if ($dirtyAfterPatch.Count -ne $expectedDirtyAfter.Count) {
    throw "A14_NB2B_DIRTY_COUNT_AFTER_PATCH_INVALID=$($dirtyAfterPatch.Count)"
}
for ($i = 0; $i -lt $expectedDirtyAfter.Count; ++$i) {
    if ($dirtyAfterPatch[$i] -ne $expectedDirtyAfter[$i]) {
        throw "A14_NB2B_DIRTY_PATH_AFTER_PATCH_INVALID=$($dirtyAfterPatch[$i])"
    }
}

$stagedAfterPatch = @(& git -C $script:G2RepoRoot diff --cached --name-only)
if ($LASTEXITCODE -ne 0 -or $stagedAfterPatch.Count -ne 0) {
    throw "A14_NB2B_INDEX_NOT_CLEAN_AFTER_PATCH"
}
Write-Host "STAGED_COUNT_AFTER_PATCH=0"

Write-Host "DNS_HEADER_SHA256=$(Get-G2Sha256 $dnsHeaderRelative)"
Write-Host "DNS_CPP_SHA256=$(Get-G2Sha256 $dnsCppRelative)"

# ---- Compile exact patched sources; no upload -------------------------

function Invoke-NB2NativeToLog {
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

function Assert-NB2RepoEthernetUsed {
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
        throw ("A14_NB2B_{0}_WRONG_ETHERNET_LIBRARY" -f $Label)
    }
}

$arduinoCli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"
if (-not (Test-Path -LiteralPath $arduinoCli)) {
    throw "A14_NB2B_ARDUINO_CLI_NOT_FOUND=$arduinoCli"
}

$fqbn = "jwplc_local:esp32:jwplcbasic"
$librariesRoot = Get-G2Path "JWPLC/2.1.0/libraries"
$expectedEthernetLibrary = Join-Path $librariesRoot "JWPLC_Ethernet"
$rawFirmwarePath = Get-G2Path $script:G2RawFirmwareRelative
$rawSketchDir = Split-Path -Parent $rawFirmwarePath

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$tempRoot = Join-Path $env:TEMP ("jwplc_a14_nb2b_dns_compile_{0}" -f $timestamp)
$rawBuildPath = Join-Path $tempRoot "raw_build"
$probeSketchDir = Join-Path $tempRoot "dns_api_probe"
$probeBuildPath = Join-Path $tempRoot "probe_build"
$rawCompileLog = Join-Path $tempRoot "raw_compile.log"
$probeCompileLog = Join-Path $tempRoot "probe_compile.log"

New-Item -ItemType Directory -Force -Path $rawBuildPath | Out-Null
New-Item -ItemType Directory -Force -Path $probeSketchDir | Out-Null
New-Item -ItemType Directory -Force -Path $probeBuildPath | Out-Null

$probeSketchPath = Join-Path $probeSketchDir "dns_api_probe.ino"
$probeSketch = @'
#include <JWPLC_Ethernet.h>
#include <Dns.h>

DNSClient dnsProbe;
IPAddress dnsResult;
volatile int dnsSink = 0;

static void compileOnlyDnsAsyncProbe()
{
    dnsProbe.begin(IPAddress(192, 168, 0, 1));

    dnsSink += dnsProbe.beginResolveAsync(
        "example.invalid",
        dnsResult,
        25);

    dnsSink += dnsProbe.pollResolveAsync();
    dnsSink += dnsProbe.resolveAsyncInProgress() ? 1 : 0;
    dnsProbe.cancelResolveAsync();

    dnsSink += dnsProbe.getHostByName(
        "127.0.0.1",
        dnsResult,
        25);
}

void setup()
{
    if (false)
    {
        compileOnlyDnsAsyncProbe();
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
Write-Host "SOURCE_MUTATION=DNS_H_DNS_CPP_ONLY"
Write-Host "UPLOAD=NO"

Write-Host ""
Write-Host "=== COMPILE RAW CANDIDATE WITH COOPERATIVE DNS ==="

$rawCompileArgs = @(
    "compile",
    "--fqbn", $fqbn,
    "--build-path", $rawBuildPath,
    "--libraries", $librariesRoot,
    $rawSketchDir
)

$rawCompileExit = Invoke-NB2NativeToLog -FilePath $arduinoCli -Arguments $rawCompileArgs -LogPath $rawCompileLog

Write-Host "RAW_COMPILE_EXIT=$rawCompileExit"
Write-Host "RAW_COMPILE_LOG=$rawCompileLog"

if ($rawCompileExit -ne 0) {
    Invoke-G2CompileFinishedSound -Success $false
    Get-Content -LiteralPath $rawCompileLog -Tail 120 |
        ForEach-Object { Write-Host $_ }
    throw "A14_NB2B_RAW_COMPILE_FAILED"
}

Assert-NB2RepoEthernetUsed -LogPath $rawCompileLog -ExpectedLibraryPath $expectedEthernetLibrary -Label "RAW"

Write-Host ""
Write-Host "=== COMPILE DNS ASYNC API PROBE ==="

$probeCompileArgs = @(
    "compile",
    "--fqbn", $fqbn,
    "--build-path", $probeBuildPath,
    "--libraries", $librariesRoot,
    $probeSketchDir
)

$probeCompileExit = Invoke-NB2NativeToLog -FilePath $arduinoCli -Arguments $probeCompileArgs -LogPath $probeCompileLog

Write-Host "API_PROBE_COMPILE_EXIT=$probeCompileExit"
Write-Host "API_PROBE_COMPILE_LOG=$probeCompileLog"

if ($probeCompileExit -ne 0) {
    Invoke-G2CompileFinishedSound -Success $false
    Get-Content -LiteralPath $probeCompileLog -Tail 120 |
        ForEach-Object { Write-Host $_ }
    throw "A14_NB2B_API_PROBE_COMPILE_FAILED"
}

Assert-NB2RepoEthernetUsed -LogPath $probeCompileLog -ExpectedLibraryPath $expectedEthernetLibrary -Label "API_PROBE"

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
    throw "A14_NB2B_BIN_OUTPUT_MISSING"
}

Invoke-G2CompileFinishedSound -Success $true
Write-Host "COMPILE_FINISHED_SOUND=PLAYED"

Assert-G2ProtectedArtifacts

$dirtyFinal = @(Get-G2TrackedDirtyPaths)
$stagedFinal = @(& git -C $script:G2RepoRoot diff --cached --name-only)

Write-Host "TRACKED_DIRTY_COUNT_FINAL=$($dirtyFinal.Count)"
$dirtyFinal | ForEach-Object { Write-Host "DIRTY_FINAL=$_" }
Write-Host "STAGED_COUNT_FINAL=$($stagedFinal.Count)"

if ($dirtyFinal.Count -ne $expectedDirtyAfter.Count -or
    $stagedFinal.Count -ne 0) {
    throw "A14_NB2B_FINAL_WORKTREE_INVALID"
}
for ($i = 0; $i -lt $expectedDirtyAfter.Count; ++$i) {
    if ($dirtyFinal[$i] -ne $expectedDirtyAfter[$i]) {
        throw "A14_NB2B_FINAL_DIRTY_PATH_INVALID=$($dirtyFinal[$i])"
    }
}

Write-Host "NB2_DNS_ASYNC_ENGINE=ADDED"
Write-Host "NB2_DNS_ASYNC_POLL_DELAY=ABSENT"
Write-Host "NB2_DNS_LEGACY_GETHOST=WRAPPED_OVER_ASYNC_ENGINE"
Write-Host "NB2_DNS_WAIT_WINDOWS=3_PRESERVED"
Write-Host "NB2_DNS_RESPONSE_TIMEOUT_MS=5000_DEFAULT_PRESERVED"
Write-Host "NB2_DNS_UDP_SEND_SYNC_DEPENDENCY=PENDING_NB3"
Write-Host "NB2_UPLOAD=NO"
Write-Host "A14_NB2_DNS_ASYNC_APPLY_COMPILE=PASS"
