#ifndef JWPLC_RUNTIME_VIEW_H
#define JWPLC_RUNTIME_VIEW_H

#include <Arduino.h>

// Resultado de JWPLC_IO.fault(): por qué las E/S no están listas.
#ifndef JWPLC_IO_FAULT_NONE
#define JWPLC_IO_FAULT_NONE       0u  // E/S listas (o todavía arrancando)
#define JWPLC_IO_FAULT_I2C        1u  // el bus I2C no arrancó
#define JWPLC_IO_FAULT_TCA_INIT   2u  // el TCA6424A no responde
#define JWPLC_IO_FAULT_TCA_CONFIG 3u  // el TCA6424A no aceptó la configuración
#endif

// Fachadas livianas sobre snapshots ya mantenidos por el runtime.
// No realizan transacciones I2C/SPI adicionales.
class JWPLC_IOView
{
public:
    uint8_t inputs() const;
    uint8_t outputs() const;

    bool input(uint8_t index) const;
    bool output(uint8_t index) const;

    bool ready() const;

    // JWPLC_IO_FAULT_NONE si las E/S arrancaron. Si el TCA6424A falló, el core
    // deja EN_IO en 0 (relés deshabilitados) y reintenta cada segundo.
    uint8_t fault() const;
    uint32_t lastScanMs() const;

    // Escribe Q0_0..Q0_7 de una vez (bit 0 = Q0_0). Equivale a
    // JWPLC_writeOutputs(bits), con la forma objeto.funcion() del package.
    void setOutputs(uint8_t bits) const;
};

class JWPLC_TimeView
{
public:
    bool present() const;
    bool valid() const;
    bool lostPower() const;

    uint8_t second() const;
    uint8_t minute() const;
    uint8_t hour() const;

    uint8_t day() const;
    uint8_t month() const;
    uint16_t year() const;
    uint8_t dayOfWeek() const;

    uint32_t lastUpdateMs() const;
};

extern JWPLC_IOView JWPLC_IO;
extern JWPLC_TimeView JWPLC_Time;

#endif // JWPLC_RUNTIME_VIEW_H
