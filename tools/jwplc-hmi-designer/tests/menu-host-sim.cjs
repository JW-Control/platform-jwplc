#!/usr/bin/env node
// Simulación en host del MENU de JWPLC_Display (A15-8).
//
// Compila el JWPLC_UI_Menu.cpp real del package con g++ contra stubs
// (tests/menu-host/stub: botonera con la semántica de JW_MatrixButtons, TFT en
// framebuffer con la fuente GLCD) y ejecuta el guion de teclas de
// tests/menu-host/menu_sim.cpp. Guarda capturas PNG del framebuffer.
//
// Uso (desde la raíz del repo):
//   node tools/jwplc-hmi-designer/tests/menu-host-sim.cjs [carpeta-salida]
// Requiere: g++ (C++17).
'use strict';

const fs = require('fs');
const os = require('os');
const path = require('path');
const zlib = require('zlib');
const { execFileSync } = require('child_process');

const ROOT = path.resolve(__dirname, '..', '..', '..');
const HOST = path.join(__dirname, 'menu-host');
const SRC = path.join(ROOT, 'JWPLC', '2.1.0', 'libraries', 'JWPLC_Display', 'src');
const OUT = path.resolve(process.argv[2] || fs.mkdtempSync(path.join(os.tmpdir(), 'jwplc-menu-sim-')));

// Glifos GLCD: los 256 de poc/gfx-classic-font.js (la misma tabla que usa el
// lienzo, copiada de glcdfont.c). Como TFT_eSPI con cp437(false), un byte
// > 175 dibuja el glifo siguiente.
function fontHeader() {
  const js = fs.readFileSync(path.join(ROOT, 'tools', 'jwplc-hmi-designer', 'poc', 'gfx-classic-font.js'), 'utf8');
  const body = js.slice(js.indexOf('new Uint8Array(['), js.indexOf(']);')).replace(/\/\/.*$/gm, '');
  const bytes = body.match(/0x[0-9A-Fa-f]{2}/g);
  if (!bytes || bytes.length !== 256 * 5) throw new Error(`fuente GLCD inesperada (${bytes && bytes.length})`);
  return `// Generado por menu-host-sim.cjs desde poc/gfx-classic-font.js
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

function main() {
  fs.mkdirSync(OUT, { recursive: true });
  const gen = path.join(OUT, 'gen');
  fs.mkdirSync(gen, { recursive: true });
  fs.writeFileSync(path.join(gen, 'font_data.h'), fontHeader());

  const exe = path.join(OUT, process.platform === 'win32' ? 'menu_sim.exe' : 'menu_sim');
  execFileSync('g++', [
    '-std=c++17', '-Wall', '-Wextra', '-Werror', '-Wno-unused-parameter',
    '-I', path.join(HOST, 'stub'), '-I', SRC, '-I', gen,
    path.join(HOST, 'menu_sim.cpp'), path.join(SRC, 'JWPLC_UI_Menu.cpp'),
    '-o', exe
  ], { stdio: 'inherit' });

  let failed = false;
  try {
    execFileSync(exe, [OUT], { stdio: 'inherit' });
  } catch (_) {
    failed = true;
  }

  for (const file of fs.readdirSync(OUT).filter((f) => f.endsWith('.ppm'))) {
    ppmToPng(path.join(OUT, file), path.join(OUT, file.replace(/\.ppm$/, '.png')));
  }
  console.log(`Capturas en ${OUT}`);
  process.exit(failed ? 1 : 0);
}

main();
