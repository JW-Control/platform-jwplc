// SHA-256 (FIPS 180-4) y HMAC (RFC 2104).
#include "JWPLC_Web_Sha256.h"

#include <string.h>

namespace jwweb
{
namespace
{
const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

inline uint32_t rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

void block(uint32_t h[8], const uint8_t *p)
{
    uint32_t w[64];
    for (int i = 0; i < 16; i++)
    {
        w[i] = ((uint32_t)p[i * 4] << 24) | ((uint32_t)p[i * 4 + 1] << 16) | ((uint32_t)p[i * 4 + 2] << 8) | p[i * 4 + 3];
    }
    for (int i = 16; i < 64; i++)
    {
        const uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        const uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
    for (int i = 0; i < 64; i++)
    {
        const uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
        const uint32_t ch = (e & f) ^ (~e & g);
        const uint32_t t1 = hh + S1 + ch + K[i] + w[i];
        const uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
        const uint32_t mj = (a & b) ^ (a & c) ^ (b & c);
        const uint32_t t2 = S0 + mj;
        hh = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }
    h[0] += a;
    h[1] += b;
    h[2] += c;
    h[3] += d;
    h[4] += e;
    h[5] += f;
    h[6] += g;
    h[7] += hh;
}
} // namespace

void Sha256::begin()
{
    static const uint32_t init[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                     0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    memcpy(h, init, sizeof(h));
    len = 0;
    used = 0;
}

void Sha256::update(const void *data, size_t n)
{
    const uint8_t *p = (const uint8_t *)data;
    len += n;
    while (n)
    {
        size_t take = 64 - used;
        if (take > n)
        {
            take = n;
        }
        memcpy(buf + used, p, take);
        used += take;
        p += take;
        n -= take;
        if (used == 64)
        {
            block(h, buf);
            used = 0;
        }
    }
}

void Sha256::finish(uint8_t out[32])
{
    const uint64_t bits = len * 8;
    const uint8_t one = 0x80;
    update(&one, 1);
    const uint8_t zero = 0;
    while (used != 56)
    {
        update(&zero, 1);
    }
    uint8_t lenBytes[8];
    for (int i = 0; i < 8; i++)
    {
        lenBytes[i] = (uint8_t)(bits >> (56 - 8 * i));
    }
    update(lenBytes, 8);
    for (int i = 0; i < 8; i++)
    {
        out[i * 4] = (uint8_t)(h[i] >> 24);
        out[i * 4 + 1] = (uint8_t)(h[i] >> 16);
        out[i * 4 + 2] = (uint8_t)(h[i] >> 8);
        out[i * 4 + 3] = (uint8_t)h[i];
    }
}

void sha256(const void *data, size_t n, uint8_t out[32])
{
    Sha256 s;
    s.begin();
    s.update(data, n);
    s.finish(out);
}

void hmacSha256(const uint8_t *key, size_t keyLen, const void *msg, size_t msgLen, uint8_t out[32])
{
    uint8_t k[64];
    memset(k, 0, sizeof(k));
    if (keyLen > 64)
    {
        sha256(key, keyLen, k);
    }
    else
    {
        memcpy(k, key, keyLen);
    }
    uint8_t pad[64];
    for (int i = 0; i < 64; i++)
    {
        pad[i] = k[i] ^ 0x36;
    }
    uint8_t inner[32];
    Sha256 s;
    s.begin();
    s.update(pad, 64);
    s.update(msg, msgLen);
    s.finish(inner);
    for (int i = 0; i < 64; i++)
    {
        pad[i] = k[i] ^ 0x5c;
    }
    s.begin();
    s.update(pad, 64);
    s.update(inner, 32);
    s.finish(out);
}

bool sameBytes(const uint8_t *a, const uint8_t *b, size_t n)
{
    uint8_t d = 0;
    for (size_t i = 0; i < n; i++)
    {
        d |= a[i] ^ b[i];
    }
    return d == 0;
}

void toHex(const uint8_t *data, size_t n, char *out)
{
    static const char hex[] = "0123456789abcdef";
    for (size_t i = 0; i < n; i++)
    {
        out[i * 2] = hex[data[i] >> 4];
        out[i * 2 + 1] = hex[data[i] & 15];
    }
    out[n * 2] = 0;
}

namespace
{
int nibble(char c)
{
    if (c >= '0' && c <= '9')
    {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f')
    {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F')
    {
        return c - 'A' + 10;
    }
    return -1;
}
} // namespace

bool fromHex(const char *hex, uint8_t *out, size_t n)
{
    if (!hex || strlen(hex) != n * 2)
    {
        return false;
    }
    for (size_t i = 0; i < n; i++)
    {
        const int hi = nibble(hex[i * 2]);
        const int lo = nibble(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0)
        {
            return false;
        }
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    return true;
}
} // namespace jwweb
