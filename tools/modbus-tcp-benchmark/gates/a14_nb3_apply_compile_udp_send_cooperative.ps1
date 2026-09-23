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
        throw "A14_NB3D_PATCH_ANCHOR_MISSING=$Label"
    }

    $second = $Text.IndexOf(
        $Old,
        $first + $Old.Length,
        [System.StringComparison]::Ordinal
    )

    if ($second -ge 0) {
        throw "A14_NB3D_PATCH_ANCHOR_NOT_UNIQUE=$Label"
    }

    return $Text.Substring(0, $first) +
        $New +
        $Text.Substring($first + $Old.Length)
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
    $used = (
        $text.IndexOf(
            $ExpectedLibraryPath,
            [System.StringComparison]::OrdinalIgnoreCase
        ) -ge 0
    )

    Write-Host "$($Label)_REPO_ETHERNET_LIBRARY_USED=$used"

    if (-not $used) {
        throw "A14_NB3D_WRONG_ETHERNET_LIBRARY=$Label"
    }
}

Write-Host "============================================================"
Write-Host " A14 NB3-D1 - APPLY + COMPILE UDP SEND COOPERATIVE ENGINE"
Write-Host "============================================================"

Assert-G2Branch

$head = Get-G2Head
$effectiveHz = Get-G2SpiHz

Write-Host "HEAD=$head"
Write-Host "EFFECTIVE_SPI_HZ=$effectiveHz"
Write-Host "UPLOAD=NO"

if ($effectiveHz -ne 26000000) {
    throw "A14_NB3D_SPI_FREQUENCY_MISMATCH"
}

$dnsCppRelative = "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.cpp"
$dnsHeaderRelative = "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.h"
$mainHeaderRelative = "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_W5x00_Ethernet.h"
$socketRelative = "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/socket.cpp"
$udpCppRelative = "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetUdp.cpp"

$expectedDirtyBefore = @(
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.cpp",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.h",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetClient.cpp",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_W5x00_Ethernet.h",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/jwplc_ethernet_async_tx.cpp",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/jwplc_ethernet_async_tx.h",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/socket.cpp",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.cpp",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.h",
    "tools/modbus-tcp-benchmark/firmware/eth14_raw_transport_server/eth14_raw_transport_server.ino"
) | Sort-Object

$expectedDirtyAfter = @(
    $expectedDirtyBefore +
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetUdp.cpp"
) | Sort-Object

$dirtyBefore = @(Get-G2TrackedDirtyPaths)
$resumePatched = $false

Write-Host "TRACKED_DIRTY_BEFORE=$($dirtyBefore.Count)"
$dirtyBefore | ForEach-Object { Write-Host "DIRTY_BEFORE=$_" }

if ($dirtyBefore.Count -eq $expectedDirtyBefore.Count) {
    for ($i = 0; $i -lt $expectedDirtyBefore.Count; ++$i) {
        if ($dirtyBefore[$i] -ne $expectedDirtyBefore[$i]) {
            throw "A14_NB3D_DIRTY_BEFORE_PATH_INVALID=$($dirtyBefore[$i])"
        }
    }
}
elseif ($dirtyBefore.Count -eq $expectedDirtyAfter.Count) {
    for ($i = 0; $i -lt $expectedDirtyAfter.Count; ++$i) {
        if ($dirtyBefore[$i] -ne $expectedDirtyAfter[$i]) {
            throw "A14_NB3D_RESUME_DIRTY_PATH_INVALID=$($dirtyBefore[$i])"
        }
    }

    $resumePatched = $true
    Write-Host "NB3_D1_RESUME_PATCHED_STATE=YES"
}
else {
    throw "A14_NB3D_DIRTY_COUNT_INVALID=$($dirtyBefore.Count)"
}

$stagedBefore = @(& git -C $script:G2RepoRoot diff --cached --name-only)
if ($LASTEXITCODE -ne 0 -or $stagedBefore.Count -ne 0) {
    throw "A14_NB3D_INDEX_NOT_CLEAN_BEFORE"
}
Write-Host "STAGED_COUNT_BEFORE=0"

Assert-G2ProtectedArtifacts

$dnsCppPath = Get-G2Path $dnsCppRelative
$dnsHeaderPath = Get-G2Path $dnsHeaderRelative
$mainHeaderPath = Get-G2Path $mainHeaderRelative
$socketPath = Get-G2Path $socketRelative
$udpCppPath = Get-G2Path $udpCppRelative

if (-not $resumePatched) {
    $expectedPreHashes = [ordered]@{
        "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.cpp" = "3DE3CD1A14BD07C1FE3B0725941B2137A5109B43647AA3E10B3EF6A1E6EEC5F5"
        "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.h" = "EF6C5392C0CBFB8545BFF8EB4D30E3FEDAC005725DF292418A5CF974BE6F6FD2"
        "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_W5x00_Ethernet.h" = "3E11094D68873D81F264F8AD97C1D55867F5399E28613061BC38484CD7DE614B"
        "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/socket.cpp" = "EF450C75880A2427494585E8EB1C1E3FD62EC7B9562E37A3AA75C48761886104"
    }

    foreach ($entry in $expectedPreHashes.GetEnumerator()) {
        $actual = Get-G2Sha256 $entry.Key
        Write-Host "PRE_SHA256=$($entry.Key)=$actual"

        if ($actual -ne $entry.Value) {
            throw "A14_NB3D_PRE_HASH_MISMATCH=$($entry.Key)"
        }
    }

    $udpDiff = @(& git -C $script:G2RepoRoot diff --name-only -- $udpCppRelative)
    if ($LASTEXITCODE -ne 0) {
        throw "A14_NB3D_UDP_CPP_DIFF_CHECK_FAILED"
    }
    if ($udpDiff.Count -ne 0) {
        throw "A14_NB3D_UDP_CPP_NOT_CLEAN_BEFORE"
    }

    Write-Host "NB3_D1_UDP_CPP_TRACKED_CLEAN_BEFORE=YES"
}

$dnsCpp = [System.IO.File]::ReadAllText($dnsCppPath)
$dnsHeader = [System.IO.File]::ReadAllText($dnsHeaderPath)
$mainHeader = [System.IO.File]::ReadAllText($mainHeaderPath)
$socketCpp = [System.IO.File]::ReadAllText($socketPath)
$udpCpp = [System.IO.File]::ReadAllText($udpCppPath)

$utf8NoBom = New-Object -TypeName System.Text.UTF8Encoding -ArgumentList $false
Write-Host "NB3_D1_SHARED_RUNTIME_INIT=READY"

if ($resumePatched) {
    Write-Host "NB3_D1_PATCH_APPLICATION=SKIPPED_ALREADY_APPLIED"
}
else {
$oldSocketDecl = @'
	static bool socketSendUDP(uint8_t s);
'@

$newSocketDecl = @'
	// JWPLC cooperative UDP SEND backend.
	// begin/poll: -1 = error/timeout, 0 = pending, 1 = SEND_OK.
	static int socketBeginSendUDP(uint8_t s);
	static int socketPollSendUDP(uint8_t s);

	// Arduino-compatible blocking wrapper over the same cooperative engine.
	static bool socketSendUDP(uint8_t s);
'@

$mainHeader = Replace-ExactOnce -Text $mainHeader -Old $oldSocketDecl -New $newSocketDecl -Label "SOCKET_UDP_DECL"

$oldUdpPrivate = @'
	uint16_t _offset; // offset into the packet being sent
'@

$newUdpPrivate = @'
	uint16_t _offset; // offset into the packet being sent
	bool _sendPending = false; // JWPLC cooperative UDP SEND state
'@

$mainHeader = Replace-ExactOnce -Text $mainHeader -Old $oldUdpPrivate -New $newUdpPrivate -Label "UDP_SEND_STATE"

$oldUdpPublic = @'
	virtual int endPacket();
	virtual size_t write(uint8_t);
'@

$newUdpPublic = @'
	// JWPLC cooperative UDP SEND extension.
	// begin/poll: -1 = failed, 0 = pending, 1 = SEND_OK.
	int beginEndPacketAsync();
	int pollEndPacketAsync();
	bool endPacketAsyncInProgress() const;
	void cancelEndPacketAsync();

	virtual int endPacket();
	virtual size_t write(uint8_t);
'@

$mainHeader = Replace-ExactOnce -Text $mainHeader -Old $oldUdpPublic -New $newUdpPublic -Label "UDP_ASYNC_PUBLIC_API"

$oldSocketSend = @'
bool EthernetClass::socketSendUDP(uint8_t s)
{
	SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
	W5100.execCmdSn(s, Sock_SEND);

	/* +2008.01 bj */
	while ( (W5100.readSnIR(s) & SnIR::SEND_OK) != SnIR::SEND_OK ) {
		if (W5100.readSnIR(s) & SnIR::TIMEOUT) {
			/* +2008.01 [bj]: clear interrupt */
			W5100.writeSnIR(s, (SnIR::SEND_OK|SnIR::TIMEOUT));
			SPI.endTransaction();
			//Serial.printf("sendUDP timeout\n");
			return false;
		}
		SPI.endTransaction();
		yield();
		SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
	}

	/* +2008.01 bj */
	W5100.writeSnIR(s, SnIR::SEND_OK);
	SPI.endTransaction();

	//Serial.printf("sendUDP ok\n");
	/* Sent ok */
	return true;
}
'@

$newSocketSend = @'
int EthernetClass::socketBeginSendUDP(uint8_t s)
{
	if (s >= MAX_SOCK_NUM) {
		return -1;
	}

	SPI.beginTransaction(SPI_ETHERNET_SETTINGS);

	// Clear stale terminal flags from a previous datagram before SEND.
	W5100.writeSnIR(s, (uint8_t)(SnIR::SEND_OK | SnIR::TIMEOUT));

	const bool commandAccepted =
		W5100.execCmdSnChecked(
			s,
			Sock_SEND,
			1000);

	SPI.endTransaction();

	return commandAccepted ? 0 : -1;
}

int EthernetClass::socketPollSendUDP(uint8_t s)
{
	if (s >= MAX_SOCK_NUM) {
		return -1;
	}

	SPI.beginTransaction(SPI_ETHERNET_SETTINGS);

	const uint8_t interruptFlags =
		W5100.readSnIR(s);

	if ((interruptFlags & SnIR::SEND_OK) != 0) {
		W5100.writeSnIR(s, SnIR::SEND_OK);
		SPI.endTransaction();
		return 1;
	}

	if ((interruptFlags & SnIR::TIMEOUT) != 0) {
		W5100.writeSnIR(
			s,
			(uint8_t)(SnIR::SEND_OK | SnIR::TIMEOUT));
		SPI.endTransaction();
		return -1;
	}

	const uint8_t status = W5100.readSnSR(s);
	SPI.endTransaction();

	if (status != SnSR::UDP) {
		return -1;
	}

	return 0;
}

bool EthernetClass::socketSendUDP(uint8_t s)
{
	int state = socketBeginSendUDP(s);

	while (state == 0) {
		yield();
		state = socketPollSendUDP(s);
	}

	return state == 1;
}
'@

$socketCpp = Replace-ExactOnce -Text $socketCpp -Old $oldSocketSend -New $newSocketSend -Label "SOCKET_SEND_UDP_ENGINE"

$oldUdpStop = @'
void EthernetUDP::stop()
{
	if (sockindex < MAX_SOCK_NUM) {
		Ethernet.socketClose(sockindex);
		sockindex = MAX_SOCK_NUM;
	}
}
'@

$newUdpStop = @'
void EthernetUDP::stop()
{
	_sendPending = false;

	if (sockindex < MAX_SOCK_NUM) {
		Ethernet.socketClose(sockindex);
		sockindex = MAX_SOCK_NUM;
	}
}
'@

$udpCpp = Replace-ExactOnce -Text $udpCpp -Old $oldUdpStop -New $newUdpStop -Label "UDP_STOP_STATE"

$oldUdpBeginPacket = @'
int EthernetUDP::beginPacket(IPAddress ip, uint16_t port)
{
	_offset = 0;
	//Serial.printf("UDP beginPacket\n");
	return Ethernet.socketStartUDP(sockindex, rawIPAddress(ip), port);
}

int EthernetUDP::endPacket()
{
	return Ethernet.socketSendUDP(sockindex);
}
'@

$newUdpBeginPacket = @'
int EthernetUDP::beginPacket(IPAddress ip, uint16_t port)
{
	if (_sendPending) {
		return 0;
	}

	_offset = 0;
	//Serial.printf("UDP beginPacket\n");
	return Ethernet.socketStartUDP(sockindex, rawIPAddress(ip), port);
}

int EthernetUDP::beginEndPacketAsync()
{
	if (_sendPending) {
		return 0;
	}

	if (sockindex >= MAX_SOCK_NUM) {
		return -1;
	}

	const int state =
		Ethernet.socketBeginSendUDP(sockindex);

	if (state < 0) {
		_sendPending = false;
		return -1;
	}

	_sendPending = (state == 0);
	return state;
}

int EthernetUDP::pollEndPacketAsync()
{
	if (!_sendPending || sockindex >= MAX_SOCK_NUM) {
		return -1;
	}

	const int state =
		Ethernet.socketPollSendUDP(sockindex);

	if (state != 0) {
		_sendPending = false;
	}

	return state;
}

bool EthernetUDP::endPacketAsyncInProgress() const
{
	return _sendPending;
}

void EthernetUDP::cancelEndPacketAsync()
{
	// A W5500 SEND already issued cannot be withdrawn here. Clearing local
	// state is safe; the next begin clears stale SEND_OK/TIMEOUT flags.
	_sendPending = false;
}

int EthernetUDP::endPacket()
{
	int state = beginEndPacketAsync();

	while (state == 0) {
		yield();
		state = pollEndPacketAsync();
	}

	return state == 1 ? 1 : 0;
}
'@

$udpCpp = Replace-ExactOnce -Text $udpCpp -Old $oldUdpBeginPacket -New $newUdpBeginPacket -Label "UDP_ASYNC_ENGINE"

$oldDnsState = @'
	bool iAsyncActive = false;
'@

$newDnsState = @'
	bool iAsyncActive = false;
	bool iAsyncSendPending = false;
'@

$dnsHeader = Replace-ExactOnce -Text $dnsHeader -Old $oldDnsState -New $newDnsState -Label "DNS_ASYNC_SEND_STATE"

$oldDnsSend = @'
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
'@

$newDnsSend = @'
	// Start UDP SEND without waiting for SEND_OK. The poll path below
	// completes SEND cooperatively before starting the DNS response timer.
	ret = iUdp.beginEndPacketAsync();
	if (ret < 0) {
		finishResolveAsync(-11);
		return -11;
	}

	iAsyncResult = &aResult;
	iAsyncTimeout = timeout;
	iAsyncWaitStartMs = (ret == 1) ? millis() : 0;
	iAsyncWaitAttempt = 1;
	iAsyncStatus = 0;
	iAsyncActive = true;
	iAsyncSendPending = (ret == 0);
	return 0;
'@

$dnsCpp = Replace-ExactOnce -Text $dnsCpp -Old $oldDnsSend -New $newDnsSend -Label "DNS_BEGIN_UDP_SEND_ASYNC"

$oldDnsPoll = @'
	if (iAsyncResult == nullptr) {
		finishResolveAsync(INVALID_RESPONSE);
		return INVALID_RESPONSE;
	}

	const int packetSize = iUdp.parsePacket();
'@

$newDnsPoll = @'
	if (iAsyncResult == nullptr) {
		finishResolveAsync(INVALID_RESPONSE);
		return INVALID_RESPONSE;
	}

	if (iAsyncSendPending) {
		const int sendState =
			iUdp.pollEndPacketAsync();

		if (sendState < 0) {
			finishResolveAsync(-11);
			return -11;
		}

		if (sendState == 0) {
			return 0;
		}

		iAsyncSendPending = false;
		iAsyncWaitStartMs = millis();

		// Keep each poll bounded: response parsing starts on the next call.
		return 0;
	}

	const int packetSize = iUdp.parsePacket();
'@

$dnsCpp = Replace-ExactOnce -Text $dnsCpp -Old $oldDnsPoll -New $newDnsPoll -Label "DNS_POLL_UDP_SEND_ASYNC"

$oldDnsCancel = @'
	iAsyncStatus = INVALID_RESPONSE;
	iAsyncActive = false;
}
'@

$newDnsCancel = @'
	iAsyncStatus = INVALID_RESPONSE;
	iAsyncActive = false;
	iAsyncSendPending = false;
}
'@

$dnsCpp = Replace-ExactOnce -Text $dnsCpp -Old $oldDnsCancel -New $newDnsCancel -Label "DNS_CANCEL_SEND_STATE"

$oldDnsFinish = @'
	iAsyncStatus = result;
	iAsyncActive = false;
}
'@

$newDnsFinish = @'
	iAsyncStatus = result;
	iAsyncActive = false;
	iAsyncSendPending = false;
}
'@

$dnsCpp = Replace-ExactOnce -Text $dnsCpp -Old $oldDnsFinish -New $newDnsFinish -Label "DNS_FINISH_SEND_STATE"

[System.IO.File]::WriteAllText($mainHeaderPath, $mainHeader, $utf8NoBom)
[System.IO.File]::WriteAllText($socketPath, $socketCpp, $utf8NoBom)
[System.IO.File]::WriteAllText($udpCppPath, $udpCpp, $utf8NoBom)
[System.IO.File]::WriteAllText($dnsHeaderPath, $dnsHeader, $utf8NoBom)
[System.IO.File]::WriteAllText($dnsCppPath, $dnsCpp, $utf8NoBom)

Write-Host "NB3_D1_PATCH_APPLICATION=APPLIED"
}

$previousPreference = $ErrorActionPreference
try {
    # Native Git may emit benign LF/CRLF warnings on stderr even when
    # 'git diff --check' succeeds. The authoritative result is the exit code.
    $ErrorActionPreference = "Continue"
    $diffCheck = @(& git -C $script:G2RepoRoot diff --check 2>&1)
    $diffCheckExit = [int]$LASTEXITCODE
}
finally {
    $ErrorActionPreference = $previousPreference
}

$diffCheckWarnings = @(
    $diffCheck | Where-Object {
        ([string]$_).StartsWith(
            "warning:",
            [System.StringComparison]::OrdinalIgnoreCase)
    }
)

Write-Host "GIT_DIFF_CHECK_EXIT=$diffCheckExit"
Write-Host "GIT_DIFF_CHECK_WARNING_COUNT=$($diffCheckWarnings.Count)"

if ($diffCheckExit -ne 0) {
    $diffCheck | ForEach-Object { Write-Host $_ }
    throw "A14_NB3D_GIT_DIFF_CHECK_FAILED"
}

Write-Host "GIT_DIFF_CHECK=PASS"

$dirtyAfterPatch = @(Get-G2TrackedDirtyPaths)
Write-Host "TRACKED_DIRTY_AFTER_PATCH=$($dirtyAfterPatch.Count)"
$dirtyAfterPatch | ForEach-Object { Write-Host "DIRTY_AFTER_PATCH=$_" }

if ($dirtyAfterPatch.Count -ne $expectedDirtyAfter.Count) {
    throw "A14_NB3D_DIRTY_AFTER_COUNT_INVALID=$($dirtyAfterPatch.Count)"
}
for ($i = 0; $i -lt $expectedDirtyAfter.Count; ++$i) {
    if ($dirtyAfterPatch[$i] -ne $expectedDirtyAfter[$i]) {
        throw "A14_NB3D_DIRTY_AFTER_PATH_INVALID=$($dirtyAfterPatch[$i])"
    }
}

$stagedAfterPatch = @(& git -C $script:G2RepoRoot diff --cached --name-only)
if ($LASTEXITCODE -ne 0 -or $stagedAfterPatch.Count -ne 0) {
    throw "A14_NB3D_INDEX_NOT_CLEAN_AFTER_PATCH"
}
Write-Host "STAGED_COUNT_AFTER_PATCH=0"

$dnsCppVerify = [System.IO.File]::ReadAllText($dnsCppPath)
$dnsHeaderVerify = [System.IO.File]::ReadAllText($dnsHeaderPath)
$mainHeaderVerify = [System.IO.File]::ReadAllText($mainHeaderPath)
$socketVerify = [System.IO.File]::ReadAllText($socketPath)
$udpCppVerify = [System.IO.File]::ReadAllText($udpCppPath)

$oldUdpWaitCount = ([regex]::Matches(
    $socketVerify,
    'while\s*\(\s*\(W5100\.readSnIR\(s\).*SEND_OK'
)).Count

Write-Host "SOCKET_UDP_DIRECT_SEND_OK_WAIT_COUNT=$oldUdpWaitCount"
if ($oldUdpWaitCount -ne 0) {
    throw "A14_NB3D_DIRECT_UDP_SEND_OK_WAIT_REMAINS"
}

foreach ($marker in @(
    "socketBeginSendUDP",
    "socketPollSendUDP"
)) {
    $headerCount = ([regex]::Matches(
        $mainHeaderVerify,
        '(?m)^[ \t]*static[ \t]+int[ \t]+' +
        [regex]::Escape($marker) +
        '[ \t]*\('
    )).Count

    $cppCount = ([regex]::Matches(
        $socketVerify,
        '(?m)^[ \t]*int[ \t]+EthernetClass::' +
        [regex]::Escape($marker) +
        '[ \t]*\('
    )).Count

    Write-Host "SOCKET_UDP_API=$marker HEADER_DECL_COUNT=$headerCount CPP_DEF_COUNT=$cppCount"

    if ($headerCount -ne 1 -or $cppCount -ne 1) {
        throw "A14_NB3D_SOCKET_API_SIGNATURE_INVALID=$marker"
    }
}

foreach ($marker in @(
    "beginEndPacketAsync",
    "pollEndPacketAsync"
)) {
    $headerCount = ([regex]::Matches(
        $mainHeaderVerify,
        '(?m)^[ \t]*int[ \t]+' +
        [regex]::Escape($marker) +
        '[ \t]*\('
    )).Count

    $cppCount = ([regex]::Matches(
        $udpCppVerify,
        '(?m)^[ \t]*int[ \t]+EthernetUDP::' +
        [regex]::Escape($marker) +
        '[ \t]*\('
    )).Count

    Write-Host "UDP_API=$marker HEADER_DECL_COUNT=$headerCount CPP_DEF_COUNT=$cppCount"

    if ($headerCount -ne 1 -or $cppCount -ne 1) {
        throw "A14_NB3D_UDP_API_SIGNATURE_INVALID=$marker"
    }
}

$progressHeaderCount = ([regex]::Matches(
    $mainHeaderVerify,
    '(?m)^[ \t]*bool[ \t]+endPacketAsyncInProgress\(\)[ \t]+const;'
)).Count
$progressCppCount = ([regex]::Matches(
    $udpCppVerify,
    '(?m)^[ \t]*bool[ \t]+EthernetUDP::endPacketAsyncInProgress\(\)[ \t]+const'
)).Count

$cancelHeaderCount = ([regex]::Matches(
    $mainHeaderVerify,
    '(?m)^[ \t]*void[ \t]+cancelEndPacketAsync\(\);'
)).Count
$cancelCppCount = ([regex]::Matches(
    $udpCppVerify,
    '(?m)^[ \t]*void[ \t]+EthernetUDP::cancelEndPacketAsync\(\)'
)).Count

Write-Host "UDP_API=endPacketAsyncInProgress HEADER_DECL_COUNT=$progressHeaderCount CPP_DEF_COUNT=$progressCppCount"
Write-Host "UDP_API=cancelEndPacketAsync HEADER_DECL_COUNT=$cancelHeaderCount CPP_DEF_COUNT=$cancelCppCount"

if ($progressHeaderCount -ne 1 -or $progressCppCount -ne 1) {
    throw "A14_NB3D_UDP_PROGRESS_API_INVALID"
}
if ($cancelHeaderCount -ne 1 -or $cancelCppCount -ne 1) {
    throw "A14_NB3D_UDP_CANCEL_API_INVALID"
}

$dnsBeginAsyncSendCount = ([regex]::Matches(
    $dnsCppVerify,
    'iUdp\.beginEndPacketAsync\s*\('
)).Count
$dnsPollAsyncSendCount = ([regex]::Matches(
    $dnsCppVerify,
    'iUdp\.pollEndPacketAsync\s*\('
)).Count
$dnsSyncEndPacketCount = ([regex]::Matches(
    $dnsCppVerify,
    'iUdp\.endPacket\s*\('
)).Count
$dnsSendStateCount = ([regex]::Matches(
    $dnsHeaderVerify,
    '(?m)^[ \t]*bool[ \t]+iAsyncSendPending[ \t]*='
)).Count

Write-Host "DNS_ASYNC_UDP_BEGIN_COUNT=$dnsBeginAsyncSendCount"
Write-Host "DNS_ASYNC_UDP_POLL_COUNT=$dnsPollAsyncSendCount"
Write-Host "DNS_SYNC_END_PACKET_COUNT=$dnsSyncEndPacketCount"
Write-Host "DNS_ASYNC_SEND_STATE_COUNT=$dnsSendStateCount"

if ($dnsBeginAsyncSendCount -ne 1 -or
    $dnsPollAsyncSendCount -ne 1 -or
    $dnsSyncEndPacketCount -ne 0 -or
    $dnsSendStateCount -ne 1) {
    throw "A14_NB3D_DNS_UDP_SEND_CONTRACT_INVALID"
}

Write-Host "DNS_CPP_SHA256=$(Get-G2Sha256 $dnsCppRelative)"
Write-Host "DNS_HEADER_SHA256=$(Get-G2Sha256 $dnsHeaderRelative)"
Write-Host "MAIN_HEADER_SHA256=$(Get-G2Sha256 $mainHeaderRelative)"
Write-Host "SOCKET_CPP_SHA256=$(Get-G2Sha256 $socketRelative)"
Write-Host "UDP_CPP_SHA256=$(Get-G2Sha256 $udpCppRelative)"

$arduinoCli = "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"
if (-not (Test-Path -LiteralPath $arduinoCli)) {
    throw "A14_NB3D_ARDUINO_CLI_NOT_FOUND=$arduinoCli"
}

$fqbn = "jwplc_local:esp32:jwplcbasic"
$librariesRoot = Get-G2Path "JWPLC/2.1.0/libraries"
$expectedEthernetLibrary = Join-Path $librariesRoot "JWPLC_Ethernet"
$rawFirmwarePath = Get-G2Path $script:G2RawFirmwareRelative
$rawSketchDir = Split-Path -Parent $rawFirmwarePath

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$tempRoot = Join-Path $env:TEMP ("jwplc_a14_nb3d_udp_send_{0}" -f $timestamp)
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
$probeMainBasename = [System.IO.Path]::GetFileNameWithoutExtension($probeSketchPath)

Write-Host "API_PROBE_SKETCH_FOLDER=$probeSketchName"
Write-Host "API_PROBE_MAIN_BASENAME=$probeMainBasename"

if ($probeSketchName -ne $probeMainBasename) {
    throw "A14_NB3D_ARDUINO_SKETCH_NAME_CONTRACT_INVALID"
}
Write-Host "API_PROBE_SKETCH_NAME_CONTRACT=PASS"

$probeSketch = @'
#include <JWPLC_Ethernet.h>
#include <Dns.h>

volatile int nb3dSink = 0;

static void compileOnlyUdpSendProbe()
{
    EthernetUDP udp;

    nb3dSink += udp.beginEndPacketAsync();
    nb3dSink += udp.pollEndPacketAsync();
    nb3dSink += udp.endPacketAsyncInProgress() ? 1 : 0;
    udp.cancelEndPacketAsync();

    nb3dSink += Ethernet.socketBeginSendUDP(0);
    nb3dSink += Ethernet.socketPollSendUDP(0);

    DNSClient dns;
    IPAddress server(192, 0, 2, 1);
    IPAddress result;
    dns.begin(server);
    nb3dSink += dns.beginResolveAsync(
        "compile-probe.invalid",
        result,
        100);
    nb3dSink += dns.pollResolveAsync();
}

void setup()
{
    if (false)
    {
        compileOnlyUdpSendProbe();
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
Write-Host "SOURCE_MUTATION=UDP_SEND_COOPERATIVE_ENGINE"
Write-Host "UPLOAD=NO"

Write-Host ""
Write-Host "=== COMPILE RAW CANDIDATE ==="

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
    throw "A14_NB3D_RAW_COMPILE_FAILED"
}

Assert-NB3RepoEthernetUsed -LogPath $rawCompileLog -ExpectedLibraryPath $expectedEthernetLibrary -Label "RAW"

Write-Host ""
Write-Host "=== COMPILE UDP SEND + DNS API PROBE ==="

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
    throw "A14_NB3D_API_PROBE_COMPILE_FAILED"
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
    throw "A14_NB3D_BIN_OUTPUT_MISSING"
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
    throw "A14_NB3D_FINAL_WORKTREE_INVALID"
}
for ($i = 0; $i -lt $expectedDirtyAfter.Count; ++$i) {
    if ($dirtyFinal[$i] -ne $expectedDirtyAfter[$i]) {
        throw "A14_NB3D_FINAL_DIRTY_PATH_INVALID=$($dirtyFinal[$i])"
    }
}

Write-Host "NB3_UDP_SEND_SOCKET_ENGINE=COOPERATIVE_BEGIN_POLL"
Write-Host "NB3_UDP_ENDPACKET_LEGACY=WRAPPER_PRESERVED"
Write-Host "NB3_DNS_ASYNC_UDP_SEND=COOPERATIVE"
Write-Host "NB3_DNS_RESPONSE_TIMER_START=AFTER_UDP_SEND_OK"
Write-Host "NB3_GIT_DIFF_CHECK_AUTHORITY=EXIT_CODE"
Write-Host "NB3_PARSE_PACKET_CHANGE=NO"
Write-Host "NB3_SPI_FREQUENCY_CHANGE=NO"
Write-Host "NB3_UPLOAD=NO"
Write-Host "A14_NB3_D1_UDP_SEND_COOPERATIVE_APPLY_COMPILE=PASS"
