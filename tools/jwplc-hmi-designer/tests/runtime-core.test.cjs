#!/usr/bin/env node
// Proyecto «Runtime» del Designer (Alpha16), sin navegador:
//   - designer-runtime-core.js: símbolos rt_*, expresiones y catálogo;
//   - toRuntimeHeader() sobre los headers golden (con variables enlazadas al
//     Runtime) y, si hay g++, compilación contra JWPLC_Runtime.h real.
//
// Uso (desde la raíz del repo):
//   node tools/jwplc-hmi-designer/tests/runtime-core.test.cjs
'use strict';

const fs = require('fs');
const os = require('os');
const path = require('path');
const { execFileSync } = require('child_process');

const POC = path.resolve(__dirname, '..', 'poc');
const LIBS = path.resolve(__dirname, '..', '..', '..', 'JWPLC', '2.1.0', 'libraries');
const STUB = path.join(__dirname, 'host-stub');
const GOLDEN = path.join(__dirname, 'golden');
const rt = require(path.join(POC, 'designer-runtime-core.js'));

const failures = [];
function check(condition, message) {
  if (condition) console.log(`  PASS ${message}`);
  else { console.log(`  FAIL ${message}`); failures.push(message); }
}
function hasGpp() {
  try { execFileSync('g++', ['--version'], { stdio: 'ignore' }); return true; } catch (_) { return false; }
}

console.log('SÍMBOLOS');
check(rt.expressionFor('rt_q1') === 'JWPLC_Runtime.output(1)', 'rt_q1 -> output(1)');
check(rt.expressionFor('rt_i8') === 'JWPLC_Runtime.input(8)', 'rt_i8 -> input(8)');
check(rt.expressionFor('RT_M3') === 'JWPLC_Runtime.marker(3)', 'mayúsculas: RT_M3 -> marker(3)');
check(rt.expressionFor('rt_b05') === 'JWPLC_Runtime.block(5)', 'rt_b05 -> block(5)');
check(rt.expressionFor('rt_b02_t') === 'JWPLC_Runtime.blockSeconds(2)', 'rt_b02_t -> blockSeconds(2)');
check(rt.expressionFor('rt_b05_c_2') === 'JWPLC_Runtime.counter(5)', 'sufijo _2: misma variable en otro objeto');
check(rt.expressionFor('rt_q9') === null && rt.expressionFor('rt_b65') === null, 'fuera de rango: Q9 y B65');
check(rt.expressionFor('presion') === null, 'variable Arduino: no es del Runtime');
check(rt.symbolFor('B05.t') === 'rt_b05_t' && rt.symbolFor('Q1') === 'rt_q1', 'symbolFor');
const boolKeys = rt.catalog('bool', 4).flatMap((g) => g.options.map((o) => o.key));
const numKeys = rt.catalog('number', 4).flatMap((g) => g.options.map((o) => o.key));
check(boolKeys.includes('Q8') && boolKeys.includes('B04') && !boolKeys.includes('B04.t'), 'catálogo bool: sin tiempo ni cuenta');
check(numKeys.includes('B04.t') && numKeys.includes('B04.c'), 'catálogo número: tiempo y cuenta');
check(rt.catalog('number', 4).every((g) => g.options.every((o) => rt.parseSymbol(o.symbol)?.key === o.key)), 'catálogo: cada símbolo vuelve a su clave');

// Variables Arduino de los golden -> variables del Runtime.
const BINDINGS = {
  'fieldsOnly.h': { mensaje: null, presion: 'rt_b02_t', valvula: 'rt_q1', tanque: 'rt_b05_c' },
  'full.h': null, // se enlazan todas por orden
  'shapesOnly.h': {},
  'navCounter.h': null
};
const POOL = ['rt_q1', 'rt_q2', 'rt_i1', 'rt_m1', 'rt_b01', 'rt_b02_t', 'rt_b03_c', 'rt_i2', 'rt_i3', 'rt_q3'];

function bind(text, map) {
  const re = /^(\s*JWPLC_Display\.(?:setBool|setValue|setBar|setText|setIconState)\(\w+, )([A-Za-z_]\w*)(\);)$/gm;
  let k = 0;
  return text.replace(re, (line, a, variable, b) => {
    if (map && Object.prototype.hasOwnProperty.call(map, variable)) return map[variable] ? `${a}${map[variable]}${b}` : line;
    if (map) return line;
    const sym = POOL[k++ % POOL.length];
    return `${a}${sym}_${k}${b}`; // sufijo: cada objeto con su nombre
  });
}

console.log('HEADER');
const gpp = hasGpp();
const out = fs.mkdtempSync(path.join(os.tmpdir(), 'jwrt-'));
for (const [file, map] of Object.entries(BINDINGS)) {
  const source = fs.readFileSync(path.join(GOLDEN, file), 'utf8');
  const pages = (source.match(/^    PAGE_\w+ = \d+/gm) || []).length || 1;
  const r = rt.toRuntimeHeader(bind(source, map), { pageCount: pages });
  const t = r.text;
  check(rt.toRuntimeHeader(t, { pageCount: pages }).text === t, `${file}: segunda pasada idéntica (idempotente)`);
  check(t.includes('#include <JWPLC_Display.h>\n#include <JWPLC_Runtime.h>'), `${file}: incluye JWPLC_Runtime.h`);
  check(t.includes('extern "C" void jwplcRuntimeHMISetup(void)') &&
    (t.includes(`        ${pages});`) || t.includes(`JWPLC_Runtime.setHMI(nullptr, 0, ${pages});`)), `${file}: setup con setHMI(..., ${pages})`);
  check(!/\bjwplcHMISetup\b|\bjwplcUIUpdate\b|\bjwplcUIPageEnter\b|jwplcHMIUserPageEnter/.test(t), `${file}: sin hooks del modo Arduino`);
  check(!/JWPLC_Display\.set(?:IdleWake|IdleReturn|UserPage|UserRefresh)/.test(t), `${file}: la IDLE y las páginas son del Runtime`);
  if (/JWPLC_Display\.set(Bool|Value|Bar|IconState)\(/.test(source)) {
    check(t.includes('extern "C" void jwplcRuntimeHMIUpdate(uint8_t page)'), `${file}: jwplcRuntimeHMIUpdate(page)`);
  }
  if (file === 'fieldsOnly.h') {
    check(t.includes('JWPLC_Display.setBool(FIELD_B, JWPLC_Runtime.output(1));'), 'fieldsOnly: BOOL -> output(1)');
    check(t.includes('JWPLC_Display.setValue(FIELD_P, JWPLC_Runtime.blockSeconds(2));'), 'fieldsOnly: VALUE -> blockSeconds(2)');
    check(r.unbound.length === 1 && r.unbound[0] === 'FIELD_MSG' && t.includes('// AVISO: 1 objeto(s)'), 'fieldsOnly: TEXT sin variable queda avisado');
    check(!/^char mensaje|^float presion/m.test(t), 'fieldsOnly: sin variables propias');
  }
  if (file === 'shapesOnly.h') check(t.includes('extern "C" void jwplcRuntimeHMIPageEnter(uint8_t page)'), 'shapesOnly: gráfica estática en PageEnter');
  if (!gpp) continue;
  const h = path.join(out, `rt_${file}`);
  fs.writeFileSync(h, t);
  const cpp = path.join(out, `rt_${file}.cpp`);
  fs.writeFileSync(cpp, `#include "${h}"\nint main() { jwplcRuntimeHMISetup(); jwplcRuntimeHMIPageEnter(0); jwplcRuntimeHMIUpdate(0); return 0; }\n`);
  try {
    execFileSync('g++', ['-std=gnu++17', '-fsyntax-only', '-Wall', `-I${STUB}`,
      `-I${path.join(LIBS, 'JWPLC_Runtime', 'src')}`, `-I${path.join(LIBS, 'JWPLC_Display', 'src')}`,
      `-I${path.join(LIBS, 'JWPLC_TFT', 'src')}`, cpp], { stdio: 'pipe' });
    check(true, `${file}: compila contra JWPLC_Runtime.h (g++ -fsyntax-only)`);
  } catch (error) {
    console.log(String(error.stderr || error).split('\n').slice(0, 12).join('\n'));
    check(false, `${file}: compila contra JWPLC_Runtime.h (g++ -fsyntax-only)`);
  }
}

const menu = rt.toRuntimeHeader(fs.readFileSync(path.join(GOLDEN, 'menu.h'), 'utf8'), { pageCount: 1 });
check(menu.unsupported.includes('MENU') && menu.text.startsWith('// ERROR RUNTIME: MENU'), 'menu.h: MENU no admitido, con error al inicio');

console.log(failures.length ? `\nRESULTADO: FAIL (${failures.length})` : '\nRESULTADO: PASS');
process.exit(failures.length ? 1 : 0);
