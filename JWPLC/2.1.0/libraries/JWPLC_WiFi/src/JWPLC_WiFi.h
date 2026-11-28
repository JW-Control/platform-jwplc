// JWPLC_WiFi: WiFi del JWPLC Basic con la misma forma de uso que JWPLC_Ethernet.
//
//   #include <JWPLC_WiFi.h>
//   void setup() { JWPLC_WiFi.connect("MiRed", "MiClave"); }
//   void loop()  { if (JWPLC_WiFi.isReady()) { ... } }
//
// - connect() no bloquea: la conexión, la IP y las reconexiones corren en segundo
//   plano (tarea propia en el núcleo 0). El loop() del sketch nunca espera a la red.
// - Si la red se cae o el router se reinicia, vuelve a conectar solo.
// - statusString() dice por qué no conecta: clave incorrecta, red no encontrada, etc.
// - Junto con JWPLC_Red, el mismo sketch puede salir por cable o por WiFi.
// - startAccessPoint() abre la red propia del PLC y qrText() da su QR para el HMI Designer.
//
// JWPLC Sim todavía no simula la radio: un sketch con esta librería compila solo para la placa.
#pragma once

#include <Arduino.h>
#include <IPAddress.h>

// Estado de la conexión (state()).
const uint8_t WIFI_STATE_OFF = 0;            // sin connect() o tras disconnect()
const uint8_t WIFI_STATE_CONNECTING = 1;     // buscando la red o esperando IP
const uint8_t WIFI_STATE_READY = 2;          // conectado y con IP
const uint8_t WIFI_STATE_WRONG_PASSWORD = 3; // la red rechazó la clave
const uint8_t WIFI_STATE_NO_NETWORK = 4;     // no se encuentra la red (nombre o alcance)
const uint8_t WIFI_STATE_LOST = 5;           // estaba conectado y se perdió; reintentando

class JWPLC_WiFiClass
{
public:
    // Conecta a la red (DHCP salvo que se llame antes a setStaticIP()). Devuelve false si el
    // nombre está vacío o es demasiado largo (máx. 32) o la clave tiene más de 63 caracteres.
    bool connect(const char *network, const char *password = "");
    bool connect(const String &network, const String &password);
    void disconnect();

    // Dirección: DHCP (por defecto) o fija. Se aplica en la próxima conexión.
    void useDHCP();
    void setStaticIP(IPAddress ip, IPAddress gateway, IPAddress subnet, IPAddress dns);
    // Nombre del equipo en la red (por defecto "jwplc-" + 4 hex de la MAC).
    void setHostname(const char *name);

    bool isReady();
    uint8_t state();
    const char *statusString();
    const char *network();

    IPAddress localIP();
    IPAddress gatewayIP();
    IPAddress subnetMask();
    IPAddress dnsServerIP();
    String macAddress();

    // Señal: RSSI en dBm (0 sin conexión) y porcentaje aproximado 0-100.
    int32_t rssi();
    uint8_t signalPercent();

    // Cuántas veces se volvió a conectar desde el primer connect().
    uint32_t reconnects();

    void printStatus(Stream &out);

    // Punto de acceso propio (red del PLC, p. ej. portal de descarga o configuración desde el
    // celular). Puede estar a la vez que connect(). IP del PLC en su red: 192.168.4.1.
    // Devuelve false si el nombre está vacío o pasa de 32 caracteres, o si la clave no está vacía
    // y no tiene entre 8 y 63 caracteres (WPA2). Clave vacía = red abierta.
    bool startAccessPoint(const char *network, const char *password = "");
    bool startAccessPoint(const String &network, const String &password);
    void stopAccessPoint();
    bool accessPointReady();
    const char *accessPointNetwork();
    IPAddress accessPointIP();
    uint8_t accessPointClients();

    // Texto para el QR «unirse a la red» del punto de acceso, en el mismo formato que el preset
    // WIFI del HMI Designer: WIFI:T:WPA;S:<red>;P:<clave>;; (o T:nopass). Vacío sin punto de acceso.
    // Para un QR VARIABLE del Designer: JWPLC_WiFi.qrText().toCharArray(qrTexto, sizeof(qrTexto));
    String qrText();

    // Interno (tarea y eventos).
    void serviceInternal();
    void onEventInternal(int32_t event, uint8_t reason);

private:
    void startTaskIfNeeded();
    void beginAttempt();

    char network_[33] = "";
    char password_[64] = "";
    char hostname_[33] = "";
    bool wanted_ = false;
    bool staticIp_ = false;
    IPAddress ip_, gateway_, subnet_, dns_;
    volatile uint8_t state_ = WIFI_STATE_OFF;
    volatile bool hadIp_ = false;
    volatile uint32_t lastAttemptMs_ = 0;
    volatile uint32_t reconnects_ = 0;
    volatile bool restart_ = false;
    bool eventsHooked_ = false;
    char apNetwork_[33] = "";
    char apPassword_[64] = "";
    volatile bool apOn_ = false;
    void *task_ = nullptr;
};

extern JWPLC_WiFiClass JWPLC_WiFi;
