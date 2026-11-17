#!/usr/bin/env node
'use strict';

// Compila el JWPLC_UI.cpp real con g++ contra stubs (icon-host/stub) y verifica
// el BAR píxel a píxel: grosor, largo, borde, esquinas, bloques y degradado.
//
//   node tools/jwplc-hmi-designer/tests/bar-host-sim.cjs [carpeta-de-salida]

const fs = require('fs');
const os = require('os');
const path = require('path');
const { execFileSync } = require('child_process');
const { buildSim } = require('./icon-host-sim.cjs');
const { fontHeader, ppmToPng } = require('./host-sim-lib.cjs');

const OUT = path.resolve(process.argv[2] || fs.mkdtempSync(path.join(os.tmpdir(), 'jwplc-bar-sim-')));
const gen = path.join(OUT, 'gen');
fs.mkdirSync(gen, { recursive: true });
fs.writeFileSync(path.join(gen, 'font_data.h'), fontHeader());

const exe = path.join(OUT, process.platform === 'win32' ? 'bar_sim.exe' : 'bar_sim');
buildSim(path.join(__dirname, 'icon-host', 'bar_sim.cpp'), exe, [gen]);
const ppm = path.join(OUT, 'bar_sim.ppm');
let failed = false;
try {
  execFileSync(exe, [ppm], { stdio: 'inherit' });
} catch (_) {
  failed = true;
}
if (fs.existsSync(ppm)) ppmToPng(ppm, ppm.replace(/\.ppm$/, '.png'));
console.log(`Captura en ${OUT}`);
process.exit(failed ? 1 : 0);
