'use strict';

const assert = require('assert');
const fs = require('fs');
const path = require('path');

const POC = path.resolve(__dirname, '..', 'poc');
const html = fs.readFileSync(path.join(POC, 'index.html'), 'utf8');
const appJs = fs.readFileSync(path.join(POC, 'app.js'), 'utf8');
const codegenJs = fs.readFileSync(path.join(POC, 'designer-codegen.js'), 'utf8');

console.log('=== raw-text-inspector.test.cjs — Selector de fuentes y posicionamiento inicial RAW_TEXT');

function test(name, fn) {
  try {
    fn();
    console.log(`  PASS ${name}`);
  } catch (err) {
    console.error(`  FAIL ${name}: ${err.message}`);
    throw err;
  }
}

test('index.html integra Tipografía y Escala (#rawFontCtrl) en Texto RAW', () => {
  const start = html.indexOf('id="rawPaneForm"');
  const end = html.indexOf('id="rawPaneContent"');
  assert.ok(start !== -1 && end > start, 'Sección #rawPaneForm presente');
  const pane = html.slice(start, end);
  assert.ok(pane.includes('Tipografía y Escala'), 'Usa tarjeta Tipografía y Escala');
  assert.ok(pane.includes('id="rawFontCtrl"'), 'Contiene #rawFontCtrl');
  assert.ok(!pane.includes('id="rawScaleChipRow"'), 'No conserva el selector antiguo de escala separado');

  const contentStart = html.indexOf('id="rawPaneContent"');
  const appStart = html.indexOf('id="rawPaneAppearance"');
  const placeStart = html.indexOf('id="rawPanePlacement"');
  const dataStart = html.indexOf('id="rawPaneData"');
  assert.ok(contentStart !== -1 && appStart > contentStart && placeStart > appStart && dataStart > placeStart, 'Subpestañas RAW presentes');
  const contentPane = html.slice(contentStart, appStart);
  const placePane = html.slice(placeStart, dataStart);
  assert.ok(contentPane.includes('id="rawAlignMatrix3x3"'), 'La matriz 3×3 está en Contenido');
  assert.ok(contentPane.includes('class="align-matrix-3x3" id="rawAlignMatrix3x3"'), 'Usa la clase compacta align-matrix-3x3');
  assert.ok(!placePane.includes('id="rawAlignMatrix3x3"'), 'La matriz 3×3 ya no está en Ubicación');
});

test('app.js renderiza y maneja el selector unificado de fuentes para Texto RAW', () => {
  assert.ok(appJs.includes('function renderRawTypographyControl('), 'Define renderRawTypographyControl');
  assert.ok(appJs.includes('function applyRawTypography('), 'Define applyRawTypography');
  assert.ok(appJs.includes('data-raw-font-family'), 'Maneja chips de familia tipográfica');
  assert.ok(appJs.includes('data-raw-font-size'), 'Maneja chips de tamaño/variante');
  assert.ok(appJs.includes('data-raw-font-bold'), 'Maneja toggle Bold para Sans/Mono');
  assert.ok(appJs.includes("{ id: 'GLCD', label: 'Clásica 5×7' }"), 'Incluye Clásica 5×7');
  assert.ok(appJs.includes("{ id: 'BITMAP', label: 'Bitmap' }"), 'Incluye Bitmap');
  assert.ok(appJs.includes("{ id: 'SANS', label: 'Sans' }"), 'Incluye Sans');
  assert.ok(appJs.includes("{ id: 'MONO', label: 'Mono' }"), 'Incluye Mono');
});

test('addRawTextField inserta con tamaño regular (2×) y centrado aproximadamente', () => {
  const fnStart = appJs.indexOf('function addRawTextField(');
  const fnEnd = appJs.indexOf('function addTextField(', fnStart);
  assert.ok(fnStart !== -1 && fnEnd > fnStart, 'Define addRawTextField');
  const fn = appJs.slice(fnStart, fnEnd);
  assert.ok(/sizeVal\s*=\s*Number\(rawState\.size\)\s*\|\|\s*2/.test(fn), 'Usa escala por defecto 2× cuando no se cambia');
  assert.ok(fn.includes('Math.round((WIDTH - tb.width) / 2)'), 'Calcula centro horizontal');
  assert.ok(fn.includes('Math.round((HEIGHT - tb.height) / 2)'), 'Calcula centro vertical');
  assert.ok(fn.includes('placeNewField(field)'), 'Aplica límites de pantalla con placeNewField');
  assert.ok(/const rawState\s*=\s*\{[\s\S]*?x:\s*107[\s\S]*?y:\s*78[\s\S]*?size:\s*2/.test(appJs), 'rawState inicia centrado y en tamaño 2×');
});

test('designer-codegen.js genera setFont para Bitmap y FreeFonts en RAW_TEXT', () => {
  assert.ok(
    codegenJs.includes("if (font && font.kind !== 'glcd' && fonts?.isSupported?.(font))"),
    'RAW_TEXT soporta fuentes bitmap y FreeFonts soportadas en firmware'
  );
});

console.log('\nResultado: 4 pruebas pasadas.');
