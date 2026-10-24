#!/usr/bin/env node
// Ejecuta todos los tests del HMI Designer y resume el resultado.
//
//   node tools/jwplc-hmi-designer/tests/run-all.cjs
//
// Sin g++ (p. ej. Windows sin MSYS2) se omiten los tests que compilan C++:
// codegen-smoke, menu-host-sim e icon-host-sim; menu-smoke, icon-smoke y codegen-core omiten sólo esos pasos.
'use strict';

const path = require('path');
const { spawnSync, execFileSync } = require('child_process');

function has(cmd, args) {
  try { execFileSync(cmd, args, { stdio: 'ignore' }); return true; } catch (_) { return false; }
}
const gpp = has('g++', ['--version']);
const python = has(process.platform === 'win32' ? 'python' : 'python3', ['--version']);

const TESTS = [
  { file: 'codegen-core.test.cjs', what: 'codegen puro en node (núcleo + MENU)' },
  { file: 'icon-catalog.test.cjs', what: 'catálogo de iconos al día y dentro de los límites del firmware' },
  { file: 'codegen-golden.cjs', what: 'headers generados == golden' },
  { file: 'value-format-parity.cjs', what: 'formato de VALUE: firmware real == Designer (sin g++ sólo casos fijos)' },
  { file: 'inspector-smoke.cjs', what: 'inspector TEXT/VALUE/BOOL/BAR + paridad BAR' },
  { file: 'menu-smoke.cjs', what: 'MENU en el Designer + paridad con el firmware' },
  { file: 'icon-smoke.cjs', what: 'ICON en el Designer (panel, inspector, guardar) + paridad con el firmware' },
  { file: 'sketch-menu-smoke.cjs', what: 'Escribir en sketch + menú del sketch (app de escritorio simulada)' },
  { file: 'sketch-guide.test.cjs', what: 'guía JWPLC_HMI_LEEME.md: contenido + sketch base compila (sin g++ sólo contenido)' },
  { file: 'agent-cli.test.cjs', what: 'CLI para agentes: spec == Designer, validación, paridad con golden, ejemplos, build' },
  { file: 'port-picker-smoke.cjs', what: 'selector de puertos serie propio (app de escritorio simulada)' },
  { file: 'live-bridge-flasher.test.cjs', what: 'flasheador Live Bridge (WebSerial + esptool-js + binarios ESP32)' },
  { file: 'live-bridge-host-sim.cjs', what: 'Live Bridge real en host (g++): TFT == lienzo, pushImage, protocolo v1/v2', needs: gpp ? null : 'g++' },
  { file: 'menu-host-sim.cjs', what: 'MENU: firmware real en host', needs: gpp ? null : 'g++' },
  { file: 'icon-host-sim.cjs', what: 'ICON: firmware real en host (estados, transparente, repintado parcial)', needs: gpp ? null : 'g++' },
  { file: 'codegen-smoke.cjs', what: 'headers compilan contra el package', needs: gpp && python ? null : 'g++ y python3' }
];

const results = [];
for (const test of TESTS) {
  if (test.needs) {
    console.log(`\n=== ${test.file} — OMITIDO (requiere ${test.needs})`);
    results.push([test, 'SKIP']);
    continue;
  }
  console.log(`\n=== ${test.file} — ${test.what}`);
  const run = spawnSync(process.execPath, [path.join(__dirname, test.file)], { stdio: 'inherit' });
  results.push([test, run.status === 0 ? 'PASS' : 'FAIL']);
}

console.log('\n================ RESUMEN ================');
for (const [test, status] of results) console.log(`${status.padEnd(5)} ${test.file.padEnd(24)} ${test.what}`);
process.exit(results.some(([, s]) => s === 'FAIL') ? 1 : 0);
