#include "JWPLC_W5500_SafeAccess.h"
/* Copyright 2018 Paul Stoffregen
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <Arduino.h>
#include "JWPLC_W5x00_Ethernet.h"
#include "Dns.h"
#include "utility/w5100.h"

int EthernetClient::connect(const char * host, uint16_t port)
{
	DNSClient dns; // Look up the host first
	IPAddress remote_addr;

	if (_sockindex < MAX_SOCK_NUM) {
		jwplc_w5500_candidate::BusScope bus(20);
		if (!bus) return 0; // preserve active socket under contention
		if (!jwplc_w5500_candidate::matchesLocked(_jwplcSocketToken)) {
			_sockindex = MAX_SOCK_NUM; // stale copied handle
		} else {
			if (Ethernet.socketStatus(_sockindex) != SnSR::CLOSED)
				Ethernet.socketDisconnect(_sockindex);
			(void)jwplc_w5500_candidate::invalidateSocketLocked(_jwplcSocketToken);
			_sockindex = MAX_SOCK_NUM;
		}
	}
	dns.begin(Ethernet.dnsServerIP());
	if (!dns.getHostByName(host, remote_addr)) return 0; // TODO: use _timeout
	return connect(remote_addr, port);
}

int EthernetClient::beginConnectAsync(IPAddress ip, uint16_t port)
{
	// An explicit new target supersedes a pending cancellation.
	_jwplcCancelConnectPending = false;
	jwplc_w5500_candidate::BusScope bus(20);
	if (!bus) {
		_jwplcDeferredConnect = true;
		_jwplcDeferredIp = ip;
		_jwplcDeferredPort = port;
		return 0; // SPI_BUSY, not failure
	}
	_jwplcDeferredConnect = false;
	// No touching an old socket whose allocation changed.
	if (_sockindex < MAX_SOCK_NUM &&
	    !jwplc_w5500_candidate::matchesLocked(_jwplcSocketToken)) {
		_sockindex = MAX_SOCK_NUM;
		_jwplcSocketToken = {};
	}
	// Una nueva conexiÃ³n invalida cualquier lifecycle cooperativo anterior.
	cancelWriteAsync();
	_stopPending = false;
	_stopStartedAtMs = 0;
	_flushPending = false;
	_flushStartedAtMs = 0;
	if (_sockindex < MAX_SOCK_NUM) {
		uint8_t stat = Ethernet.socketStatus(_sockindex);
		if (stat == SnSR::ESTABLISHED || stat == SnSR::CLOSE_WAIT) {
			// Inicia cierre TCP graceful sin esperar su finalización.
			// El W5x00 continúa el handshake de cierre en hardware.
			Ethernet.socketDisconnect(_sockindex);
		} else if (stat != SnSR::CLOSED) {
			// Una conexión todavía en progreso se aborta inmediatamente.
			Ethernet.socketClose(_sockindex);
		}
		(void)jwplc_w5500_candidate::invalidateSocketLocked(_jwplcSocketToken);
		_sockindex = MAX_SOCK_NUM;
	}

#if defined(ESP8266) || defined(ESP32)
	if (ip == IPAddress((uint32_t)0) || ip == IPAddress(0xFFFFFFFFul)) return -1;
#else
	if (ip == IPAddress(0ul) || ip == IPAddress(0xFFFFFFFFul)) return -1;
#endif

	_sockindex = Ethernet.socketBegin(SnMR::TCP, 0);
	if (_sockindex >= MAX_SOCK_NUM) return -1;
	_jwplcSocketToken = jwplc_w5500_candidate::currentSocketTokenLocked(_sockindex);
	if (!bool(_jwplcSocketToken)) {
		_sockindex = MAX_SOCK_NUM;
		return -1; // Never issue CONNECT with an unowned socket.
	}

	// socketConnect() sólo configura destino y dispara Sock_CONNECT en el
	// W5x00. La espera de ESTABLISHED se hace luego mediante pollConnectAsync().
	Ethernet.socketConnect(_sockindex, rawIPAddress(ip), port);
	return pollConnectAsync();
}

int EthernetClient::pollConnectAsync()
{
	if (_jwplcCancelConnectPending) {
		cancelConnectAsync();
		return _jwplcCancelConnectPending ? 0 : -1;
	}
	if (_jwplcDeferredConnect)
		return beginConnectAsync(_jwplcDeferredIp,_jwplcDeferredPort);
	if (_sockindex >= MAX_SOCK_NUM) return -1;
	jwplc_w5500_candidate::BusScope bus(20);
	if (!bus) return 0;
	if (!jwplc_w5500_candidate::matchesLocked(_jwplcSocketToken)) {
		_sockindex = MAX_SOCK_NUM;
		return -1;
	}
	if (_sockindex >= MAX_SOCK_NUM) return -1;

	uint8_t stat = Ethernet.socketStatus(_sockindex);
	if (stat == SnSR::ESTABLISHED || stat == SnSR::CLOSE_WAIT) return 1;

	if (stat == SnSR::CLOSED) {
		(void)jwplc_w5500_candidate::invalidateSocketLocked(_jwplcSocketToken);
		_sockindex = MAX_SOCK_NUM;
		return -1;
	}

	return 0;
}

bool EthernetClient::connectAsyncInProgress()
{
	if (_jwplcCancelConnectPending) return true;
	if (_jwplcDeferredConnect) return true;
	if (_sockindex >= MAX_SOCK_NUM) return false;
	jwplc_w5500_candidate::BusScope bus(20);
	if (!bus) return true; // Unknown under contention, not disconnected.
	if (!jwplc_w5500_candidate::matchesLocked(_jwplcSocketToken)) return false;
	if (_sockindex >= MAX_SOCK_NUM) return false;

	uint8_t stat = Ethernet.socketStatus(_sockindex);
	return stat != SnSR::ESTABLISHED &&
		stat != SnSR::CLOSE_WAIT &&
		stat != SnSR::CLOSED;
}

void EthernetClient::cancelConnectAsync()
{
	_jwplcDeferredConnect = false; // cancel the queued retarget too
	if (_sockindex >= MAX_SOCK_NUM) {
		_jwplcDeferredConnect = false;
		_jwplcCancelConnectPending = false;
		return;
	}
	jwplc_w5500_candidate::BusScope bus(20);
	if (!bus) {
		_jwplcCancelConnectPending = true;
		// A temporary client may die after a timeout. The autoload
		// service owns this token until CLOSE or a newer allocation.
		(void)jwplc_w5500_candidate::requestDeferredClose(_jwplcSocketToken);
		return; // retain the handle for an explicit retry too
	}
	_jwplcCancelConnectPending = false;
	_jwplcDeferredConnect = false;
	if (!jwplc_w5500_candidate::matchesLocked(_jwplcSocketToken)) {
		_sockindex = MAX_SOCK_NUM;
		return;
	}
	if (_sockindex >= MAX_SOCK_NUM) return;

	uint8_t stat = Ethernet.socketStatus(_sockindex);

	if (stat == SnSR::ESTABLISHED || stat == SnSR::CLOSE_WAIT) {
		// Notifica al peer mediante cierre TCP graceful.
		Ethernet.socketDisconnect(_sockindex);
	} else if (stat != SnSR::CLOSED) {
		// Durante CONNECT/SYN se puede abortar directamente.
		Ethernet.socketClose(_sockindex);
	}

	(void)jwplc_w5500_candidate::invalidateSocketLocked(_jwplcSocketToken);
	_sockindex = MAX_SOCK_NUM;
}

int EthernetClient::connect(IPAddress ip, uint16_t port)
{
	// Compatibilidad Arduino: esta API sigue siendo bloqueante y conserva el
	// timeout configurado. Internamente usa el mismo motor cooperativo nuevo
	// para evitar dos implementaciones distintas del establecimiento TCP.
	int state = beginConnectAsync(ip, port);
	if (state < 0) return 0;
	if (state > 0) return 1;

	uint32_t start = millis();
	while (1) {
		state = pollConnectAsync();
		if (state > 0) return 1;
		if (state < 0) return 0;
		if (millis() - start > _timeout) break;
		delay(1);
	}

	cancelConnectAsync();
	return 0;
}

int EthernetClient::availableForWrite(void)
{
	jwplc_w5500_candidate::BusScope bus(20);
	if (!bus || _sockindex >= MAX_SOCK_NUM) return 0;
	if (!jwplc_w5500_candidate::matchesLocked(_jwplcSocketToken)) return 0;
	return Ethernet.socketSendAvailable(_sockindex);
}

size_t EthernetClient::write(uint8_t b)
{
	return write(&b, 1);
}

size_t EthernetClient::write(const uint8_t *buf, size_t size)
{
    if (size == 0) return 0;
    if (_sockindex >= MAX_SOCK_NUM || buf == nullptr ||
        writeAsyncInProgress() || _stopPending || _flushPending) {
        setWriteError();
        return 0;
    }

    // One SEND step per poll; yield only after its bus scope was released.
    const uint32_t startedMs = millis();
    size_t totalWritten = 0;
    while (totalWritten < size) {
        if ((uint32_t)(millis() - startedMs) >= _timeout) break;
        const size_t remaining = size - totalWritten;
        const uint16_t chunk = (uint16_t)(
            remaining < (size_t)W5100.SSIZE ? remaining : (size_t)W5100.SSIZE);
        int state = beginWriteAsync(buf + totalWritten, chunk);
        while (state == 0) {
            if ((uint32_t)(millis() - startedMs) >= _timeout) {
                // If SEND was not issued, cancellation releases the buffer
                // even under BUS_BUSY. After SEND, cleanup retains its lease.
                cancelWriteAsync();
                break;
            }
            yield(); // no JWPLC SPI mutex held here
            state = pollWriteAsync();
        }
        if (state != 1) break;
        totalWritten += chunk;
    }
    if (totalWritten != size) setWriteError();
    return totalWritten;
}

int EthernetClient::beginWriteAsync(const uint8_t *buf, size_t size)
{
	if (_writeAsyncState != 0 || _stopPending || _flushPending) return -1;
	if (size == 0) return 1;
	if (_sockindex >= MAX_SOCK_NUM || buf == nullptr ||
		size > UINT16_MAX || size > W5100.SSIZE) {
		return -1;
	}

	_writeAsyncBuffer = buf;
	_writeAsyncLength = (uint16_t)size;
	_writeAsyncStartedAtMs = millis();
	_writeAsyncState = 1; // waiting for TX free space
	return pollWriteAsync();
}

int EthernetClient::pollWriteAsync()
{
	if (_writeAsyncState == 0 || _sockindex >= MAX_SOCK_NUM) return -1;

	if ((uint32_t)(millis() - _writeAsyncStartedAtMs) >= _timeout) {
		cancelWriteAsync();
		return -1;
	}


    jwplc_w5500_candidate::BusScope bus(20);
    if (!bus) return 0; // retain the SEND state and its buffer
    if (!jwplc_w5500_candidate::matchesLocked(_jwplcSocketToken)) {
        _sockindex = MAX_SOCK_NUM;
        _writeAsyncState = 0;
        _writeAsyncBuffer = nullptr;
        _writeAsyncLength = 0;
        _writeAsyncStartedAtMs = 0;
        return -1; // recycled slot or chip epoch: no W5500 access
    }
	if (_writeAsyncState == 1) {
		const int state = Ethernet.socketBeginSendTCP(
			_sockindex,
			_writeAsyncBuffer,
			_writeAsyncLength);

		if (state < 0) {
			_writeAsyncState = 0;
			_writeAsyncBuffer = nullptr;
			_writeAsyncLength = 0;
			_writeAsyncStartedAtMs = 0;
			return -1;
		}

		if (state == 0) return 0;

		_writeAsyncState = 2; // payload copied; waiting for SEND_OK
		_writeAsyncBuffer = nullptr;
		return 0;
	}

	const int state = Ethernet.socketPollSendTCP(_sockindex);
	if (state == 0) return 0;

	_writeAsyncState = 0;
	_writeAsyncBuffer = nullptr;
	_writeAsyncLength = 0;
	_writeAsyncStartedAtMs = 0;
	return state;
}

bool EthernetClient::writeAsyncInProgress() const
{
	return _writeAsyncState != 0;
}

void EthernetClient::cancelWriteAsync()
{
	// Guard before touching any shared W5500 socket, including SEND pending.
    if (_writeAsyncState == 1) {
        // No SEND command was issued; dropping the borrowed buffer is safe.
        _writeAsyncState = 0;
        _writeAsyncBuffer = nullptr;
        _writeAsyncLength = 0;
        _writeAsyncStartedAtMs = 0;
        return;
    }
	jwplc_w5500_candidate::BusScope bus(20);
	if (!bus) return; // SPI_BUSY: preserve TX state/handle.
	if (_sockindex < MAX_SOCK_NUM &&
	    !jwplc_w5500_candidate::matchesLocked(_jwplcSocketToken)) {
		_sockindex=MAX_SOCK_NUM; // no hardware operation on recycled slot
		_writeAsyncState=0;
		_writeAsyncBuffer=nullptr;
		_writeAsyncLength=0;
		_writeAsyncStartedAtMs=0;
		return;
	}
	const bool sendWasIssued = _writeAsyncState == 2;

	_writeAsyncState = 0;
	_writeAsyncBuffer = nullptr;
	_writeAsyncLength = 0;
	_writeAsyncStartedAtMs = 0;

	if (sendWasIssued && _sockindex < MAX_SOCK_NUM) {
		Ethernet.socketClose(_sockindex);
		(void)jwplc_w5500_candidate::invalidateSocketLocked(_jwplcSocketToken);
		_sockindex = MAX_SOCK_NUM;
	}
}

int EthernetClient::available()
{
	jwplc_w5500_candidate::BusScope bus(20);
	if (!bus) return 0;
	if (!jwplc_w5500_candidate::matchesLocked(_jwplcSocketToken)) return 0;
	if (_sockindex >= MAX_SOCK_NUM) return 0;
	return Ethernet.socketRecvAvailable(_sockindex);
	// TODO: do the WIZnet chips automatically retransmit TCP ACK
	// packets if they are lost by the network?  Someday this should
	// be checked by a man-in-the-middle test which discards certain
	// packets.  If ACKs aren't resent, we would need to check for
	// returning 0 here and after a timeout do another Sock_RECV
	// command to cause the WIZnet chip to resend the ACK packet.
}

int EthernetClient::read(uint8_t *buf, size_t size)
{
	jwplc_w5500_candidate::BusScope bus(20);
	if (!bus) return 0;
	if (!jwplc_w5500_candidate::matchesLocked(_jwplcSocketToken)) return 0;
    if (_sockindex >= MAX_SOCK_NUM || size == 0) return 0;

    // socketRecv() recibe int16_t: size_t >= 32768 podría ser negativo.
    const int16_t boundedSize = (int16_t)(
        size > (size_t)INT16_MAX ? (size_t)INT16_MAX : size);
    return Ethernet.socketRecv(_sockindex, buf, boundedSize);
}

int EthernetClient::jwplcReadTcpFastDeferred(uint8_t *buf, size_t size)
{
	jwplc_w5500_candidate::BusScope bus(20);
	if (!bus) return 0;
	if (!jwplc_w5500_candidate::matchesLocked(_jwplcSocketToken)) return -1;
	if (_sockindex >= MAX_SOCK_NUM || size > UINT16_MAX) return -1;
	return Ethernet.socketRecvTCPFastDeferred(
		_sockindex,
		buf,
		(uint16_t)size);
}

bool EthernetClient::jwplcCommitRxFast()
{
	jwplc_w5500_candidate::BusScope bus(20);
	if (!bus) return false;
	if (!jwplc_w5500_candidate::matchesLocked(_jwplcSocketToken)) return false;
	if (_sockindex >= MAX_SOCK_NUM) return false;
	return Ethernet.socketCommitTCPFast(_sockindex);
}

int EthernetClient::peek()
{
	jwplc_w5500_candidate::BusScope bus(20);
	if (!bus) return -1;
	if (!jwplc_w5500_candidate::matchesLocked(_jwplcSocketToken)) return -1;
	if (_sockindex >= MAX_SOCK_NUM) return -1;
	if (!available()) return -1;
	return Ethernet.socketPeek(_sockindex);
}

int EthernetClient::read()
{
	jwplc_w5500_candidate::BusScope bus(20);
	if (!bus) return -1;
	if (!jwplc_w5500_candidate::matchesLocked(_jwplcSocketToken)) return -1;
	uint8_t b;
	if (Ethernet.socketRecv(_sockindex, &b, 1) > 0) return b;
	return -1;
}

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
	// Preserve a real cooperative boundary when a SEND is already pending.
	// The first flush poll will advance that write and then verify TX_FSR.
	if (writeAsyncInProgress()) return 0;
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

	if (writeAsyncInProgress()) {
		const int writeState = pollWriteAsync();
		if (writeState < 0) {
			_flushPending = false;
			_flushStartedAtMs = 0;
			return -1;
		}
		if (writeState == 0) return 0;
	}

    // pollWriteAsync() has completed its own short bus scope. Acquire a
    // fresh lease before reading status and TX space on this socket.
    jwplc_w5500_candidate::BusScope bus(20);
    if (!bus) {
        if ((uint32_t)(millis() - _flushStartedAtMs) < _timeout) return 0;
        _flushPending = false;
        _flushStartedAtMs = 0;
        return -1;
    }
    if (!jwplc_w5500_candidate::matchesLocked(_jwplcSocketToken)) {
        _sockindex = MAX_SOCK_NUM;
        _flushPending = false;
        _flushStartedAtMs = 0;
        return -1;
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
	jwplc_w5500_candidate::BusScope bus(20);
	if (!bus) return 0;
	if (_sockindex < MAX_SOCK_NUM &&
	    !jwplc_w5500_candidate::matchesLocked(_jwplcSocketToken)) {
		_sockindex = MAX_SOCK_NUM;
		_stopPending = false;
		_stopStartedAtMs = 0;
		return 1; // antiguo propietario: no tocar W5500
	}
	cancelFlushAsync();
	cancelWriteAsync();

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
		(void)jwplc_w5500_candidate::invalidateSocketLocked(_jwplcSocketToken);
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
		// Un begin previo pudo quedar pendiente exclusivamente por BUS_BUSY.
		return (_sockindex < MAX_SOCK_NUM) ? beginStopAsync() : 1;
	}

	if (_sockindex >= MAX_SOCK_NUM) {
		_stopPending = false;
		_stopStartedAtMs = 0;
		return 1;
	}
	jwplc_w5500_candidate::BusScope bus(20);
	if (!bus) return 0;
	if (!jwplc_w5500_candidate::matchesLocked(_jwplcSocketToken)) {
		_sockindex = MAX_SOCK_NUM;
		_stopPending = false;
		_stopStartedAtMs = 0;
		return 1;
	}

	if (Ethernet.socketStatus(_sockindex) == SnSR::CLOSED) {
		(void)jwplc_w5500_candidate::invalidateSocketLocked(_jwplcSocketToken);
		_sockindex = MAX_SOCK_NUM;
		_stopPending = false;
		_stopStartedAtMs = 0;
		return 1;
	}

	if ((uint32_t)(millis() - _stopStartedAtMs) >= _timeout) {
		// Conserva la semÃ¡ntica legacy: al vencer timeout se fuerza CLOSE.
		Ethernet.socketClose(_sockindex);
		(void)jwplc_w5500_candidate::invalidateSocketLocked(_jwplcSocketToken);
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
    jwplc_w5500_candidate::BusScope bus(20);
    if (!bus) {
		(void)jwplc_w5500_candidate::requestDeferredClose(_jwplcSocketToken);
		return; // retain the handle for an explicit retry too
	}
    if (_sockindex < MAX_SOCK_NUM &&
        !jwplc_w5500_candidate::matchesLocked(_jwplcSocketToken)) {
        _sockindex = MAX_SOCK_NUM; // stale handle: no tocar hardware ajeno
        _stopPending = false;
        _stopStartedAtMs = 0;
        return;
    }
	cancelFlushAsync();
	cancelWriteAsync();

	if (_sockindex < MAX_SOCK_NUM) {
		Ethernet.socketClose(_sockindex);
        (void)jwplc_w5500_candidate::invalidateSocketLocked(_jwplcSocketToken);
	}

	_sockindex = MAX_SOCK_NUM;
	_stopPending = false;
	_stopStartedAtMs = 0;
}

void EthernetClient::stop()
{
	const uint32_t waitStartedMs = millis();
	int state = beginStopAsync();
	while (state == 0) {
		// Void legacy API: la contención no permite cerrar W5500.
		// Bounded return: conservar handle y dejar reintento explícito.
		if ((uint32_t)(millis() - waitStartedMs) >= _timeout) {
			// The caller may destroy this client after a void stop().
			(void)jwplc_w5500_candidate::requestDeferredClose(_jwplcSocketToken);
			return;
		}
		delay(1);
		state = pollStopAsync();
	}
}

uint8_t EthernetClient::connected()
{
	jwplc_w5500_candidate::BusScope bus(20);
	if (!bus) return 0;
	if (_sockindex >= MAX_SOCK_NUM) return 0;
	if (!jwplc_w5500_candidate::matchesLocked(_jwplcSocketToken)) return 0;

	uint8_t s = Ethernet.socketStatus(_sockindex);
	return !(s == SnSR::LISTEN || s == SnSR::CLOSED || s == SnSR::FIN_WAIT ||
		(s == SnSR::CLOSE_WAIT && !available()));
}

uint8_t EthernetClient::status()
{
	if (_sockindex >= MAX_SOCK_NUM) return SnSR::CLOSED;
	jwplc_w5500_candidate::BusScope bus(20);
	if (!bus) return SnSR::CLOSED;
	if (!jwplc_w5500_candidate::matchesLocked(_jwplcSocketToken)) return SnSR::CLOSED;
	return Ethernet.socketStatus(_sockindex);
}

// the next function allows us to use the client returned by
// EthernetServer::available() as the condition in an if-statement.
bool EthernetClient::operator==(const EthernetClient& rhs)
{
	if (_sockindex != rhs._sockindex) return false;
	if (_sockindex >= MAX_SOCK_NUM) return false;
	if (rhs._sockindex >= MAX_SOCK_NUM) return false;
	return true;
}

// https://github.com/per1234/EthernetMod
// from: https://github.com/ntruchsess/Arduino-1/commit/937bce1a0bb2567f6d03b15df79525569377dabd
uint16_t EthernetClient::localPort()
{
	jwplc_w5500_candidate::BusScope bus(20);
	if (!bus || _sockindex >= MAX_SOCK_NUM) return 0;
	if (!jwplc_w5500_candidate::matchesLocked(_jwplcSocketToken)) return 0;
	uint16_t port;
	SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
	port = W5100.readSnPORT(_sockindex);
	SPI.endTransaction();
	return port;
}

// https://github.com/per1234/EthernetMod
// returns the remote IP address: https://forum.arduino.cc/index.php?topic=82416.0
IPAddress EthernetClient::remoteIP()
{
	jwplc_w5500_candidate::BusScope bus(20);
	if (!bus || _sockindex >= MAX_SOCK_NUM) return IPAddress((uint32_t)0);
	if (!jwplc_w5500_candidate::matchesLocked(_jwplcSocketToken)) return IPAddress((uint32_t)0);
	uint8_t remoteIParray[4];
	SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
	W5100.readSnDIPR(_sockindex, remoteIParray);
	SPI.endTransaction();
	return IPAddress(remoteIParray);
}

// https://github.com/per1234/EthernetMod
// from: https://github.com/ntruchsess/Arduino-1/commit/ca37de4ba4ecbdb941f14ac1fe7dd40f3008af75
uint16_t EthernetClient::remotePort()
{
	jwplc_w5500_candidate::BusScope bus(20);
	if (!bus || _sockindex >= MAX_SOCK_NUM) return 0;
	if (!jwplc_w5500_candidate::matchesLocked(_jwplcSocketToken)) return 0;
	uint16_t port;
	SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
	port = W5100.readSnDPORT(_sockindex);
	SPI.endTransaction();
	return port;
}


// G10-I B01: Status bajo ownership y la misma exclusión W5500.
jwplc_w5500_candidate::StatusReadOutcome
EthernetClient::jwplcProbeSocketStatus(uint8_t& result)
{
    if (_sockindex >= MAX_SOCK_NUM)
        return jwplc_w5500_candidate::StatusReadOutcome::Stale;
    return jwplc_w5500_candidate::readStatusIfCurrent(
        _jwplcSocketToken,
        [](uint8_t index, void*) -> uint8_t {
            return Ethernet.socketStatus(index);
        },
        nullptr, 20, &result);
}
