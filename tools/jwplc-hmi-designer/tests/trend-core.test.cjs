#!/usr/bin/env node
// Test unitario para las funciones matemáticas puras de tendencia (auto-escala, geometría y diezmado)
'use strict';

const path = require('path');
const trendCore = require('../poc/designer-trend-core.js');

let failures = 0;
function check(condition, message) {
  console.log(`  ${condition ? 'PASS' : 'FAIL'} ${message}`);
  if (!condition) failures += 1;
}

console.log('=== trend-core.test.cjs — Motor de Auto-Escala y Geometría de Tendencia ===');

// 1. Algoritmo calculateGraphScale
const s1 = trendCore.calculateGraphScale(20.0, 70.0);
check(s1.low <= 20.0, `Escala inferior (low=${s1.low}) <= minVal (20.0)`);
check(s1.high >= 70.0, `Escala superior (high=${s1.high}) >= maxVal (70.0)`);
check(Math.abs(s1.mid - (s1.low + (s1.high - s1.low)/2)) < 0.001, 'Guía media está exactamente centrada');

// Caso datos planos (ej. 45.0 constante)
const sFlat = trendCore.calculateGraphScale(45.0, 45.0);
check(sFlat.high > sFlat.low, `Datos planos expanden escala: low=${sFlat.low}, high=${sFlat.high}`);
check(sFlat.step > 0, `Paso de escala positivo: step=${sFlat.step}`);

// Caso datos invertidos o NaN
const sSafe = trendCore.calculateGraphScale(NaN, Infinity);
check(Number.isFinite(sSafe.low) && Number.isFinite(sSafe.high), 'Valores no finitos devuelven escala segura por defecto');

// 2. Decimales de eje
check(trendCore.axisDecimals(0.01) === 3, 'Paso 0.01 -> 3 decimales');
check(trendCore.axisDecimals(0.2) === 2, 'Paso 0.2 -> 2 decimales');
check(trendCore.axisDecimals(1.5) === 1, 'Paso 1.5 -> 1 decimal');
check(trendCore.axisDecimals(10.0) === 0, 'Paso 10.0 -> 0 decimales');

// 3. Geometría del widget
const geo1 = trendCore.computeTrendGeometry({
  x: 10, y: 20, width: 200, height: 100,
  showAxes: true, showTimeAxis: true, decimals: 1
});
check(geo1.plotX > 10, `plotX (${geo1.plotX}) reserva margen para el eje Y`);
check(geo1.plotW < 200, `plotW (${geo1.plotW}) descuenta margen del eje Y`);
check(geo1.plotH < 100, `plotH (${geo1.plotH}) descuenta margen del eje X de tiempo`);
check(geo1.upperGuideY < geo1.middleGuideY && geo1.middleGuideY < geo1.lowerGuideY, 'Las tres guías están en orden vertical');

// 4. Submuestreo / Diezmado de datos (Decimation)
const rawSamples = [];
for (let i = 0; i < 1000; i++) rawSamples.push(Math.sin(i * 0.1) * 50 + 50);

const targetPoints = 100;
const decimated = trendCore.decimateSamples(rawSamples, targetPoints);
check(decimated.length === targetPoints, `Diezmado produce exactamente ${targetPoints} muestras`);
check(Math.min(...decimated) >= 0 && Math.max(...decimated) <= 100, 'Conserva el rango de los datos originales');

console.log(`\nResultado: ${failures === 0 ? 'TODAS PASARON' : `${failures} FALLARON`}`);
process.exit(failures === 0 ? 0 : 1);
