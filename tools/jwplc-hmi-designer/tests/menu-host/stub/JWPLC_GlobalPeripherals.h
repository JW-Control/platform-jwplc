// Stub host de la botonera con la semántica de JW_MatrixButtons:
// pressed() es latcheado y consumible; applyAxis() consume los PRESS de sus dos
// teclas (sin repeats en la simulación).
#pragma once
#include <Arduino.h>

enum : uint8_t { BTN_LEFT = 0, BTN_UP, BTN_RIGHT, BTN_ESC, BTN_OK, BTN_DOWN, BTN_COUNT };

class JW_MatrixButtons
{
public:
  mutable uint8_t pend[BTN_COUNT] = {};
  void press(uint8_t id) { pend[id]++; }
  bool isDown(uint8_t) const { return false; }
  bool pressed(uint8_t id) const { if (pend[id]) { pend[id]--; return true; } return false; }
  void clearPendingInput() const { for (auto &p : pend) p = 0; }
  bool applyAxis(uint32_t *val, uint32_t minv, uint32_t maxv, uint8_t decId, uint8_t incId,
                 bool wrap = true, bool snap = true) const
  {
    (void)snap;
    uint32_t v = *val; bool changed = false;
    uint8_t dec = pend[decId], inc = pend[incId];
    pend[decId] = pend[incId] = 0;
    for (uint8_t i = 0; i < dec; i++) {
      if (v <= minv) { if (wrap && minv < maxv) { v = maxv; changed = true; } }
      else { v--; changed = true; }
    }
    for (uint8_t i = 0; i < inc; i++) {
      if (v >= maxv) { if (wrap && minv < maxv) { v = minv; changed = true; } }
      else { v++; changed = true; }
    }
    *val = v;
    return changed;
  }
};
extern JW_MatrixButtons JWPLC_Buttons;
namespace JWPLCButtons { bool isReady(); void clearPendingInput(); }
