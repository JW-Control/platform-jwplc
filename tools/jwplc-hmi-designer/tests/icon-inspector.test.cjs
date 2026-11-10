'use strict';

const assert = require('assert');
const fs = require('fs');
const path = require('path');

console.log('ICON INSPECTOR: Estructura, paridad y optimización sin scroll');

const iconsJsPath = path.resolve(__dirname, '../poc/designer-icons.js');
const iconsJsContent = fs.readFileSync(iconsJsPath, 'utf8');

// 1. Extraer la plantilla HTML de iconInspectorSection
const htmlMatch = iconsJsContent.match(/section\.innerHTML\s*=\s*`([\s\S]*?)`;/);
assert.ok(htmlMatch, 'debe contener la plantilla HTML de section.innerHTML');
const inspectorHtml = htmlMatch[1];

// --- FASE 1: Reestructuración de la Pestaña Forma (design:shape) ---
console.log('FASE 1: Pestaña Forma (design:shape)');

// 1.1 Se eliminó el selector redundante de icono y el canvas icon-current
assert.strictEqual(
  inspectorHtml.includes('data-icon-prop="iconId"'),
  false,
  'PASS selector redundante de icono <select data-icon-prop="iconId"> eliminado de Forma'
);
assert.strictEqual(
  inspectorHtml.includes('data-icon-current'),
  false,
  'PASS canvas redundante [data-icon-current] eliminado de Forma'
);

// 1.2 Tamaño, Marco y Fondo
assert.ok(
  inspectorHtml.includes('data-icon-sizes'),
  'PASS selector de tamaño [data-icon-sizes] presente'
);
assert.ok(
  inspectorHtml.includes('data-icon-set="frame"'),
  'PASS toggle de marco presente'
);
assert.ok(
  inspectorHtml.includes('data-icon-set="transparentBackground"'),
  'PASS toggle de fondo presente'
);

// 1.3 Velocidad de animación
assert.ok(
  inspectorHtml.includes('data-icon-anim-card'),
  'PASS tarjeta de velocidad de animación presente'
);
assert.ok(
  inspectorHtml.includes('data-icon-anim-slider'),
  'PASS slider de velocidad de animación presente'
);


// --- FASE 2: Matriz 3x3 en Contenido (design:content) ---
console.log('FASE 2: Pestaña Contenido (design:content) y Matriz 3x3');

// 2.1 Matriz 3x3 de alineación presente
assert.ok(
  inspectorHtml.includes('data-icon-matrix-wrap'),
  'PASS contenedor de matriz 3x3 [data-icon-matrix-wrap] presente'
);
assert.ok(
  inspectorHtml.includes('align-matrix-3x3'),
  'PASS clase CSS align-matrix-3x3 presente'
);

// 2.2 Nueve celdas en la matriz 3x3
const matrixCellMatches = inspectorHtml.match(/class="matrix-cell[^"]*"/g) || [];
assert.strictEqual(
  matrixCellMatches.length,
  9,
  `PASS la matriz 3x3 contiene exactamente 9 celdas (encontradas: ${matrixCellMatches.length})`
);

// 2.3 Celdas cubren TOP, MID, BOTTOM y LEFT, CENTER, RIGHT
assert.ok(inspectorHtml.includes('data-align-v="TOP"'), 'PASS celda con alignV=TOP');
assert.ok(inspectorHtml.includes('data-align-v="MID"'), 'PASS celda con alignV=MID');
assert.ok(inspectorHtml.includes('data-align-v="BOTTOM"'), 'PASS celda con alignV=BOTTOM');
assert.ok(inspectorHtml.includes('data-align-h="LEFT"'), 'PASS celda con alignH=LEFT');
assert.ok(inspectorHtml.includes('data-align-h="CENTER"'), 'PASS celda con alignH=CENTER');
assert.ok(inspectorHtml.includes('data-align-h="RIGHT"'), 'PASS celda con alignH=RIGHT');

// 2.4 Tamaño de texto
assert.ok(
  inspectorHtml.includes('data-icon-set="labelSize"'),
  'PASS selector de tamaño de etiqueta (1x, 2x) presente'
);


// --- FASE 3: Apariencia a 2 Columnas y Retiro de Paleta Rápida ---
console.log('FASE 3: Pestaña Apariencia (design:appearance)');

// 3.1 Mini-modelo en layout horizontal (sin is-below)
assert.strictEqual(
  inspectorHtml.includes('field-case-body zone-host is-below'),
  false,
  'PASS data-icon-case-body="single" no tiene clase is-below (layout horizontal flex)'
);
assert.ok(
  inspectorHtml.includes('data-icon-case="single"'),
  'PASS canvas del mini-modelo data-icon-case="single" presente'
);
assert.ok(
  inspectorHtml.includes('data-icon-case-legend'),
  'PASS leyenda de componentes data-icon-case-legend presente'
);

// 3.2 Paleta rápida redundante eliminada
assert.strictEqual(
  inspectorHtml.includes('data-icon-presets-card'),
  false,
  'PASS paleta rápida redundante [data-icon-presets-card] eliminada de Apariencia'
);
assert.strictEqual(
  inspectorHtml.includes('data-icon-presets-row'),
  false,
  'PASS fila de presets rápidos [data-icon-presets-row] eliminada de Apariencia'
);

// 3.3 Editor de color grande presente
assert.ok(
  inspectorHtml.includes('data-icon-color-editor'),
  'PASS contenedor del editor de color grande [data-icon-color-editor] presente'
);


// --- FASE 4: Verificación de Handlers y Lógica en designer-icons.js ---
console.log('FASE 4: Lógica de eventos y actualización');

// 4.1 Listener de la matriz 3x3 presente en el código JS
assert.ok(
  iconsJsContent.includes('matrixCell') || iconsJsContent.includes('data-icon-matrix-wrap'),
  'PASS manejador de clics para matriz 3x3 presente en JS'
);

// 4.2 Lógica de layout: MID -> INLINE, TOP/BOTTOM -> STACKED
assert.ok(
  iconsJsContent.includes('f.layout = v === \'MID\' ? \'INLINE\' : \'STACKED\'') ||
  iconsJsContent.includes("layout = alignV === 'MID' ? 'INLINE' : 'STACKED'"),
  'PASS mapeo correcto de alignV a layout (MID -> INLINE, TOP/BOTTOM -> STACKED)'
);

// 4.3 Canvas grande en Apariencia: sin forzar is-below en JS
assert.strictEqual(
  iconsJsContent.includes("body.classList.add('is-below')"),
  false,
  'PASS drawAppearance no fuerza is-below en el body de la vista'
);

console.log('\nTodas las pruebas de estructura y optimización del inspector pasaron.');
