// JWPLC_Cloud: variables y alarmas del JWPLC en JW Cloud, por cable o WiFi.
//
//   #include <JWPLC_Cloud.h>
//
//   float presion = 0;
//
//   void setup()
//   {
//       JWPLC_Cloud.addNumber("p", "Presión", "bar", 2, 0, 6); // id, nombre, unidad, decimales, rango
//       JWPLC_Cloud.addBool("b1", "Bomba 1");
//       JWPLC_Cloud.addAlarm("E01", "Cisterna vacía");
//       JWPLC_Cloud.begin("token-del-equipo");                // lo da JW Control al dar de alta el equipo
//   }
//
//   void loop()
//   {
//       JWPLC_Cloud.set("p", presion);
//       JWPLC_Cloud.set("b1", digitalRead(Q0_0));
//       JWPLC_Cloud.alarm("E01", !digitalRead(I0_0));
//   }
//
// - set() y alarm() solo guardan el valor: nunca esperan a la red. Una tarea aparte toma una muestra
//   cada setInterval() (10 s por defecto) y la manda por HTTPS a JW Cloud (ota.jw-control.com).
// - Sale por la red activa de JWPLC_Red: cable por defecto; con JWPLC_WiFi y JWPLC_Red.useAuto(),
//   cable o WiFi sin cambiar el sketch.
// - Sin red guarda hasta 120 muestras (20 min a 10 s) y las manda al volver con su hora real.
// - Una alarma que se activa llega como aviso al celular del cliente en la app de JW Cloud.
// - status() y statusString() dicen cómo va (EN LINEA, SIN RED, TOKEN NO VALIDO...).
// - Pruebas sin Internet: setServer("192.168.1.10", 8788, false) apunta a un JW Cloud local por HTTP
//   (en JWPLC Sim la PC es 192.168.250.1). No usar HTTP sin TLS fuera de la red local.
#pragma once

#include <Arduino.h>
#include <JWPLC_Red.h>

// Estado (status()).
const uint8_t CLOUD_OFF = 0;           // sin begin()
const uint8_t CLOUD_STARTING = 1;      // todavía no intentó enviar
const uint8_t CLOUD_ONLINE = 2;        // el último envío llegó
const uint8_t CLOUD_NO_NETWORK = 3;    // ninguna red lista o el servidor no responde
const uint8_t CLOUD_BAD_TOKEN = 4;     // JW Cloud no reconoce el token
const uint8_t CLOUD_SERVER_ERROR = 5;  // JW Cloud rechazó los datos o respondió con error
const uint8_t CLOUD_TLS_ERROR = 6;     // certificado no válido o saludo TLS fallido

// Límites (los de JW Cloud son 32 variables y 20 alarmas por envío).
const uint8_t CLOUD_MAX_VARIABLES = 16;
const uint8_t CLOUD_MAX_ALARMS = 16;

struct JWPLC_CloudState;

class JWPLC_CloudClass
{
public:
    // Arranca el envío con el token del equipo (texto). Se puede llamar después de declarar todo.
    void begin(const char token[]);
    void begin(const String &token);
    void end();

    // Servidor: por defecto ota.jw-control.com, puerto 443, con TLS.
    void setServer(const char host[], uint16_t port = 443, bool tls = true);
    // Cada cuánto se toma y se manda una muestra (2 s a 10 min; 10 s por defecto).
    void setInterval(uint32_t ms);
    // Nombre del programa que se ve en JW Cloud (por ejemplo "Bombeo v1.2").
    void setProgram(const char name[]);

    // Variables. id: letras, números o "_" (máx. 15). Devuelven false si no cabe o el id no sirve.
    // Textos en UTF-8: nombre hasta 40 bytes, unidad hasta 12, texto de alarma hasta 60.
    bool addNumber(const char id[], const char name[], const char unit[] = "", uint8_t decimals = 1);
    bool addNumber(const char id[], const char name[], const char unit[], uint8_t decimals, float min, float max);
    bool addBool(const char id[], const char name[]);
    bool addAlarm(const char id[], const char text[]);

    // Valor actual (no espera a la red).
    void set(const char id[], float value);
    void set(const char id[], double value);
    void set(const char id[], int value);
    void set(const char id[], unsigned int value);
    void set(const char id[], long value);
    void set(const char id[], unsigned long value);
    void set(const char id[], bool value);
    // Alarma activa o no. Solo cuenta el cambio: se puede llamar en cada vuelta del loop.
    void alarm(const char id[], bool active);

    uint8_t status();
    const char *statusString();
    bool isOnline();
    // Muestras guardadas que esperan red, y envíos que llegaron desde begin().
    uint16_t pending();
    uint32_t sentCount();

private:
    JWPLC_CloudState *st();
    void setValue(const char id[], float value);
    JWPLC_CloudState *state_ = nullptr;
};

extern JWPLC_CloudClass JWPLC_Cloud;
