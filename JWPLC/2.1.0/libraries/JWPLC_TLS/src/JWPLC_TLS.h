// JWPLC_TLS: HTTPS (TLS) sobre JWPLC_Red, por cable o por WiFi con el mismo código.
//
//   #include <JWPLC_TLS.h>
//
//   JWPLC_TLSClient cliente;      // cliente TLS: sale por la red activa de JWPLC_Red
//
//   void loop()
//   {
//       if (JWPLC_Red.isReady() && cliente.connect("ota.jw-control.com", 443))
//       {
//           cliente.print("GET /health HTTP/1.1\r\nHost: ota.jw-control.com\r\nConnection: close\r\n\r\n");
//           ...
//       }
//   }
//
// - Es un Client de Arduino: print, write, available, read, stop. Sirve para HTTPS, MQTT con TLS y
//   JW Cloud, y se puede pasar a librerías que reciben un Client.
// - Verifica el certificado del servidor con las raíces de Let's Encrypt (las de JW Cloud) que trae
//   la librería. Para otra CA: setCACert(pem). Solo para pruebas: setInsecure().
// - connect() hace el saludo TLS y espera como máximo setHandshakeTimeout() (10 s por defecto); el
//   resto de las llamadas no esperan a la red. Para que loop() nunca espere, conectar desde una tarea
//   aparte (así lo hace JWPLC_Cloud).
// - Si connect() falla, lastError() y errorString() dicen por qué (sin red, certificado no válido...).
// - En JWPLC Sim el cifrado lo hace la PC (puente de red del Sim, con las CA del sistema): el sketch
//   es el mismo y sale por el W5500 simulado.
#pragma once

#include <Arduino.h>
#include <Client.h>
#include <JWPLC_Red.h>

// Motivo del último fallo (lastError()).
const uint8_t TLS_OK = 0;
const uint8_t TLS_ERROR_NO_NETWORK = 1;  // ninguna red lista o no se pudo abrir la conexión TCP
const uint8_t TLS_ERROR_HANDSHAKE = 2;   // el servidor cortó o no respondió el saludo TLS a tiempo
const uint8_t TLS_ERROR_CERTIFICATE = 3; // certificado no válido para ese nombre o CA desconocida
const uint8_t TLS_ERROR_MEMORY = 4;      // sin memoria para el saludo TLS (~40 KB de RAM)
const uint8_t TLS_ERROR_CA = 5;          // el PEM de setCACert() no se pudo leer
const uint8_t TLS_ERROR_CLOSED = 6;      // la conexión se cerró durante el envío o la lectura

struct JWPLC_TLSState;

class JWPLC_TLSClient : public Client
{
public:
    JWPLC_TLSClient();
    ~JWPLC_TLSClient();

    // Conecta por nombre (necesario para verificar el certificado). Devuelve 1 si quedó cifrado.
    int connect(const char *host, uint16_t port) override;
    // Por IP no se puede verificar el nombre del certificado: solo con setInsecure().
    int connect(IPAddress ip, uint16_t port) override;

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

    // Confianza. Por defecto: raíces de Let's Encrypt (ISRG X1, X2, YE, YR).
    void setCACert(const char pem[]); // texto PEM con una o varias CA (debe seguir existiendo)
    void useDefaultCA();
    void setInsecure();               // NO verifica el certificado: solo para pruebas

    void setHandshakeTimeout(uint32_t ms);

    uint8_t lastError();
    const char *errorString();
    // Red por la que va la conexión (RED_ETHERNET, RED_WIFI o RED_NONE).
    uint8_t network();

private:
    JWPLC_TLSClient(const JWPLC_TLSClient &) = delete;
    JWPLC_TLSClient &operator=(const JWPLC_TLSClient &) = delete;

    int openSecure(const char *host, IPAddress ip, bool byName, uint16_t port);
    void freeState();

    JWPLC_RedClient tcp_;
    JWPLC_TLSState *state_ = nullptr;
    const char *caPem_ = nullptr; // nullptr = raíces por defecto
    bool insecure_ = false;
    uint32_t handshakeTimeoutMs_ = 10000;
    uint8_t error_ = TLS_OK;
    int peeked_ = -1;
};
