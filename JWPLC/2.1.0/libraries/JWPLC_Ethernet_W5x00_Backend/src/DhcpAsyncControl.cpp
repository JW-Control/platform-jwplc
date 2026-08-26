#include <Arduino.h>
#include "Ethernet.h"
#include "Dhcp.h"

void DhcpClass::cancelDHCP()
{
    _dhcpUdpSocket.stop();
    _asyncActive = false;
    _dhcp_state = STATE_DHCP_START;
}

void EthernetClass::cancelDHCP()
{
    if (_dhcp == NULL)
    {
        return;
    }

    _dhcp->cancelDHCP();
    _dhcp = NULL;
}
