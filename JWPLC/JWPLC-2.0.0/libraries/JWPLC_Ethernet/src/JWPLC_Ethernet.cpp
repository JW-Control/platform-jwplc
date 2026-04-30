#include "JWPLC_Ethernet.h"

#include <SPI.h>

#if defined(ESP32)
#include <Esp.h>
#endif

JWPLC_EthernetClass JWPLC_Ethernet;

JWPLC_EthernetClass::JWPLC_EthernetClass()
    : _csPin(JWPLC_ETH_CS),
      _resetPin(JWPLC_ETH_RESET_PIN),
      _mode(JWPLC_ETH_MODE_DHCP),
      _localIP(0, 0, 0, 0),
      _dnsIP(0, 0, 0, 0),
      _gatewayIP(0, 0, 0, 0),
      _subnetMask(255, 255, 255, 0),
      _dhcpTimeoutMs(JWPLC_ETH_DHCP_TIMEOUT_MS),
      _responseTimeoutMs(JWPLC_ETH_RESPONSE_TIMEOUT_MS),
      _retransmissionCount(JWPLC_ETH_RETRANSMISSION_COUNT),
      _beginAttempted(false),
      _ready(false),
      _lastError(JWPLC_ETH_OK)
{
    generateDefaultMac();
}

void JWPLC_EthernetClass::configure(uint8_t csPin, uint8_t resetPin)
{
    _csPin = csPin;
    _resetPin = resetPin;
}

void JWPLC_EthernetClass::setResetPin(uint8_t resetPin)
{
    _resetPin = resetPin;
}

void JWPLC_EthernetClass::setMac(const uint8_t mac[6])
{
    if (mac == nullptr)
    {
        generateDefaultMac();
        return;
    }

    memcpy(_mac, mac, 6);
}

void JWPLC_EthernetClass::useDefaultMac()
{
    generateDefaultMac();
}

void JWPLC_EthernetClass::useDHCP()
{
    _mode = JWPLC_ETH_MODE_DHCP;
}

void JWPLC_EthernetClass::setStaticIP(
    IPAddress localIP,
    IPAddress dnsIP,
    IPAddress gatewayIP,
    IPAddress subnetMask)
{
    _mode = JWPLC_ETH_MODE_STATIC;
    _localIP = localIP;
    _dnsIP = dnsIP;
    _gatewayIP = gatewayIP;
    _subnetMask = subnetMask;
}

void JWPLC_EthernetClass::setTimeouts(uint32_t dhcpTimeoutMs, uint32_t responseTimeoutMs)
{
    _dhcpTimeoutMs = dhcpTimeoutMs;
    _responseTimeoutMs = responseTimeoutMs;
}

void JWPLC_EthernetClass::setRetransmissionCount(uint8_t count)
{
    _retransmissionCount = count;
}

bool JWPLC_EthernetClass::begin()
{
#if !JWPLC_HAS_ETHERNET
    _beginAttempted = true;
    _ready = false;
    setError(JWPLC_ETH_DISABLED);
    return false;
#else
    _beginAttempted = true;
    _ready = false;
    clearError();

    if (!prepareSPI())
    {
        _ready = false;
        return false;
    }

    resetHardwareIfNeeded();

    if (!acquireBus(500))
    {
        setError(JWPLC_ETH_BUS_LOCK_TIMEOUT);
        return false;
    }

    jwplcSPI_deselectAll();

    Ethernet.init(_csPin);
    Ethernet.setRetransmissionTimeout((uint16_t)_responseTimeoutMs);
    Ethernet.setRetransmissionCount(_retransmissionCount);

    // Inicialización rápida del W5500 sin DHCP.
    // Esto permite que hardwareStatus() y linkStatus() funcionen
    // sin bloquear varios segundos cuando no hay cable RJ45.
    Ethernet.begin(
        _mac,
        IPAddress(0, 0, 0, 0),
        IPAddress(0, 0, 0, 0),
        IPAddress(0, 0, 0, 0),
        IPAddress(255, 255, 255, 0));

    EthernetHardwareStatus hw = Ethernet.hardwareStatus();

    if (hw == EthernetNoHardware)
    {
        releaseBus();

        _ready = false;
        setError(JWPLC_ETH_NO_HARDWARE);
        return false;
    }

    EthernetLinkStatus link = Ethernet.linkStatus();

    if (link != LinkON)
    {
        releaseBus();

        _ready = false;
        setError(JWPLC_ETH_LINK_OFF);
        return false;
    }

    int ok = 0;

    if (_mode == JWPLC_ETH_MODE_DHCP)
    {
        ok = Ethernet.begin(_mac, _dhcpTimeoutMs, _responseTimeoutMs);
    }
    else
    {
        Ethernet.begin(_mac, _localIP, _dnsIP, _gatewayIP, _subnetMask);
        ok = 1;
    }

    releaseBus();

    if (_mode == JWPLC_ETH_MODE_DHCP && ok == 0)
    {
        _ready = false;
        setError(JWPLC_ETH_DHCP_FAILED);
        return false;
    }

    IPAddress ip = localIP();

    if (ip == IPAddress(0, 0, 0, 0))
    {
        _ready = false;
        setError(JWPLC_ETH_INVALID_IP);
        return false;
    }

    if (!linkUp())
    {
        // El hardware e IP pueden estar listos, pero el cable no.
        // No se marca como error fatal: se reporta por statusString().
        _ready = true;
        setError(JWPLC_ETH_LINK_OFF);
        return true;
    }

    _ready = true;
    clearError();
    return true;
#endif
}

bool JWPLC_EthernetClass::begin(const uint8_t mac[6])
{
    setMac(mac);
    return begin();
}

bool JWPLC_EthernetClass::begin(
    IPAddress localIP,
    IPAddress dnsIP,
    IPAddress gatewayIP,
    IPAddress subnetMask)
{
    setStaticIP(localIP, dnsIP, gatewayIP, subnetMask);
    return begin();
}

int JWPLC_EthernetClass::maintain()
{
#if !JWPLC_HAS_ETHERNET
    return 0;
#else
    if (!_ready)
    {
        return 0;
    }

    if (!acquireBus(200))
    {
        setError(JWPLC_ETH_BUS_LOCK_TIMEOUT);
        return 0;
    }

    jwplcSPI_deselectAll();
    int result = Ethernet.maintain();

    releaseBus();

    // Ethernet.maintain():
    // 0 = nada
    // 1 = renew failed
    // 2 = renew success
    // 3 = rebind failed
    // 4 = rebind success
    if (result == 1 || result == 3)
    {
        setError(JWPLC_ETH_DHCP_FAILED);
    }

    return result;
#endif
}

bool JWPLC_EthernetClass::isEnabled() const
{
#if JWPLC_HAS_ETHERNET
    return true;
#else
    return false;
#endif
}

bool JWPLC_EthernetClass::isBeginAttempted() const
{
    return _beginAttempted;
}

bool JWPLC_EthernetClass::isReady() const
{
#if JWPLC_HAS_ETHERNET
    return _ready;
#else
    return false;
#endif
}

bool JWPLC_EthernetClass::hardwarePresent()
{
#if !JWPLC_HAS_ETHERNET
    return false;
#else
    return hardwareStatus() != EthernetNoHardware;
#endif
}

bool JWPLC_EthernetClass::linkUp()
{
#if !JWPLC_HAS_ETHERNET
    return false;
#else
    return linkStatus() == LinkON;
#endif
}

EthernetHardwareStatus JWPLC_EthernetClass::hardwareStatus()
{
#if !JWPLC_HAS_ETHERNET
    return EthernetNoHardware;
#else
    if (!acquireBus(200))
    {
        setError(JWPLC_ETH_BUS_LOCK_TIMEOUT);
        return EthernetNoHardware;
    }

    jwplcSPI_deselectAll();
    EthernetHardwareStatus status = Ethernet.hardwareStatus();

    releaseBus();
    return status;
#endif
}

EthernetLinkStatus JWPLC_EthernetClass::linkStatus()
{
#if !JWPLC_HAS_ETHERNET
    return Unknown;
#else
    if (!acquireBus(200))
    {
        setError(JWPLC_ETH_BUS_LOCK_TIMEOUT);
        return Unknown;
    }

    jwplcSPI_deselectAll();
    EthernetLinkStatus status = Ethernet.linkStatus();

    releaseBus();
    return status;
#endif
}

IPAddress JWPLC_EthernetClass::localIP()
{
#if !JWPLC_HAS_ETHERNET
    return IPAddress(0, 0, 0, 0);
#else
    if (!acquireBus(200))
    {
        setError(JWPLC_ETH_BUS_LOCK_TIMEOUT);
        return IPAddress(0, 0, 0, 0);
    }

    jwplcSPI_deselectAll();
    IPAddress ip = Ethernet.localIP();

    releaseBus();
    return ip;
#endif
}

IPAddress JWPLC_EthernetClass::subnetMask()
{
#if !JWPLC_HAS_ETHERNET
    return IPAddress(0, 0, 0, 0);
#else
    if (!acquireBus(200))
    {
        setError(JWPLC_ETH_BUS_LOCK_TIMEOUT);
        return IPAddress(0, 0, 0, 0);
    }

    jwplcSPI_deselectAll();
    IPAddress ip = Ethernet.subnetMask();

    releaseBus();
    return ip;
#endif
}

IPAddress JWPLC_EthernetClass::gatewayIP()
{
#if !JWPLC_HAS_ETHERNET
    return IPAddress(0, 0, 0, 0);
#else
    if (!acquireBus(200))
    {
        setError(JWPLC_ETH_BUS_LOCK_TIMEOUT);
        return IPAddress(0, 0, 0, 0);
    }

    jwplcSPI_deselectAll();
    IPAddress ip = Ethernet.gatewayIP();

    releaseBus();
    return ip;
#endif
}

IPAddress JWPLC_EthernetClass::dnsServerIP()
{
#if !JWPLC_HAS_ETHERNET
    return IPAddress(0, 0, 0, 0);
#else
    if (!acquireBus(200))
    {
        setError(JWPLC_ETH_BUS_LOCK_TIMEOUT);
        return IPAddress(0, 0, 0, 0);
    }

    jwplcSPI_deselectAll();
    IPAddress ip = Ethernet.dnsServerIP();

    releaseBus();
    return ip;
#endif
}

JWPLCEthernetMode JWPLC_EthernetClass::mode() const
{
    return _mode;
}

JWPLCEthernetError JWPLC_EthernetClass::lastError() const
{
    return _lastError;
}

const char *JWPLC_EthernetClass::lastErrorString() const
{
    switch (_lastError)
    {
    case JWPLC_ETH_OK:
        return "OK";
    case JWPLC_ETH_DISABLED:
        return "Ethernet disabled";
    case JWPLC_ETH_SPI_NOT_READY:
        return "SPI not ready";
    case JWPLC_ETH_NO_HARDWARE:
        return "No Ethernet hardware";
    case JWPLC_ETH_LINK_OFF:
        return "Link OFF";
    case JWPLC_ETH_DHCP_FAILED:
        return "DHCP failed";
    case JWPLC_ETH_INVALID_IP:
        return "Invalid IP";
    case JWPLC_ETH_BUS_LOCK_TIMEOUT:
        return "SPI lock timeout";
    default:
        return "Unknown Ethernet error";
    }
}

const char *JWPLC_EthernetClass::statusString()
{
#if !JWPLC_HAS_ETHERNET
    return "Ethernet disabled";
#else
    if (!_beginAttempted)
    {
        return "Not started";
    }

    if (!hardwarePresent())
    {
        return "No Ethernet hardware";
    }

    if (!_ready)
    {
        return lastErrorString();
    }

    if (!linkUp())
    {
        return "Link OFF";
    }

    return "OK";
#endif
}

const uint8_t *JWPLC_EthernetClass::mac() const
{
    return _mac;
}

uint8_t JWPLC_EthernetClass::csPin() const
{
    return _csPin;
}

uint8_t JWPLC_EthernetClass::resetPin() const
{
    return _resetPin;
}

void JWPLC_EthernetClass::printStatus(Stream &out)
{
    out.print("Ethernet enabled: ");
    out.println(isEnabled() ? "yes" : "no");

    out.print("Begin attempted: ");
    out.println(isBeginAttempted() ? "yes" : "no");

    out.print("Ready: ");
    out.println(isReady() ? "yes" : "no");

    out.print("Hardware: ");
    out.println(hardwarePresent() ? "present" : "not found");

    out.print("Link: ");
    out.println(linkUp() ? "up" : "down");

    out.print("Status: ");
    out.println(statusString());

    out.print("IP: ");
    out.println(localIP());

    out.print("Gateway: ");
    out.println(gatewayIP());

    out.print("Subnet: ");
    out.println(subnetMask());

    out.print("DNS: ");
    out.println(dnsServerIP());

    out.print("MAC: ");
    for (uint8_t i = 0; i < 6; i++)
    {
        if (i)
        {
            out.print(":");
        }

        if (_mac[i] < 0x10)
        {
            out.print("0");
        }

        out.print(_mac[i], HEX);
    }
    out.println();
}

void JWPLC_EthernetClass::generateDefaultMac()
{
#if defined(ESP32)
    uint64_t efuseMac = ESP.getEfuseMac();

    // Locally administered MAC:
    // 02:4A:57 = 02 + 'J' + 'W'
    _mac[0] = 0x02;
    _mac[1] = 0x4A;
    _mac[2] = 0x57;
    _mac[3] = (uint8_t)((efuseMac >> 16) & 0xFF);
    _mac[4] = (uint8_t)((efuseMac >> 8) & 0xFF);
    _mac[5] = (uint8_t)(efuseMac & 0xFF);
#else
    _mac[0] = 0x02;
    _mac[1] = 0x4A;
    _mac[2] = 0x57;
    _mac[3] = 0x00;
    _mac[4] = 0x00;
    _mac[5] = 0x01;
#endif
}

void JWPLC_EthernetClass::resetHardwareIfNeeded()
{
    if (_resetPin == 255)
    {
        return;
    }

    pinMode(_resetPin, OUTPUT);
    digitalWrite(_resetPin, LOW);
    delay(10);
    digitalWrite(_resetPin, HIGH);
    delay(80);
}

bool JWPLC_EthernetClass::prepareSPI()
{
#if !JWPLC_HAS_ETHERNET
    setError(JWPLC_ETH_DISABLED);
    return false;
#else
    SPI.begin(JWPLC_SPI_SCK, JWPLC_SPI_MISO, JWPLC_SPI_MOSI);

    if (!jwplcSPI_begin())
    {
        setError(JWPLC_ETH_SPI_NOT_READY);
        return false;
    }

    return true;
#endif
}

bool JWPLC_EthernetClass::acquireBus(uint32_t timeoutMs)
{
#if !JWPLC_HAS_ETHERNET
    (void)timeoutMs;
    return false;
#else
    return jwplcSPI_acquire(timeoutMs);
#endif
}

void JWPLC_EthernetClass::releaseBus()
{
#if JWPLC_HAS_ETHERNET
    jwplcSPI_release();
#endif
}

void JWPLC_EthernetClass::setError(JWPLCEthernetError error)
{
    _lastError = error;
}

void JWPLC_EthernetClass::clearError()
{
    _lastError = JWPLC_ETH_OK;
}

// =====================================================
// Hook automático del runtime JWPLC
// =====================================================
// El core llama periódicamente este hook desde jwplcSystemTask().
// Esto permite que Ethernet arranque y se mantenga sin que el usuario
// tenga que llamar obligatoriamente JWPLC_Ethernet.begin() en el sketch.

extern "C" void jwplcEthernetTickCallback(void)
{
#if JWPLC_HAS_ETHERNET
    static uint32_t lastRetryMs = 0;

    if (!JWPLC_Ethernet.isEnabled())
    {
        return;
    }

    if (JWPLC_Ethernet.isReady())
    {
        JWPLC_Ethernet.maintain();
        return;
    }

    uint32_t now = millis();

    if (!JWPLC_Ethernet.isBeginAttempted() ||
        ((uint32_t)(now - lastRetryMs) >= 5000UL))
    {
        lastRetryMs = now;
        (void)JWPLC_Ethernet.begin();
    }
#endif
}