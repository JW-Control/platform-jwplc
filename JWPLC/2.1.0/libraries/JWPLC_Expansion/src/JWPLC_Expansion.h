// JWPLC_Expansion: módulos de expansión del JWPLC Basic por Modbus RTU.
//
// Esclavo (este equipo como módulo de otro JWPLC con Runtime):
//
//   #include <JWPLC_Expansion.h>
//   void setup() { JWPLC_ExpansionSlave.begin(); }   // lo configura el maestro
//   void loop() {}
//
// o con la configuración fija desde el sketch (el maestro solo la lee):
//
//   JWPLC_ExpansionSlave.setSafeState(1, SAFE_ON);    // Q1 encendida si se corta el bus
//   JWPLC_ExpansionSlave.begin(3, 115200, SERIAL_8N1);
//
// Volver a fábrica: mantener ESC + OK 3 s al encender.
#pragma once

#include <Arduino.h>

#include "JWPLC_ExpansionProtocol.h"

#define JWPLC_EXPANSION_VERSION "0.1.0"

class JWPLC_ExpansionSlaveClass
{
public:
    // Configuración de su FRAM (o de fábrica: ID 247, 115200 8N1).
    bool begin();
    // Configuración fija desde el sketch: el maestro la lee pero no la cambia.
    bool begin(uint8_t id, uint32_t baud, uint32_t config);

    // Antes de begin(id, ...): estado seguro de la salida q (1..8) y tiempo
    // sin el maestro (1000..5000 ms) tras el que se aplica.
    void setSafeState(uint8_t q, uint8_t mode);
    void setWatchdog(uint16_t ms);

    uint8_t id() const;
    bool locked() const;
    bool linked() const;
    bool safeActive() const;   // estado seguro aplicado (sin maestro)
    uint8_t outputs() const;   // salidas actuales (bit 0 = Q1)

private:
    bool start(bool fromSketch);
};

extern JWPLC_ExpansionSlaveClass JWPLC_ExpansionSlave;
