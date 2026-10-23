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
const { execFileSync } = require('child_process');

const ROOT = path.resolve(__dirname, '..', '..', '..');
const HOST = path.join(__dirname, 'menu-host');
const SRC = path.join(ROOT, 'JWPLC', '2.1.0', 'libraries', 'JWPLC_Display', 'src');
const OUT = path.resolve(process.argv[2] || fs.mkdtempSync(path.join(os.tmpdir(), 'jwplc-menu-sim-')));

const { fontHeader, ppmToPng } = require('./host-sim-lib.cjs');

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

  // Despertar desde IDLE con páginas y MENU reales: la tecla que despierta no
  // actúa además sobre el selector ni sobre el MENU.
  const wake = path.join(OUT, process.platform === 'win32' ? 'wake_sim.exe' : 'wake_sim');
  execFileSync('g++', [
    '-std=c++17', '-Wall', '-Wextra', '-Werror', '-Wno-unused-parameter',
    '-I', path.join(HOST, 'stub'), '-I', SRC,
    path.join(HOST, 'wake_sim.cpp'), path.join(SRC, 'JWPLC_UI_Pages.cpp'), path.join(SRC, 'JWPLC_UI_Menu.cpp'),
    '-o', wake
  ], { stdio: 'inherit' });
  try {
    execFileSync(wake, [], { stdio: 'inherit' });
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
