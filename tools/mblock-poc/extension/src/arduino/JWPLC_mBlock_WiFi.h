// JWPLC_mBlock_WiFi.h
//
// WiFi y mensajes entre placas para los bloques de mBlock en la JWPLC Basic.
// Separado de JWPLC_mBlock.h: sólo lo incluyen los bloques WiFi, así los
// programas sin WiFi no compilan ni enlazan la librería WiFi.
//
//   JWPLC_Net.connect("MiRed", "clave");  // espera hasta 10 s
//   JWPLC_Net.send("hola", 5);            // mensaje a las placas de la misma red
//   if (JWPLC_Net.received("hola")) ...   // una vez por mensaje recibido
//   JWPLC_Net.value("hola");              // último valor recibido

#ifndef JWPLC_MBLOCK_WIFI_H
#define JWPLC_MBLOCK_WIFI_H

#include "JWPLC_mBlock.h"
#include <WiFi.h>
#include <WiFiUdp.h>

// WiFi y mensajes entre placas, como el "LAN broadcast" de CyberPi: cada
// mensaje viaja como UDP broadcast en la red WiFi local, en texto
// "JWPLC|<canal>|<mensaje>|<valor>" al puerto 47821. Sólo se aceptan
// mensajes del mismo canal (para separar grupos en un aula). Va por la
// interfaz WiFi (lwIP); la Ethernet del JWPLC usa su propia pila W5x00.
class JWPLC_NetHelper
{
public:
    static const uint16_t PORT = 47821;
    static const uint8_t SLOTS = 8;

    // Conecta y espera hasta timeoutSeconds. delay() cede la CPU al runtime.
    bool connect(const String &ssid, const String &password, double timeoutSeconds = 10)
    {
        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid.c_str(), password.c_str());
        const unsigned long limit = (unsigned long)(timeoutSeconds * 1000);
        const unsigned long start = millis();
        while (!connected() && millis() - start < limit)
            delay(100);
        _udpStarted = false;
        return connected();
    }

    bool connected() const { return WiFi.status() == WL_CONNECTED; }
    String ip() const { return connected() ? WiFi.localIP().toString() : String("0.0.0.0"); }

    void disconnect()
    {
        _udp.stop();
        _udpStarted = false;
        WiFi.disconnect(true);
    }

    void channel(int number) { _channel = number; }

    void send(const String &message) { sendRaw(message, ""); }

    template <typename T>
    void send(const String &message, T value) { sendRaw(message, jwplc_mblock::text(value)); }

    // true una vez por cada mensaje recibido con ese nombre (lo consume).
    bool received(const String &message)
    {
        poll();
        Slot *slot = find(message);
        if (!slot || !slot->pending)
            return false;
        slot->pending = false;
        return true;
    }

    // Valor del último mensaje recibido con ese nombre (0 si nunca llegó).
    double value(const String &message)
    {
        poll();
        Slot *slot = find(message);
        return slot ? slot->value.toDouble() : 0;
    }

private:
    struct Slot
    {
        String name;
        String value;
        bool pending = false;
        unsigned long lastMs = 0;
    };

    WiFiUDP _udp;
    bool _udpStarted = false;
    int _channel = 1;
    Slot _slots[SLOTS];

    static String clean(const String &text)
    {
        String out = text;
        out.replace("|", "/");
        return out;
    }

    bool ensureUdp()
    {
        if (!connected())
        {
            _udpStarted = false;
            return false;
        }
        if (!_udpStarted)
            _udpStarted = _udp.begin(PORT);
        return _udpStarted;
    }

    void sendRaw(const String &message, const String &value)
    {
        if (!ensureUdp())
            return;
        const String packet = "JWPLC|" + String(_channel) + "|" + clean(message) + "|" + clean(value);
        _udp.beginPacket(WiFi.broadcastIP(), PORT);
        _udp.print(packet);
        _udp.endPacket();
    }

    Slot *find(const String &message)
    {
        const String name = clean(message);
        for (uint8_t i = 0; i < SLOTS; i++)
            if (_slots[i].lastMs != 0 && _slots[i].name == name)
                return &_slots[i];
        return nullptr;
    }

    // Lee los paquetes pendientes y guarda el último valor de cada mensaje
    // (hasta SLOTS nombres; si no hay sitio se reemplaza el más antiguo).
    void poll()
    {
        if (!ensureUdp())
            return;
        while (_udp.parsePacket() > 0)
        {
            char buffer[128];
            const int len = _udp.read(buffer, sizeof(buffer) - 1);
            if (len <= 0)
                continue;
            buffer[len] = 0;
            const String packet(buffer);
            const int a = packet.indexOf('|');
            const int b = packet.indexOf('|', a + 1);
            const int c = packet.indexOf('|', b + 1);
            if (a < 0 || b < 0 || c < 0 || packet.substring(0, a) != "JWPLC")
                continue;
            if (packet.substring(a + 1, b).toInt() != _channel)
                continue;
            store(packet.substring(b + 1, c), packet.substring(c + 1));
        }
    }

    void store(const String &name, const String &value)
    {
        Slot *slot = find(name);
        if (!slot)
        {
            slot = &_slots[0];
            for (uint8_t i = 1; i < SLOTS; i++)
                if (_slots[i].lastMs < slot->lastMs)
                    slot = &_slots[i];
            slot->name = name;
        }
        slot->value = value;
        slot->pending = true;
        slot->lastMs = millis() | 1; // 0 = libre
    }
};

inline JWPLC_NetHelper JWPLC_Net;

#endif // JWPLC_MBLOCK_WIFI_H
