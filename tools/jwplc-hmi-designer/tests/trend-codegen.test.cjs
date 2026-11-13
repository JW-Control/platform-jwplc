#!/usr/bin/env node
// Test unitario para la generación C++ de campos de tipo TREND en designer-codegen-core.js
'use strict';

const codegenCore = require('../poc/designer-codegen-core.js');

let failures = 0;
function check(condition, message) {
  console.log(`  ${condition ? 'PASS' : 'FAIL'} ${message}`);
  if (!condition) failures += 1;
}

console.log('=== trend-codegen.test.cjs — Codegen C++ para TREND ===');

check(typeof codegenCore.trendFieldBlock === 'function', 'trendFieldBlock existe en JWPLCHMICodegenCore');

const sampleField = {
  type: 'TREND',
  x: 10,
  y: 20,
  width: 250,
  height: 90,
  title: 'TEMP_HORNO',
  unit: '°C',
  variable: 'temp_horno',
  sampleIntervalMs: 500,
  visibleSamples: 120,
  scaleMode: 'AUTO',
  minValue: 0,
  maxValue: 120,
  decimals: 1,
  lineWidth: 2,
  showAxes: true,
  showTimeAxis: true,
  enableSD: true,
  sdIntervalMs: 1000,
  curveColor: 0xFD20,
  axesColor: 0x4208,
  page: 1
};

if (typeof codegenCore.trendFieldBlock === 'function') {
  const block = codegenCore.trendFieldBlock(sampleField, 'FIELD_TREND_1');
  check(/JWPLC_UITrendField/.test(block), 'Emite llamada a JWPLC_UITrendField');
  check(/JWPLC_UIRect\(10, 20, 250, 90\)/.test(block), 'Emite dimensiones rectangulares en JWPLC_UIRect');
  check(/"TEMP_HORNO"/.test(block), 'Emite título del campo');
  check(/JWPLC_UITrendConfig/.test(block), 'Emite bloque JWPLC_UITrendConfig');
  check(/500/.test(block), 'Emite sampleIntervalMs 500');
  check(/120/.test(block), 'Emite visibleSamples 120');
  check(/true/.test(block), 'Emite enableSD true');

  const varDecl = codegenCore.variableDeclaration(sampleField, 'temp_horno');
  check(varDecl === 'float temp_horno = 0.0f;', `Declara variable float: ${varDecl}`);

  const setter = codegenCore.setterCall(sampleField, 'FIELD_TREND_1', 'temp_horno');
  check(setter === 'JWPLC_Display.setTrend(FIELD_TREND_1, temp_horno);', `Emite llamada a setTrend: ${setter}`);

  const dispatch = codegenCore.fieldBlock(sampleField, 'FIELD_TREND_1');
  check(/JWPLC_UITrendField/.test(dispatch), 'fieldBlock despacha TREND correctamente');

  // Multicanal (2 y 3 canales) + umbrales
  const multiField = {
    ...sampleField,
    ch2Enabled: true,
    ch2Variable: 'sp_horno',
    ch2Color: 0x07FF,
    ch3Enabled: true,
    ch3Variable: 'amb_horno',
    ch3Color: 0x07E0,
    thr1Enabled: true,
    thr1Value: 95.5,
    thr1Color: 0xF800,
    thr2Enabled: true,
    thr2Value: 18,
    thr2Color: 0xFFE0
  };
  const multiBlock = codegenCore.trendFieldBlock(multiField, 'FIELD_TREND_1');
  check(/,\s*3,\s*0x07FF,\s*0x07E0,\s*2,\s*95\.5f,\s*0xF800,\s*18(\.0)?f,\s*0xFFE0\)/.test(multiBlock), 'trendFieldBlock emite channelCount=3, thresholdCount=2 y sus parámetros');

  const multiDecl = codegenCore.variableDeclaration(multiField, 'temp_horno');
  check(multiDecl === 'float temp_horno = 0.0f;\nfloat sp_horno = 0.0f;\nfloat amb_horno = 0.0f;', 'variableDeclaration declara los 3 canales float');

  const multiSetter = codegenCore.setterCall(multiField, 'FIELD_TREND_1', 'temp_horno');
  check(multiSetter === 'JWPLC_Display.setTrend(FIELD_TREND_1, temp_horno, sp_horno, amb_horno);', 'setterCall emite setTrend de 3 canales');

  const twoChField = { ...sampleField, ch2Enabled: true, ch2Variable: 'sp_horno', thr1Enabled: true, thr1Value: 80, thr1Color: 0xF800 };
  const twoSetter = codegenCore.setterCall(twoChField, 'FIELD_TREND_1', 'temp_horno');
  check(twoSetter === 'JWPLC_Display.setTrend(FIELD_TREND_1, temp_horno, sp_horno);', 'setterCall emite setTrend de 2 canales cuando solo ch2Enabled está activo');
}

console.log(`\nResultado: ${failures === 0 ? 'TODAS PASARON' : `${failures} FALLARON`}`);
process.exit(failures === 0 ? 0 : 1);
