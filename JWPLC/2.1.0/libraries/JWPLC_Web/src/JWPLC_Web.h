// JWPLC_Web: panel web del JWPLC Basic, con usuarios y contraseña.
//
//   #include <JWPLC_Web.h>
//
//   float presion = 0;
//   float consigna = 3.5;
//
//   void setup()
//   {
//       JWPLC_Web.begin("Bombeo Edificio A");
//       JWPLC_Web.publish("presion", presion).label("Presión de red").unit("bar").range(0, 10).trend();
//       JWPLC_Web.publish("consigna", consigna).label("Consigna").unit("bar").range(1, 6).writable(WEB_OPERADOR);
//       JWPLC_Web.addAlarm("baja", "Presión baja", WEB_ALARMA_ALTA);
//   }
//
//   void loop()
//   {
//       JWPLC_Web.setAlarm("baja", presion < 1.0);
//   }
//
// - El panel se arma solo con las variables publicadas: no hay que escribir HTML.
// - Sin clave de fábrica: el primer ingreso pide el código de 6 dígitos que da el
//   equipo (MENU > Red > Conectar celular en el Runtime, o pairingCode() en el
//   sketch) y crea el usuario administrador.
// - La contraseña no viaja por la red: la página demuestra que la conoce firmando un
//   reto del equipo (HMAC-SHA256 sobre una clave derivada con PBKDF2).
// - Roles: WEB_VISOR (mira), WEB_OPERADOR (cambia consignas, reconoce alarmas, usa
//   las teclas de la Pantalla), WEB_TECNICO y WEB_ADMIN (usuarios).
// - Incluye la página «Pantalla»: la TFT en vivo con la botonera (antes JWPLC_WebScreen).
// - Atiende por cable, por WiFi o por los dos según JWPLC_Red (useEthernet, useWiFi,
//   useAuto). Para WiFi, incluir también <JWPLC_WiFi.h> en el sketch.
#pragma once

#include <Arduino.h>

// Roles (de menor a mayor).
const uint8_t WEB_VISOR = 1;
const uint8_t WEB_OPERADOR = 2;
const uint8_t WEB_TECNICO = 3;
const uint8_t WEB_ADMIN = 4;
const uint8_t WEB_NADIE = 0xFF; // writable(WEB_NADIE): solo lectura (por defecto)

// Prioridad de una alarma.
const uint8_t WEB_ALARMA_BAJA = 1;
const uint8_t WEB_ALARMA_MEDIA = 2;
const uint8_t WEB_ALARMA_ALTA = 3;

// Límites (memoria fija, sin reservar en el loop).
const uint8_t WEB_MAX_VARIABLES = 32;
const uint8_t WEB_MAX_ALARMAS = 16;
const uint8_t WEB_MAX_USUARIOS = 6;
const uint8_t WEB_MAX_TENDENCIAS = 4;

// Una variable publicada. Se configura encadenando: publish(...).label(...).unit(...).
// Si publish() falla (id repetido o más de WEB_MAX_VARIABLES), las llamadas no hacen nada.
class JWPLC_WebVar
{
public:
    JWPLC_WebVar label(const char *text);
    JWPLC_WebVar unit(const char *text);
    JWPLC_WebVar group(const char *text);
    JWPLC_WebVar range(float minimum, float maximum);
    JWPLC_WebVar decimals(uint8_t digits);
    // Rol mínimo para cambiarla desde la web (por defecto nadie: solo lectura).
    JWPLC_WebVar writable(uint8_t role = WEB_OPERADOR);
    // La guarda en la tendencia (hasta WEB_MAX_TENDENCIAS variables).
    JWPLC_WebVar trend();

    bool valid() const;

private:
    friend class JWPLC_WebClass;
    explicit JWPLC_WebVar(int8_t index) : index_(index) {}
    int8_t index_;
};

class JWPLC_WebClass
{
public:
    // Arranca el panel (puerto 80). Escucha en cuanto Ethernet tiene IP.
    bool begin(const char *deviceName = "JWPLC Basic", uint16_t port = 80);
    bool begin(const String &deviceName, uint16_t port = 80);
    bool isRunning() const;
    uint16_t port() const;
    // Dirección para el navegador, p. ej. "http://192.168.1.50/" (vacía sin IP).
    String url() const;

    // Variables del programa. La librería lee y escribe la variable del sketch
    // directamente: publicar variables globales (no locales del loop).
    JWPLC_WebVar publish(const char *id, bool &variable);
    JWPLC_WebVar publish(const char *id, int &variable);
    JWPLC_WebVar publish(const char *id, long &variable);
    JWPLC_WebVar publish(const char *id, unsigned long &variable);
    JWPLC_WebVar publish(const char *id, float &variable);

    // Alarmas: se declaran en setup() y se encienden o apagan en el loop.
    bool addAlarm(const char *id, const char *text, uint8_t priority = WEB_ALARMA_MEDIA);
    void setAlarm(const char *id, bool active);
    bool alarmActive(const char *id) const;
    uint8_t activeAlarms() const;

    // Cada cuántos segundos se guarda un punto de tendencia (1 a 3600, por defecto 5).
    // Hay 120 puntos por variable: con 5 s, los últimos 10 minutos.
    void trendEvery(uint16_t seconds);

    // Primer ingreso: código de 6 dígitos para crear el administrador.
    // Vacío si el equipo ya tiene usuarios. Cambia cada 60 s.
    String pairingCode();
    // Dirección con el código incluido (para un QR): "http://IP/?c=123456".
    String pairingURL();
    bool hasUsers() const;
    // Borra todos los usuarios y sesiones (recuperación desde el equipo, p. ej.
    // manteniendo una tecla al encender). El próximo ingreso pide código otra vez.
    void resetUsers();

    // Página «Pantalla»: la TFT en vivo. Teclas permitidas (rol Operador).
    void allowKeys(bool allow);
    bool keysAllowed() const;
    // Hay alguien mirando la pantalla en vivo (pidió un cuadro en los últimos 3 s).
    bool viewerConnected() const;
    // Sesiones abiertas.
    uint8_t sessions() const;
};

extern JWPLC_WebClass JWPLC_Web;
