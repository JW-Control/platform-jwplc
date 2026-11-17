#!/usr/bin/env node
// Pruebas unitarias en Node puro para designer-qr-core.js, spec/normalize/validate de QR.
'use strict';

const assert = require('assert');
const qrCore = require('../poc/designer-qr-core.js');
const spec = require('../cli/lib/spec.cjs');
const { normalizeProject } = require('../cli/lib/normalize.cjs');
const { validateProject } = require('../cli/lib/validate.cjs');

let failures = 0;
function test(name, fn) {
  try {
    fn();
    console.log(`  PASS ${name}`);
  } catch (err) {
    console.error(`  FAIL ${name}: ${err.message}`);
    failures += 1;
  }
}

console.log('=== qr-core.test.cjs — Codificador QR, presets, geometría y validación ===');

test('codifica URL corta en v1/v2 con buscadores válidos (7x7)', () => {
  const res = qrCore.encode('http://192.168.1.50', 'M');
  assert.strictEqual(res.ok, true);
  assert.strictEqual(res.version, 2);
  assert.strictEqual(res.size, 25);
  // Esquina superior izquierda del finder pattern: fila 0 cols 0..6 todas oscuras
  for (let x = 0; x < 7; x += 1) {
    assert.strictEqual(res.modules[x], 1, `finder top row col ${x}`);
  }
  // Anillo blanco interior (x=1..5, y=1) debe tener 1 en 1? no, y=1 x=1..5 es 0
  for (let x = 1; x <= 5; x += 1) {
    assert.strictEqual(res.modules[res.size + x], 0, `finder inner white col ${x}`);
  }
  // Centro 3x3 oscuro (2..4, 2..4)
  for (let y = 2; y <= 4; y += 1) {
    for (let x = 2; x <= 4; x += 1) {
      assert.strictEqual(res.modules[y * res.size + x], 1, `finder core (${x},${y})`);
    }
  }
});

test('soporta niveles L, M, Q, H y versiones 1..10', () => {
  const v1 = qrCore.encode('JWPLC', 'L');
  assert.strictEqual(v1.ok, true);
  assert.strictEqual(v1.version, 1);
  assert.strictEqual(v1.size, 21);

  const v10 = qrCore.encode('A'.repeat(271), 'L');
  assert.strictEqual(v10.ok, true);
  assert.strictEqual(v10.version, 10);
  assert.strictEqual(v10.size, 57);

  const over = qrCore.encode('A'.repeat(272), 'L');
  assert.strictEqual(over.ok, false);
  assert.strictEqual(over.overflow, true);
});

test('empaquetado de bits MSB-first para PROGMEM/Flash coincide con modules[]', () => {
  const res = qrCore.encode('http://192.168.1.50/v?c=123456', 'M');
  assert.strictEqual(res.ok, true);
  const packed = qrCore.packModulesToBytes(res);
  assert.strictEqual(packed.length, Math.ceil((res.size * res.size) / 8));
  for (let i = 0; i < res.size * res.size; i += 1) {
    const bit = (packed[i >> 3] & (0x80 >> (i & 7))) !== 0 ? 1 : 0;
    assert.strictEqual(bit, res.modules[i], `mismatch at bit ${i}`);
  }
});

test('formatWifiText y parseWifiText hacen round-trip con caracteres escapados', () => {
  const s = qrCore.formatWifiText({ auth: 'WPA', ssid: 'Planta;A:1', password: 'clav\\e,1', hidden: false });
  assert.strictEqual(s, 'WIFI:T:WPA;S:Planta\\;A\\:1;P:clav\\\\e\\,1;;');
  const parsed = qrCore.parseWifiText(s);
  assert.strictEqual(parsed.auth, 'WPA');
  assert.strictEqual(parsed.ssid, 'Planta;A:1');
  assert.strictEqual(parsed.password, 'clav\\e,1');

  const openNet = qrCore.formatWifiText({ auth: 'nopass', ssid: 'Invitados', password: 'ignored' });
  assert.strictEqual(openNet, 'WIFI:T:nopass;S:Invitados;;');
});

test('computeMetrics calcula tamaño exacto y detecta desbordamiento de pantalla', () => {
  const m = qrCore.computeMetrics({ text: 'http://192.168.1.50', ecc: 'M', scale: 3, quietZone: 4, frame: false, x: 10, y: 10 });
  // v2 = 25 módulos + 8 quiet = 33; 33 * 3 = 99 px
  assert.strictEqual(m.moduleCount, 25);
  assert.strictEqual(m.width, 99);
  assert.strictEqual(m.height, 99);
  assert.strictEqual(m.fitsScreen, true);
  assert.strictEqual(m.inBounds, true);

  const big = qrCore.computeMetrics({ text: 'A'.repeat(120), ecc: 'H', scale: 4, quietZone: 4, frame: true, x: 0, y: 0 });
  assert.strictEqual(big.fitsScreen, false);
});

test('contrastCheck advierte si está invertido o con contraste bajo', () => {
  assert.strictEqual(qrCore.contrastCheck(0x0000, 0xFFFF).ok, true);
  assert.strictEqual(qrCore.contrastCheck(0xFFFF, 0x0000).inverted, true);
  assert.strictEqual(qrCore.contrastCheck(0x7BEF, 0x8410).lowContrast, true);
});

test('spec, normalize y validate aceptan QR estático y dinámico y no gastan cupo de 64 campos', () => {
  assert.ok(spec.TYPES.QR, 'QR existe en spec.TYPES');
  const dynamic64 = Array.from({ length: 64 }, (_, i) => ({ type: 'BOOL', x: (i % 8) * 30, y: Math.floor(i / 8) * 16 }));
  const proj = normalizeProject({
    pages: ['Principal'],
    fields: [
      ...dynamic64,
      { type: 'QR', x: 10, y: 10, preset: 'WIFI' },
      { type: 'QR', x: 130, y: 10, sourceMode: 'VARIABLE', text: 'http://192.168.1.50/ok', scale: 2 }
    ]
  });
  const qrs = proj.fields.filter((f) => f.type === 'QR');
  assert.strictEqual(qrs.length, 2);
  assert.strictEqual(qrs[0].text, qrCore.PRESETS.WIFI.defaultText);
  assert.strictEqual(qrs[1].sourceMode, 'VARIABLE');
  assert.ok(qrs[1].variable, 'QR dinámico genera variable');
  const rep = validateProject(proj);
  assert.strictEqual(rep.errors.length, 0, `errores inesperados: ${JSON.stringify(rep.errors)}`);
});

if (failures) {
  console.log(`\n${failures} prueba(s) fallaron.`);
  process.exit(1);
}
console.log('\nTodas las pruebas de QR core pasaron.');
