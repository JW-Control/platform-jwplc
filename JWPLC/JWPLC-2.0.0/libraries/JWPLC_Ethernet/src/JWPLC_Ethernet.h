#ifndef JWPLC_ETHERNET_H
#define JWPLC_ETHERNET_H

#include <Arduino.h>
#include <IPAddress.h>
#include <Ethernet.h>

#include "jwplc_hardware_config.h"
#include "jwplc_spi_bus.h"

#ifndef JWPLC_ETH_RESET_PIN
#define JWPLC_ETH_RESET_PIN 255
#endif

#ifndef JWPLC_ETH_DHCP_TIMEOUT_MS
#define JWPLC_ETH_DHCP_TIMEOUT_MS 15000UL
#endif

#ifndef JWPLC_ETH_RESPONSE_TIMEOUT_MS
#define JWPLC_ETH_RESPONSE_TIMEOUT_MS 4000UL
#endif

#ifndef JWPLC_ETH_RETRANSMISSION_COUNT
#define JWPLC_ETH_RETRANSMISSION_COUNT 3
#endif

enum JWPLCEthernetError : uint8_t
{
    JWPLC_ETH_OK = 0,
    JWPLC_ETH_DISABLED,
    JWPLC_ETH_SPI_NOT_READY,
    JWPLC_ETH_NO_HARDWARE,
    JWPLC_ETH_LINK_OFF,
    JWPLC_ETH_DHCP_FAILED,
    JWPLC_ETH_INVALID_IP,
    JWPLC_ETH_BUS_LOCK_TIMEOUT,
    JWPLC_ETH_UNKNOWN_ERROR
};

enum JWPLCEthernetMode : uint8_t
{
    JWPLC_ETH_MODE_DHCP = 0,
    JWPLC_ETH_MODE_STATIC
};

class JWPLC_EthernetClass
{
public:
    JWPLC_EthernetClass();

    // Configuración base del hardware.
    void configure(
        uint8_t csPin = JWPLC_ETH_CS,
        uint8_t resetPin = JWPLC_ETH_RESET_PIN);

    void setResetPin(uint8_t resetPin);
    void setMac(const uint8_t mac[6]);
    void useDefaultMac();

    void useDHCP();
    void setStaticIP(
        IPAddress localIP,
        IPAddress dnsIP,
        IPAddress gatewayIP,
        IPAddress subnetMask);

    void setTimeouts(uint32_t dhcpTimeoutMs, uint32_t responseTimeoutMs);
    void setRetransmissionCount(uint8_t count);

    // Inicialización.
    bool begin();
    bool begin(const uint8_t mac[6]);
    bool begin(
        IPAddress localIP,
        IPAddress dnsIP,
        IPAddress gatewayIP,
        IPAddress subnetMask);

    // Mantenimiento DHCP.
    // Devuelve el resultado original de Ethernet.maintain().
    int maintain();

    // Estado general.
    bool isEnabled() const;
    bool isBeginAttempted() const;
    bool isReady() const;
    bool hardwarePresent();
    bool linkUp();

    EthernetHardwareStatus hardwareStatus();
    EthernetLinkStatus linkStatus();

    IPAddress localIP();
    IPAddress subnetMask();
    IPAddress gatewayIP();
    IPAddress dnsServerIP();

    JWPLCEthernetMode mode() const;
    JWPLCEthernetError lastError() const;
    const char *lastErrorString() const;
    const char *statusString();

    const uint8_t *mac() const;
    uint8_t csPin() const;
    uint8_t resetPin() const;

    void printStatus(Stream &out);

private:
    uint8_t _csPin;
    uint8_t _resetPin;
    uint8_t _mac[6];

    JWPLCEthernetMode _mode;
    IPAddress _localIP;
    IPAddress _dnsIP;
    IPAddress _gatewayIP;
    IPAddress _subnetMask;

    uint32_t _dhcpTimeoutMs;
    uint32_t _responseTimeoutMs;
    uint8_t _retransmissionCount;

    bool _beginAttempted;
    bool _ready;
    JWPLCEthernetError _lastError;

    void generateDefaultMac();
    void resetHardwareIfNeeded();
    bool prepareSPI();
    bool acquireBus(uint32_t timeoutMs);
    void releaseBus();
    void setError(JWPLCEthernetError error);
    void clearError();
};

extern JWPLC_EthernetClass JWPLC_Ethernet;

#endif // JWPLC_ETHERNET_H
