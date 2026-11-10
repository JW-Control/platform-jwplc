/**
 * per-type-counter.test.cjs — Verificación del contador de secuencia por tipo de objeto
 */
'use strict';

const assert = require('assert');

console.log('=== per-type-counter.test.cjs — Contador de objetos independiente por tipo ===');

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

// Simulamos la lógica de nextTypeSerial
function createTypeCounter(fieldsRef) {
  return function nextTypeSerial(type) {
    const used = new Set();
    let unnumberedCount = 0;
    (fieldsRef.fields || []).forEach((f) => {
      const matchType = (type === 'CIRCULAR')
        ? (f.type === 'BAR' && f.barPreset === 'CIRCULAR')
        : (type === 'BAR')
          ? (f.type === 'BAR' && f.barPreset !== 'CIRCULAR')
          : (f.type === type);

      if (matchType) {
        const candidates = [
          f.name && String(f.name).match(/(\d+)\s*$/),
          f.id && String(f.id).match(/_(\d+)$/),
          f.label && String(f.label).match(/(\d+)\s*$/),
          f.variable && String(f.variable).match(/(\d+)$/)
        ];
        let found = false;
        for (const m of candidates) {
          if (m) {
            const num = parseInt(m[1], 10);
            if (num > 0) {
              used.add(num);
              found = true;
            }
          }
        }
        if (!found) unnumberedCount++;
      }
    });
    for (let i = 1; i <= unnumberedCount; i++) {
      used.add(i);
    }
    let n = 1;
    while (used.has(n)) n += 1;
    return n;
  };
}

test('Proyecto vacío: cada tipo inicia en 1', () => {
  const state = { fields: [] };
  const nextTypeSerial = createTypeCounter(state);

  assert.strictEqual(nextTypeSerial('TEXT'), 1);
  assert.strictEqual(nextTypeSerial('RECT'), 1);
  assert.strictEqual(nextTypeSerial('ELLIPSE'), 1);
  assert.strictEqual(nextTypeSerial('LINE'), 1);
  assert.strictEqual(nextTypeSerial('VALUE'), 1);
  assert.strictEqual(nextTypeSerial('BOOL'), 1);
  assert.strictEqual(nextTypeSerial('BAR'), 1);
});

test('Secuencia intercalada Texto -> Rect -> Elipse -> Rect mantiene contadores independientes', () => {
  const state = { fields: [] };
  const nextTypeSerial = createTypeCounter(state);

  // 1. Crear primer Texto
  const text1Serial = nextTypeSerial('TEXT');
  assert.strictEqual(text1Serial, 1, 'Primer texto debe ser 1');
  state.fields.push({ type: 'TEXT', name: `Texto ${text1Serial}`, id: `FIELD_TEXT_${text1Serial}` });

  // 2. Crear primer Rectángulo
  const rect1Serial = nextTypeSerial('RECT');
  assert.strictEqual(rect1Serial, 1, 'Primer rectángulo debe ser 1 (no 2)');
  state.fields.push({ type: 'RECT', name: `Rect ${rect1Serial}`, id: `FIELD_RECT_${rect1Serial}` });

  // 3. Crear primera Elipse
  const elipse1Serial = nextTypeSerial('ELLIPSE');
  assert.strictEqual(elipse1Serial, 1, 'Primera elipse debe ser 1 (no 3)');
  state.fields.push({ type: 'ELLIPSE', name: `Elipse ${elipse1Serial}`, id: `FIELD_ELLIPSE_${elipse1Serial}` });

  // 4. Crear segundo Rectángulo
  const rect2Serial = nextTypeSerial('RECT');
  assert.strictEqual(rect2Serial, 2, 'Segundo rectángulo debe ser 2 (no 4)');
  state.fields.push({ type: 'RECT', name: `Rect ${rect2Serial}`, id: `FIELD_RECT_${rect2Serial}` });

  // 5. Crear segunda Elipse
  const elipse2Serial = nextTypeSerial('ELLIPSE');
  assert.strictEqual(elipse2Serial, 2, 'Segunda elipse debe ser 2');
  state.fields.push({ type: 'ELLIPSE', name: `Elipse ${elipse2Serial}`, id: `FIELD_ELLIPSE_${elipse2Serial}` });

  // 6. Crear segundo Texto
  const text2Serial = nextTypeSerial('TEXT');
  assert.strictEqual(text2Serial, 2, 'Segundo texto debe ser 2');
});

test('Campo inicial sin número (ej. Estado) cuenta como posición 1 de TEXT', () => {
  const state = {
    fields: [
      { type: 'TEXT', name: 'Estado', id: 'FIELD_STATUS', label: 'Estado', variable: 'estadoTexto' }
    ]
  };
  const nextTypeSerial = createTypeCounter(state);

  assert.strictEqual(nextTypeSerial('TEXT'), 2, 'Siguiente texto debe ser 2');
  assert.strictEqual(nextTypeSerial('RECT'), 1, 'Rect sigue siendo 1');
});

test('Eliminar un objeto libera su número para reutilización limpia', () => {
  const state = {
    fields: [
      { type: 'RECT', name: 'Rect 1', id: 'FIELD_RECT_1' },
      { type: 'RECT', name: 'Rect 2', id: 'FIELD_RECT_2' },
      { type: 'RECT', name: 'Rect 3', id: 'FIELD_RECT_3' }
    ]
  };
  const nextTypeSerial = createTypeCounter(state);
  assert.strictEqual(nextTypeSerial('RECT'), 4, 'Siguiente normal es 4');

  // Eliminamos Rect 2
  state.fields = state.fields.filter(f => f.name !== 'Rect 2');
  assert.strictEqual(nextTypeSerial('RECT'), 2, 'Debe reutilizar el hueco libre 2');
});

test('app.js integra nextTypeSerial en creadores de objetos y exporta la API', () => {
  const fs = require('fs');
  const path = require('path');
  const appJs = fs.readFileSync(path.resolve(__dirname, '..', 'poc', 'app.js'), 'utf8');

  assert.ok(appJs.includes('function nextTypeSerial(type)'), 'app.js define nextTypeSerial');
  assert.ok(appJs.includes('SHAPE_TYPE_LABELS = {'), 'app.js define etiquetas en español para figuras');
  assert.ok(appJs.includes("nextTypeSerial: (type) => nextTypeSerial(type)"), 'app.js exporta nextTypeSerial');

  // Verificamos que los métodos de creación usen nextTypeSerial
  const creators = [
    'addShapeField',
    'addRawTextField',
    'addTextField',
    'addEventLogField',
    'addMenuField',
    'addIconField',
    'addValueField'
  ];
  for (const fn of creators) {
    const rx = new RegExp(`function ${fn}[\\s\\S]*?nextTypeSerial\\(`);
    assert.ok(rx.test(appJs), `${fn} debe invocar nextTypeSerial`);
  }
});

console.log(`\nResultado: ${passed} pruebas pasadas.`);
if (process.exitCode) process.exit(process.exitCode);
