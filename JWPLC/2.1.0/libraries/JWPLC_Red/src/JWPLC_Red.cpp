#include "JWPLC_Red.h"

#include <JWPLC_Ethernet.h>

JWPLC_RedClass JWPLC_Red;

// Definidos en JWPLC_WiFi.cpp. Son débiles: si el sketch no incluye JWPLC_WiFi.h, el enlazador
// los deja en nulo y JWPLC_Red trabaja solo con el cable.
bool jwplc_red_wifi_ready() __attribute__((weak));
IPAddress jwplc_red_wifi_local_ip() __attribute__((weak));
Client *jwplc_red_wifi_new_client() __attribute__((weak));
Client *jwplc_red_wifi_copy_client(const Client *c) __attribute__((weak));
bool jwplc_red_wifi_server_ready() __attribute__((weak));
void *jwplc_red_wifi_server_new(uint16_t port) __attribute__((weak));
void jwplc_red_wifi_server_begin(void *server) __attribute__((weak));
void jwplc_red_wifi_server_end(void *server) __attribute__((weak));
void jwplc_red_wifi_server_delete(void *server) __attribute__((weak));
Client *jwplc_red_wifi_server_accept(void *server) __attribute__((weak));

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

JWPLC_RedClient::JWPLC_RedClient(const JWPLC_RedClient &other) : Client()
{
    *this = other;
}

JWPLC_RedClient &JWPLC_RedClient::operator=(const JWPLC_RedClient &other)
{
    if (this == &other)
        return *this;
    releaseWiFi();
    ethernet_ = other.ethernet_;
    if (other.wifi_ != nullptr && jwplc_red_wifi_copy_client != nullptr)
        wifi_ = jwplc_red_wifi_copy_client(other.wifi_);
    network_ = other.network_;
    return *this;
}

JWPLC_RedClient::~JWPLC_RedClient()
{
    // Como EthernetClient y WiFiClient: salir del ámbito no cierra la conexión (para eso, stop()).
    releaseWiFi();
}

void JWPLC_RedClient::releaseWiFi()
{
    delete wifi_;
    wifi_ = nullptr;
}

Client *JWPLC_RedClient::current()
{
    if (network_ == RED_ETHERNET)
        return &ethernet_;
    if (network_ == RED_WIFI)
        return wifi_;
    return nullptr;
}

void JWPLC_RedClient::adoptEthernetInternal(const EthernetClient &c)
{
    releaseWiFi();
    ethernet_ = c;
    network_ = RED_ETHERNET;
}

void JWPLC_RedClient::adoptWiFiInternal(Client *c)
{
    releaseWiFi();
    wifi_ = c;
    network_ = c != nullptr ? RED_WIFI : RED_NONE;
}

int JWPLC_RedClient::connect(IPAddress ip, uint16_t port)
{
    stop();
    network_ = JWPLC_Red.active();
    if (network_ == RED_WIFI && wifi_ == nullptr && jwplc_red_wifi_new_client != nullptr)
        wifi_ = jwplc_red_wifi_new_client();
    Client *c = current();
    if (c == nullptr)
    {
        network_ = RED_NONE;
        return 0;
    }
    return c->connect(ip, port);
}

int JWPLC_RedClient::connect(const char *host, uint16_t port)
{
    stop();
    network_ = JWPLC_Red.active();
    if (network_ == RED_WIFI && wifi_ == nullptr && jwplc_red_wifi_new_client != nullptr)
        wifi_ = jwplc_red_wifi_new_client();
    Client *c = current();
    if (c == nullptr)
    {
        network_ = RED_NONE;
        return 0;
    }
    return c->connect(host, port);
}

size_t JWPLC_RedClient::write(uint8_t b)
{
    Client *c = current();
    return c != nullptr ? c->write(b) : 0;
}

size_t JWPLC_RedClient::write(const uint8_t *buf, size_t size)
{
    Client *c = current();
    return c != nullptr ? c->write(buf, size) : 0;
}

int JWPLC_RedClient::available()
{
    Client *c = current();
    return c != nullptr ? c->available() : 0;
}

int JWPLC_RedClient::read()
{
    Client *c = current();
    return c != nullptr ? c->read() : -1;
}

int JWPLC_RedClient::read(uint8_t *buf, size_t size)
{
    Client *c = current();
    return c != nullptr ? c->read(buf, size) : -1;
}

int JWPLC_RedClient::peek()
{
    Client *c = current();
    return c != nullptr ? c->peek() : -1;
}

void JWPLC_RedClient::flush()
{
    Client *c = current();
    if (c != nullptr)
        c->flush();
}

void JWPLC_RedClient::stop()
{
    Client *c = current();
    if (c != nullptr)
        c->stop();
    network_ = RED_NONE;
}

uint8_t JWPLC_RedClient::connected()
{
    Client *c = current();
    return c != nullptr ? c->connected() : 0;
}

JWPLC_RedClient::operator bool()
{
    return connected() != 0;
}

uint8_t JWPLC_RedClient::network()
{
    return current() != nullptr ? network_ : RED_NONE;
}

// ---------------------------------------------------------------- JWPLC_RedServer

namespace
{
bool wifiServerHooks()
{
    return jwplc_red_wifi_server_ready != nullptr && jwplc_red_wifi_server_new != nullptr &&
           jwplc_red_wifi_server_begin != nullptr && jwplc_red_wifi_server_end != nullptr &&
           jwplc_red_wifi_server_delete != nullptr && jwplc_red_wifi_server_accept != nullptr;
}
} // namespace

JWPLC_RedServer::JWPLC_RedServer(uint16_t port) : port_(port)
{
}

JWPLC_RedServer::~JWPLC_RedServer()
{
    end();
}

void JWPLC_RedServer::begin()
{
    started_ = true;
    serviceListeners();
}

void JWPLC_RedServer::begin(uint16_t port)
{
    if (port != port_)
        end();
    port_ = port;
    begin();
}

void JWPLC_RedServer::end()
{
    started_ = false;
    // EthernetServer no tiene end(): sus sockets en escucha se reusan en el próximo begin().
    delete ethernet_;
    ethernet_ = nullptr;
    ethernetListening_ = false;
    if (wifi_ != nullptr)
    {
        jwplc_red_wifi_server_end(wifi_);
        jwplc_red_wifi_server_delete(wifi_);
        wifi_ = nullptr;
    }
    wifiListening_ = false;
}

uint16_t JWPLC_RedServer::port()
{
    return port_;
}

bool JWPLC_RedServer::listeningEthernet()
{
    return ethernetListening_;
}

bool JWPLC_RedServer::listeningWiFi()
{
    return wifiListening_;
}

bool JWPLC_RedServer::wantEthernet()
{
    const uint8_t m = JWPLC_Red.mode();
    return m == RED_MODE_ETHERNET || m == RED_MODE_AUTO;
}

bool JWPLC_RedServer::wantWiFi()
{
    const uint8_t m = JWPLC_Red.mode();
    return (m == RED_MODE_WIFI || m == RED_MODE_AUTO) && wifiServerHooks();
}

void JWPLC_RedServer::serviceListeners()
{
    if (!started_)
        return;

    // Cable: escuchar cuando el W5500 tiene IP; si se pierde, volver a escuchar al regresar.
    if (wantEthernet() && JWPLC_Ethernet.isReady())
    {
        if (!ethernetListening_)
        {
            if (ethernet_ == nullptr)
                ethernet_ = new EthernetServer(port_);
            ethernet_->begin(); // toma el bus W5500 por dentro
            ethernetListening_ = true;
        }
    }
    else
    {
        ethernetListening_ = false;
    }

    // WiFi: red del router o punto de acceso propio.
    if (wantWiFi() && jwplc_red_wifi_server_ready())
    {
        if (!wifiListening_)
        {
            if (wifi_ == nullptr)
                wifi_ = jwplc_red_wifi_server_new(port_);
            if (wifi_ != nullptr)
            {
                jwplc_red_wifi_server_begin(wifi_);
                wifiListening_ = true;
            }
        }
    }
    else if (wifiListening_)
    {
        jwplc_red_wifi_server_end(wifi_);
        wifiListening_ = false;
    }
}

bool JWPLC_RedServer::acceptEthernet(JWPLC_RedClient &out)
{
    if (!ethernetListening_ || ethernet_ == nullptr)
        return false;
    EthernetClient c = ethernet_->accept(); // toma el bus W5500 por dentro
    if (!c)
        return false;
    out.adoptEthernetInternal(c);
    return true;
}

bool JWPLC_RedServer::acceptWiFi(JWPLC_RedClient &out)
{
    if (!wifiListening_ || wifi_ == nullptr)
        return false;
    Client *c = jwplc_red_wifi_server_accept(wifi_);
    if (c == nullptr)
        return false;
    out.adoptWiFiInternal(c);
    return true;
}

JWPLC_RedClient JWPLC_RedServer::available()
{
    JWPLC_RedClient out;
    serviceListeners();
    preferWiFi_ = !preferWiFi_;
    if (preferWiFi_)
    {
        if (!acceptWiFi(out))
            acceptEthernet(out);
    }
    else
    {
        if (!acceptEthernet(out))
            acceptWiFi(out);
    }
    return out;
}
