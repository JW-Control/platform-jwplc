/*
 *  Udp.cpp: Library to send/receive UDP packets with the Arduino Ethernet Shield.
 *  This version only offers minimal wrapping of socket.cpp
 *  Drop Udp.h/.cpp into the Ethernet library directory at hardware/libraries/Ethernet/
 *
 * MIT License:
 * Copyright (c) 2008 Bjoern Hartmann
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * bjoern@cs.stanford.edu 12/30/2008
 */

#include <Arduino.h>
#include "JWPLC_W5x00_Ethernet.h"
#include "Dns.h"
#include "utility/w5100.h"

/* Start EthernetUDP socket, listening at local port PORT */
uint8_t EthernetUDP::begin(uint16_t port)
{
	if (sockindex < MAX_SOCK_NUM) Ethernet.socketClose(sockindex);
	sockindex = MAX_SOCK_NUM;
	_port = 0;
	_remaining = 0;
	_offset = 0;
	_sendPending = false;

	sockindex = Ethernet.socketBegin(SnMR::UDP, port);
	if (sockindex >= MAX_SOCK_NUM) return 0;
	_port = port;
	return 1;
}

/* return number of bytes available in the current packet,
   will return zero if parsePacket hasn't been called yet */
int EthernetUDP::available()
{
	return _remaining;
}

/* Release any resources being used by this EthernetUDP instance */
void EthernetUDP::stop()
{
	_sendPending = false;
	_offset = 0;
	_remaining = 0;
	_port = 0;
	_remotePort = 0;
	_remoteIP = IPAddress(0, 0, 0, 0);
	if (sockindex < MAX_SOCK_NUM) Ethernet.socketClose(sockindex);
	sockindex = MAX_SOCK_NUM;
}

int EthernetUDP::beginPacket(const char *host, uint16_t port)
{
	// Look up the host first
	int ret = 0;
	DNSClient dns;
	IPAddress remote_addr;

	dns.begin(Ethernet.dnsServerIP());
	ret = dns.getHostByName(host, remote_addr);
	if (ret != 1) return ret;
	return beginPacket(remote_addr, port);
}

int EthernetUDP::beginPacket(IPAddress ip, uint16_t port)
{
	if (_sendPending || sockindex >= MAX_SOCK_NUM) return 0;
	_offset = 0;
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

size_t EthernetUDP::write(uint8_t byte)
{
	return write(&byte, 1);
}

size_t EthernetUDP::write(const uint8_t *buffer, size_t size)
{
	if (sockindex >= MAX_SOCK_NUM || _sendPending ||
	    buffer == nullptr || size == 0 || _offset >= W5100.SSIZE) {
		return 0;
	}

	// The W5500 TX window is limited; never truncate size_t to
	// uint16_t or allow the datagram offset to cross that window.
	const size_t room = (size_t)W5100.SSIZE - _offset;
	const uint16_t bounded = (uint16_t)(size < room ? size : room);
	const uint16_t written =
		Ethernet.socketBufferData(sockindex, _offset, buffer, bounded);
	_offset += written;
	return written;
}

int EthernetUDP::parsePacket()
{
	if (sockindex >= MAX_SOCK_NUM) return 0;

	// Preserve Alpha14's bounded, cooperative single-drain contract.
	if (_remaining > 0) {
		const int drained = read((uint8_t *)NULL, _remaining);
		if (drained <= 0 || _remaining > 0) return 0;
	}

	// A W5500 UDP record has an 8-byte header before the payload.
	// Do not parse an incomplete header or read uninitialised bytes.
	if (Ethernet.socketRecvAvailable(sockindex) < 8) return 0;
	uint8_t tmpBuf[8] = {0};
	if (Ethernet.socketRecv(sockindex, tmpBuf, 8) != 8) return 0;

	_remoteIP = tmpBuf;
	_remotePort = (uint16_t)(((uint16_t)tmpBuf[4] << 8) | tmpBuf[5]);
	_remaining = (uint16_t)(((uint16_t)tmpBuf[6] << 8) | tmpBuf[7]);
	return _remaining;
}

int EthernetUDP::jwplcReadPacketFastDeferred(
	uint8_t *buffer,
	size_t len)
{
	if (
		sockindex >= MAX_SOCK_NUM ||
		buffer == nullptr ||
		len == 0 ||
		len > (size_t)UINT16_MAX ||
		len > (size_t)W5100.SSIZE)
	{
		return -1;
	}

	uint8_t header[8];

	const int got =
		Ethernet.socketRecvUDPFastDeferred(
			sockindex,
			header,
			buffer,
			(uint16_t)len);

	if (got > 0) {
		_remoteIP = header;
		_remotePort =
			((uint16_t)header[4] << 8) |
			(uint16_t)header[5];
		_remaining = 0;
	}

	return got;
}

bool EthernetUDP::jwplcCommitRxFast()
{
	if (sockindex >= MAX_SOCK_NUM) {
		return false;
	}

	return Ethernet.socketCommitUDPFast(
		sockindex);
}


int EthernetUDP::read()
{
	if (sockindex >= MAX_SOCK_NUM) return -1;
	uint8_t byte;
	if (_remaining > 0 && Ethernet.socketRecv(sockindex, &byte, 1) > 0) {
		_remaining--;
		return byte;
	}
	return -1;
}

int EthernetUDP::read(unsigned char *buffer, size_t len)
{
	if (sockindex >= MAX_SOCK_NUM || _remaining == 0 || len == 0)
		return -1;

	// socketRecv takes int16_t. A size_t >= 32768 must not become
	// negative. Null buffer is valid for bounded discard/drain.
	size_t bounded = len < _remaining ? len : _remaining;
	if (bounded > (size_t)INT16_MAX) bounded = (size_t)INT16_MAX;
	const int got =
		Ethernet.socketRecv(sockindex, buffer, (int16_t)bounded);
	if (got > 0) {
		_remaining -= got;
		return got;
	}
	return -1;
}

int EthernetUDP::peek()
{
	// Unlike recv, peek doesn't check to see if there's any data available, so we must.
	// If the user hasn't called parsePacket yet then return nothing otherwise they
	// may get the UDP header
	if (sockindex >= MAX_SOCK_NUM || _remaining == 0) return -1;
	return Ethernet.socketPeek(sockindex);
}

void EthernetUDP::flush()
{
	// TODO: we should wait for TX buffer to be emptied
}

/* Start EthernetUDP socket, listening at local port PORT */
uint8_t EthernetUDP::beginMulticast(IPAddress ip, uint16_t port)
{
	if (sockindex < MAX_SOCK_NUM) Ethernet.socketClose(sockindex);
	sockindex = Ethernet.socketBeginMulticast(SnMR::UDP | SnMR::MULTI, ip, port);
	if (sockindex >= MAX_SOCK_NUM) return 0;
	_port = port;
	_remaining = 0;
	_offset = 0;
	_sendPending = false;
	return 1;
}
