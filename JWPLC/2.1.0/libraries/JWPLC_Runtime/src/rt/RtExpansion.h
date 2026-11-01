// Maestro de los módulos de expansión (protocolo Remote I/O v2 por RS-485).
//
// La tarea del Runtime llama service() en cada vuelta: un intercambio Modbus
// por vez, sin bloquear (por módulo: FC15 salidas y FC02 entradas). Tres
// fallos seguidos = fuera de línea (entradas a 0); se vuelve a probar cada
// 1 s y, al responder, se verifica que sea el mismo equipo (UID).
//
// La puesta en marcha (agregar, buscar, vincular, estado seguro) usa
// llamadas bloqueantes y solo se hace con el programa detenido.
//
// Diseño: docs/v2.1.0-alpha.16/A16_RT2_EXPANSION_DISENO_20261101.md
#pragma once

#include "RtStore.h"

namespace jwrt
{

enum XState : uint8_t
{
    XS_NONE = 0,  // posición libre
    XS_CHECK,     // por verificar (arranque o vuelta a línea)
    XS_ONLINE,
    XS_OFFLINE,
    XS_WRONG,     // en su ID responde otro equipo
};

// Ficha de un esclavo (FC04 240..253 y FC03 224..236).
struct XInfo
{
    bool ok;
    uint8_t id;
    uint16_t product;
    uint16_t firmware;
    uint16_t io;
    uint16_t caps;
    uint8_t status;      // JWX_STATUS_*
    uint8_t mac[6];
    uint32_t uid;
    uint8_t baud;        // código
    uint8_t frame;
    uint8_t safeValue;
    uint8_t safeHold;
    uint8_t watchdog;
    uint8_t flags;       // JWX_FLAG_*
    uint32_t link;
};

// Resultado de la búsqueda.
struct XFound
{
    uint8_t id;
    uint8_t status;      // JWX_STATUS_*
    uint8_t linked;      // 0 = ninguno, 1 = este maestro, 2 = otro maestro
    int8_t module;       // módulo de este maestro con ese ID (-1 = no)
};

static const uint8_t X_FOUND_MAX = 12;

class Expansion
{
public:
    void begin(Store &store);
    void service(uint32_t nowMs, bool run);

    // Salidas que se escriben a cada módulo (las calcula el motor en RUN).
    void setOutputs(uint8_t module, uint8_t bits) { if (module < NUM_X) _out[module] = bits; }
    uint8_t inputs(uint8_t module) const { return module < NUM_X && _state[module] == XS_ONLINE ? _in[module] : 0; }

    const XLink &link(uint8_t module) const { return _links[module]; }
    bool used(uint8_t module) const { return module < NUM_X && (_links[module].flags & XL_USED); }
    uint8_t state(uint8_t module) const { return module < NUM_X ? _state[module] : XS_NONE; }
    uint16_t fails(uint8_t module) const { return module < NUM_X ? _failTotal[module] : 0; }
    uint8_t count() const;
    uint32_t cycleMs() const { return _cycleMs; }
    uint32_t masterUid() const { return _masterUid; }
    // "X1" (sin respuesta) o "XU1" (otro equipo) del primer módulo con problema; "" si todo bien.
    void errCode(char out[], size_t size) const;
    // Cambió el estado de algún módulo desde la última consulta (para eventos).
    bool takeChange(uint8_t &module, uint8_t &state);

    // Bus (todos los módulos usan la trama del maestro).
    uint8_t busBaud() const { return _baud; }
    uint8_t busFrame() const { return _frame; }
    bool setBus(uint8_t baud, uint8_t frame);

    // ---------------------------------------------------- puesta en marcha (STOP)
    // Lee la ficha del esclavo id. factoryFrame = con la trama de fábrica.
    bool probe(uint8_t id, bool factoryFrame, XInfo &info);
    // Configura el módulo de fábrica (ID 247) y lo vincula en la primera
    // posición libre. Devuelve el módulo (0..6) o -1; why explica el fallo.
    int8_t addFactory(uint8_t newId, uint8_t safeValue, uint8_t safeHold, uint8_t watchdog,
                      char why[], size_t size);
    // Vincula un esclavo ya configurado (encontrado en la búsqueda).
    int8_t linkExisting(uint8_t id, char why[], size_t size);
    // Cambia el estado seguro de un módulo vinculado (no bloqueado).
    bool setSafe(uint8_t module, uint8_t safeValue, uint8_t safeHold, uint8_t watchdog, char why[], size_t size);
    bool remove(uint8_t module);
    // Barre los ID 1..246 con la trama del bus.
    uint8_t scan(XFound found[], uint8_t max);
    // Primer ID libre (ni vinculado ni de fábrica).
    uint8_t freeId() const;

    XInfo lastProbe;

private:
    void startBus(uint8_t baud, uint8_t frame);
    void waitIdle();
    void verify(uint8_t module, const XInfo &info);
    void setState(uint8_t module, uint8_t state);
    bool writeConfig(uint8_t id, const XInfo &cfg, char why[], size_t size);

    Store *_store = nullptr;
    XLink _links[NUM_X];
    uint8_t _state[NUM_X] = {0};
    uint8_t _in[NUM_X] = {0};
    uint8_t _out[NUM_X] = {0};
    uint8_t _failRun[NUM_X] = {0};
    uint16_t _failTotal[NUM_X] = {0};
    uint32_t _retryAt[NUM_X] = {0};
    uint8_t _changed = 0;       // bit n = cambió el estado del módulo n

    uint8_t _baud = 4;
    uint8_t _frame = 0;
    uint32_t _masterUid = 0;
    bool _busOn = false;

    // Ciclo en curso.
    uint8_t _cur = 0;
    uint8_t _step = 0;          // 0 = identificar, 1 = salidas, 2 = entradas
    bool _pending = false;
    uint16_t _rx[16];
    uint8_t _rxBits = 0;
    uint32_t _cycleStart = 0;
    uint32_t _cycleMs = 0;
};

} // namespace jwrt
