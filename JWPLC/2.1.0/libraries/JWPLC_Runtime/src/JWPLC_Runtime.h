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
#include <JWPLC_Display.h>

#define JWPLC_RUNTIME_VERSION_MAJOR 0
#define JWPLC_RUNTIME_VERSION_MINOR 2
#define JWPLC_RUNTIME_VERSION_PATCH 0
// Se muestra en la pantalla RUN y en Configuración > Versión.
#define JWPLC_RUNTIME_VERSION "0.2.0"

// Programas de ejemplo para loadDemoIfEmpty().
static const uint8_t RUNTIME_DEMO_FERIA = 0;
static const uint8_t RUNTIME_DEMO_INTERMITENTE = 1;

class JWPLC_RuntimeClass
{
public:
    // Activa el modo Runtime: lee el programa de la FRAM, aplica la política
    // de arranque y toma las salidas Q, la botonera y la pantalla USER.
    bool begin();

    // Si la FRAM no tiene programa, carga un programa de ejemplo (útil para
    // probar); Programa > Cargar ejemplo carga ese mismo. Ejemplos:
    //   RUNTIME_DEMO_FERIA         «Demo feria» (marcha/parada, bomba, contador)
    //   RUNTIME_DEMO_INTERMITENTE  «Intermitente»: Q1 y Q2 alternan cada 1 s
    //                              con dos TON, sin entradas
    void loadDemoIfEmpty(bool enable, uint8_t demo = RUNTIME_DEMO_FERIA);

    void run();
    void stop();
    bool isRunning() const;

    // Valores del programa en marcha, para Serial, Modbus o la página HMI.
    bool input(uint8_t number) const;  // I1..I8
    bool output(uint8_t number) const; // Q1..Q8
    bool marker(uint8_t number) const; // M1..M8
    bool block(uint8_t number) const;  // salida del bloque B01..
    float blockSeconds(uint8_t number) const; // tiempo transcurrido de un temporizador (s)
    int32_t counter(uint8_t number) const;    // cuenta de un contador
    uint8_t blockCount() const;

    // Página HMI: la registra el header que genera el HMI Designer en un
    // proyecto «Runtime» (dentro de jwplcRuntimeHMISetup). No hace falta
    // llamarla desde el sketch.
    void setHMI(const JWPLC_UIField fields[], uint8_t count, uint8_t pages);
    bool hasHMI() const;

private:
    bool _demoIfEmpty = false;
};

extern JWPLC_RuntimeClass JWPLC_Runtime;

// Hooks del header generado por el HMI Designer (proyecto «Runtime»). Son
// weak en la librería: sin header, el Runtime no tiene página HMI.
//   jwplcRuntimeHMISetup()          al arrancar: JWPLC_Runtime.setHMI(...)
//   jwplcRuntimeHMIPageEnter(page)  gráfica estática, con la pantalla recién borrada
//   jwplcRuntimeHMIUpdate(page)     cada ciclo: setters con los valores del programa
extern "C" void jwplcRuntimeHMISetup(void);
extern "C" void jwplcRuntimeHMIPageEnter(uint8_t page);
extern "C" void jwplcRuntimeHMIUpdate(uint8_t page);
