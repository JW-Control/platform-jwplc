// JWPLC_mBlock_Ethernet.h
//
// Ethernet (W5500) del JWPLC Basic para los bloques de mBlock. El runtime del
// package arranca y atiende la Ethernet solo; los bloques sólo eligen DHCP o
// IP fija, y deben ir en "cuando JWPLC Basic arranca" (setup()), que termina
// antes de que el runtime empiece la red.
//
//   JWPLC_EthernetEasy.useStaticIP("192.168.0.50", "192.168.0.1");
//   JWPLC_EthernetEasy.ip();   // "0.0.0.0" hasta que esté lista

#ifndef JWPLC_MBLOCK_ETHERNET_H
#define JWPLC_MBLOCK_ETHERNET_H

#include "JWPLC_mBlock.h"
#include <JWPLC_Ethernet.h>

class JWPLC_EthernetEasyHelper
{
public:
    void useDHCP() { JWPLC_Ethernet.useDHCP(); }

    // Máscara 255.255.255.0; la puerta de enlace también hace de DNS.
    bool useStaticIP(const String &ip, const String &gateway)
    {
        IPAddress local, gw;
        if (!local.fromString(ip) || !gw.fromString(gateway))
            return false;
        JWPLC_Ethernet.setStaticIP(local, gw, gw, IPAddress(255, 255, 255, 0));
        return true;
    }

    bool ready() const { return JWPLC_Ethernet.isReady(); }

    String ip() const
    {
        return JWPLC_Ethernet.isReady() ? JWPLC_Ethernet.localIP().toString() : String("0.0.0.0");
    }
};

inline JWPLC_EthernetEasyHelper JWPLC_EthernetEasy;

#endif // JWPLC_MBLOCK_ETHERNET_H
