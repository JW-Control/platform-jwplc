// tools/jwplc-hmi-designer/tests/trend-inspector.test.cjs
// Prueba unitaria para el inspector visual de Gráfica de Tendencia (TREND).
'use strict';

const assert = require('assert');

// Mock simple de DOM para Node.js
class MockElement {
  constructor(tagName = 'div') {
    this.tagName = tagName.toUpperCase();
    this.id = '';
    this.className = '';
    this.classList = {
      _classes: new Set(),
      add: (c) => this.classList._classes.add(c),
      remove: (c) => this.classList._classes.delete(c),
      toggle: (c, force) => {
        if (force === undefined) {
          if (this.classList._classes.has(c)) this.classList._classes.delete(c);
          else this.classList._classes.add(c);
        } else if (force) {
          this.classList._classes.add(c);
        } else {
          this.classList._classes.delete(c);
        }
      },
      contains: (c) => this.classList._classes.has(c)
    };
    this.children = [];
    this.dataset = {};
    this.style = {};
    this.hidden = false;
    this.value = '';
    this.textContent = '';
    this._listeners = {};
  }
  appendChild(child) {
    this.children.push(child);
    return child;
  }
  querySelector(sel) {
    return this._findBySelector(sel);
  }
  querySelectorAll(sel) {
    const results = [];
    this._findAllBySelector(sel, results);
    return results;
  }
  _findBySelector(sel) {
    const list = [];
    this._findAllBySelector(sel, list);
    return list[0] || null;
  }
  _findAllBySelector(sel, results) {
    if (sel.startsWith('#') && this.id === sel.slice(1)) results.push(this);
    if (sel.startsWith('.') && this.classList.contains(sel.slice(1))) results.push(this);
    if (sel.startsWith('[data-') && sel.endsWith(']')) {
      const attr = sel.slice(1, -1);
      const [key, val] = attr.split('=');
      const dataKey = key.replace(/^data-/, '').replace(/-([a-z])/g, (_, c) => c.toUpperCase());
      if (val) {
        const cleanVal = val.replace(/['"]/g, '');
        if (this.dataset[dataKey] === cleanVal) results.push(this);
      } else if (this.dataset[dataKey] !== undefined) {
        results.push(this);
      }
    }
    for (const ch of this.children) {
      if (ch instanceof MockElement) ch._findAllBySelector(sel, results);
    }
  }
  addEventListener(evt, fn) {
    if (!this._listeners[evt]) this._listeners[evt] = [];
    this._listeners[evt].push(fn);
  }
  dispatchEvent(event) {
    const list = this._listeners[event.type] || [];
    list.forEach((fn) => fn(event));
  }
}

global.document = {
  getElementById: (id) => global._mockElements?.[id] || null,
  createElement: (tag) => new MockElement(tag),
  head: new MockElement('head'),
  body: new MockElement('body'),
  querySelectorAll: () => []
};

global.window = {
  document: global.document,
  addEventListener: () => {},
  dispatchEvent: () => {},
  CustomEvent: class CustomEvent { constructor(t, d) { this.type = t; this.detail = d?.detail; } }
};

console.log('=== trend-inspector.test.cjs — Inspector de Tendencia ===');

// Cargar trend core
const trendCore = require('../poc/designer-trend-core.js');
global.JWPLCTrendCore = trendCore;
global.window.JWPLCTrendCore = trendCore;

let trendModule;
try {
  trendModule = require('../poc/designer-trend.js');
} catch (e) {
  trendModule = null;
}

assert(trendModule !== null, 'El módulo designer-trend.js debe existir');
assert(typeof trendModule.applyPreset === 'function', 'applyPreset debe ser una función');
assert(typeof trendModule.frequencyToMs === 'function', 'frequencyToMs debe ser una función');
assert(typeof trendModule.msToFrequencyLabel === 'function', 'msToFrequencyLabel debe ser una función');

// 1. Presets de tamaño
const field = { width: 100, height: 50, x: 0, y: 0 };
trendModule.applyPreset(field, 'fullscreen');
assert.strictEqual(field.width, 300, 'Preset fullscreen debe tener ancho 300');
assert.strictEqual(field.height, 140, 'Preset fullscreen debe tener alto 140');
assert.strictEqual(field.x, 10, 'Preset fullscreen debe tener x 10');
assert.strictEqual(field.y, 15, 'Preset fullscreen debe tener y 15');
console.log('  PASS applyPreset(fullscreen) aplica 300x140 en (10, 15)');

trendModule.applyPreset(field, 'compact');
assert.strictEqual(field.width, 160, 'Preset compact debe tener ancho 160');
assert.strictEqual(field.height, 80, 'Preset compact debe tener alto 80');
console.log('  PASS applyPreset(compact) aplica 160x80');

// 2. Mapeo de frecuencias
assert.strictEqual(trendModule.frequencyToMs('10Hz'), 100, '10 Hz = 100 ms');
assert.strictEqual(trendModule.frequencyToMs('5Hz'), 200, '5 Hz = 200 ms');
assert.strictEqual(trendModule.frequencyToMs('1Hz'), 1000, '1 Hz = 1000 ms');
assert.strictEqual(trendModule.frequencyToMs('60s'), 60000, '60 s = 60000 ms');
console.log('  PASS frequencyToMs mapea strings a milisegundos');

assert.strictEqual(trendModule.msToFrequencyLabel(100), '10 Hz', '100 ms = 10 Hz');
assert.strictEqual(trendModule.msToFrequencyLabel(1000), '1 Hz', '1000 ms = 1 Hz');
assert.strictEqual(trendModule.msToFrequencyLabel(5000), '5 s', '5000 ms = 5 s');
console.log('  PASS msToFrequencyLabel formatea etiquetas legibles');

// 3. Estimación de RAM
const ramBytes = trendModule.estimateRamUsage(150);
assert.strictEqual(ramBytes, 600, '150 puntos float deben ser 600 bytes');
console.log('  PASS estimateRamUsage calcula 4 bytes por muestra');

console.log('\nResultado: TODAS PASARON');
