param()

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")

Write-Host "============================================================"
Write-Host " A14 NB1 - APPLY ETHERNETCLIENT ASYNC LIFECYCLE"
Write-Host "============================================================"

Assert-G2Branch

$headerRelative = "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_W5x00_Ethernet.h"
$cppRelative = "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetClient.cpp"
$headerPath = Get-G2Path $headerRelative
$cppPath = Get-G2Path $cppRelative

$dirtyBefore = @(Get-G2TrackedDirtyPaths)
$expectedDirtyBefore = @(
    $script:G2SpiHeaderRelative,
    $script:G2RawFirmwareRelative
) | Sort-Object

Write-Host "HEAD=$(Get-G2Head)"
Write-Host "TRACKED_DIRTY_BEFORE=$($dirtyBefore.Count)"
$dirtyBefore | ForEach-Object { Write-Host "DIRTY_BEFORE=$_" }

if ($dirtyBefore.Count -ne $expectedDirtyBefore.Count) {
    throw "A14_NB1_DIRTY_COUNT_BEFORE_INVALID=$($dirtyBefore.Count)"
}
for ($i = 0; $i -lt $expectedDirtyBefore.Count; ++$i) {
    if ($dirtyBefore[$i] -ne $expectedDirtyBefore[$i]) {
        throw "A14_NB1_DIRTY_PATH_BEFORE_INVALID=$($dirtyBefore[$i])"
    }
}

$stagedBefore = @(& git -C $script:G2RepoRoot diff --cached --name-only)
if ($LASTEXITCODE -ne 0 -or $stagedBefore.Count -ne 0) {
    throw "A14_NB1_INDEX_NOT_CLEAN_BEFORE"
}
Write-Host "STAGED_COUNT_BEFORE=0"

foreach ($relative in @($headerRelative, $cppRelative)) {
    $existingDiff = @(& git -C $script:G2RepoRoot diff --name-only -- $relative)
    if ($LASTEXITCODE -ne 0) {
        throw "A14_NB1_DIFF_CHECK_SOURCE_FAILED=$relative"
    }
    if ($existingDiff.Count -ne 0) {
        throw "A14_NB1_SOURCE_ALREADY_DIRTY=$relative"
    }
}

Assert-G2ProtectedArtifacts

$headerText = [System.IO.File]::ReadAllText($headerPath)
$cppText = [System.IO.File]::ReadAllText($cppPath)

if ($headerText.Contains("beginStopAsync") -or $headerText.Contains("beginFlushAsync")) {
    throw "A14_NB1_HEADER_ALREADY_PATCHED"
}
if ($cppText.Contains("EthernetClient::beginStopAsync") -or $cppText.Contains("EthernetClient::beginFlushAsync")) {
    throw "A14_NB1_CPP_ALREADY_PATCHED"
}

$declPattern = '(?m)^([ \t]*void cancelConnectAsync\(\);[ \t]*)\r?$'
$declRegex = [regex]::new($declPattern)
$declMatches = @($declRegex.Matches($headerText))
Write-Host "HEADER_DECLARATION_ANCHOR_COUNT=$($declMatches.Count)"
if ($declMatches.Count -ne 1) {
    throw "A14_NB1_HEADER_DECLARATION_ANCHOR_INVALID=$($declMatches.Count)"
}

$declReplacement = @'
	void cancelConnectAsync();

	// JWPLC cooperative TCP-close extension.
	// begin/poll: -1 = forced close after timeout/error, 0 = pending, 1 = closed.
	// Legacy stop() remains blocking and source-compatible, but uses this engine.
	int beginStopAsync();
	int pollStopAsync();
	bool stopAsyncInProgress() const;
	void cancelStopAsync();

	// JWPLC cooperative TX-flush extension.
	// begin/poll: -1 = timeout/error, 0 = pending, 1 = flushed/not connected.
	// Legacy flush() remains blocking and source-compatible, but is now bounded
	// by the configured connection timeout and uses this engine.
	int beginFlushAsync();
	int pollFlushAsync();
	bool flushAsyncInProgress() const;
	void cancelFlushAsync();
'@
$headerText = $declRegex.Replace($headerText, $declReplacement, 1)

$privatePattern = '(?m)^([ \t]*uint16_t _timeout;[ \t]*)\r?$'
$privateRegex = [regex]::new($privatePattern)
$privateMatches = @($privateRegex.Matches($headerText))
Write-Host "HEADER_PRIVATE_ANCHOR_COUNT=$($privateMatches.Count)"
if ($privateMatches.Count -ne 1) {
    throw "A14_NB1_HEADER_PRIVATE_ANCHOR_INVALID=$($privateMatches.Count)"
}

$privateReplacement = @'
	uint16_t _timeout;
	bool _stopPending = false;
	uint32_t _stopStartedAtMs = 0;
	bool _flushPending = false;
	uint32_t _flushStartedAtMs = 0;
'@
$headerText = $privateRegex.Replace($headerText, $privateReplacement, 1)

$connectPattern = '(?m)^int EthernetClient::beginConnectAsync\(IPAddress ip, uint16_t port\)[ \t]*\r?\n\{[ \t]*\r?$'
$connectRegex = [regex]::new($connectPattern)
$connectMatches = @($connectRegex.Matches($cppText))
Write-Host "CPP_CONNECT_ANCHOR_COUNT=$($connectMatches.Count)"
if ($connectMatches.Count -ne 1) {
    throw "A14_NB1_CPP_CONNECT_ANCHOR_INVALID=$($connectMatches.Count)"
}

$connectReplacement = @'
int EthernetClient::beginConnectAsync(IPAddress ip, uint16_t port)
{
	// Una nueva conexión invalida cualquier lifecycle cooperativo anterior.
	_stopPending = false;
	_stopStartedAtMs = 0;
	_flushPending = false;
	_flushStartedAtMs = 0;
'@
$cppText = $connectRegex.Replace($cppText, $connectReplacement, 1)

$lifecyclePattern = '(?ms)^void EthernetClient::flush\(\)[ \t]*\r?\n\{.*?^\}[ \t]*\r?\n[ \t]*\r?\nvoid EthernetClient::stop\(\)[ \t]*\r?\n\{.*?^\}'
$lifecycleRegex = [regex]::new($lifecyclePattern)
$lifecycleMatches = @($lifecycleRegex.Matches($cppText))
Write-Host "CPP_LIFECYCLE_ANCHOR_COUNT=$($lifecycleMatches.Count)"
if ($lifecycleMatches.Count -ne 1) {
    throw "A14_NB1_CPP_LIFECYCLE_ANCHOR_INVALID=$($lifecycleMatches.Count)"
}

$lifecycleReplacement = @'
int EthernetClient::beginFlushAsync()
{
	if (_stopPending) return -1;

	if (_sockindex >= MAX_SOCK_NUM) {
		_flushPending = false;
		_flushStartedAtMs = 0;
		return 1;
	}

	_flushPending = true;
	_flushStartedAtMs = millis();
	return pollFlushAsync();
}

int EthernetClient::pollFlushAsync()
{
	if (!_flushPending) {
		return (_sockindex >= MAX_SOCK_NUM) ? 1 : -1;
	}

	if (_sockindex >= MAX_SOCK_NUM) {
		_flushPending = false;
		_flushStartedAtMs = 0;
		return 1;
	}

	const uint8_t stat = Ethernet.socketStatus(_sockindex);
	if (stat != SnSR::ESTABLISHED && stat != SnSR::CLOSE_WAIT) {
		_flushPending = false;
		_flushStartedAtMs = 0;
		return 1;
	}

	if (Ethernet.socketSendAvailable(_sockindex) >= W5100.SSIZE) {
		_flushPending = false;
		_flushStartedAtMs = 0;
		return 1;
	}

	if ((uint32_t)(millis() - _flushStartedAtMs) >= _timeout) {
		_flushPending = false;
		_flushStartedAtMs = 0;
		return -1;
	}

	return 0;
}

bool EthernetClient::flushAsyncInProgress() const
{
	return _flushPending;
}

void EthernetClient::cancelFlushAsync()
{
	_flushPending = false;
	_flushStartedAtMs = 0;
}

void EthernetClient::flush()
{
	int state = beginFlushAsync();
	while (state == 0) {
		delay(1);
		state = pollFlushAsync();
	}
}

int EthernetClient::beginStopAsync()
{
	cancelFlushAsync();

	if (_sockindex >= MAX_SOCK_NUM) {
		_stopPending = false;
		_stopStartedAtMs = 0;
		return 1;
	}

	if (_stopPending) {
		return pollStopAsync();
	}

	const uint8_t stat = Ethernet.socketStatus(_sockindex);
	if (stat == SnSR::CLOSED) {
		_sockindex = MAX_SOCK_NUM;
		_stopPending = false;
		_stopStartedAtMs = 0;
		return 1;
	}

	// Dispara FIN/DISCON una sola vez. La espera se realiza mediante poll.
	Ethernet.socketDisconnect(_sockindex);
	_stopStartedAtMs = millis();
	_stopPending = true;
	return 0;
}

int EthernetClient::pollStopAsync()
{
	if (!_stopPending) {
		return (_sockindex >= MAX_SOCK_NUM) ? 1 : -1;
	}

	if (_sockindex >= MAX_SOCK_NUM) {
		_stopPending = false;
		_stopStartedAtMs = 0;
		return 1;
	}

	if (Ethernet.socketStatus(_sockindex) == SnSR::CLOSED) {
		_sockindex = MAX_SOCK_NUM;
		_stopPending = false;
		_stopStartedAtMs = 0;
		return 1;
	}

	if ((uint32_t)(millis() - _stopStartedAtMs) >= _timeout) {
		// Conserva la semántica legacy: al vencer timeout se fuerza CLOSE.
		Ethernet.socketClose(_sockindex);
		_sockindex = MAX_SOCK_NUM;
		_stopPending = false;
		_stopStartedAtMs = 0;
		return -1;
	}

	return 0;
}

bool EthernetClient::stopAsyncInProgress() const
{
	return _stopPending;
}

void EthernetClient::cancelStopAsync()
{
	cancelFlushAsync();

	if (_sockindex < MAX_SOCK_NUM) {
		Ethernet.socketClose(_sockindex);
	}

	_sockindex = MAX_SOCK_NUM;
	_stopPending = false;
	_stopStartedAtMs = 0;
}

void EthernetClient::stop()
{
	int state = beginStopAsync();
	while (state == 0) {
		delay(1);
		state = pollStopAsync();
	}
}
'@
$cppText = $lifecycleRegex.Replace($cppText, $lifecycleReplacement, 1)

$utf8NoBom = New-Object -TypeName System.Text.UTF8Encoding -ArgumentList $false
[System.IO.File]::WriteAllText($headerPath, $headerText, $utf8NoBom)
[System.IO.File]::WriteAllText($cppPath, $cppText, $utf8NoBom)

& git -C $script:G2RepoRoot diff --check
if ($LASTEXITCODE -ne 0) {
    throw "A14_NB1_DIFF_CHECK_FAILED"
}

$headerVerify = [System.IO.File]::ReadAllText($headerPath)
$cppVerify = [System.IO.File]::ReadAllText($cppPath)
foreach ($marker in @(
    "beginStopAsync",
    "pollStopAsync",
    "stopAsyncInProgress",
    "cancelStopAsync",
    "beginFlushAsync",
    "pollFlushAsync",
    "flushAsyncInProgress",
    "cancelFlushAsync"
)) {
    $headerCount = ([regex]::Matches($headerVerify, [regex]::Escape($marker))).Count
    $cppCount = ([regex]::Matches($cppVerify, [regex]::Escape("EthernetClient::$marker"))).Count
    Write-Host "MARKER=$marker HEADER_COUNT=$headerCount CPP_COUNT=$cppCount"
    if ($headerCount -ne 1 -or $cppCount -ne 1) {
        throw "A14_NB1_MARKER_COUNT_INVALID=$marker"
    }
}

$dirtyAfter = @(Get-G2TrackedDirtyPaths)
$expectedDirtyAfter = @(
    $script:G2SpiHeaderRelative,
    $script:G2RawFirmwareRelative,
    $headerRelative,
    $cppRelative
) | Sort-Object

Write-Host "TRACKED_DIRTY_AFTER=$($dirtyAfter.Count)"
$dirtyAfter | ForEach-Object { Write-Host "DIRTY_AFTER=$_" }
if ($dirtyAfter.Count -ne $expectedDirtyAfter.Count) {
    throw "A14_NB1_DIRTY_COUNT_AFTER_INVALID=$($dirtyAfter.Count)"
}
for ($i = 0; $i -lt $expectedDirtyAfter.Count; ++$i) {
    if ($dirtyAfter[$i] -ne $expectedDirtyAfter[$i]) {
        throw "A14_NB1_DIRTY_PATH_AFTER_INVALID=$($dirtyAfter[$i])"
    }
}

$stagedAfter = @(& git -C $script:G2RepoRoot diff --cached --name-only)
if ($LASTEXITCODE -ne 0 -or $stagedAfter.Count -ne 0) {
    throw "A14_NB1_INDEX_NOT_CLEAN_AFTER"
}
Write-Host "STAGED_COUNT_AFTER=0"

$headerNumstat = @(& git -C $script:G2RepoRoot diff --numstat -- $headerRelative)
$cppNumstat = @(& git -C $script:G2RepoRoot diff --numstat -- $cppRelative)
Write-Host "HEADER_DIFF_NUMSTAT=$($headerNumstat -join ';')"
Write-Host "CPP_DIFF_NUMSTAT=$($cppNumstat -join ';')"
Write-Host "HEADER_SHA256=$(Get-G2Sha256 $headerRelative)"
Write-Host "CPP_SHA256=$(Get-G2Sha256 $cppRelative)"

Assert-G2ProtectedArtifacts

Write-Host "NB1_STOP_ASYNC=ADDED"
Write-Host "NB1_FLUSH_ASYNC=ADDED"
Write-Host "NB1_LEGACY_STOP=WRAPPED_OVER_ASYNC_ENGINE"
Write-Host "NB1_LEGACY_FLUSH=BOUNDED_BY_CONNECTION_TIMEOUT"
Write-Host "A14_NB1_APPLY_CLIENT_ASYNC=PASS"
