// JWPLC_Red: cable o WiFi con el mismo código.
//
//   #include <JWPLC_Red.h>
//   #include <JWPLC_WiFi.h>        // solo si se usará WiFi
//
//   JWPLC_RedClient cliente;      // cliente TCP: sale por la red activa
//
//   void setup()
//   {
//       JWPLC_WiFi.connect("MiRed", "MiClave");
//       JWPLC_Red.useAuto();      // cable si está conectado; si no, WiFi
//   }
//
//   void loop()
//   {
//       if (JWPLC_Red.isReady() && cliente.connect("ejemplo.com", 80)) { ... }
//   }
//
// - Modos: useEthernet(), useWiFi() o useAuto() (por defecto: Ethernet, como hasta ahora).
// - JWPLC_RedClient es un Client de Arduino: sirve para cualquier protocolo sobre TCP (HTTP,
//   MQTT, JW Cloud) y se puede pasar a librerías que reciben un Client.
// - La red se elige al abrir cada conexión: si en modo automático se desenchufa el cable, la
//   próxima conexión sale por WiFi sin tocar el sketch.
// - WiFi es opcional: sin #include <JWPLC_WiFi.h> la librería solo usa el cable y no agrega la
//   pila WiFi al programa. Así también corre en JWPLC Sim, que hoy simula solo el W5500.
#pragma once

#include <Arduino.h>
#include <Client.h>
#include <IPAddress.h>

// Modo elegido (mode()).
const uint8_t RED_MODE_ETHERNET = 1;
const uint8_t RED_MODE_WIFI = 2;
const uint8_t RED_MODE_AUTO = 3;

// Red por la que sale ahora (active()).
const uint8_t RED_NONE = 0;
const uint8_t RED_ETHERNET = 1;
const uint8_t RED_WIFI = 2;

class JWPLC_RedClass
{
public:
    void useEthernet();
    void useWiFi();
    void useAuto();
    uint8_t mode();

    // Red que se usaría para una conexión nueva ahora mismo (RED_NONE si ninguna está lista).
    uint8_t active();
    const char *activeName();
    bool isReady();
    IPAddress localIP();

    // true si el sketch incluye JWPLC_WiFi.h.
    bool wifiAvailable();
    bool ethernetReady();
    bool wifiReady();

    void printStatus(Stream &out);

private:
    volatile uint8_t mode_ = RED_MODE_ETHERNET;
};

extern JWPLC_RedClass JWPLC_Red;

// Cliente TCP que sale por la red activa al momento de connect().
class JWPLC_RedClient : public Client
{
public:
    JWPLC_RedClient();
    ~JWPLC_RedClient();

    int connect(IPAddress ip, uint16_t port) override;
    int connect(const char *host, uint16_t port) override;
    size_t write(uint8_t b) override;
    size_t write(const uint8_t *buf, size_t size) override;
    int available() override;
    int read() override;
    int read(uint8_t *buf, size_t size) override;
    int peek() override;
    void flush() override;
    void stop() override;
    uint8_t connected() override;
    operator bool() override;
    using Print::write;

    // Red por la que salió la conexión actual (RED_NONE si no hay).
    uint8_t network();

private:
    Client *pick();

    Client *ethernet_ = nullptr;
    Client *wifi_ = nullptr;
    Client *current_ = nullptr;
    uint8_t network_ = RED_NONE;
};
