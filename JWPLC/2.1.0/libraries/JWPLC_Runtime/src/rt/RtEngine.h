// Motor del Runtime: evalúa el programa una vez por scan.
//
// - Orden de evaluación topológico, calculado al cargar. Un lazo (salida que
//   vuelve a una entrada anterior) lee el valor del scan anterior, como en el
//   LOGO!, en vez de prohibirse.
// - Q y M usadas como fuente también dan el valor del scan anterior.
// - Sin memoria dinámica: todo el estado está en arreglos fijos.
#pragma once

#include "RtProgram.h"

namespace jwrt
{

struct ClockNow
{
    bool valid;
    uint8_t weekday; // 0 = lunes .. 6 = domingo
    uint16_t minute; // minuto del día 0..1439
};

class Engine
{
public:
    // Copia el programa, calcula el orden y pone el estado en cero (los
    // retentivos se restauran después con setRetained()).
    void load(const Program &p);
    const Program &program() const { return _p; }

    void reset();
    void scan(uint8_t inputs, uint32_t nowMs, const ClockNow &clock);

    // Módulos de expansión: entradas leídas antes del scan (fuera de línea = 0)
    // y salidas calculadas en el scan.
    void setRemoteInputs(uint8_t module, uint8_t bits) { if (module < NUM_X) _xi[module] = bits; }
    uint8_t remoteInputs(uint8_t module) const { return module < NUM_X ? _xi[module] : 0; }
    uint8_t remoteOutputs(uint8_t module) const { return module < NUM_X ? _xq[module] : 0; }
    // Salida o del programa (0..7 = Q1..Q8, 8.. = 1.Q1..).
    bool outputValue(uint8_t o) const;

    bool blockOut(uint8_t b) const { return b < _p.count && _out[b]; }
    bool pinValue(uint8_t b, uint8_t pin) const;
    bool sourceValue(uint8_t src) const;
    uint8_t outputs() const { return _q; }
    uint8_t markers() const { return _m; }
    uint8_t inputs() const { return _in; }

    // Valores para mostrar: tiempo transcurrido (ms) y cuenta.
    uint32_t elapsedMs(uint8_t b, uint32_t nowMs) const;
    int32_t count(uint8_t b) const { return b < MAX_BLOCKS ? _cnt[b] : 0; }

    // Retentivos: un valor de 32 bits por bloque (estado, acumulado o cuenta).
    bool retainedDirty() const { return _retDirty; }
    void clearRetainedDirty() { _retDirty = false; }
    uint32_t retained(uint8_t b) const;
    void setRetained(uint8_t b, uint32_t value);

    // Avisos activos (bloques ALARM con En = 1): bit i = bloque i.
    uint64_t alarms() const { return _alarms; }

    // Parámetro de un bloque cambiado en marcha (menú Parámetros).
    void setParam(uint8_t b, int32_t p1, int32_t p2, uint8_t aux);

private:
    bool readSrc(uint8_t src) const;
    bool pin(uint8_t b, uint8_t k, bool openValue) const;
    void evalBlock(uint8_t b, uint32_t nowMs, const ClockNow &clock);
    void buildOrder();
    void visit(uint8_t b, uint8_t state[]);

    Program _p;
    uint8_t _order[MAX_BLOCKS];
    uint8_t _orderCount = 0;

    bool _out[MAX_BLOCKS];
    uint8_t _prevIn[MAX_BLOCKS]; // bit k = valor de la entrada k en el scan anterior
    uint32_t _t0[MAX_BLOCKS];
    uint32_t _acc[MAX_BLOCKS];
    int32_t _cnt[MAX_BLOCKS];
    bool _timing[MAX_BLOCKS];

    uint8_t _in = 0;
    uint8_t _q = 0;
    uint8_t _m = 0;
    uint8_t _xi[NUM_X] = {0};
    uint8_t _xq[NUM_X] = {0};
    uint64_t _alarms = 0;
    bool _retDirty = false;
};

} // namespace jwrt
