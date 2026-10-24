// Utilidades de las simulaciones en host (menu-host-sim, icon-host-sim):
// cabecera C++ con la fuente GLCD del lienzo y PPM -> PNG sin dependencias.
'use strict';

const fs = require('fs');
const path = require('path');
const zlib = require('zlib');

const ROOT = path.resolve(__dirname, '..', '..', '..');

// Glifos GLCD: los 256 de poc/gfx-classic-font.js (la misma tabla que usa el
// lienzo, copiada de glcdfont.c). Como TFT_eSPI con cp437(false), un byte
// > 175 dibuja el glifo siguiente.
function fontHeader() {
  const js = fs.readFileSync(path.join(ROOT, 'tools', 'jwplc-hmi-designer', 'poc', 'gfx-classic-font.js'), 'utf8');
  const body = js.slice(js.indexOf('new Uint8Array(['), js.indexOf(']);')).replace(/\/\/.*$/gm, '');
  const bytes = body.match(/0x[0-9A-Fa-f]{2}/g);
  if (!bytes || bytes.length !== 256 * 5) throw new Error(`fuente GLCD inesperada (${bytes && bytes.length})`);
  return `// Generado por tests/host-sim-lib.cjs desde poc/gfx-classic-font.js
#pragma once
#include <stdint.h>
static const uint8_t JWPLC_SIM_FONT[] = {${bytes.join(',')}};
inline const uint8_t *jwplcSimGlyph(uint8_t c) {
  unsigned index = c;
  if (index > 175) index++;
  if (index > 255) index = '?';
  return &JWPLC_SIM_FONT[index * 5];
}
`;
}

function crc32(buf) {
  let c;
  const table = crc32.table || (crc32.table = Array.from({ length: 256 }, (_, n) => {
    c = n;
    for (let k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320 ^ (c >>> 1) : c >>> 1;
    return c >>> 0;
  }));
  let crc = 0xFFFFFFFF;
  for (const b of buf) crc = table[(crc ^ b) & 0xFF] ^ (crc >>> 8);
  return (crc ^ 0xFFFFFFFF) >>> 0;
}

function ppmToPng(ppmPath, pngPath, scale = 3) {
  const data = fs.readFileSync(ppmPath);
  const header = data.toString('latin1', 0, 20).match(/^P6\s+(\d+)\s+(\d+)\s+255\s/);
  const w = Number(header[1]);
  const h = Number(header[2]);
  const pixels = data.subarray(header[0].length);
  const W = w * scale;
  const raw = Buffer.alloc((W * 3 + 1) * h * scale);
  let o = 0;
  for (let y = 0; y < h * scale; y++) {
    raw[o++] = 0;
    for (let x = 0; x < W; x++) {
      const i = (Math.floor(y / scale) * w + Math.floor(x / scale)) * 3;
      raw[o++] = pixels[i]; raw[o++] = pixels[i + 1]; raw[o++] = pixels[i + 2];
    }
  }
  const chunk = (type, body) => {
    const len = Buffer.alloc(4); len.writeUInt32BE(body.length);
    const td = Buffer.concat([Buffer.from(type), body]);
    const crc = Buffer.alloc(4); crc.writeUInt32BE(crc32(td));
    return Buffer.concat([len, td, crc]);
  };
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(W, 0); ihdr.writeUInt32BE(h * scale, 4);
  ihdr[8] = 8; ihdr[9] = 2;
  fs.writeFileSync(pngPath, Buffer.concat([
    Buffer.from([0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A]),
    chunk('IHDR', ihdr), chunk('IDAT', zlib.deflateSync(raw)), chunk('IEND', Buffer.alloc(0))
  ]));
}

module.exports = { fontHeader, ppmToPng };
