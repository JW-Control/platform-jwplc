// Arduino DNS client for WIZnet W5100-based Ethernet shield
// (c) Copyright 2009-2010 MCQN Ltd.
// Released under Apache License, version 2.0

#include <Arduino.h>
#include "JWPLC_W5x00_Ethernet.h"
#include "Dns.h"
#include "utility/w5100.h"


#define SOCKET_NONE              255
// Various flags and header field values for a DNS message
#define UDP_HEADER_SIZE          8
#define DNS_HEADER_SIZE          12
#define TTL_SIZE                 4
#define QUERY_FLAG               (0)
#define RESPONSE_FLAG            (1<<15)
#define QUERY_RESPONSE_MASK      (1<<15)
#define OPCODE_STANDARD_QUERY    (0)
#define OPCODE_INVERSE_QUERY     (1<<11)
#define OPCODE_STATUS_REQUEST    (2<<11)
#define OPCODE_MASK              (15<<11)
#define AUTHORITATIVE_FLAG       (1<<10)
#define TRUNCATION_FLAG          (1<<9)
#define RECURSION_DESIRED_FLAG   (1<<8)
#define RECURSION_AVAILABLE_FLAG (1<<7)
#define RESP_NO_ERROR            (0)
#define RESP_FORMAT_ERROR        (1)
#define RESP_SERVER_FAILURE      (2)
#define RESP_NAME_ERROR          (3)
#define RESP_NOT_IMPLEMENTED     (4)
#define RESP_REFUSED             (5)
#define RESP_MASK                (15)
#define TYPE_A                   (0x0001)
#define CLASS_IN                 (0x0001)
#define LABEL_COMPRESSION_MASK   (0xC0)
// Port number that DNS servers listen on
#define DNS_PORT        53

// Possible return codes from ProcessResponse
#define SUCCESS          1
#define TIMED_OUT        -1
#define INVALID_SERVER   -2
#define TRUNCATED        -3
#define INVALID_RESPONSE -4

void DNSClient::begin(const IPAddress& aDNSServer)
{
	iDNSServer = aDNSServer;
	iRequestId = 0;
}


int DNSClient::inet_aton(const char* address, IPAddress& result)
{
	uint16_t acc = 0; // Accumulator
	uint8_t dots = 0;

	while (*address) {
		char c = *address++;
		if (c >= '0' && c <= '9') {
			acc = acc * 10 + (c - '0');
			if (acc > 255) {
				// Value out of [0..255] range
				return 0;
			}
		} else if (c == '.') {
			if (dots == 3) {
				// Too much dots (there must be 3 dots)
				return 0;
			}
			result[dots++] = acc;
			acc = 0;
		} else {
			// Invalid char
			return 0;
		}
	}

	if (dots != 3) {
		// Too few dots (there must be 3 dots)
		return 0;
	}
	result[3] = acc;
	return 1;
}

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
	iAsyncSendPending = false;
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
	iAsyncSendPending = false;
}

uint16_t DNSClient::BuildRequest(const char* aName)
{
	// Build header
	//                                    1  1  1  1  1  1
	//      0  1  2  3  4  5  6  7  8  9  0  1  2  3  4  5
	//    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
	//    |                      ID                       |
	//    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
	//    |QR|   Opcode  |AA|TC|RD|RA|   Z    |   RCODE   |
	//    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
	//    |                    QDCOUNT                    |
	//    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
	//    |                    ANCOUNT                    |
	//    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
	//    |                    NSCOUNT                    |
	//    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
	//    |                    ARCOUNT                    |
	//    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
	// As we only support one request at a time at present, we can simplify
	// some of this header
	iRequestId = millis(); // generate a random ID
	uint16_t twoByteBuffer;

	// FIXME We should also check that there's enough space available to write to, rather
	// FIXME than assume there's enough space (as the code does at present)
	iUdp.write((uint8_t*)&iRequestId, sizeof(iRequestId));

	twoByteBuffer = htons(QUERY_FLAG | OPCODE_STANDARD_QUERY | RECURSION_DESIRED_FLAG);
	iUdp.write((uint8_t*)&twoByteBuffer, sizeof(twoByteBuffer));

	twoByteBuffer = htons(1);  // One question record
	iUdp.write((uint8_t*)&twoByteBuffer, sizeof(twoByteBuffer));

	twoByteBuffer = 0;  // Zero answer records
	iUdp.write((uint8_t*)&twoByteBuffer, sizeof(twoByteBuffer));

	iUdp.write((uint8_t*)&twoByteBuffer, sizeof(twoByteBuffer));
	// and zero additional records
	iUdp.write((uint8_t*)&twoByteBuffer, sizeof(twoByteBuffer));

	// Build question
	const char* start =aName;
	const char* end =start;
	uint8_t len;
	// Run through the name being requested
	while (*end) {
		// Find out how long this section of the name is
		end = start;
		while (*end && (*end != '.') ) {
			end++;
		}

		if (end-start > 0) {
			// Write out the size of this section
			len = end-start;
			iUdp.write(&len, sizeof(len));
			// And then write out the section
			iUdp.write((uint8_t*)start, end-start);
		}
		start = end+1;
	}

	// We've got to the end of the question name, so
	// terminate it with a zero-length section
	len = 0;
	iUdp.write(&len, sizeof(len));
	// Finally the type and class of question
	twoByteBuffer = htons(TYPE_A);
	iUdp.write((uint8_t*)&twoByteBuffer, sizeof(twoByteBuffer));

	twoByteBuffer = htons(CLASS_IN);  // Internet class of question
	iUdp.write((uint8_t*)&twoByteBuffer, sizeof(twoByteBuffer));
	// Success!  Everything buffered okay
	return 1;
}


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
// We've had a reply!
union {
uint8_t  byte[DNS_HEADER_SIZE];
uint16_t word[DNS_HEADER_SIZE/2];
} header;

// Check that it's a response from the right server and the right port.
if ((iDNSServer != iUdp.remoteIP()) || (iUdp.remotePort() != DNS_PORT)) {
return INVALID_SERVER;
}

// Every packet read must either make forward progress or fail.
// EthernetUDP::read() may return -1 on a truncated datagram without
// modifying the destination buffer, so unchecked reads can otherwise
// leave stale parser state inside the name loops.
auto readExact = [this](uint8_t* buffer, size_t length) -> bool {
size_t offset = 0;

while (offset < length) {
uint8_t* target =
(buffer == nullptr) ? nullptr : buffer + offset;

const int got =
iUdp.read(target, length - offset);

if (got <= 0) {
return false;
}

offset += (size_t)got;
}

return true;
};

if (iUdp.available() < DNS_HEADER_SIZE) {
return TRUNCATED;
}

if (!readExact(header.byte, DNS_HEADER_SIZE)) {
return TRUNCATED;
}

uint16_t header_flags = htons(header.word[1]);

// Check that it's a response to this request.
if ((iRequestId != header.word[0]) ||
    ((header_flags & QUERY_RESPONSE_MASK) != (uint16_t)RESPONSE_FLAG)) {
iUdp.flush();
return INVALID_RESPONSE;
}

if ((header_flags & TRUNCATION_FLAG) ||
    (header_flags & RESP_MASK)) {
iUdp.flush();
return -5;
}

uint16_t answerCount = htons(header.word[3]);

if (answerCount == 0) {
iUdp.flush();
return -6;
}

// Skip over any questions.
for (uint16_t i = 0; i < htons(header.word[2]); i++) {
uint8_t len = 0;

do {
if (!readExact(&len, sizeof(len))) {
return TRUNCATED;
}

if (len > 0) {
if (!readExact(nullptr, (size_t)len)) {
return TRUNCATED;
}
}
} while (len != 0);

// TYPE + CLASS.
if (!readExact(nullptr, 4)) {
return TRUNCATED;
}
}

// Walk answers until the first A/IN record is found.
for (uint16_t i = 0; i < answerCount; i++) {
uint8_t len = 0;

do {
if (!readExact(&len, sizeof(len))) {
return TRUNCATED;
}

if ((len & LABEL_COMPRESSION_MASK) == 0) {
if (len > 0) {
if (!readExact(nullptr, (size_t)len)) {
return TRUNCATED;
}
}
}
else {
// Compressed name pointer: consume its second byte.
if (!readExact(nullptr, 1)) {
return TRUNCATED;
}

len = 0;
}
} while (len != 0);

uint16_t answerType;
uint16_t answerClass;

if (!readExact(
        reinterpret_cast<uint8_t*>(&answerType),
        sizeof(answerType))) {
return TRUNCATED;
}

if (!readExact(
        reinterpret_cast<uint8_t*>(&answerClass),
        sizeof(answerClass))) {
return TRUNCATED;
}

// TTL.
if (!readExact(nullptr, TTL_SIZE)) {
return TRUNCATED;
}

// RDLENGTH. Reuse header_flags as in the historical implementation.
if (!readExact(
        reinterpret_cast<uint8_t*>(&header_flags),
        sizeof(header_flags))) {
return TRUNCATED;
}

const uint16_t answerLength =
htons(header_flags);

if ((htons(answerType) == TYPE_A) &&
    (htons(answerClass) == CLASS_IN)) {

if (answerLength != 4) {
iUdp.flush();
return -9;
}

if (!readExact(
        aAddress.raw_address(),
        4)) {
return TRUNCATED;
}

return SUCCESS;
}

// Not an A/IN answer: consume exactly this RDATA payload.
if (!readExact(nullptr, answerLength)) {
return TRUNCATED;
}
}

iUdp.flush();

return -10;
}
