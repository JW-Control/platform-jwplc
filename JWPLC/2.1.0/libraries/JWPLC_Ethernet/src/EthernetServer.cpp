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
#include "utility/w5100.h"

uint16_t EthernetServer::server_port[MAX_SOCK_NUM];


void EthernetServer::begin()
{
    jwplc_w5500_candidate::BusScope bus(20);
    if (!bus) return;
	uint8_t sockindex = Ethernet.socketBegin(SnMR::TCP, _port);
	if (sockindex < MAX_SOCK_NUM) {
		if (Ethernet.socketListen(sockindex)) {
			server_port[sockindex] = _port;
            jwplc_w5500_candidate::mapListenerLocked(sockindex,_port,
                jwplc_w5500_candidate::currentSocketTokenLocked(sockindex));
		} else {
			Ethernet.socketDisconnect(sockindex);
		}
	}
}

EthernetClient EthernetServer::available()
{
    jwplc_w5500_candidate::BusScope bus(20);
    if (!bus) return EthernetClient(MAX_SOCK_NUM);
	bool listening = false;
	uint8_t sockindex = MAX_SOCK_NUM;
	uint8_t chip, maxindex=MAX_SOCK_NUM;

	chip = W5100.getChip();
	if (!chip) return EthernetClient(MAX_SOCK_NUM);
#if MAX_SOCK_NUM > 4
	if (chip == 51) maxindex = 4; // W5100 chip never supports more than 4 sockets
#endif
	for (uint8_t i=0; i < maxindex; i++) {
		if (server_port[i] == _port && jwplc_w5500_candidate::listenerMatchesLocked(i,_port,jwplc_w5500_candidate::currentSocketTokenLocked(i))) {
			uint8_t stat = Ethernet.socketStatus(i);
			if (stat == SnSR::ESTABLISHED || stat == SnSR::CLOSE_WAIT) {
				if (Ethernet.socketRecvAvailable(i) > 0) {
					sockindex = i;
				} else {
					// remote host closed connection, our end still open
					if (stat == SnSR::CLOSE_WAIT) {
						Ethernet.socketDisconnect(i);
						// status becomes LAST_ACK for short time
					}
				}
			} else if (stat == SnSR::LISTEN) {
				listening = true;
			} else if (stat == SnSR::CLOSED) {
				server_port[i] = 0;
			}
		}
	}
	if (!listening) begin();
	return EthernetClient(sockindex);
}

EthernetClient EthernetServer::accept()
{
    jwplc_w5500_candidate::BusScope bus(20);
    if (!bus) return EthernetClient(MAX_SOCK_NUM);
	bool listening = false;
	uint8_t sockindex = MAX_SOCK_NUM;
	uint8_t chip, maxindex=MAX_SOCK_NUM;

	chip = W5100.getChip();
	if (!chip) return EthernetClient(MAX_SOCK_NUM);
#if MAX_SOCK_NUM > 4
	if (chip == 51) maxindex = 4; // W5100 chip never supports more than 4 sockets
#endif
	for (uint8_t i=0; i < maxindex; i++) {
		if (server_port[i] == _port && jwplc_w5500_candidate::listenerMatchesLocked(i,_port,jwplc_w5500_candidate::currentSocketTokenLocked(i))) {
			uint8_t stat = Ethernet.socketStatus(i);
			if (sockindex == MAX_SOCK_NUM &&
			  (stat == SnSR::ESTABLISHED || stat == SnSR::CLOSE_WAIT)) {
				// Return the connected client even if no data received.
				// Some protocols like FTP expect the server to send the
				// first data.
				sockindex = i;
				server_port[i] = 0; // only return the client once
			} else if (stat == SnSR::LISTEN) {
				listening = true;
			} else if (stat == SnSR::CLOSED) {
				server_port[i] = 0;
			}
		}
	}
	if (!listening) begin();
	return EthernetClient(sockindex);
}

EthernetServer::operator bool()
{
	jwplc_w5500_candidate::BusScope bus(20);
	if (!bus) return false; // public bool cannot encode BUS_BUSY
	uint8_t maxindex=MAX_SOCK_NUM;
#if MAX_SOCK_NUM > 4
	if (W5100.getChip() == 51) maxindex = 4; // W5100 chip never supports more than 4 sockets
#endif
	for (uint8_t i=0; i < maxindex; i++) {
		if (server_port[i] == _port &&
            jwplc_w5500_candidate::listenerMatchesLocked(
                i, _port, jwplc_w5500_candidate::currentSocketTokenLocked(i))) {
			if (Ethernet.socketStatus(i) == SnSR::LISTEN) {
				return true; // server is listening for incoming clients
			}
		}
	}
	return false;
}

#if 0
void EthernetServer::statusreport()
{
	Serial.printf("EthernetServer, port=%d\n", _port);
	for (uint8_t i=0; i < MAX_SOCK_NUM; i++) {
		uint16_t port = server_port[i];
		uint8_t stat = Ethernet.socketStatus(i);
		const char *name;
		switch (stat) {
			case 0x00: name = "CLOSED"; break;
			case 0x13: name = "INIT"; break;
			case 0x14: name = "LISTEN"; break;
			case 0x15: name = "SYNSENT"; break;
			case 0x16: name = "SYNRECV"; break;
			case 0x17: name = "ESTABLISHED"; break;
			case 0x18: name = "FIN_WAIT"; break;
			case 0x1A: name = "CLOSING"; break;
			case 0x1B: name = "TIME_WAIT"; break;
			case 0x1C: name = "CLOSE_WAIT"; break;
			case 0x1D: name = "LAST_ACK"; break;
			case 0x22: name = "UDP"; break;
			case 0x32: name = "IPRAW"; break;
			case 0x42: name = "MACRAW"; break;
			case 0x5F: name = "PPPOE"; break;
			default: name = "???";
		}
		int avail = Ethernet.socketRecvAvailable(i);
		Serial.printf("  %d: port=%d, status=%s (0x%02X), avail=%d\n",
			i, port, name, stat, avail);
	}
}
#endif

size_t EthernetServer::write(uint8_t b)
{
	return write(&b, 1);
}

size_t EthernetServer::write(const uint8_t *buffer, size_t size)
{
    if (buffer == nullptr || size == 0) return 0;
    // Keep the legacy listener-maintenance pass, outside the snapshot lock.
    available();

    EthernetClient clients[MAX_SOCK_NUM];
    uint8_t count = 0;
    {
        jwplc_w5500_candidate::BusScope bus(20);
        if (!bus) return 0; // no listener or socket inference on SPI_BUSY
        const uint8_t chip = W5100.getChip();
        if (!chip) return 0;
        uint8_t maxindex = MAX_SOCK_NUM;
#if MAX_SOCK_NUM > 4
        if (chip == 51) maxindex = 4;
#endif
        for (uint8_t i = 0; i < maxindex; i++) {
            const auto token = jwplc_w5500_candidate::currentSocketTokenLocked(i);
            if (server_port[i] != _port ||
                !jwplc_w5500_candidate::listenerMatchesLocked(i, _port, token) ||
                Ethernet.socketStatus(i) != SnSR::ESTABLISHED) continue;
            // The constructor captures the token while this same scope owns
            // SPI. A recycled slot cannot become a new broadcast target.
            clients[count] = EthernetClient(i);
            if (clients[count].getSocketNumber() == i &&
                jwplc_w5500_candidate::matchesLocked(clients[count]._jwplcSocketToken))
                ++count;
        }
    }

    // Each write may wait for SEND_OK. Never hold the server scan lock here.
    size_t minimumWritten = size;
    for (uint8_t i = 0; i < count; i++) {
        const size_t written = clients[i].write(buffer, size);
        if (written < minimumWritten) minimumWritten = written;
    }
    return count ? minimumWritten : 0;
}


// G10-I B01: query the listener under SPI and its mapped socket lease.
jwplc_w5500_candidate::StatusReadOutcome
EthernetServer::jwplcProbeListening(bool& observed)
{
    jwplc_w5500_candidate::BusScope bus(20);
    if (!bus) return jwplc_w5500_candidate::StatusReadOutcome::BusBusy;
    const uint8_t chip = W5100.getChip();
    if (!chip) return jwplc_w5500_candidate::StatusReadOutcome::Stale;
    uint8_t maxindex = MAX_SOCK_NUM;
#if MAX_SOCK_NUM > 4
    if (chip == 51) maxindex = 4;
#endif
    bool hasCurrentListener = false;
    bool listening = false;
    for (uint8_t i = 0; i < maxindex; ++i) {
        if (server_port[i] != _port) continue;
        const auto token = jwplc_w5500_candidate::currentSocketTokenLocked(i);
        if (!jwplc_w5500_candidate::listenerMatchesLocked(i, _port, token))
            continue;
        hasCurrentListener = true;
        if (Ethernet.socketStatus(i) == SnSR::LISTEN) listening = true;
    }
    if (!hasCurrentListener)
        return jwplc_w5500_candidate::StatusReadOutcome::Stale;
    observed = listening;
    return jwplc_w5500_candidate::StatusReadOutcome::Read;
}
