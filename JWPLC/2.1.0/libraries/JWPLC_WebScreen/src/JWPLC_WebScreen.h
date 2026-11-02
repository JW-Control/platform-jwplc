// JWPLC_WebScreen: la pantalla del JWPLC Basic en el navegador.
//
//   #include <JWPLC_WebScreen.h>
//   void setup() { JWPLC_WebScreen.begin(); }
//
// Con el equipo en red (DHCP o IP fija, como siempre), abrir en la laptop
// http://<IP del equipo>/ : la TFT se ve en vivo y la botonera de la página
// (o las flechas, Enter y Esc del teclado) manejan el equipo como las teclas
// físicas. La IP sale en la pantalla de reposo o en Red del Runtime.
//
// Memoria: 54 KB de RAM para la copia de la pantalla, reservados al arrancar
// la TFT. Red: una sola conexión abierta; la página recibe solo los tramos
// de fila que cambiaron, en cuanto cambian; con la pantalla quieta casi no
// hay tráfico.
#pragma once

#include <Arduino.h>

class JWPLC_WebScreenClass
{
public:
    // Arranca el servidor web (puerto 80 por defecto). Se puede llamar antes
    // de que la red esté lista: escucha en cuanto Ethernet tenga IP.
    bool begin(uint16_t port = 80);
    bool isRunning() const;
    uint16_t port() const;

    // true si hay un navegador mirando (pidió la pantalla en los últimos 3 s).
    bool viewerConnected() const;

    // Dirección para escribir en el navegador, p. ej. "http://192.168.1.50/".
    // Vacía mientras Ethernet no tenga IP.
    String url() const;

    // Permitir o no que la página pulse teclas (true por defecto).
    void allowKeys(bool allow);
    bool keysAllowed() const;
};

extern JWPLC_WebScreenClass JWPLC_WebScreen;
