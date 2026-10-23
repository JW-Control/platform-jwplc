// Stub host para la simulación del MENU (no es el core real).
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
class Print {
public:
  virtual ~Print() {}
  virtual size_t write(uint8_t) = 0;
  virtual size_t write(const uint8_t *buffer, size_t size) { size_t n = 0; while (size--) n += write(*buffer++); return n; }
  size_t print(const char *s) { return write((const uint8_t *)s, strlen(s)); }
};
class String { public: String(const char *s = "") { (void)s; } };
typedef uint8_t byte;
inline unsigned long millis() { return 0; }
inline unsigned long micros() { return 0; }
