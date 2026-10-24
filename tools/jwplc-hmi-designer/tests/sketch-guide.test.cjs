#!/usr/bin/env node
// Guía del sketch (poc/designer-sketch-guide.js -> JWPLC_HMI_LEEME.md).
//
// Para cada header golden: arma la guía, verifica su contenido (marca,
// variables, nodos editables del MENU) y compila el sketch base que propone
// (g++ -fsyntax-only contra los headers reales del package, como
// codegen-smoke). Sin g++ sólo se omite la compilación.
//
//   node tools/jwplc-hmi-designer/tests/sketch-guide.test.cjs
'use strict';

const fs = require('fs');
const os = require('os');
const path = require('path');
const { execFileSync } = require('child_process');

const guide = require('../poc/designer-sketch-guide.js');

const GOLDEN = path.join(__dirname, 'golden');
const STUB = path.join(__dirname, 'host-stub');
const LIBS = path.resolve(__dirname, '..', '..', '..', 'JWPLC', '2.1.0', 'libraries');

let failures = 0;
function check(condition, message) {
  console.log(`  ${condition ? 'PASS' : 'FAIL'} ${message}`);
  if (!condition) failures += 1;
}

let gpp = true;
try { execFileSync('g++', ['--version'], { stdio: 'ignore' }); } catch (_) { gpp = false; }
const OUT = fs.mkdtempSync(path.join(os.tmpdir(), 'jwplc-hmi-guide-'));

const goldens = fs.readdirSync(GOLDEN).filter((f) => f.endsWith('.h') && !f.endsWith('.actual.h'));
for (const file of goldens) {
  const name = file.replace(/\.h$/, '');
  const header = fs.readFileSync(path.join(GOLDEN, file), 'utf8');
  console.log(name.toUpperCase());
  const text = guide.build({ sketchName: name, header, date: '2026-10-19' });
  const vars = guide.parseVariables(header);
  const menus = guide.parseMenus(header);

  check(text.startsWith(guide.GUIDE_MARK), 'primera línea con la marca (el escritorio sólo reemplaza guías propias)');
  check(/el Designer nunca lo modifica/.test(text), 'aclara que el .ino no se toca');
  const declared = [...header.matchAll(/^(?:char|bool|float|int32_t|uint32_t)\s+(\w+)/gm)].map((m) => m[1]);
  check(declared.every((v) => vars.some((x) => x.name === v)) && vars.every((v) => text.includes(`| \`${v.name}\` |`)),
    `las ${vars.length} variables del header están en la tabla`);
  menus.forEach((menu) => {
    const editable = menu.nodes.filter((n) => n.editable).map((n) => n.name);
    const cases = [...text.matchAll(new RegExp(`case (${menu.id}_\\w+):`, 'g'))].map((m) => m[1]);
    check(JSON.stringify(cases) === JSON.stringify(editable), `${menu.id}: un case por nodo editable (${cases.join(', ') || 'ninguno'})`);
  });

  const sketch = /```cpp\n([\s\S]*?)\n```/.exec(text)?.[1] || '';
  check(sketch.includes('jwplcHMISetup();') && !/\bdelay\(/.test(sketch.replace(/\/\/.*$/gm, '')), 'sketch base: jwplcHMISetup() y sin delay()');
  if (!gpp) { console.log('  (sin g++: se omite la compilación)'); continue; }
  const dir = path.join(OUT, name);
  fs.mkdirSync(dir, { recursive: true });
  fs.writeFileSync(path.join(dir, 'JWPLC_HMI_Generated.h'), header);
  fs.writeFileSync(path.join(dir, 'sketch.cpp'), `${sketch}\n\nint main() { setup(); loop(); return 0; }\n`);
  let ok = true;
  let log = '';
  try {
    execFileSync('g++', ['-std=gnu++17', '-fsyntax-only', '-Wall', '-Werror=unused-variable', `-I${dir}`, `-I${STUB}`,
      `-I${path.join(LIBS, 'JWPLC_Display', 'src')}`, `-I${path.join(LIBS, 'JWPLC_TFT', 'src')}`, path.join(dir, 'sketch.cpp')], { stdio: 'pipe' });
  } catch (error) {
    ok = false;
    log = String(error.stderr || error.message).split('\n').slice(0, 8).join('\n');
  }
  check(ok, `el sketch base compila contra el header${ok ? '' : `\n${log}`}`);
}

console.log('VARIOS MENU');
const twoMenus = ['A', 'B'].map((id) => `// Nodos de MENU_${id}: x\nenum : uint8_t\n{\n    MENU_${id}_X = 0,\n};\n` +
  `static const JWPLC_UIMenuNode HMI_MENU_${id}_NODES[] =\n{\n    JWPLC_UIMenuEditBool(JWPLC_UI_MENU_ROOT, "X", &x${id}, "NO", "SI")\n};\n`).join('\n');
const base = guide.baseSketch(`// Variables HMI\n// MENU A\nbool xA = false;\n// MENU B\nbool xB = false;\n\n${twoMenus}`);
check((base.match(/uint8_t nodo;/g) || []).length === 1 && (base.match(/menuChanged\(/g) || []).length === 2,
  'dos MENU: una sola declaración de «nodo» y un menuChanged() por MENU');

console.log(failures ? `\n${failures} verificación(es) fallaron.` : '\nTodas las verificaciones pasaron.');
process.exit(failures ? 1 : 0);
