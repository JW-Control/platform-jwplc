// Stub mínimo para -fsyntax-only en host (no es el core real).
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
class Print {
public:
  virtual ~Print() {}
  virtual size_t write(uint8_t) = 0;
  virtual size_t write(const uint8_t *buffer, size_t size) { size_t n = 0; while (size--) n += write(*buffer++); return n; }
  size_t print(const char *s) { return write((const uint8_t *)s, strlen(s)); }
  size_t print(int v) { (void)v; return 0; }
  size_t println(const char *s) { return print(s); }
};
class String { public: String(const char *s = "") { (void)s; } };
typedef uint8_t byte;
unsigned long millis(); unsigned long micros();
