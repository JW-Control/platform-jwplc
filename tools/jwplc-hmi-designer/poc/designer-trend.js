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
    axesColor: 'Ejes de coordenadas',
    textColor: 'Textos y etiquetas',
    gridColor: 'Guías de fondo',
    backgroundColor: 'Fondo de gráfica'
  };

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

  function estimateRamUsage(visibleSamples) {
    const pts = Math.max(1, Number(visibleSamples) || 150);
    return pts * 4; // float = 4 bytes
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
          <div class="insp-panel" data-trend-panel="design:appearance">
            <div class="bar-card">
              <div class="bar-card-section-title">Vista previa en vivo</div>
              <div class="trend-preview-wrap" style="display:flex; flex-direction:column; align-items:center; justify-content:center; padding:6px 0;">
                <canvas class="trend-preview-canvas" data-trend-preview-canvas width="200" height="80"></canvas>
              </div>
            </div>

            <div class="bar-card">
              <div class="bar-card-section-title">Elemento a colorear</div>
              <div class="insp-chip-row" data-trend-color-tabs>
                <button type="button" class="insp-chip active" data-trend-colorkey="curveColor">
                  <span class="trend-color-swatch" data-trend-swatch="curveColor" style="display:inline-block; width:10px; height:10px; border-radius:2px; margin-right:4px;"></span>
                  Línea
                </button>
                <button type="button" class="insp-chip" data-trend-colorkey="axesColor">
                  <span class="trend-color-swatch" data-trend-swatch="axesColor" style="display:inline-block; width:10px; height:10px; border-radius:2px; margin-right:4px;"></span>
                  Ejes
                </button>
                <button type="button" class="insp-chip" data-trend-colorkey="textColor">
                  <span class="trend-color-swatch" data-trend-swatch="textColor" style="display:inline-block; width:10px; height:10px; border-radius:2px; margin-right:4px;"></span>
                  Textos
                </button>
                <button type="button" class="insp-chip" data-trend-colorkey="gridColor">
                  <span class="trend-color-swatch" data-trend-swatch="gridColor" style="display:inline-block; width:10px; height:10px; border-radius:2px; margin-right:4px;"></span>
                  Guías
                </button>
                <button type="button" class="insp-chip" data-trend-colorkey="backgroundColor">
                  <span class="trend-color-swatch" data-trend-swatch="backgroundColor" style="display:inline-block; width:10px; height:10px; border-radius:2px; margin-right:4px;"></span>
                  Fondo
                </button>
              </div>
            </div>
            <div class="bar-card" data-trend-color-host>
              <!-- JWPLCColorEditor injected here -->
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
          </div>

          <!-- PANEL: Datos -->
          <div class="insp-panel" data-trend-panel="data" hidden>
            <div class="bar-card">
              <label class="field-label">Variable Analógica PLC
                <input class="field-input" data-trend-prop="variable" type="text" placeholder="temperatura" />
              </label>
            </div>

            <div class="bar-card">
              <div class="bar-card-section-title">Modo de Escala</div>
              <div class="bar-visual-grid-2">
                <button type="button" class="bar-visual-btn" data-trend-set="scaleMode" data-value="AUTO">
                  <strong>Auto-escala Dinámica</strong>
                  <span style="font-size:10px; opacity:0.8;">3 guías inteligentes</span>
                </button>
                <button type="button" class="bar-visual-btn" data-trend-set="scaleMode" data-value="MANUAL">
                  <strong>Rango Fijo Manual</strong>
                  <span style="font-size:10px; opacity:0.8;">Mínimo y Máximo fijos</span>
                </button>
              </div>

              <div class="insp-two insp-gap" data-trend-manual-range>
                <label class="field-label">Valor Mínimo
                  <input class="field-input" data-trend-prop="minValue" type="number" step="any" />
                </label>
                <label class="field-label">Valor Máximo
                  <input class="field-input" data-trend-prop="maxValue" type="number" step="any" />
                </label>
              </div>
            </div>

            <div class="bar-card">
              <div class="bar-card-section-title">Frecuencia de Muestreo</div>
              <div class="insp-chip-row" data-trend-freq-chips>
                <button type="button" class="insp-chip" data-trend-freq="10Hz">10 Hz</button>
                <button type="button" class="insp-chip" data-trend-freq="5Hz">5 Hz</button>
                <button type="button" class="insp-chip" data-trend-freq="2Hz">2 Hz</button>
                <button type="button" class="insp-chip" data-trend-freq="1Hz">1 Hz</button>
                <button type="button" class="insp-chip" data-trend-freq="2s">2 s</button>
                <button type="button" class="insp-chip" data-trend-freq="5s">5 s</button>
                <button type="button" class="insp-chip" data-trend-freq="10s">10 s</button>
                <button type="button" class="insp-chip" data-trend-freq="60s">60 s</button>
              </div>
              <label class="field-label insp-gap">Periodo exacto (ms)
                <input class="field-input" data-trend-prop="sampleIntervalMs" type="number" min="50" max="60000" step="50" />
              </label>
            </div>

            <div class="bar-card">
              <label class="field-label">Puntos en vivo visibles en ventana
                <input class="field-input" data-trend-prop="visibleSamples" type="number" min="20" max="320" />
              </label>
              <div class="insp-readouts insp-gap">
                <span>Duración visible: <strong data-trend-readout="windowSpan">—</strong></span>
              </div>
            </div>
          </div>

          <!-- PANEL: MicroSD -->
          <div class="insp-panel" data-trend-panel="sd" hidden>
            <div class="bar-card">
              <div class="bar-card-section-title">Registro en Tarjeta MicroSD</div>
              <label class="field-label" style="flex-direction:row; justify-content:space-between; align-items:center;">
                <span>Activar volcado continuo en SD</span>
                <input type="checkbox" data-trend-prop="enableSD" />
              </label>
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
      if (field[k] !== undefined) {
        sw.style.background = rgb565ToCss(field[k]);
      }
    });
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

    // Visual buttons (lineWidth, scaleMode, decimals, frame, showAxes, showGuides, showTimeAxis)
    section.querySelectorAll('[data-trend-set]').forEach((btn) => {
      btn.addEventListener('click', () => {
        const prop = btn.dataset.trendSet;
        const rawVal = btn.dataset.value;
        let val;
        if (prop === 'frame' || prop === 'showAxes' || prop === 'showGuides' || prop === 'showTimeAxis') {
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

    // Color tabs
    section.querySelectorAll('[data-trend-colorkey]').forEach((btn) => {
      btn.addEventListener('click', () => {
        activeColorKey = btn.dataset.trendColorkey;
        section.querySelectorAll('[data-trend-colorkey]').forEach((b) => {
          b.classList.toggle('active', b === btn);
        });
        const f = selectedTrendField();
        if (f && trendColorEditor) {
          trendColorEditor.set(f[activeColorKey] ?? 0xFFFF, COLOR_NAMES[activeColorKey] || 'Color');
        }
      });
    });

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
        if (prop === 'enableSD') {
          input.checked = Boolean(f.enableSD);
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
      if (prop === 'frame' || prop === 'showAxes' || prop === 'showGuides' || prop === 'showTimeAxis') {
        const isBtnTrue = (val === 'true' || val === '1');
        const currentBool = f[prop] !== false;
        btn.classList.toggle('active', currentBool === isBtnTrue);
      } else {
        const cur = String(f[prop]);
        btn.classList.toggle('active', cur === val);
      }
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

    const ramEl = section.querySelector('[data-trend-readout="ramBytes"]');
    if (ramEl) ramEl.textContent = `${estimateRamUsage(f.visibleSamples)} bytes`;

    // Color Swatches
    updateColorSwatches(section, f);

    // Active color in color editor
    if (trendColorEditor) {
      trendColorEditor.set(f[activeColorKey] ?? 0xFFFF, COLOR_NAMES[activeColorKey] || 'Color');
    }

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
    drawAppearancePreview
  };
});
