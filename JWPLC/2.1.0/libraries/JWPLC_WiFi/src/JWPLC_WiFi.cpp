#include "JWPLC_WiFi.h"

#include <WiFi.h>
#include <esp_mac.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

JWPLC_WiFiClass JWPLC_WiFi;

namespace
{
// Cada cuánto la tarea revisa el estado y cuánto espera antes de reintentar.
constexpr uint32_t kTickMs = 250;
constexpr uint32_t kRetryMs = 15000;          // conectando o conexión perdida
constexpr uint32_t kRetryAfterErrorMs = 30000; // clave incorrecta o red no encontrada
constexpr uint32_t kTaskStack = 4096;

// Motivos de desconexión del ESP-IDF 5 (esp_wifi_types.h, wifi_err_reason_t).
bool isPasswordReason(uint8_t r)
{
    return r == 2 /* AUTH_EXPIRE */ || r == 15 /* 4WAY_HANDSHAKE_TIMEOUT */ ||
           r == 202 /* AUTH_FAIL */ || r == 204 /* HANDSHAKE_TIMEOUT */;
}

bool isNoNetworkReason(uint8_t r)
{
    return r == 201 /* NO_AP_FOUND */ || r == 210 /* NO_AP_FOUND_W_COMPATIBLE_SECURITY */ ||
           r == 211 /* NO_AP_FOUND_IN_AUTHMODE_THRESHOLD */ || r == 212 /* NO_AP_FOUND_IN_RSSI_THRESHOLD */;
}

void taskMain(void *)
{
    for (;;)
    {
        JWPLC_WiFi.serviceInternal();
        vTaskDelay(pdMS_TO_TICKS(kTickMs));
    }
}

wifi_mode_t radioMode(bool station, bool accessPoint)
{
    if (station && accessPoint)
        return WIFI_AP_STA;
    if (accessPoint)
        return WIFI_AP;
    return station ? WIFI_STA : WIFI_OFF;
}

// Escapa \ ; , : " como el preset WIFI del HMI Designer (designer-qr-core.js, escapeWifiField).
void appendQrField(String &out, const char *val)
{
    for (const char *p = val; *p; ++p)
    {
        if (*p == '\\' || *p == ';' || *p == ',' || *p == ':' || *p == '"')
            out += '\\';
        out += *p;
    }
}

bool copyText(char *dst, size_t size, const char *src)
{
    if (src == nullptr)
        src = "";
    const size_t n = strlen(src);
    if (n >= size)
        return false;
    memcpy(dst, src, n + 1);
    return true;
}
} // namespace

bool JWPLC_WiFiClass::connect(const char *network, const char *password)
{
    if (network == nullptr || network[0] == '\0')
        return false;
    char net[sizeof(network_)];
    char pass[sizeof(password_)];
    if (!copyText(net, sizeof(net), network) || !copyText(pass, sizeof(pass), password))
        return false;

    memcpy(network_, net, sizeof(net));
    memcpy(password_, pass, sizeof(pass));
    hadIp_ = false;
    reconnects_ = 0;
    state_ = WIFI_STATE_CONNECTING;
    wanted_ = true;
    restart_ = true;
    startTaskIfNeeded();
    return true;
}

bool JWPLC_WiFiClass::connect(const String &network, const String &password)
{
    return connect(network.c_str(), password.c_str());
}

void JWPLC_WiFiClass::disconnect()
{
    wanted_ = false;
    restart_ = false;
    state_ = WIFI_STATE_OFF;
    // Si el punto de acceso sigue abierto, la radio queda encendida solo para él.
    WiFi.disconnect(!apOn_, false);
    if (apOn_)
        WiFi.mode(WIFI_AP);
}

void JWPLC_WiFiClass::useDHCP()
{
    staticIp_ = false;
}

void JWPLC_WiFiClass::setStaticIP(IPAddress ip, IPAddress gateway, IPAddress subnet, IPAddress dns)
{
    ip_ = ip;
    gateway_ = gateway;
    subnet_ = subnet;
    dns_ = dns;
    staticIp_ = true;
}

void JWPLC_WiFiClass::setHostname(const char *name)
{
    copyText(hostname_, sizeof(hostname_), name);
}

void JWPLC_WiFiClass::startTaskIfNeeded()
{
    if (!eventsHooked_)
    {
        eventsHooked_ = true;
        WiFi.onEvent([](arduino_event_id_t event, arduino_event_info_t info) {
            uint8_t reason = 0;
            if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED)
                reason = info.wifi_sta_disconnected.reason;
            JWPLC_WiFi.onEventInternal(static_cast<int32_t>(event), reason);
        });
    }
    if (task_ == nullptr)
    {
        TaskHandle_t handle = nullptr;
        xTaskCreatePinnedToCore(taskMain, "jwplc_wifi", kTaskStack, nullptr, 1, &handle, 0);
        task_ = handle;
    }
}

void JWPLC_WiFiClass::beginAttempt()
{
    lastAttemptMs_ = millis();
    if (hostname_[0] == '\0')
    {
        uint8_t mac[6] = {0};
        esp_read_mac(mac, ESP_MAC_WIFI_STA);
        snprintf(hostname_, sizeof(hostname_), "jwplc-%02x%02x", mac[4], mac[5]);
    }
    WiFi.mode(radioMode(true, apOn_));
    WiFi.setHostname(hostname_);
    WiFi.setAutoReconnect(true);
    if (staticIp_)
        WiFi.config(ip_, gateway_, subnet_, dns_);
    else
        WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
    WiFi.disconnect(false, false);
    WiFi.begin(network_, password_[0] ? password_ : nullptr);
}

void JWPLC_WiFiClass::serviceInternal()
{
    if (!wanted_)
        return;
    if (restart_)
    {
        restart_ = false;
        beginAttempt();
        return;
    }
    const uint8_t s = state_;
    if (s == WIFI_STATE_READY)
        return;
    const uint32_t wait =
        (s == WIFI_STATE_WRONG_PASSWORD || s == WIFI_STATE_NO_NETWORK) ? kRetryAfterErrorMs : kRetryMs;
    if (millis() - lastAttemptMs_ >= wait)
        beginAttempt();
}

void JWPLC_WiFiClass::onEventInternal(int32_t event, uint8_t reason)
{
    if (!wanted_)
        return;
    switch (event)
    {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
        if (hadIp_)
            reconnects_ = reconnects_ + 1;
        hadIp_ = true;
        state_ = WIFI_STATE_READY;
        break;
    case ARDUINO_EVENT_WIFI_STA_LOST_IP:
        state_ = WIFI_STATE_LOST;
        break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
        if (isPasswordReason(reason) && !hadIp_)
            state_ = WIFI_STATE_WRONG_PASSWORD;
        else if (isNoNetworkReason(reason))
            state_ = hadIp_ ? WIFI_STATE_LOST : WIFI_STATE_NO_NETWORK;
        else if (state_ == WIFI_STATE_READY)
            state_ = WIFI_STATE_LOST;
        break;
    default:
        break;
    }
}

bool JWPLC_WiFiClass::isReady()
{
    return state_ == WIFI_STATE_READY;
}

uint8_t JWPLC_WiFiClass::state()
{
    return state_;
}

const char *JWPLC_WiFiClass::statusString()
{
    switch (state_)
    {
    case WIFI_STATE_CONNECTING:
        return "CONECTANDO";
    case WIFI_STATE_READY:
        return "LISTO";
    case WIFI_STATE_WRONG_PASSWORD:
        return "CLAVE INCORRECTA";
    case WIFI_STATE_NO_NETWORK:
        return "RED NO ENCONTRADA";
    case WIFI_STATE_LOST:
        return "RECONECTANDO";
    default:
        return "APAGADO";
    }
}

const char *JWPLC_WiFiClass::network()
{
    return network_;
}

IPAddress JWPLC_WiFiClass::localIP()
{
    return isReady() ? WiFi.localIP() : IPAddress();
}

IPAddress JWPLC_WiFiClass::gatewayIP()
{
    return isReady() ? WiFi.gatewayIP() : IPAddress();
}

IPAddress JWPLC_WiFiClass::subnetMask()
{
    return isReady() ? WiFi.subnetMask() : IPAddress();
}

IPAddress JWPLC_WiFiClass::dnsServerIP()
{
    return isReady() ? WiFi.dnsIP() : IPAddress();
}

String JWPLC_WiFiClass::macAddress()
{
    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char txt[18];
    snprintf(txt, sizeof(txt), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(txt);
}

int32_t JWPLC_WiFiClass::rssi()
{
    return isReady() ? WiFi.RSSI() : 0;
}

uint8_t JWPLC_WiFiClass::signalPercent()
{
    const int32_t r = rssi();
    if (r == 0 || r <= -90)
        return 0;
    if (r >= -30)
        return 100;
    return static_cast<uint8_t>((r + 90) * 100 / 60);
}

uint32_t JWPLC_WiFiClass::reconnects()
{
    return reconnects_;
}

void JWPLC_WiFiClass::printStatus(Stream &out)
{
    out.print("WiFi: ");
    out.print(statusString());
    if (network_[0])
    {
        out.print(" | red: ");
        out.print(network_);
    }
    if (isReady())
    {
        out.print(" | IP: ");
        out.print(localIP());
        out.print(" | señal: ");
        out.print(signalPercent());
        out.print("% (");
        out.print(rssi());
        out.print(" dBm)");
    }
    out.print(" | reconexiones: ");
    out.print(reconnects());
    if (apOn_)
    {
        out.print(" | punto de acceso: ");
        out.print(apNetwork_);
        out.print(" ");
        out.print(accessPointIP());
        out.print(" (");
        out.print(accessPointClients());
        out.print(" conectados)");
    }
    out.println();
}

bool JWPLC_WiFiClass::startAccessPoint(const char *network, const char *password)
{
    if (network == nullptr || network[0] == '\0')
        return false;
    if (password == nullptr)
        password = "";
    const size_t passLen = strlen(password);
    if (passLen != 0 && (passLen < 8 || passLen > 63))
        return false;
    if (!copyText(apNetwork_, sizeof(apNetwork_), network))
        return false;
    memcpy(apPassword_, password, passLen + 1);

    WiFi.mode(radioMode(wanted_, true));
    const bool ok = WiFi.softAP(apNetwork_, passLen ? apPassword_ : nullptr);
    apOn_ = ok;
    return ok;
}

bool JWPLC_WiFiClass::startAccessPoint(const String &network, const String &password)
{
    return startAccessPoint(network.c_str(), password.c_str());
}

void JWPLC_WiFiClass::stopAccessPoint()
{
    if (!apOn_)
        return;
    apOn_ = false;
    WiFi.softAPdisconnect(false);
    WiFi.mode(radioMode(wanted_, false));
}

bool JWPLC_WiFiClass::accessPointReady()
{
    return apOn_;
}

const char *JWPLC_WiFiClass::accessPointNetwork()
{
    return apOn_ ? apNetwork_ : "";
}

IPAddress JWPLC_WiFiClass::accessPointIP()
{
    return apOn_ ? WiFi.softAPIP() : IPAddress();
}

uint8_t JWPLC_WiFiClass::accessPointClients()
{
    return apOn_ ? WiFi.softAPgetStationNum() : 0;
}

String JWPLC_WiFiClass::qrText()
{
    String out;
    if (!apOn_)
        return out;
    out.reserve(24 + strlen(apNetwork_) * 2 + strlen(apPassword_) * 2);
    out += apPassword_[0] ? "WIFI:T:WPA;S:" : "WIFI:T:nopass;S:";
    appendQrField(out, apNetwork_);
    if (apPassword_[0])
    {
        out += ";P:";
        appendQrField(out, apPassword_);
    }
    out += ";;";
    return out;
}

// Enganche con JWPLC_Red. JWPLC_Red los declara como símbolos débiles: existen solo si el sketch
// incluye JWPLC_WiFi.h, así JWPLC_Red no arrastra la pila WiFi a los sketches que solo usan cable.
bool jwplc_red_wifi_ready()
{
    return JWPLC_WiFi.isReady();
}

IPAddress jwplc_red_wifi_local_ip()
{
    return JWPLC_WiFi.localIP();
}

Client *jwplc_red_wifi_new_client()
{
    return new WiFiClient();
}
