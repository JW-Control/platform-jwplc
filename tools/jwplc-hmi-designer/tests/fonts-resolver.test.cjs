'use strict';

const assert = require('assert');
const path = require('path');
const fonts = require('../poc/designer-fonts.js');

let pass = 0;
function test(name, fn) {
  try {
    fn();
    console.log(`  PASS ${name}`);
    pass++;
  } catch (err) {
    console.error(`  FAIL ${name}: ${err.message}`);
    throw err;
  }
}

console.log('=== fonts-resolver.test.cjs — Resolución de familias, variantes y alturas');

test('fontFamilyOf identifica correctamente las familias', () => {
  assert.strictEqual(fonts.fontFamilyOf('GLCD'), 'GLCD');
  assert.strictEqual(fonts.fontFamilyOf(''), 'GLCD');
  assert.strictEqual(fonts.fontFamilyOf(null), 'GLCD');
  assert.strictEqual(fonts.fontFamilyOf('FONT_16'), 'BITMAP');
  assert.strictEqual(fonts.fontFamilyOf('FONT_26'), 'BITMAP');
  assert.strictEqual(fonts.fontFamilyOf('FONT_7SEG_48'), '7SEG');
  assert.strictEqual(fonts.fontFamilyOf('SANS_9'), 'SANS');
  assert.strictEqual(fonts.fontFamilyOf('SANS_BOLD_12'), 'SANS');
  assert.strictEqual(fonts.fontFamilyOf('MONO_9'), 'MONO');
  assert.strictEqual(fonts.fontFamilyOf('SERIF_12'), 'SERIF');
});

test('fontSizeVariantOf extrae la variante o escala', () => {
  assert.strictEqual(fonts.fontSizeVariantOf('GLCD', 1), 1);
  assert.strictEqual(fonts.fontSizeVariantOf('GLCD', 2), 2);
  assert.strictEqual(fonts.fontSizeVariantOf('FONT_16', 1), '16');
  assert.strictEqual(fonts.fontSizeVariantOf('FONT_26', 1), '26');
  assert.strictEqual(fonts.fontSizeVariantOf('FONT_7SEG_48', 1), '48');
  assert.strictEqual(fonts.fontSizeVariantOf('SANS_9', 1), '9');
  assert.strictEqual(fonts.fontSizeVariantOf('SANS_BOLD_12', 1), '12');
  assert.strictEqual(fonts.fontSizeVariantOf('SANS_18', 1), '18');
});

test('isBold detecta variantes en negrita', () => {
  assert.strictEqual(fonts.isBold('SANS_BOLD_12'), true);
  assert.strictEqual(fonts.isBold('SANS_12'), false);
  assert.strictEqual(fonts.isBold('GLCD'), false);
  assert.strictEqual(fonts.isBold('FONT_16'), false);
  assert.strictEqual(fonts.isBold('FONT_7SEG_48'), false);
});

test('fontIdFrom compone el ID correcto', () => {
  assert.strictEqual(fonts.fontIdFrom('GLCD', 2), 'GLCD');
  assert.strictEqual(fonts.fontIdFrom('BITMAP', '16'), 'FONT_16');
  assert.strictEqual(fonts.fontIdFrom('BITMAP', '26'), 'FONT_26');
  assert.strictEqual(fonts.fontIdFrom('7SEG', '48'), 'FONT_7SEG_48');
  assert.strictEqual(fonts.fontIdFrom('SANS', '9', false), 'SANS_9');
  assert.strictEqual(fonts.fontIdFrom('SANS', '12', true), 'SANS_BOLD_12');
  assert.strictEqual(fonts.fontIdFrom('MONO', '9', false), 'MONO_9');
});

test('fontHeightPx calcula la altura real en píxeles', () => {
  assert.strictEqual(fonts.fontHeightPx('GLCD', 1), 8);
  assert.strictEqual(fonts.fontHeightPx('GLCD', 2), 16);
  assert.strictEqual(fonts.fontHeightPx('GLCD', 3), 24);
  assert.strictEqual(fonts.fontHeightPx('GLCD', 4), 32);
  assert.strictEqual(fonts.fontHeightPx('FONT_16', 1), 16);
  assert.strictEqual(fonts.fontHeightPx('FONT_26', 1), 26);
  assert.strictEqual(fonts.fontHeightPx('FONT_7SEG_48', 1), 48);
  assert.strictEqual(fonts.fontHeightPx('SANS_9', 1), 18);
  assert.strictEqual(fonts.fontHeightPx('SANS_12', 1), 24);
  assert.strictEqual(fonts.fontHeightPx('SANS_18', 1), 34);
});

console.log(`\nResultado: ${pass} pruebas pasadas.`);
