#!/usr/bin/env node
// Test unitario para el esquema y validación del nuevo tipo TREND en cli/lib
'use strict';

const spec = require('../cli/lib/spec.cjs');
const { normalizeProject } = require('../cli/lib/normalize.cjs');
const { validateProject, cppType } = require('../cli/lib/validate.cjs');

let failures = 0;
function check(condition, message) {
  console.log(`  ${condition ? 'PASS' : 'FAIL'} ${message}`);
  if (!condition) failures += 1;
}

console.log('=== trend-schema.test.cjs — Validación del tipo TREND ===');

// 1. spec.cjs
check(Boolean(spec.TYPES.TREND), 'spec.TYPES.TREND existe');
check(spec.FIELD_TYPES && spec.FIELD_TYPES.includes('TREND'), 'spec.FIELD_TYPES incluye TREND');

if (spec.TYPES.TREND) {
  const props = spec.TYPES.TREND.props;
  check(props.sampleIntervalMs && props.sampleIntervalMs.default === 1000, 'sampleIntervalMs default 1000 ms');
  check(props.scaleMode && props.scaleMode.default === 'AUTO', 'scaleMode default AUTO');
  check(props.lineWidth && props.lineWidth.default === 2, 'lineWidth default 2');
  check(props.enableSD !== undefined && props.enableSD.default === false, 'enableSD default false');
}

// 2. Normalización
const rawProject = {
  projectName: 'TestTrend',
  pages: [{ id: 0, name: 'Main' }],
  fields: [
    { type: 'TREND', x: 20, y: 30 }
  ]
};

const norm = normalizeProject(rawProject);
const trend = norm.fields[0];
check(trend && trend.type === 'TREND', 'Normaliza tipo TREND');
check(trend.width === 200, 'Asigna width por defecto (200)');
check(trend.height === 80, 'Asigna height por defecto (80)');
check(trend.sampleIntervalMs === 1000, 'Asigna sampleIntervalMs por defecto (1000)');
check(trend.scaleMode === 'AUTO', 'Asigna scaleMode AUTO');
check(trend.lineWidth === 2, 'Asigna lineWidth 2');
check(cppType(trend) === 'float', 'cppType de TREND es float');

// 3. Validación
const validReport = validateProject(norm);
check(validReport.errors.length === 0, `Proyecto válido sin errores (encontrados ${validReport.errors.length})`);

// 4. Caso erróneo en validación (maxValue <= minValue en modo MANUAL)
const badProject = normalizeProject({
  projectName: 'BadTrend',
  pages: [{ id: 0, name: 'Main' }],
  fields: [
    { type: 'TREND', x: 10, y: 10, scaleMode: 'MANUAL', minValue: 100, maxValue: 50 }
  ]
});
const badReport = validateProject(badProject);
check(badReport.errors.some(e => e.path.includes('maxValue')), 'Detecta error si maxValue <= minValue en modo MANUAL');

console.log(`\nResultado: ${failures === 0 ? 'TODAS PASARON' : `${failures} FALLARON`}`);
process.exit(failures === 0 ? 0 : 1);
