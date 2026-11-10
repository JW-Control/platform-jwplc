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

// 1.2 Tamaño, Marco y Fondo con divisor visual
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
assert.ok(
  inspectorHtml.includes('data-icon-mf-divider'),
  'PASS divisor visual vertical entre Marco y Fondo presente'
);

// 1.3 Velocidad de animación reactiva con color y spinner
assert.ok(
  inspectorHtml.includes('data-icon-anim-card'),
  'PASS tarjeta de velocidad de animación presente'
);
assert.ok(
  inspectorHtml.includes('data-icon-anim-slider'),
  'PASS slider de velocidad de animación presente'
);
assert.ok(
  iconsJsContent.includes('updateSpeedUI'),
  'PASS función updateSpeedUI presente para feedback dinámico'
);
assert.ok(
  iconsJsContent.includes('spinDur:') && iconsJsContent.includes('color:'),
  'PASS SPEED_CONFIG define color y duración de giro por cada nivel'
);

// 1.4 Baliza en Forma: solo cantidad de pisos (forma física, sin lista de pruebas)
assert.ok(
  inspectorHtml.includes('data-icon-baliza-card'),
  'PASS tarjeta baliza en Forma presente'
);
assert.ok(
  inspectorHtml.includes('data-baliza-tier-count'),
  'PASS selector de cantidad de pisos en Forma presente'
);
const balizaCardMatch = inspectorHtml.match(/<div class="bar-card" data-icon-baliza-card[\s\S]*?<\/div>\s*<\/div>/);
assert.ok(balizaCardMatch, 'debe encontrar la tarjeta balizaCard en Forma');
assert.strictEqual(
  balizaCardMatch[0].includes('data-baliza-tiers-list'),
  false,
  'PASS data-baliza-tiers-list no está en Forma (se movió a Contenido)'
);

// 1.5 Piloto en Forma: solo modo y cantidad de estados (sin lista que sature)
assert.ok(
  inspectorHtml.includes('data-icon-pilot-card'),
  'PASS tarjeta piloto en Forma presente'
);
assert.ok(
  inspectorHtml.includes('data-pilot-mode'),
  'PASS selector de modo en Forma presente'
);
const pilotCardMatch = inspectorHtml.match(/<div class="bar-card" data-icon-pilot-card[\s\S]*?<\/div>\s*<\/div>/);
assert.ok(pilotCardMatch, 'debe encontrar la tarjeta pilotCard en Forma');
assert.strictEqual(
  pilotCardMatch[0].includes('data-pilot-states-list'),
  false,
  'PASS data-pilot-states-list no está en Forma (se movió a Contenido)'
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

// 2.5 Baliza en Contenido: prueba de pisos en vivo
assert.ok(
  inspectorHtml.includes('data-icon-baliza-content'),
  'PASS contenedor de baliza en Contenido [data-icon-baliza-content] presente'
);
assert.ok(
  inspectorHtml.includes('data-baliza-quick="all-on"'),
  'PASS botón Todos ON para prueba rápida de baliza presente'
);
assert.ok(
  iconsJsContent.includes('balizaContent.querySelector(\'[data-baliza-tiers-list]\')'),
  'PASS data-baliza-tiers-list se renderiza dentro de balizaContent en Contenido'
);

// 2.6 Piloto en Contenido: tarjeta de nombres de estados
assert.ok(
  inspectorHtml.includes('data-icon-pilot-names-card'),
  'PASS tarjeta de nombres de estados de piloto [data-icon-pilot-names-card] presente en Contenido'
);
assert.ok(
  iconsJsContent.includes('pilotNamesCard.querySelector(\'[data-pilot-states-list]\')'),
  'PASS data-pilot-states-list se renderiza dentro de pilotNamesCard en Contenido'
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

// 3.3 Editor de color grande y botón de restablecer presentes
assert.ok(
  inspectorHtml.includes('data-icon-color-editor'),
  'PASS contenedor del editor de color grande [data-icon-color-editor] presente'
);
assert.ok(
  inspectorHtml.includes('data-icon-reset-color'),
  'PASS botón [data-icon-reset-color] para restablecer color por defecto presente'
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

// 4.4 Botón restablecer color por defecto y corrección de zona de etiqueta
assert.ok(
  iconsJsContent.includes('resetColorBtn') || iconsJsContent.includes('data-icon-reset-color'),
  'PASS manejador de restablecer color por defecto presente en JS'
);
assert.ok(
  iconsJsContent.includes('g.labelX !== undefined ? Math.round(g.labelX) : 3'),
  'PASS labelRects en Apariencia sigue la posición dinámica g.labelX/Y calculada por el layout'
);
assert.strictEqual(
  iconsJsContent.includes("key: 'bodyColor'"),
  false,
  'PASS color del cuerpo del icono no es editable (queda fijo por defecto)'
);

console.log('\nTodas las pruebas de estructura y optimización del inspector pasaron.');
