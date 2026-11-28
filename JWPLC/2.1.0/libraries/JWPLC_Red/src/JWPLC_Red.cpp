#include "JWPLC_Red.h"

#include <JWPLC_Ethernet.h>

JWPLC_RedClass JWPLC_Red;

// Definidos en JWPLC_WiFi.cpp. Son débiles: si el sketch no incluye JWPLC_WiFi.h, el enlazador
// los deja en nulo y JWPLC_Red trabaja solo con el cable.
bool jwplc_red_wifi_ready() __attribute__((weak));
IPAddress jwplc_red_wifi_local_ip() __attribute__((weak));
Client *jwplc_red_wifi_new_client() __attribute__((weak));

// ---------------------------------------------------------------- JWPLC_Red

void JWPLC_RedClass::useEthernet()
{
    mode_ = RED_MODE_ETHERNET;
}

void JWPLC_RedClass::useWiFi()
{
    mode_ = RED_MODE_WIFI;
}

void JWPLC_RedClass::useAuto()
{
    mode_ = RED_MODE_AUTO;
}

uint8_t JWPLC_RedClass::mode()
{
    return mode_;
}

bool JWPLC_RedClass::wifiAvailable()
{
    return jwplc_red_wifi_ready != nullptr && jwplc_red_wifi_new_client != nullptr;
}

bool JWPLC_RedClass::ethernetReady()
{
    return JWPLC_Ethernet.isReady();
}

bool JWPLC_RedClass::wifiReady()
{
    return wifiAvailable() && jwplc_red_wifi_ready();
}

uint8_t JWPLC_RedClass::active()
{
    switch (mode_)
    {
    case RED_MODE_WIFI:
        return wifiReady() ? RED_WIFI : RED_NONE;
    case RED_MODE_AUTO:
        // El cable manda: si está conectado y con IP es la red preferida.
        if (ethernetReady())
            return RED_ETHERNET;
        return wifiReady() ? RED_WIFI : RED_NONE;
    default:
        return ethernetReady() ? RED_ETHERNET : RED_NONE;
    }
}

const char *JWPLC_RedClass::activeName()
{
    switch (active())
    {
    case RED_ETHERNET:
        return "Ethernet";
    case RED_WIFI:
        return "WiFi";
    default:
        return "Sin red";
    }
}

bool JWPLC_RedClass::isReady()
{
    return active() != RED_NONE;
}

IPAddress JWPLC_RedClass::localIP()
{
    switch (active())
    {
    case RED_ETHERNET:
        return JWPLC_Ethernet.localIP();
    case RED_WIFI:
        return jwplc_red_wifi_local_ip != nullptr ? jwplc_red_wifi_local_ip() : IPAddress();
    default:
        return IPAddress();
    }
}

void JWPLC_RedClass::printStatus(Stream &out)
{
    static const char *const kModos[] = {"?", "Ethernet", "WiFi", "Automático"};
    const uint8_t m = mode_ <= RED_MODE_AUTO ? mode_ : 0;
    out.print("Red: modo ");
    out.print(kModos[m]);
    out.print(" | sale por: ");
    out.print(activeName());
    if (isReady())
    {
        out.print(" | IP: ");
        out.print(localIP());
    }
    out.print(" | cable: ");
    out.print(ethernetReady() ? "listo" : "no");
    out.print(" | WiFi: ");
    out.println(!wifiAvailable() ? "no incluida" : (wifiReady() ? "listo" : "no"));
}

// ---------------------------------------------------------------- JWPLC_RedClient

JWPLC_RedClient::JWPLC_RedClient() = default;

JWPLC_RedClient::~JWPLC_RedClient()
{
    if (current_ != nullptr)
        current_->stop();
    delete ethernet_;
    delete wifi_;
}

Client *JWPLC_RedClient::pick()
{
    network_ = JWPLC_Red.active();
    if (network_ == RED_ETHERNET)
    {
        if (ethernet_ == nullptr)
            ethernet_ = new EthernetClient();
        return ethernet_;
    }
    if (network_ == RED_WIFI)
    {
        if (wifi_ == nullptr && jwplc_red_wifi_new_client != nullptr)
            wifi_ = jwplc_red_wifi_new_client();
        return wifi_;
    }
    return nullptr;
}

int JWPLC_RedClient::connect(IPAddress ip, uint16_t port)
{
    stop();
    current_ = pick();
    if (current_ == nullptr)
    {
        network_ = RED_NONE;
        return 0;
    }
    return current_->connect(ip, port);
}

int JWPLC_RedClient::connect(const char *host, uint16_t port)
{
    stop();
    current_ = pick();
    if (current_ == nullptr)
    {
        network_ = RED_NONE;
        return 0;
    }
    return current_->connect(host, port);
}

size_t JWPLC_RedClient::write(uint8_t b)
{
    return current_ != nullptr ? current_->write(b) : 0;
}

size_t JWPLC_RedClient::write(const uint8_t *buf, size_t size)
{
    return current_ != nullptr ? current_->write(buf, size) : 0;
}

int JWPLC_RedClient::available()
{
    return current_ != nullptr ? current_->available() : 0;
}

int JWPLC_RedClient::read()
{
    return current_ != nullptr ? current_->read() : -1;
}

int JWPLC_RedClient::read(uint8_t *buf, size_t size)
{
    return current_ != nullptr ? current_->read(buf, size) : -1;
}

int JWPLC_RedClient::peek()
{
    return current_ != nullptr ? current_->peek() : -1;
}

void JWPLC_RedClient::flush()
{
    if (current_ != nullptr)
        current_->flush();
}

void JWPLC_RedClient::stop()
{
    if (current_ != nullptr)
        current_->stop();
    current_ = nullptr;
    network_ = RED_NONE;
}

uint8_t JWPLC_RedClient::connected()
{
    return current_ != nullptr ? current_->connected() : 0;
}

JWPLC_RedClient::operator bool()
{
    return connected() != 0;
}

uint8_t JWPLC_RedClient::network()
{
    return current_ != nullptr ? network_ : RED_NONE;
}
