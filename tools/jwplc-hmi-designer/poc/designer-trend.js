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

  function applyPreset(field, presetKey) {
    if (!field || !PRESETS[presetKey]) return false;
    const p = PRESETS[presetKey];
    field.width = p.width;
    field.height = p.height;
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
  let activeSubTab = 'shape';
  let activeColorKey = 'curveColor';
  let trendColorEditor = null;

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

  function ensureTrendInspectorSection() {
    if (typeof document === 'undefined') return null;

    let section = document.getElementById('trendInspectorSection');
    if (section) return section;

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
            <button type="button" class="bar-sub-tab active" data-trend-sub="shape">Forma</button>
            <button type="button" class="bar-sub-tab" data-trend-sub="content">Contenido</button>
            <button type="button" class="bar-sub-tab" data-trend-sub="appearance">Apariencia</button>
            <button type="button" class="bar-sub-tab" data-trend-sub="placement">Ubicación</button>
          </div>
        </div>

        <div class="insp-body">
          <!-- PANEL: Forma -->
          <div class="insp-panel" data-trend-panel="design:shape">
            <div class="bar-card">
              <div class="bar-card-section-title">Presets rápidos de tamaño</div>
              <div class="bar-visual-grid-2">
                <button type="button" class="bar-visual-btn" data-trend-preset="fullscreen">
                  <strong>Pantalla Completa</strong>
                  <span style="font-size:10px; opacity:0.8;">300 × 140 px</span>
                </button>
                <button type="button" class="bar-visual-btn" data-trend-preset="compact">
                  <strong>Recuadro Compacto</strong>
                  <span style="font-size:10px; opacity:0.8;">160 × 80 px</span>
                </button>
              </div>
            </div>

            <div class="bar-card">
              <div class="bar-card-section-title">Grosor de la curva</div>
              <div class="bar-visual-grid-2">
                <button type="button" class="bar-visual-btn" data-trend-set="lineWidth" data-value="1"><span>1 px (Fino)</span></button>
                <button type="button" class="bar-visual-btn" data-trend-set="lineWidth" data-value="2"><span>2 px (Nítido)</span></button>
              </div>
            </div>

            <div class="bar-card">
              <div class="bar-card-section-title">Guías y Ejes</div>
              <label class="field-label" style="flex-direction:row; justify-content:space-between; align-items:center;">
                <span>Marco exterior del gráfico</span>
                <input type="checkbox" data-trend-prop="frame" />
              </label>
              <label class="field-label insp-gap" style="flex-direction:row; justify-content:space-between; align-items:center;">
                <span>Eje Y y 3 líneas guía (Auto/Manual)</span>
                <input type="checkbox" data-trend-prop="showAxes" />
              </label>
              <label class="field-label insp-gap" style="flex-direction:row; justify-content:space-between; align-items:center;">
                <span>Eje X con marcas de tiempo</span>
                <input type="checkbox" data-trend-prop="showTimeAxis" />
              </label>
            </div>
          </div>

          <!-- PANEL: Contenido -->
          <div class="insp-panel" data-trend-panel="design:content" hidden>
            <div class="bar-card">
              <label class="field-label">Título / Identificador
                <input class="field-input" data-trend-prop="title" type="text" maxlength="16" placeholder="TEND" />
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
          <div class="insp-panel" data-trend-panel="design:appearance" hidden>
            <div class="bar-card">
              <div class="bar-card-section-title">Elemento a colorear</div>
              <div class="insp-chip-row" data-trend-color-tabs>
                <button type="button" class="insp-chip active" data-trend-colorkey="curveColor">
                  <span class="trend-color-swatch" data-trend-swatch="curveColor" style="display:inline-block; width:10px; height:10px; border-radius:2px; margin-right:4px;"></span>
                  Curva
                </button>
                <button type="button" class="insp-chip" data-trend-colorkey="axesColor">
                  <span class="trend-color-swatch" data-trend-swatch="axesColor" style="display:inline-block; width:10px; height:10px; border-radius:2px; margin-right:4px;"></span>
                  Ejes/Guías
                </button>
                <button type="button" class="insp-chip" data-trend-colorkey="textColor">
                  <span class="trend-color-swatch" data-trend-swatch="textColor" style="display:inline-block; width:10px; height:10px; border-radius:2px; margin-right:4px;"></span>
                  Etiquetas
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

    // Visual buttons (lineWidth, scaleMode, decimals)
    section.querySelectorAll('[data-trend-set]').forEach((btn) => {
      btn.addEventListener('click', () => {
        const prop = btn.dataset.trendSet;
        const rawVal = btn.dataset.value;
        const val = (prop === 'lineWidth' || prop === 'decimals') ? Number(rawVal) : rawVal;
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
          trendColorEditor.setColor(f[activeColorKey] ?? 0xFFFF);
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
          else if (isNum) f[prop] = Number(input.value) || 0;
          else f[prop] = input.value;
        });
      });

      if (!isCheck) {
        input.addEventListener('input', () => {
          const field = selectedTrendField();
          if (!field) return;
          if (isNum) field[prop] = Number(input.value) || 0;
          else field[prop] = input.value;
          editor()?.render?.();
        });
      }
    });
  }

  function switchTab(section) {
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
  }

  function syncInspector(field) {
    const f = field || selectedTrendField();
    const section = ensureTrendInspectorSection();
    if (!section || !f) return;

    // Sync input fields
    section.querySelectorAll('[data-trend-prop]').forEach((input) => {
      const prop = input.dataset.trendProp;
      if (f[prop] === undefined) return;
      if (input.type === 'checkbox') {
        input.checked = Boolean(f[prop]);
      } else {
        if (document.activeElement !== input) {
          input.value = String(f[prop]);
        }
      }
    });

    // Visual button states
    section.querySelectorAll('[data-trend-set]').forEach((btn) => {
      const prop = btn.dataset.trendSet;
      const val = btn.dataset.value;
      const cur = String(f[prop]);
      btn.classList.toggle('active', cur === val);
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
      trendColorEditor.setColor(f[activeColorKey] ?? 0xFFFF);
    }

    // Code preview
    const codeEl = section.querySelector('[data-trend-readout="code"]');
    if (codeEl) {
      const id = f.id || 'FIELD_TREND_1';
      const codegenCore = window.JWPLCHMICodegenCore;
      codeEl.textContent = codegenCore?.trendFieldBlock ? codegenCore.trendFieldBlock(f, id) : `JWPLC_UITrendField(...)`;
    }

    switchTab(section);
  }

  // Auto-init on DOM ready
  if (typeof document !== 'undefined') {
    if (document.readyState === 'loading') {
      document.addEventListener('DOMContentLoaded', () => ensureTrendInspectorSection());
    } else {
      setTimeout(() => ensureTrendInspectorSection(), 0);
    }
  }

  return {
    PRESETS,
    FREQUENCIES,
    applyPreset,
    frequencyToMs,
    msToFrequencyLabel,
    estimateRamUsage,
    ensureTrendInspectorSection,
    syncInspector
  };
});
