// SHA-256 y HMAC-SHA256 (uso interno de JWPLC_Web). Propio y sin dependencias para
// que compile igual en la placa y en JWPLC Sim.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace jwweb
{
struct Sha256
{
    uint32_t h[8];
    uint8_t buf[64];
    uint64_t len;
    size_t used;

    void begin();
    void update(const void *data, size_t n);
    void finish(uint8_t out[32]);
};

void sha256(const void *data, size_t n, uint8_t out[32]);
void hmacSha256(const uint8_t *key, size_t keyLen, const void *msg, size_t msgLen, uint8_t out[32]);

// Comparación en tiempo constante.
bool sameBytes(const uint8_t *a, const uint8_t *b, size_t n);

void toHex(const uint8_t *data, size_t n, char *out); // out: 2n + 1
bool fromHex(const char *hex, uint8_t *out, size_t n); // exactamente 2n dígitos
} // namespace jwweb
