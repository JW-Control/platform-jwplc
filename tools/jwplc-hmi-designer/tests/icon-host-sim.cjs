#!/usr/bin/env node
// Simulación en host del ICON de JWPLC_Display (Alpha16).
//
// Compila el JWPLC_UI.cpp real del package (con JWPLC_UI_API.cpp y
// JWPLC_UI_Format.cpp) con g++ contra stubs (tests/icon-host/stub: TFT en
// framebuffer con la fuente GLCD) y ejecuta tests/icon-host/icon_sim.cpp:
// decodificación 4 bpp, transparente, estados, etiqueta, 64 px y que un cambio
// de estado sólo repinta el icono. Guarda una captura PNG.
//
// Uso (desde la raíz del repo):
//   node tools/jwplc-hmi-designer/tests/icon-host-sim.cjs [carpeta-salida]
// Requiere: g++ (C++17).
'use strict';

const fs = require('fs');
const os = require('os');
const path = require('path');
const { execFileSync } = require('child_process');
const { fontHeader, ppmToPng } = require('./host-sim-lib.cjs');

const ROOT = path.resolve(__dirname, '..', '..', '..');
const HOST = path.join(__dirname, 'icon-host');
const PKG = path.join(ROOT, 'JWPLC', '2.1.0');
const SRC = path.join(PKG, 'libraries', 'JWPLC_Display', 'src');
const OUT = path.resolve(process.argv[2] || fs.mkdtempSync(path.join(os.tmpdir(), 'jwplc-icon-sim-')));

// Compila un ejecutable de simulación con el motor de campos real.
function buildSim(mainCpp, exe, includeDirs = []) {
  execFileSync('g++', [
    '-std=c++17', '-Wall', '-Wextra', '-Werror', '-Wno-unused-parameter',
    '-I', path.join(HOST, 'stub'), '-I', SRC, '-I', path.join(PKG, 'cores', 'jwcontrol'),
    ...includeDirs.flatMap((dir) => ['-I', dir]),
    mainCpp,
    path.join(SRC, 'JWPLC_UI.cpp'), path.join(SRC, 'JWPLC_UI_API.cpp'), path.join(SRC, 'JWPLC_UI_Format.cpp'),
    path.join(SRC, 'JWPLC_UI_PixelMap.cpp'),
    '-o', exe
  ], { stdio: 'inherit' });
}

function main() {
  fs.mkdirSync(OUT, { recursive: true });
  const gen = path.join(OUT, 'gen');
  fs.mkdirSync(gen, { recursive: true });
  fs.writeFileSync(path.join(gen, 'font_data.h'), fontHeader());

  const exe = path.join(OUT, process.platform === 'win32' ? 'icon_sim.exe' : 'icon_sim');
  buildSim(path.join(HOST, 'icon_sim.cpp'), exe, [gen]);
  const ppm = path.join(OUT, 'icon_sim.ppm');
  let failed = false;
  try {
    execFileSync(exe, [ppm], { stdio: 'inherit' });
  } catch (_) {
    failed = true;
  }
  if (fs.existsSync(ppm)) ppmToPng(ppm, ppm.replace(/\.ppm$/, '.png'));
  console.log(`Captura en ${OUT}`);
  process.exit(failed ? 1 : 0);
}

if (require.main === module) main();
module.exports = { buildSim };
