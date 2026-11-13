// Motor matemático y geometría de la Gráfica de Tendencia (TREND)
// Sin DOM: funciona en Node.js (tests, CLI) y en navegador (window.JWPLCTrendCore).
(function (root, factory) {
  const api = factory();
  if (typeof module === 'object' && module.exports) module.exports = api;
  else root.JWPLCTrendCore = api;
})(typeof self !== 'undefined' ? self : this, () => {
  'use strict';

  // Algoritmo de 3 guías replicado del sensor industrial COD (Adafruit_ST7789 / TFT ST7789)
  // Calcula low, mid y high para las 3 guías horizontales evitando colapsos en datos planos
  function calculateGraphScale(dataMin, dataMax) {
    let min = Number(dataMin);
    let max = Number(dataMax);

    if (!Number.isFinite(min) || !Number.isFinite(max)) {
      return { low: 0.0, mid: 50.0, high: 100.0, step: 50.0 };
    }

    if (min > max) {
      const temp = min;
      min = max;
      max = temp;
    }

    // Si los datos son planos (línea recta constante)
    if (Math.abs(max - min) < 0.000001) {
      let val = (min === 0.0) ? 1.0 : Math.abs(min * 0.05);
      if (val < 0.005) val = 0.005;
      min -= val;
      max += val;
    }

    const diff = max - min;
    const step = diff / 2.0;
    return {
      low: min,
      mid: min + step,
      high: max,
      step
    };
  }

  // Decimales recomendados en el eje Y según el paso de la escala
  function axisDecimals(step) {
    const s = Math.abs(Number(step) || 0);
    if (s < 0.05) return 3;
    if (s < 0.5) return 2;
    if (s < 5.0) return 1;
    return 0;
  }

  // Geometría del recuadro del widget y área útil de trazado
  function computeTrendGeometry(field) {
    const x = Math.max(0, Math.trunc(Number(field?.x) || 0));
    const y = Math.max(0, Math.trunc(Number(field?.y) || 0));
    const w = Math.max(40, Math.trunc(Number(field?.width || field?.w) || 200));
    const h = Math.max(30, Math.trunc(Number(field?.height || field?.h) || 80));

    const showAxes = field?.showAxes !== false;
    const showTimeAxis = field?.showTimeAxis !== false;
    const decimals = Math.max(0, Math.min(3, Math.trunc(Number(field?.decimals) || 1)));

    // Margen izquierdo para etiquetas numéricas del eje Y
    let axisW = 0;
    if (showAxes) {
      axisW = decimals > 1 ? 46 : (decimals === 1 ? 38 : 30);
    }

    // Margen inferior para marcas de tiempo del eje X
    const timeH = showTimeAxis ? 14 : 0;

    const plotX = x + axisW + 2;
    const plotY = y + 4;
    const plotW = Math.max(20, w - axisW - 6);
    const plotH = Math.max(20, h - timeH - 8);

    // Eje X horizontal base (el cero / floor de la escala)
    const baseAxisY = plotY + plotH;
    // Margen superior (100% / max de la escala)
    const topGuideY = plotY + 4;
    const activeH = Math.max(10, baseAxisY - topGuideY);

    // Líneas guía puramente referenciales al 25% y 75% (y 50% central)
    const guide75Y = Math.round(baseAxisY - 0.75 * activeH);
    const guide50Y = Math.round(baseAxisY - 0.50 * activeH);
    const guide25Y = Math.round(baseAxisY - 0.25 * activeH);

    return {
      x,
      y,
      w,
      h,
      axisW,
      timeH,
      plotX,
      plotY,
      plotW,
      plotH,
      baseAxisY,
      topGuideY,
      activeH,
      guide75Y,
      guide50Y,
      guide25Y,
      // Compatibilidad con identificadores anteriores
      upperGuideY: guide75Y,
      middleGuideY: guide50Y,
      lowerGuideY: guide25Y,
      guideMarginPx: 4
    };
  }

  // Submuestreo diezmado (Decimation) para leer streaming de MicroSD a píxeles
  function decimateSamples(rawSamples, targetPoints) {
    if (!Array.isArray(rawSamples) || rawSamples.length === 0) return [];
    const count = Math.max(2, Math.trunc(Number(targetPoints) || 100));

    if (rawSamples.length <= count) {
      return rawSamples.slice();
    }

    const result = new Float32Array(count);
    const bucketSize = (rawSamples.length - 1) / (count - 1);

    for (let i = 0; i < count; i += 1) {
      const idx = Math.min(rawSamples.length - 1, Math.round(i * bucketSize));
      result[i] = rawSamples[idx];
    }

    return Array.from(result);
  }

  // Generador de datos sintéticos senoidales para previsualización en el lienzo
  function generatePreviewData(points = 100, minVal = 20, maxVal = 80) {
    const data = [];
    const range = (maxVal - minVal) * 0.7;
    const base = minVal + (maxVal - minVal) * 0.15;
    for (let i = 0; i < points; i += 1) {
      const phase = (i / points) * Math.PI * 4;
      const v = base + (Math.sin(phase) * 0.5 + 0.5) * range + (Math.sin(phase * 3) * 0.1 * range);
      data.push(Math.round(v * 10) / 10);
    }
    return data;
  }

  // Señal escalonada de consigna / setpoint para previsualizar el Canal 2
  function generateSetpointPreviewData(points = 100, lowVal = 55, highVal = 72) {
    const data = [];
    const stepAt = Math.floor(points * 0.45);
    for (let i = 0; i < points; i += 1) {
      data.push(i < stepAt ? lowVal : highVal);
    }
    return data;
  }

  // Señal desfasada suave para previsualizar el Canal 3
  function generateTertiaryPreviewData(points = 100, minVal = 15, maxVal = 75) {
    const data = [];
    const range = (maxVal - minVal) * 0.65;
    const base = minVal + (maxVal - minVal) * 0.18;
    for (let i = 0; i < points; i += 1) {
      const phase = (i / points) * Math.PI * 3 + 1.2;
      const v = base + (Math.cos(phase) * 0.5 + 0.5) * range;
      data.push(Math.round(v * 10) / 10);
    }
    return data;
  }

  // Convierte un valor de ingeniería a coordenada Y en píxeles dentro del área útil
  function trendValueToY(val, minVal, maxVal, topGuideY, activeH) {
    const v = Number(val);
    const min = Number(minVal);
    const max = Number(maxVal);
    if (!Number.isFinite(v) || !Number.isFinite(min) || !Number.isFinite(max) || max <= min) {
      return { y: Math.round(topGuideY + activeH / 2), inRange: false };
    }
    const ratio = (v - min) / (max - min);
    const inRange = ratio >= 0 && ratio <= 1;
    const clamped = Math.max(0, Math.min(1, ratio));
    const y = Math.round(topGuideY + (1 - clamped) * activeH);
    return { y, inRange };
  }

  return {
    calculateGraphScale,
    axisDecimals,
    computeTrendGeometry,
    decimateSamples,
    generatePreviewData,
    generateSetpointPreviewData,
    generateTertiaryPreviewData,
    trendValueToY
  };
});
