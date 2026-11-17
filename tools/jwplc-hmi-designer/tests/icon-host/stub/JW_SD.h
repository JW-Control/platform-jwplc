// Stub host de JW_SD: desde #239 JWPLC_UI.cpp registra las tendencias (TREND)
// en la MicroSD. En las simulaciones en host no hay tarjeta: la SD nunca está
// lista y el registro no se abre, así que el dibujo no cambia.
#pragma once
#include <Arduino.h>

class JW_SD
{
public:
  bool isReady() const { return false; }
};

inline JW_SD JWPLC_SD;

namespace JWPLCSD
{
  inline bool isEnabled() { return false; }
  inline bool begin() { return false; }
}

class JWPLCDataLog
{
public:
  bool begin(JW_SD &, const char *) { return false; }
  size_t writeLine(const char *) { return 0; }
  bool close(bool = true) { return true; }
};
