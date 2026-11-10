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

    // Margen de resguardo para que la curva no toque las guías superior e inferior
    const guideMarginPx = Math.max(4, Math.min(10, Math.floor(plotH * 0.12)));
    const upperGuideY = plotY + guideMarginPx;
    const lowerGuideY = plotY + plotH - guideMarginPx;
    const middleGuideY = Math.round((upperGuideY + lowerGuideY) / 2);

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
      upperGuideY,
      middleGuideY,
      lowerGuideY,
      guideMarginPx
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

  return {
    calculateGraphScale,
    axisDecimals,
    computeTrendGeometry,
    decimateSamples,
    generatePreviewData
  };
});
