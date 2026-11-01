// JWPLC_Runtime: programación en el panel del JWPLC Basic, estilo LOGO! 8.
//
// El Runtime es un modo de trabajo: con él activo, el equipo se programa
// desde la TFT y la botonera (editor FBD, parámetros, configuración) y el
// sketch no maneja las salidas ni la pantalla USER.
//
//   #include <JWPLC_Runtime.h>
//
//   void setup()
//   {
//       JWPLC_Runtime.begin();
//   }
//
//   void loop()
//   {
//   }
//
// API solo con «.» (regla 9 del CLAUDE.md de platform-jwplc).
#pragma once

#include <Arduino.h>

#define JWPLC_RUNTIME_VERSION_MAJOR 0
#define JWPLC_RUNTIME_VERSION_MINOR 1
#define JWPLC_RUNTIME_VERSION_PATCH 2
// Se muestra en la pantalla RUN y en Configuración > Versión.
#define JWPLC_RUNTIME_VERSION "0.1.2"

class JWPLC_RuntimeClass
{
public:
    // Activa el modo Runtime: lee el programa de la FRAM, aplica la política
    // de arranque y toma las salidas Q, la botonera y la pantalla USER.
    bool begin();

    // Si la FRAM no tiene programa, carga el de ejemplo (útil para probar).
    void loadDemoIfEmpty(bool enable) { _demoIfEmpty = enable; }

    void run();
    void stop();
    bool isRunning() const;

    // Valores del programa en marcha, para Serial o Modbus.
    bool output(uint8_t number) const; // Q1..Q8
    bool marker(uint8_t number) const; // M1..M8
    uint8_t blockCount() const;

private:
    bool _demoIfEmpty = false;
};

extern JWPLC_RuntimeClass JWPLC_Runtime;
