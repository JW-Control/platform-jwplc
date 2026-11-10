/**
 * layout-resizer.test.cjs — Verificación de splitters interactivos de paneles y barra inferior
 */
'use strict';

const fs = require('fs');
const path = require('path');
const assert = require('assert');

console.log('=== layout-resizer.test.cjs — Redimensionamiento interactivo de paneles y barra inferior ===');

let passed = 0;
function test(name, fn) {
  try {
    fn();
    console.log(`  PASS ${name}`);
    passed++;
  } catch (err) {
    console.error(`  FAIL ${name}`);
    console.error(err);
    process.exitCode = 1;
  }
}

const mockStorage = new Map();
global.localStorage = {
  getItem: (key) => (mockStorage.has(key) ? mockStorage.get(key) : null),
  setItem: (key, val) => mockStorage.set(key, String(val)),
  removeItem: (key) => mockStorage.delete(key),
  clear: () => mockStorage.clear()
};

const Resizer = require('../poc/designer-layout-resizer.js');

test('Resizer exporta API pública y constantes requeridas', () => {
  assert(typeof Resizer === 'object', 'Resizer debe ser un objeto');
  assert(typeof Resizer.clamp === 'function', 'clamp debe ser función');
  assert(typeof Resizer.getSizes === 'function', 'getSizes debe ser función');
  assert(typeof Resizer.setSizes === 'function', 'setSizes debe ser función');
  assert(typeof Resizer.reset === 'function', 'reset debe ser función');
  assert(Resizer.LIMITS && Resizer.LIMITS.minLeft >= 180, 'LIMITS.minLeft >= 180');
  assert(Resizer.LIMITS.minRight >= 240, 'LIMITS.minRight >= 240');
  assert(Resizer.LIMITS.minBottom >= 50, 'LIMITS.minBottom >= 50');
});

test('clamp() restringe valores fuera de los límites superior e inferior', () => {
  assert.strictEqual(Resizer.clamp(100, 180, 500), 180, 'clamp por debajo debe devolver min');
  assert.strictEqual(Resizer.clamp(600, 180, 500), 500, 'clamp por encima debe devolver max');
  assert.strictEqual(Resizer.clamp(300, 180, 500), 300, 'clamp dentro debe devolver el valor');
  assert.strictEqual(Resizer.clamp(NaN, 180, 500), 180, 'clamp con NaN debe devolver min de forma segura');
});

test('Persistencia en localStorage y recuperación de medidas', () => {
  mockStorage.clear();
  const initial = Resizer.getSizes();
  assert.strictEqual(initial.left, Resizer.LIMITS.defaultLeft, 'Default left coincide');
  assert.strictEqual(initial.right, Resizer.LIMITS.defaultRight, 'Default right coincide');
  assert.strictEqual(initial.bottom, Resizer.LIMITS.defaultBottom, 'Default bottom coincide');

  // Guardar medidas personalizadas
  localStorage.setItem(Resizer.KEYS.left, '320');
  localStorage.setItem(Resizer.KEYS.right, '420');
  localStorage.setItem(Resizer.KEYS.bottom, '220');

  const loaded = Resizer.getSizes();
  assert.strictEqual(loaded.left, 320, 'Left personalizado cargado');
  assert.strictEqual(loaded.right, 420, 'Right personalizado cargado');
  assert.strictEqual(loaded.bottom, 220, 'Bottom personalizado cargado');
});

test('reset() elimina claves selectivas o totales de localStorage', () => {
  localStorage.setItem(Resizer.KEYS.left, '350');
  localStorage.setItem(Resizer.KEYS.right, '450');
  localStorage.setItem(Resizer.KEYS.bottom, '250');

  Resizer.reset('left');
  assert.strictEqual(localStorage.getItem(Resizer.KEYS.left), null, 'Left debe haberse eliminado');
  assert.strictEqual(localStorage.getItem(Resizer.KEYS.right), '450', 'Right debe mantenerse');

  Resizer.reset();
  assert.strictEqual(localStorage.getItem(Resizer.KEYS.right), null, 'Right debe haberse eliminado');
  assert.strictEqual(localStorage.getItem(Resizer.KEYS.bottom), null, 'Bottom debe haberse eliminado');
});

test('index.html contiene los elementos splitters y el script registrado', () => {
  const htmlPath = path.join(__dirname, '../poc/index.html');
  const html = fs.readFileSync(htmlPath, 'utf8');

  assert(html.includes('id="resizerLeft"'), 'Debe existir #resizerLeft en index.html');
  assert(html.includes('id="resizerRight"'), 'Debe existir #resizerRight en index.html');
  assert(html.includes('id="resizerBottom"'), 'Debe existir #resizerBottom en index.html');
  assert(html.includes('designer-layout-resizer.js'), 'Debe incluir designer-layout-resizer.js');
});

test('styles.css y desktop-responsive.css configuran las variables CSS', () => {
  const stylesPath = path.join(__dirname, '../poc/styles.css');
  const styles = fs.readFileSync(stylesPath, 'utf8');

  assert(styles.includes('--left-panel-width'), 'styles.css debe usar --left-panel-width');
  assert(styles.includes('--right-panel-width'), 'styles.css debe usar --right-panel-width');
  assert(styles.includes('--bottom-panel-height'), 'styles.css debe usar --bottom-panel-height');
  assert(styles.includes('.layout-resizer'), 'styles.css debe definir .layout-resizer');
  assert(styles.includes('col-resize'), 'styles.css debe definir cursor col-resize');
  assert(styles.includes('row-resize'), 'styles.css debe definir cursor row-resize');

  const respPath = path.join(__dirname, '../poc/desktop-responsive.css');
  const resp = fs.readFileSync(respPath, 'utf8');
  assert(resp.includes('--left-panel-width'), 'desktop-responsive.css debe usar --left-panel-width');
  assert(resp.includes('--right-panel-width'), 'desktop-responsive.css debe usar --right-panel-width');
  assert(resp.includes('--bottom-panel-height'), 'desktop-responsive.css debe usar --bottom-panel-height');
});

console.log(`\nResultado: ${passed} pruebas pasadas.`);
if (process.exitCode) {
  process.exit(process.exitCode);
}
