// Compilación en host (g++ -fsyntax-only) del header generado contra los
// headers reales de JWPLC_Display / JWPLC_UI / JWPLC_TFT, como
// tests/codegen-smoke.cjs y tests/sketch-guide.test.cjs. No sustituye la
// compilación ESP32 real (tools/alpha15/regenerate_libJWPLC_TFT.sh compile).
'use strict';

const fs = require('fs');
const os = require('os');
const path = require('path');
const { execFileSync } = require('child_process');

const DESIGNER = path.resolve(__dirname, '..', '..');
const ROOT = path.resolve(DESIGNER, '..', '..');
const LIBS = path.join(ROOT, 'JWPLC', '2.1.0', 'libraries');
const STUB = path.join(DESIGNER, 'tests', 'host-stub');

function has(cmd, args) {
  try { execFileSync(cmd, args, { stdio: 'ignore' }); return true; } catch (_) { return false; }
}

function gpp(dir, file) {
  try {
    execFileSync('g++', ['-std=gnu++17', '-fsyntax-only', '-Wall', `-I${dir}`, `-I${STUB}`,
      `-I${path.join(LIBS, 'JWPLC_Display', 'src')}`, `-I${path.join(LIBS, 'JWPLC_TFT', 'src')}`, file], { stdio: 'pipe' });
    return { ok: true, log: '' };
  } catch (error) {
    return { ok: false, log: String(error.stderr || error.message).split('\n').slice(0, 12).join('\n') };
  }
}

// header: texto de JWPLC_HMI_Generated.h. sketch: código de un .ino (opcional;
// por defecto el sketch base de la guía JWPLC_HMI_LEEME.md).
function hostCheck(header, options = {}) {
  if (!has('g++', ['--version'])) return { skipped: 'g++ no está instalado' };
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'jwhmi-check-'));
  const guide = require(path.join(DESIGNER, 'poc', 'designer-sketch-guide.js'));
  fs.writeFileSync(path.join(dir, 'JWPLC_HMI_Generated.h'), header);
  const sketch = options.sketch || guide.baseSketch(header);
  fs.writeFileSync(path.join(dir, 'sketch.cpp'), `${sketch}\n\nint main() { setup(); loop(); return 0; }\n`);
  const result = gpp(dir, path.join(dir, 'sketch.cpp'));

  // Cada llamada emitida debe existir en la librería (.a o fuente).
  let missing = [];
  try {
    const { librarySymbols } = require(path.join(DESIGNER, 'tests', 'lib-symbols.cjs'));
    const symbols = new Set();
    for (const lib of ['JWPLC_Display', 'JWPLC_TFT']) librarySymbols(path.join(LIBS, lib)).symbols.forEach((s) => symbols.add(s));
    const calls = new Set();
    for (const m of header.matchAll(/\btft\.([A-Za-z]+)\(/g)) calls.add(`JWPLC_TFTClass::${m[1]}`);
    for (const m of header.matchAll(/JWPLC_Display\.([A-Za-z]+)\(/g)) calls.add(`JWPLC_DisplayClass::${m[1]}`);
    const inlineOk = new Set(['JWPLC_TFTClass::print', 'JWPLC_DisplayClass::setValue']);
    missing = [...calls].filter((c) => !symbols.has(c) && !inlineOk.has(c));
  } catch (_) {
    // sin nm (librería precompilada) no se puede verificar: queda la compilación
  }
  fs.rmSync(dir, { recursive: true, force: true });
  return { ok: result.ok && missing.length === 0, compiled: result.ok, log: result.log, missing, sketch };
}

module.exports = { hostCheck };
