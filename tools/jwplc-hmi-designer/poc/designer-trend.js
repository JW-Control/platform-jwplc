// tools/jwplc-hmi-designer/poc/designer-trend.js
// Controlador e Inspector visual para el widget TREND (Gráfica de Tendencia).
'use strict';

(function (root, factory) {
  if (typeof module === 'object' && module.exports) {
    module.exports = factory();
  } else {
    root.JWPLCHMITrend = factory();
  }
})(typeof globalThis !== 'undefined' ? globalThis : this, function () {

  const PRESETS = {
    fullscreen: { width: 300, height: 140, x: 10, y: 15 },
    compact: { width: 160, height: 80, x: 20, y: 20 }
  };

  const FREQUENCIES = {
    '10Hz': 100,
    '5Hz': 200,
    '2Hz': 500,
    '1Hz': 1000,
    '2s': 2000,
    '5s': 5000,
    '10s': 10000,
    '60s': 60000
  };

  const COLOR_NAMES = {
    curveColor: 'Línea / Curva',
    ch2Color: 'Curva Canal 2',
    ch3Color: 'Curva Canal 3',
    thr1Color: 'Umbral 1',
    thr2Color: 'Umbral 2',
    axesColor: 'Ejes de coordenadas',
    textColor: 'Textos y etiquetas',
    gridColor: 'Guías de fondo',
    backgroundColor: 'Fondo de gráfica'
  };

  const DEFAULT_COLORS = {
    curveColor: 0xFD20,
    ch2Color: 0x07FF,
    ch3Color: 0x07E0,
    thr1Color: 0xF800,
    thr2Color: 0xFFE0,
    axesColor: 0xFFFF,
    textColor: 0xFFFF,
    gridColor: 0x4208,
    backgroundColor: 0x0000
  };

  function colorForKey(field, key) {
    const val = field?.[key];
    return (val !== undefined && val !== null) ? Number(val) : (DEFAULT_COLORS[key] ?? 0xFFFF);
  }

  function activeChannelCount(field) {
    let count = 1;
    if (field?.ch2Enabled) count += 1;
    if (field?.ch3Enabled) count += 1;
    return count;
  }

  function applyPreset(field, presetKey) {
    if (!field || !PRESETS[presetKey]) return false;
    const p = PRESETS[presetKey];
    field.width = p.width;
    field.height = p.height;
    field.w = p.width;
    field.h = p.height;
    if (p.x !== undefined) field.x = p.x;
    if (p.y !== undefined) field.y = p.y;
    return true;
  }

  function frequencyToMs(freqStr) {
    return FREQUENCIES[freqStr] || 1000;
  }

  function msToFrequencyLabel(ms) {
    const num = Number(ms) || 1000;
    for (const [lbl, val] of Object.entries(FREQUENCIES)) {
      if (val === num) return lbl.replace('Hz', ' Hz').replace('s', ' s');
    }
    if (num < 1000) {
      const hz = Math.round(1000 / num);
      return `${hz} Hz`;
    }
    const sec = Math.round(num / 1000);
    return `${sec} s`;
  }

  function estimateRamUsage(visibleSamples, channelCount = 1) {
    const pts = Math.max(1, Number(visibleSamples) || 150);
    const channels = Math.max(1, Math.min(3, Number(channelCount) || 1));
    return pts * 4 * channels; // float = 4 bytes por muestra por canal
  }

  function rgb565ToCss(v565) {
    const num = Number(v565) & 0xFFFF;
    const r = ((num >> 11) & 0x1F) * 255 / 31;
    const g = ((num >> 5) & 0x3F) * 255 / 63;
    const b = (num & 0x1F) * 255 / 31;
    return `rgb(${Math.round(r)}, ${Math.round(g)}, ${Math.round(b)})`;
  }

  let activeMainTab = 'design';
  let activeSubTab = 'appearance';
  let activeColorKey = 'curveColor';
  let trendColorEditor = null;
  let sectionInstance = null;

  function editor() {
    return typeof window !== 'undefined' ? window.JWPLCHMIEditor : null;
  }

  function selectedTrendField() {
    const ed = editor();
    const f = ed?.getSelectedField?.();
    return (f && f.type === 'TREND') ? f : null;
  }

  function mutateField(fn) {
    const field = selectedTrendField();
    if (!field) return;
    fn(field);
    editor()?.render?.();
    editor()?.commitHistory?.();
    syncInspector(field);
  }

  function escapeHtml(str) {
    return String(str || '').replace(/[&<>"]/g, (ch) => ({
      '&': '&amp;',
      '<': '&lt;',
      '>': '&gt;',
      '"': '&quot;'
    }[ch]));
  }

  function getTrendCore() {
    if (typeof window !== 'undefined' && window.JWPLCTrendCore) return window.JWPLCTrendCore;
    if (typeof require === 'function') {
      try {
        return require('./designer-trend-core.js');
      } catch {
        return null;
      }
    }
    return null;
  }

  function fallbackGeometry(field, w, h) {
    const plotX = 35;
    const topGuideY = 8;
    const baseAxisY = h - 16;
    const activeH = Math.max(10, baseAxisY - topGuideY);
    return {
      plotX,
      plotY: 4,
      plotW: Math.max(10, w - 40),
      plotH: Math.max(10, h - 20),
      baseAxisY,
      topGuideY,
      activeH,
      guide75Y: Math.round(topGuideY + activeH * 0.25),
      guide50Y: Math.round(topGuideY + activeH * 0.50),
      guide25Y: Math.round(topGuideY + activeH * 0.75),
      timeH: 14
    };
  }

  function curveEnvelopeRect(pts, geo, core, activeH, minCurve = 0, maxCurve = 100) {
    if (!pts || pts.length < 2 || !core?.trendValueToY) {
      return { x: geo.plotX, y: geo.topGuideY, w: geo.plotW, h: activeH };
    }
    let minY = geo.baseAxisY;
    let maxY = geo.topGuideY;
    pts.forEach((v) => {
      const py = core.trendValueToY(v, minCurve, maxCurve, geo.topGuideY, activeH).y;
      if (py < minY) minY = py;
      if (py > maxY) maxY = py;
    });
    const y0 = Math.max(geo.topGuideY, minY - 2);
    const y1 = Math.min(geo.baseAxisY, maxY + 2);
    return { x: geo.plotX, y: y0, w: geo.plotW, h: Math.max(5, y1 - y0 + 1) };
  }

  function trendZonesFor(field) {
    if (!field) return {};
    const w = Math.max(40, Number(field.width || field.w) || 200);
    const h = Math.max(30, Number(field.height || field.h) || 80);
    const core = getTrendCore();
    const preview = { ...field, x: 0, y: 0, width: w, height: h, w, h };
    const geo = core?.computeTrendGeometry ? core.computeTrendGeometry(preview) : fallbackGeometry(preview, w, h);
    const activeH = geo.activeH || Math.max(10, geo.baseAxisY - geo.topGuideY);

    const minVal = Number(field.minValue) || 0;
    const maxVal = Number(field.maxValue) || 100;
    const scale = core?.calculateGraphScale
      ? core.calculateGraphScale(minVal, maxVal)
      : { low: minVal, mid: (minVal + maxVal) / 2, high: maxVal };

    const showAxes = field.showAxes !== false;
    const showGuides = field.showGuides !== false;
    const showTime = field.showTimeAxis !== false;

    const thrRect = (enabled, rawValue) => {
      if (!enabled) return [];
      const val = Number(rawValue);
      if (!Number.isFinite(val)) return [];
      const res = core?.trendValueToY
        ? core.trendValueToY(val, scale.low, scale.high, geo.topGuideY, activeH)
        : {
            y: Math.round(geo.topGuideY + (1 - (val - scale.low) / ((scale.high - scale.low) || 1)) * activeH),
            inRange: val >= scale.low && val <= scale.high
          };
      if (!res.inRange || res.y < geo.topGuideY || res.y > geo.baseAxisY) return [];
      return [{ x: geo.plotX, y: Math.max(0, res.y - 2), w: geo.plotW, h: 5 }];
    };

    const guideRects = [];
    if (showGuides) {
      guideRects.push({ x: geo.plotX, y: Math.max(0, geo.guide75Y - 1), w: geo.plotW, h: 3 });
      guideRects.push({ x: geo.plotX, y: Math.max(0, geo.guide25Y - 1), w: geo.plotW, h: 3 });
    } else if (field.frame !== false) {
      guideRects.push({ x: 0, y: 0, w, h, inset: true });
    }

    const textRects = [];
    if (showAxes && geo.plotX > 6) {
      textRects.push({
        x: 1,
        y: Math.max(0, geo.topGuideY - 4),
        w: Math.max(8, geo.plotX - 4),
        h: Math.max(8, geo.baseAxisY - geo.topGuideY + 6)
      });
    }
    if (showTime && geo.timeH > 0) {
      textRects.push({
        x: geo.plotX,
        y: geo.baseAxisY + 2,
        w: geo.plotW,
        h: Math.max(8, h - (geo.baseAxisY + 3))
      });
    }

    const ch1Pts = core?.generatePreviewData ? core.generatePreviewData(geo.plotW, 20, 80) : null;
    const ch2Pts = (field.ch2Enabled && core?.generateSetpointPreviewData)
      ? core.generateSetpointPreviewData(geo.plotW, 55, 72)
      : null;
    const ch3Pts = (field.ch3Enabled && core?.generateTertiaryPreviewData)
      ? core.generateTertiaryPreviewData(geo.plotW, 15, 70)
      : null;

    return {
      curveColor: [curveEnvelopeRect(ch1Pts, geo, core, activeH, 0, 100)],
      ch2Color: ch2Pts ? [curveEnvelopeRect(ch2Pts, geo, core, activeH, 0, 100)] : [],
      ch3Color: ch3Pts ? [curveEnvelopeRect(ch3Pts, geo, core, activeH, 0, 100)] : [],
      thr1Color: thrRect(Boolean(field.thr1Enabled), field.thr1Value ?? 85),
      thr2Color: thrRect(Boolean(field.thr2Enabled), field.thr2Value ?? 20),
      axesColor: showAxes
        ? [
            { x: Math.max(0, geo.plotX - 3), y: geo.topGuideY, w: 3, h: Math.max(1, geo.baseAxisY - geo.topGuideY + 1) },
            { x: Math.max(0, geo.plotX - 1), y: geo.baseAxisY, w: geo.plotW + 1, h: 3 }
          ]
        : [],
      textColor: textRects,
      gridColor: guideRects,
      backgroundColor: [{ x: 0, y: 0, w, h, inset: true }]
    };
  }

  function pointInRect(px, py, r, pad = 2) {
    return px >= r.x - pad && px <= r.x + r.w + pad && py >= r.y - pad && py <= r.y + r.h + pad;
  }

  function pickTrendColorKeyAt(field, lx, ly) {
    const zones = trendZonesFor(field);
    const priority = ['thr1Color', 'thr2Color', 'axesColor', 'textColor', 'ch2Color', 'ch3Color', 'gridColor', 'curveColor'];
    for (const key of priority) {
      const rects = zones[key] || [];
      const pad = key === 'curveColor' ? 0 : 2;
      if (rects.some((r) => pointInRect(lx, ly, r, pad))) {
        return key;
      }
    }
    return 'backgroundColor';
  }

  function drawTrendZoneIndicator(section, field) {
    if (!section || !field || typeof window === 'undefined') return;
    const body = section.querySelector('[data-trend-preview-body]');
    const canvas = section.querySelector('[data-trend-preview-canvas]');
    if (!body || !canvas) return;
    const zones = trendZonesFor(field);
    const rects = zones[activeColorKey] || [];
    const paint = () => window.JWPLCZoneIndicator?.draw?.(body, canvas, rects);
    if (typeof requestAnimationFrame === 'function') requestAnimationFrame(paint);
    else paint();
  }

  function drawAppearancePreview(section, field) {
    if (!section) return;
    const canvas = section.querySelector('[data-trend-preview-canvas]');
    if (!canvas) return;

    const f = field || selectedTrendField();
    if (!f) return;

    const w = Math.max(40, Number(f.width || f.w) || 200);
    const h = Math.max(30, Number(f.height || f.h) || 80);

    const previewField = {
      ...f,
      x: 0,
      y: 0,
      width: w,
      height: h,
      w,
      h
    };

    const ed = editor();
    const out = ed?.renderFieldPreview ? ed.renderFieldPreview(previewField) : null;
    if (!out || !out.buffer) return;

    if (canvas.width !== w) canvas.width = w;
    if (canvas.height !== h) canvas.height = h;
    canvas.dataset.logicalW = String(w);
    canvas.dataset.logicalH = String(h);
    if (canvas.style) {
      const scale = Math.min(264 / w, 118 / h);
      canvas.style.width = `${Math.round(w * scale)}px`;
      canvas.style.height = `${Math.round(h * scale)}px`;
      canvas.style.aspectRatio = `${w} / ${h}`;
    }

    const ctx = canvas.getContext('2d');
    const img = ctx.createImageData(w, h);
    for (let y = 0; y < h; y++) {
      const row = y * out.width;
      for (let x = 0; x < w; x++) {
        const v = out.buffer[row + x];
        const o = (y * w + x) * 4;
        img.data[o] = ((v >> 11) & 0x1F) * 255 / 31;
        img.data[o + 1] = ((v >> 5) & 0x3F) * 255 / 63;
        img.data[o + 2] = (v & 0x1F) * 255 / 31;
        img.data[o + 3] = 255;
      }
    }
    ctx.putImageData(img, 0, 0);
    drawTrendZoneIndicator(section, previewField);
  }

  function ensureTrendInspectorSection() {
    if (typeof document === 'undefined') return null;

    let section = document.getElementById('trendInspectorSection');
    if (section) {
      sectionInstance = section;
      return section;
    }

    const anchor = document.getElementById('textFieldControlsSection');
    if (!anchor || !anchor.parentNode) return null;

    section = document.createElement('section');
    section.id = 'trendInspectorSection';
    section.className = 'inspector-section';
    section.hidden = true;
    section.style.display = 'none';

    section.innerHTML = `
      <div class="inspector-title">
        <h2>Inspector — TENDENCIA</h2>
        <span>
          <button type="button" class="icon-button" data-trend-action="duplicate" title="Duplicar">⧉</button>
          <button type="button" class="icon-button" data-trend-action="delete" title="Eliminar">🗑</button>
        </span>
      </div>
      <div class="insp-wrap">
        <div class="bar-nav-header insp-nav-header">
          <div class="bar-main-tabs" role="tablist">
            <button type="button" class="bar-main-tab active" data-trend-main="design">Diseño</button>
            <button type="button" class="bar-main-tab" data-trend-main="data">Datos</button>
            <button type="button" class="bar-main-tab" data-trend-main="sd">MicroSD</button>
            <button type="button" class="bar-main-tab" data-trend-main="tech">Técnico</button>
          </div>
          <div class="bar-sub-tabs" data-trend-subtabs>
            <button type="button" class="bar-sub-tab" data-trend-sub="shape">Forma</button>
            <button type="button" class="bar-sub-tab" data-trend-sub="content">Contenido</button>
            <button type="button" class="bar-sub-tab active" data-trend-sub="appearance">Apariencia</button>
            <button type="button" class="bar-sub-tab" data-trend-sub="placement">Ubicación</button>
          </div>
        </div>

        <div class="insp-body">
          <!-- PANEL: Forma -->
          <div class="insp-panel" data-trend-panel="design:shape" hidden>
            <div class="bar-card">
              <div class="bar-card-section-title">Presets rápidos de tamaño</div>
              <div class="bar-visual-grid-2">
                <button type="button" class="bar-visual-btn" data-trend-preset="fullscreen" title="Ocupa el ancho útil de la pantalla (300 × 140 px)">
                  <svg width="24" height="16" viewBox="0 0 24 16"><rect x="1" y="1" width="22" height="14" rx="2" stroke-width="1.5"/><line x1="4" y1="12" x2="20" y2="12" stroke-width="1" stroke-dasharray="2 1"/><polyline points="4,10 8,6 12,8 20,4" stroke-width="1.2"/></svg>
                  <strong>Pantalla Completa</strong>
                  <span style="font-size:10px; opacity:0.8;">300 × 140 px</span>
                </button>
                <button type="button" class="bar-visual-btn" data-trend-preset="compact" title="Recuadro compacto para vistas compartidas (160 × 80 px)">
                  <svg width="24" height="16" viewBox="0 0 24 16"><rect x="4" y="2" width="16" height="12" rx="2" stroke-width="1.5"/><polyline points="7,9 10,7 13,8 17,5" stroke-width="1.2"/></svg>
                  <strong>Recuadro Compacto</strong>
                  <span style="font-size:10px; opacity:0.8;">160 × 80 px</span>
                </button>
              </div>
              <div class="insp-two insp-gap" style="margin-top:10px;">
                <label class="field-label">Ancho (px)
                  <input class="field-input" data-trend-prop="width" type="number" min="40" max="320" step="1" />
                </label>
                <label class="field-label">Alto (px)
                  <input class="field-input" data-trend-prop="height" type="number" min="30" max="240" step="1" />
                </label>
              </div>
            </div>

            <div class="bar-card">
              <div class="bar-card-section-title">Grosor de la curva</div>
              <div class="bar-visual-grid-2">
                <button type="button" class="bar-visual-btn" data-trend-set="lineWidth" data-value="1" title="Trazo fino de 1 px para máxima resolución temporal">
                  <svg width="34" height="14" viewBox="0 0 34 14"><path d="M2,10 Q9,2 17,7 T32,4" stroke-width="1" fill="none"/></svg>
                  <strong>1 px (Fino)</strong>
                  <span style="font-size:10px; opacity:0.8;">Estándar (por defecto)</span>
                </button>
                <button type="button" class="bar-visual-btn" data-trend-set="lineWidth" data-value="2" title="Trazo nítido de 2 px para alta visibilidad a distancia">
                  <svg width="34" height="14" viewBox="0 0 34 14"><path d="M2,10 Q9,2 17,7 T32,4" stroke-width="2.5" fill="none"/></svg>
                  <strong>2 px (Nítido)</strong>
                  <span style="font-size:10px; opacity:0.8;">Resaltado industrial</span>
                </button>
              </div>
            </div>

            <div class="bar-card">
              <div class="bar-card-section-title">Marco exterior</div>
              <div class="bar-visual-grid-2">
                <button type="button" class="bar-visual-btn" data-trend-set="frame" data-value="true" title="Borde perimetral cerrado">
                  <svg width="30" height="14" viewBox="0 0 30 14"><rect x="1.5" y="1.5" width="27" height="11" rx="1.5" stroke-width="1.5"/><rect x="5" y="4" width="20" height="6" fill="currentColor" opacity="0.2"/></svg>
                  <strong>Con marco</strong>
                  <span style="font-size:10px; opacity:0.8;">Borde perimetral</span>
                </button>
                <button type="button" class="bar-visual-btn" data-trend-set="frame" data-value="false" title="Sin borde exterior">
                  <svg width="30" height="14" viewBox="0 0 30 14"><rect x="1.5" y="1.5" width="27" height="11" rx="1.5" stroke-width="1.5" stroke-dasharray="2 2" opacity="0.4"/><rect x="5" y="4" width="20" height="6" fill="currentColor" opacity="0.1"/></svg>
                  <strong>Sin marco</strong>
                  <span style="font-size:10px; opacity:0.8;">Fondo abierto</span>
                </button>
              </div>

              <div class="bar-card-section-title insp-gap">Ejes de coordenadas (L)</div>
              <div class="bar-visual-grid-2">
                <button type="button" class="bar-visual-btn" data-trend-set="showAxes" data-value="true" title="Eje Y vertical de valores y eje X horizontal en el cero">
                  <svg width="30" height="16" viewBox="0 0 30 16"><polyline points="5,2 5,13 27,13" stroke-width="2" fill="none"/><line x1="2" y1="5" x2="5" y2="5" stroke-width="1.5"/><line x1="2" y1="9" x2="5" y2="9" stroke-width="1.5"/><line x1="16" y1="13" x2="16" y2="15" stroke-width="1.5"/></svg>
                  <strong>Ejes L Visibles</strong>
                  <span style="font-size:10px; opacity:0.8;">Eje Y + Eje X (cero)</span>
                </button>
                <button type="button" class="bar-visual-btn" data-trend-set="showAxes" data-value="false" title="Ocultar ejes de coordenadas">
                  <svg width="30" height="16" viewBox="0 0 30 16"><polyline points="5,2 5,13 27,13" stroke-width="1" stroke-dasharray="2 2" opacity="0.3" fill="none"/><path d="M7,10 Q15,4 25,8" stroke-width="1.2" fill="none"/></svg>
                  <strong>Ocultos</strong>
                  <span style="font-size:10px; opacity:0.8;">Solo gráfico puro</span>
                </button>
              </div>
            </div>

            <div class="bar-card">
              <div class="bar-card-section-title">Líneas guía referenciales</div>
              <div class="bar-visual-grid-2">
                <button type="button" class="bar-visual-btn" data-trend-set="showGuides" data-value="true" title="Líneas horizontales tenues referenciales al 25% y 75%">
                  <svg width="30" height="16" viewBox="0 0 30 16"><line x1="3" y1="5" x2="27" y2="5" stroke-width="1" stroke-dasharray="3 2"/><line x1="3" y1="11" x2="27" y2="11" stroke-width="1" stroke-dasharray="3 2"/></svg>
                  <strong>Guías 25% y 75%</strong>
                  <span style="font-size:10px; opacity:0.8;">Líneas de referencia</span>
                </button>
                <button type="button" class="bar-visual-btn" data-trend-set="showGuides" data-value="false" title="Sin líneas guía intermedias">
                  <svg width="30" height="16" viewBox="0 0 30 16"><rect x="3" y="3" width="24" height="10" rx="1" stroke-width="1" opacity="0.2" fill="none"/></svg>
                  <strong>Sin guías</strong>
                  <span style="font-size:10px; opacity:0.8;">Área despejada</span>
                </button>
              </div>

              <div class="bar-card-section-title insp-gap">Eje de tiempo (X)</div>
              <div class="bar-visual-grid-2">
                <button type="button" class="bar-visual-btn" data-trend-set="showTimeAxis" data-value="true" title="Muestra las marcas de tiempo 0s y rango total en la base">
                  <svg width="30" height="16" viewBox="0 0 30 16"><circle cx="7" cy="8" r="5" stroke-width="1.2"/><polyline points="7,5 7,8 9,8" stroke-width="1"/><line x1="16" y1="8" x2="28" y2="8" stroke-width="1.2"/><polyline points="25,6 28,8 25,10" stroke-width="1.2"/></svg>
                  <strong>Marcas de tiempo</strong>
                  <span style="font-size:10px; opacity:0.8;">0s y duración total</span>
                </button>
                <button type="button" class="bar-visual-btn" data-trend-set="showTimeAxis" data-value="false" title="Sin etiquetas de tiempo inferiores">
                  <svg width="30" height="16" viewBox="0 0 30 16"><line x1="4" y1="12" x2="26" y2="12" stroke-width="1" opacity="0.3"/></svg>
                  <strong>Sin tiempo</strong>
                  <span style="font-size:10px; opacity:0.8;">Sin marcas de tiempo</span>
                </button>
              </div>
            </div>
          </div>

          <!-- PANEL: Contenido -->
          <div class="insp-panel" data-trend-panel="design:content" hidden>
            <div class="bar-card">
              <label class="field-label">Título / Identificador
                <input class="field-input" data-trend-prop="title" type="text" maxlength="16" placeholder="TEND" />
              </label>
              <label class="field-label insp-gap">Valor de prueba
                <input class="field-input" data-trend-prop="preview" type="text" placeholder="24.5" />
              </label>
              <label class="field-label insp-gap">Unidad de ingeniería
                <input class="field-input" data-trend-prop="unit" type="text" maxlength="8" placeholder="°C, RPM, BAR" />
              </label>
            </div>

            <div class="bar-card">
              <div class="bar-card-section-title">Decimales en eje Y</div>
              <div class="insp-chip-row" data-trend-decimals-row>
                <button type="button" class="insp-chip" data-trend-set="decimals" data-value="0">0</button>
                <button type="button" class="insp-chip" data-trend-set="decimals" data-value="1">1</button>
                <button type="button" class="insp-chip" data-trend-set="decimals" data-value="2">2</button>
                <button type="button" class="insp-chip" data-trend-set="decimals" data-value="3">3</button>
              </div>
            </div>
          </div>

          <!-- PANEL: Apariencia -->
          <div class="insp-panel trend-appearance-panel" data-trend-panel="design:appearance">
            <div class="bar-card trend-preview-card">
              <div class="bar-card-header" style="margin-bottom:4px;">
                <span class="bar-card-section-title" style="margin:0;">Vista previa en vivo</span>
                <span class="insp-hint" style="margin:0;">Clic en zona o chip</span>
              </div>
              <div class="menu-case" style="margin-bottom:0;">
                <div class="menu-case-body zone-host" data-trend-preview-body style="justify-content:center; padding:2px 0;">
                  <canvas class="menu-case-canvas trend-preview-canvas" data-trend-preview-canvas width="200" height="80"></canvas>
                </div>
              </div>
            </div>

            <div class="bar-card trend-color-card">
              <div class="insp-chip-row trend-color-chips" data-trend-color-tabs>
                <button type="button" class="insp-chip active" data-trend-colorkey="curveColor">
                  <span class="trend-color-swatch" data-trend-swatch="curveColor"></span>Línea
                </button>
                <button type="button" class="insp-chip" data-trend-colorkey="axesColor">
                  <span class="trend-color-swatch" data-trend-swatch="axesColor"></span>Ejes
                </button>
                <button type="button" class="insp-chip" data-trend-colorkey="textColor">
                  <span class="trend-color-swatch" data-trend-swatch="textColor"></span>Textos
                </button>
                <button type="button" class="insp-chip" data-trend-colorkey="gridColor">
                  <span class="trend-color-swatch" data-trend-swatch="gridColor"></span>Guías
                </button>
                <button type="button" class="insp-chip" data-trend-colorkey="backgroundColor">
                  <span class="trend-color-swatch" data-trend-swatch="backgroundColor"></span>Fondo
                </button>
                <button type="button" class="insp-chip" data-trend-colorkey="ch2Color" data-trend-color-flag="ch2Enabled" style="display:none;">
                  <span class="trend-color-swatch" data-trend-swatch="ch2Color"></span>CH2
                </button>
                <button type="button" class="insp-chip" data-trend-colorkey="ch3Color" data-trend-color-flag="ch3Enabled" style="display:none;">
                  <span class="trend-color-swatch" data-trend-swatch="ch3Color"></span>CH3
                </button>
                <button type="button" class="insp-chip" data-trend-colorkey="thr1Color" data-trend-color-flag="thr1Enabled" style="display:none;">
                  <span class="trend-color-swatch" data-trend-swatch="thr1Color"></span>Umb 1
                </button>
                <button type="button" class="insp-chip" data-trend-colorkey="thr2Color" data-trend-color-flag="thr2Enabled" style="display:none;">
                  <span class="trend-color-swatch" data-trend-swatch="thr2Color"></span>Umb 2
                </button>
              </div>
              <div data-trend-color-host>
                <!-- JWPLCColorEditor injected here -->
              </div>
            </div>
          </div>

          <!-- PANEL: Ubicación -->
          <div class="insp-panel" data-trend-panel="design:placement" hidden>
            <div class="bar-card">
              <div class="insp-two">
                <label class="field-label">X (px)
                  <input class="field-input" data-trend-prop="x" type="number" min="0" max="320" />
                </label>
                <label class="field-label">Y (px)
                  <input class="field-input" data-trend-prop="y" type="number" min="0" max="170" />
                </label>
              </div>
              <div class="insp-two insp-gap">
                <label class="field-label">Ancho (px)
                  <input class="field-input" data-trend-prop="width" type="number" min="40" max="320" />
                </label>
                <label class="field-label">Alto (px)
                  <input class="field-input" data-trend-prop="height" type="number" min="30" max="170" />
                </label>
              </div>
            </div>
            <div class="bar-card">
              <div class="bar-card-section-title">Página</div>
              <div class="insp-chip-row" data-trend-page-chips></div>
            </div>
          </div>

          <!-- PANEL: Datos -->
          <div class="insp-panel trend-data-panel" data-trend-panel="data" hidden>
            <div class="bar-card">
              <div class="bar-card-section-title" style="margin-bottom:6px;">Variables (Canales)</div>
              <div class="trend-chan-list">
                <div class="trend-chan-row trend-chan-primary">
                  <span class="trend-chan-tag" title="Canal principal">
                    <span class="trend-color-swatch" data-trend-swatch="curveColor"></span>CH1
                  </span>
                  <input class="field-input" data-trend-prop="variable" type="text" placeholder="temperatura" title="Variable analógica PLC principal" />
                </div>
                <div class="trend-chan-row" data-trend-chan-row="ch2Enabled">
                  <label class="trend-chan-toggle" title="Activar Canal 2 (ej. Setpoint)">
                    <input type="checkbox" data-trend-prop="ch2Enabled" />
                    <span class="trend-color-swatch" data-trend-swatch="ch2Color"></span>CH2
                  </label>
                  <input class="field-input" data-trend-prop="ch2Variable" type="text" placeholder="setpoint" title="Variable PLC Canal 2" data-trend-flag-input="ch2Enabled" />
                  <input class="field-input trend-chan-lbl" data-trend-prop="ch2Label" type="text" maxlength="10" placeholder="SP" title="Etiqueta Canal 2" data-trend-flag-input="ch2Enabled" />
                </div>
                <div class="trend-chan-row" data-trend-chan-row="ch3Enabled">
                  <label class="trend-chan-toggle" title="Activar Canal 3">
                    <input type="checkbox" data-trend-prop="ch3Enabled" />
                    <span class="trend-color-swatch" data-trend-swatch="ch3Color"></span>CH3
                  </label>
                  <input class="field-input" data-trend-prop="ch3Variable" type="text" placeholder="salida" title="Variable PLC Canal 3" data-trend-flag-input="ch3Enabled" />
                  <input class="field-input trend-chan-lbl" data-trend-prop="ch3Label" type="text" maxlength="10" placeholder="OUT" title="Etiqueta Canal 3" data-trend-flag-input="ch3Enabled" />
                </div>
              </div>
            </div>

            <div class="bar-card">
              <div class="bar-card-header" style="margin-bottom:6px;">
                <span class="bar-card-section-title" style="margin:0;">Escala Y y Umbrales</span>
                <div class="insp-chip-row">
                  <button type="button" class="insp-chip" data-trend-set="scaleMode" data-value="AUTO" title="Ajusta mínimo y máximo automáticamente">Auto</button>
                  <button type="button" class="insp-chip" data-trend-set="scaleMode" data-value="MANUAL" title="Rango fijo definido por Mínimo y Máximo">Manual</button>
                </div>
              </div>
              <div class="insp-two" data-trend-manual-range style="margin-bottom:6px;">
                <label class="field-label">Mínimo
                  <input class="field-input" data-trend-prop="minValue" type="number" step="any" />
                </label>
                <label class="field-label">Máximo
                  <input class="field-input" data-trend-prop="maxValue" type="number" step="any" />
                </label>
              </div>
              <div class="insp-two trend-thr-grid">
                <div class="trend-thr-item" data-trend-chan-row="thr1Enabled">
                  <label class="trend-chan-toggle" title="Activar Umbral 1 (alarma alta)">
                    <input type="checkbox" data-trend-prop="thr1Enabled" />
                    <span class="trend-color-swatch" data-trend-swatch="thr1Color"></span>Umb 1
                  </label>
                  <input class="field-input" data-trend-prop="thr1Value" type="number" step="any" placeholder="85" data-trend-flag-input="thr1Enabled" />
                </div>
                <div class="trend-thr-item" data-trend-chan-row="thr2Enabled">
                  <label class="trend-chan-toggle" title="Activar Umbral 2 (mínimo)">
                    <input type="checkbox" data-trend-prop="thr2Enabled" />
                    <span class="trend-color-swatch" data-trend-swatch="thr2Color"></span>Umb 2
                  </label>
                  <input class="field-input" data-trend-prop="thr2Value" type="number" step="any" placeholder="20" data-trend-flag-input="thr2Enabled" />
                </div>
              </div>
            </div>

            <div class="bar-card">
              <div class="bar-card-header" style="margin-bottom:6px;">
                <span class="bar-card-section-title" style="margin:0;">Muestreo y Ventana</span>
                <span class="insp-hint" style="margin:0;">Ventana: <strong data-trend-readout="windowSpan" style="color:var(--text);">—</strong></span>
              </div>
              <div class="trend-freq-grid" data-trend-freq-chips>
                <button type="button" class="insp-chip" data-trend-freq="10Hz">10 Hz</button>
                <button type="button" class="insp-chip" data-trend-freq="5Hz">5 Hz</button>
                <button type="button" class="insp-chip" data-trend-freq="2Hz">2 Hz</button>
                <button type="button" class="insp-chip" data-trend-freq="1Hz">1 Hz</button>
                <button type="button" class="insp-chip" data-trend-freq="2s">2 s</button>
                <button type="button" class="insp-chip" data-trend-freq="5s">5 s</button>
                <button type="button" class="insp-chip" data-trend-freq="10s">10 s</button>
                <button type="button" class="insp-chip" data-trend-freq="60s">60 s</button>
              </div>
              <div class="insp-two insp-gap">
                <label class="field-label">Periodo (ms)
                  <input class="field-input" data-trend-prop="sampleIntervalMs" type="number" min="50" max="60000" step="50" />
                </label>
                <label class="field-label">Puntos visibles
                  <input class="field-input" data-trend-prop="visibleSamples" type="number" min="20" max="320" />
                </label>
              </div>
            </div>
          </div>

          <!-- PANEL: MicroSD -->
          <div class="insp-panel" data-trend-panel="sd" hidden>
            <div class="bar-card">
              <div class="bar-card-section-title">Registro en Tarjeta MicroSD</div>
              <div class="bar-card-header">
                <span class="bar-card-title">Activar volcado continuo en SD</span>
                <div class="insp-chip-row">
                  <button type="button" class="insp-chip" data-trend-set="enableSD" data-value="false">No</button>
                  <button type="button" class="insp-chip" data-trend-set="enableSD" data-value="true">Sí</button>
                </div>
              </div>
              <label class="field-label insp-gap" data-trend-sd-interval-wrap>Periodo de volcado SD (ms)
                <input class="field-input" data-trend-prop="sdIntervalMs" type="number" min="100" max="60000" step="100" />
              </label>
            </div>

            <div class="bar-card">
              <div class="bar-card-section-title">Arquitectura de Consulta Histórica</div>
              <p style="font-size:11px; line-height:1.4; color:var(--text); opacity:0.85; margin:0 0 6px;">
                Los datos se guardan en streaming en bloques de 512 bytes respetando el mutex SPI del display.
              </p>
              <p style="font-size:11px; line-height:1.4; color:var(--text); opacity:0.85; margin:0;">
                Para consultar "última hora" o "últimas 24h", el firmware realiza diezmado al vuelo cargando sólo <strong>1 KB</strong> temporal en RAM sin saturar DRAM ni desgastar la Flash.
              </p>
            </div>
          </div>

          <!-- PANEL: Técnico -->
          <div class="insp-panel" data-trend-panel="tech" hidden>
            <div class="bar-card">
              <label class="field-label">Identificador C++ (Enum)
                <input class="field-input" data-trend-prop="id" type="text" />
              </label>
              <label class="field-label insp-gap">Nombre descriptivo
                <input class="field-input" data-trend-prop="name" type="text" />
              </label>
            </div>

            <div class="bar-card">
              <div class="insp-readouts">
                <span>Tipo de dato C++: <strong>float</strong></span>
                <span>Memoria RAM del buffer: <strong data-trend-readout="ramBytes">—</strong></span>
              </div>
            </div>

            <div class="bar-card">
              <div class="bar-card-section-title">C++ Generado</div>
              <div class="bar-code-box bar-code-box-multiline">
                <code data-trend-readout="code">—</code>
              </div>
            </div>
          </div>
        </div>
      </div>
    `;

    anchor.parentNode.insertBefore(section, anchor.nextSibling);
    sectionInstance = section;

    bindEvents(section);
    initColorEditor(section);

    return section;
  }

  function initColorEditor(section) {
    if (!section || !window.JWPLCColorEditor) return;
    const host = section.querySelector('[data-trend-color-host]');
    if (!host) return;

    trendColorEditor = window.JWPLCColorEditor.create({
      paletteAttr: 'data-trend-palette',
      onInput: (v565) => {
        const field = selectedTrendField();
        if (!field) return;
        field[activeColorKey] = Number(v565) & 0xFFFF;
        editor()?.render?.();
        updateColorSwatches(section, field);
        drawAppearancePreview(section, field);
      },
      onCommit: (v565) => {
        mutateField((f) => {
          f[activeColorKey] = Number(v565) & 0xFFFF;
        });
      }
    });

    host.appendChild(trendColorEditor.el);
  }

  function updateColorSwatches(section, field) {
    if (!section || !field) return;
    const swatches = section.querySelectorAll('[data-trend-swatch]');
    swatches.forEach((sw) => {
      const k = sw.dataset.trendSwatch;
      sw.style.background = rgb565ToCss(colorForKey(field, k));
    });
  }

  const DEFAULT_TRUE_BOOL_PROPS = new Set(['frame', 'showAxes', 'showGuides', 'showTimeAxis']);
  const DEFAULT_FALSE_BOOL_PROPS = new Set(['ch2Enabled', 'ch3Enabled', 'thr1Enabled', 'thr2Enabled', 'enableSD']);
  const COLOR_FLAG_MAP = {
    ch2Color: 'ch2Enabled',
    ch3Color: 'ch3Enabled',
    thr1Color: 'thr1Enabled',
    thr2Color: 'thr2Enabled'
  };

  function selectColorKey(section, key) {
    if (!key) return;
    activeColorKey = key;
    section.querySelectorAll('[data-trend-colorkey]').forEach((b) => {
      b.classList.toggle('active', b.dataset.trendColorkey === activeColorKey);
    });
    const f = selectedTrendField();
    if (f && trendColorEditor) {
      trendColorEditor.set(colorForKey(f, activeColorKey), COLOR_NAMES[activeColorKey] || 'Color');
    }
    if (f) drawTrendZoneIndicator(section, f);
  }

  function syncTrendPages(section, field) {
    const host = section.querySelector('[data-trend-page-chips]');
    if (!host || !field) return;
    const pages = editor()?.getPages?.() || [];
    const current = Number(field.page || 0);
    const signature = pages.map((p) => `${p.id}:${p.name}`).join('|') + `#${current}`;
    if (host.dataset.signature === signature) return;
    host.dataset.signature = signature;
    host.innerHTML = pages.map((p) => {
      const id = Number(p.id);
      const name = p.name || `Página ${id + 1}`;
      return `<button type="button" class="insp-chip${id === current ? ' active' : ''}" data-trend-page-id="${id}" title="${escapeHtml(name)}">${String(id + 1).padStart(2, '0')} · ${escapeHtml(name)}</button>`;
    }).join('');
  }

  function bindEvents(section) {
    // Main tabs
    section.querySelectorAll('[data-trend-main]').forEach((btn) => {
      btn.addEventListener('click', () => {
        activeMainTab = btn.dataset.trendMain;
        switchTab(section);
      });
    });

    // Sub tabs
    section.querySelectorAll('[data-trend-sub]').forEach((btn) => {
      btn.addEventListener('click', () => {
        activeSubTab = btn.dataset.trendSub;
        switchTab(section);
      });
    });

    // Actions
    section.querySelector('[data-trend-action="duplicate"]')?.addEventListener('click', () => {
      editor()?.duplicateSelectedField?.();
    });
    section.querySelector('[data-trend-action="delete"]')?.addEventListener('click', () => {
      editor()?.deleteSelectedField?.();
    });

    // Presets
    section.querySelectorAll('[data-trend-preset]').forEach((btn) => {
      btn.addEventListener('click', () => {
        const key = btn.dataset.trendPreset;
        mutateField((f) => {
          applyPreset(f, key);
        });
      });
    });

    // Visual buttons (lineWidth, scaleMode, decimals, booleans)
    section.querySelectorAll('[data-trend-set]').forEach((btn) => {
      btn.addEventListener('click', () => {
        const prop = btn.dataset.trendSet;
        const rawVal = btn.dataset.value;
        let val;
        if (DEFAULT_TRUE_BOOL_PROPS.has(prop) || DEFAULT_FALSE_BOOL_PROPS.has(prop)) {
          val = (rawVal === 'true' || rawVal === '1');
        } else if (prop === 'lineWidth' || prop === 'decimals') {
          val = Number(rawVal);
        } else {
          val = rawVal;
        }
        mutateField((f) => {
          f[prop] = val;
        });
      });
    });

    // Frequency chips
    section.querySelectorAll('[data-trend-freq]').forEach((btn) => {
      btn.addEventListener('click', () => {
        const freqKey = btn.dataset.trendFreq;
        const ms = frequencyToMs(freqKey);
        mutateField((f) => {
          f.sampleIntervalMs = ms;
        });
      });
    });

    // Color tabs & preview canvas click selection
    section.querySelectorAll('[data-trend-colorkey]').forEach((btn) => {
      btn.addEventListener('click', () => {
        selectColorKey(section, btn.dataset.trendColorkey);
      });
    });

    const previewCanvas = section.querySelector('[data-trend-preview-canvas]');
    if (previewCanvas) {
      previewCanvas.addEventListener('click', (e) => {
        const f = selectedTrendField();
        if (!f) return;
        const rect = previewCanvas.getBoundingClientRect();
        if (!rect.width || !rect.height) return;
        const logicalW = Number(previewCanvas.dataset.logicalW) || previewCanvas.width || 200;
        const logicalH = Number(previewCanvas.dataset.logicalH) || previewCanvas.height || 80;
        const lx = ((e.clientX - rect.left) / rect.width) * logicalW;
        const ly = ((e.clientY - rect.top) / rect.height) * logicalH;
        const key = pickTrendColorKeyAt(f, lx, ly);
        selectColorKey(section, key);
      });
    }

    const previewBody = section.querySelector('[data-trend-preview-body]');
    if (previewBody && typeof ResizeObserver !== 'undefined') {
      new ResizeObserver(() => {
        const f = selectedTrendField();
        if (f && activeMainTab === 'design' && activeSubTab === 'appearance') {
          drawTrendZoneIndicator(section, f);
        }
      }).observe(previewBody);
    }

    // Move to page chips (Ubicación)
    const pageChipsHost = section.querySelector('[data-trend-page-chips]');
    if (pageChipsHost) {
      pageChipsHost.addEventListener('click', (e) => {
        const chip = e.target.closest('[data-trend-page-id]');
        if (!chip) return;
        editor()?.moveSelectedFieldToPage?.(Number(chip.dataset.trendPageId));
      });
    }

    // Direct property inputs
    section.querySelectorAll('[data-trend-prop]').forEach((input) => {
      const prop = input.dataset.trendProp;
      const isCheck = input.type === 'checkbox';
      const isNum = input.type === 'number';

      input.addEventListener('change', () => {
        mutateField((f) => {
          if (isCheck) f[prop] = input.checked;
          else if (isNum) {
            const nVal = Number(input.value) || 0;
            f[prop] = nVal;
            if (prop === 'width') f.w = nVal;
            if (prop === 'height') f.h = nVal;
          } else {
            f[prop] = input.value;
          }
        });
      });

      if (!isCheck) {
        input.addEventListener('input', () => {
          const field = selectedTrendField();
          if (!field) return;
          if (isNum) {
            const nVal = Number(input.value) || 0;
            field[prop] = nVal;
            if (prop === 'width') field.w = nVal;
            if (prop === 'height') field.h = nVal;
          } else {
            field[prop] = input.value;
          }
          editor()?.render?.();
          drawAppearancePreview(section, field);
        });
      }
    });
  }

  function switchTab(section) {
    if (!section) return;

    // Update main tab headers
    section.querySelectorAll('[data-trend-main]').forEach((btn) => {
      btn.classList.toggle('active', btn.dataset.trendMain === activeMainTab);
    });

    const subTabsContainer = section.querySelector('[data-trend-subtabs]');
    if (subTabsContainer) {
      subTabsContainer.style.display = (activeMainTab === 'design') ? 'flex' : 'none';
    }

    // Update sub tab headers
    section.querySelectorAll('[data-trend-sub]').forEach((btn) => {
      btn.classList.toggle('active', btn.dataset.trendSub === activeSubTab);
    });

    // Panels
    section.querySelectorAll('[data-trend-panel]').forEach((panel) => {
      const pKey = panel.dataset.trendPanel;
      let visible = false;
      if (activeMainTab === 'design') {
        visible = (pKey === `design:${activeSubTab}`);
      } else {
        visible = (pKey === activeMainTab);
      }
      panel.hidden = !visible;
      panel.style.display = visible ? '' : 'none';
    });

    if (activeMainTab === 'design' && activeSubTab === 'appearance') {
      const f = selectedTrendField();
      if (f) drawAppearancePreview(section, f);
    }
  }

  function syncInspector(field) {
    const f = field || selectedTrendField();
    const section = ensureTrendInspectorSection();
    if (!section || !f) return;

    // Sync input fields
    section.querySelectorAll('[data-trend-prop]').forEach((input) => {
      const prop = input.dataset.trendProp;
      if (input.type === 'checkbox') {
        if (DEFAULT_FALSE_BOOL_PROPS.has(prop)) {
          input.checked = Boolean(f[prop]);
        } else {
          input.checked = f[prop] !== false;
        }
      } else {
        if (document.activeElement !== input) {
          input.value = (f[prop] !== undefined && f[prop] !== null) ? String(f[prop]) : '';
        }
      }
    });

    // Visual button states
    section.querySelectorAll('[data-trend-set]').forEach((btn) => {
      const prop = btn.dataset.trendSet;
      const val = btn.dataset.value;
      if (DEFAULT_TRUE_BOOL_PROPS.has(prop)) {
        const isBtnTrue = (val === 'true' || val === '1');
        const currentBool = f[prop] !== false;
        btn.classList.toggle('active', currentBool === isBtnTrue);
      } else if (DEFAULT_FALSE_BOOL_PROPS.has(prop)) {
        const isBtnTrue = (val === 'true' || val === '1');
        const currentBool = Boolean(f[prop]);
        btn.classList.toggle('active', currentBool === isBtnTrue);
      } else {
        const cur = String(f[prop]);
        btn.classList.toggle('active', cur === val);
      }
    });

    // Dynamic canal / threshold compact rows & inputs
    section.querySelectorAll('[data-trend-chan-row]').forEach((row) => {
      const flag = row.dataset.trendChanRow;
      row.classList.toggle('is-disabled', !Boolean(f[flag]));
    });
    section.querySelectorAll('[data-trend-flag-input]').forEach((el) => {
      const flag = el.dataset.trendFlagInput;
      el.disabled = !Boolean(f[flag]);
    });

    // Dynamic extra color chips (Canales y Umbrales)
    section.querySelectorAll('[data-trend-color-flag]').forEach((chip) => {
      const flag = chip.dataset.trendColorFlag;
      chip.style.display = Boolean(f[flag]) ? '' : 'none';
    });
    const requiredFlag = COLOR_FLAG_MAP[activeColorKey];
    if (requiredFlag && !f[requiredFlag]) {
      activeColorKey = 'curveColor';
    }
    section.querySelectorAll('[data-trend-colorkey]').forEach((btn) => {
      btn.classList.toggle('active', btn.dataset.trendColorkey === activeColorKey);
    });

    // Frequency chips
    const curMs = Number(f.sampleIntervalMs) || 1000;
    section.querySelectorAll('[data-trend-freq]').forEach((btn) => {
      const freqKey = btn.dataset.trendFreq;
      const ms = frequencyToMs(freqKey);
      btn.classList.toggle('active', ms === curMs);
    });

    // Manual range visibility
    const manualRangeWrap = section.querySelector('[data-trend-manual-range]');
    if (manualRangeWrap) {
      manualRangeWrap.style.display = (f.scaleMode === 'MANUAL') ? 'grid' : 'none';
    }

    // SD interval visibility
    const sdIntervalWrap = section.querySelector('[data-trend-sd-interval-wrap]');
    if (sdIntervalWrap) {
      sdIntervalWrap.style.display = f.enableSD ? 'flex' : 'none';
    }

    // Readouts
    const spanSec = Math.round(((Number(f.sampleIntervalMs) || 1000) * (Number(f.visibleSamples) || 150)) / 1000);
    const spanText = spanSec >= 60 ? `${Math.round(spanSec / 60)} min (${spanSec} s)` : `${spanSec} s`;
    const spanEl = section.querySelector('[data-trend-readout="windowSpan"]');
    if (spanEl) spanEl.textContent = spanText;

    const chCount = activeChannelCount(f);
    const ramEl = section.querySelector('[data-trend-readout="ramBytes"]');
    if (ramEl) {
      ramEl.textContent = `${estimateRamUsage(f.visibleSamples, chCount)} bytes${chCount > 1 ? ` (${chCount} canales)` : ''}`;
    }

    // Color Swatches
    updateColorSwatches(section, f);

    // Active color in color editor
    if (trendColorEditor) {
      trendColorEditor.set(colorForKey(f, activeColorKey), COLOR_NAMES[activeColorKey] || 'Color');
    }

    // Pages (Ubicación)
    syncTrendPages(section, f);

    // Mini-preview
    drawAppearancePreview(section, f);

    // Code preview
    const codeEl = section.querySelector('[data-trend-readout="code"]');
    if (codeEl) {
      const id = f.id || 'FIELD_TREND_1';
      const codegenCore = window.JWPLCHMICodegenCore;
      codeEl.textContent = codegenCore?.trendFieldBlock ? codegenCore.trendFieldBlock(f, id) : `JWPLC_UITrendField(...)`;
    }

    switchTab(section);
  }

  // Auto-init on DOM ready and listen to editor events
  if (typeof window !== 'undefined') {
    window.addEventListener('jwplc:editor-refresh', () => {
      const f = selectedTrendField();
      if (f) syncInspector(f);
    });
  }

  if (typeof document !== 'undefined') {
    if (document.readyState === 'loading') {
      document.addEventListener('DOMContentLoaded', () => ensureTrendInspectorSection());
    } else {
      ensureTrendInspectorSection();
    }
  }

  return {
    PRESETS,
    FREQUENCIES,
    COLOR_NAMES,
    applyPreset,
    frequencyToMs,
    msToFrequencyLabel,
    estimateRamUsage,
    ensureTrendInspectorSection,
    syncInspector,
    drawAppearancePreview,
    trendZonesFor,
    pickTrendColorKeyAt
  };
});
