(() => {
  'use strict';

  // A15-7B/7C/7D: inspector Propuesta 2 para TEXT, VALUE y BOOL.
  //
  // No duplica lógica de app.js: cada control escribe en el input viejo
  // equivalente (#fieldLabel, #fieldLayout, ...) y dispara input/change. Así
  // siguen funcionando validación de IDs, historial (undo/redo), render y
  // codegen. Los <details> viejos quedan ocultos con la clase .insp-v2 en la
  // sección (regla CSS con !important: designer-bar.js limpia estilos inline).

  const TYPES = new Set(['TEXT', 'VALUE', 'BOOL']);

  // Paleta RGB565 = COLORS de app.js (nombres aceptados por colorByName).
  const PALETTE = [
    ['BLACK', 0x0000, 'Negro'],
    ['WHITE', 0xFFFF, 'Blanco'],
    ['RED', 0xF800, 'Rojo'],
    ['GREEN', 0x07E0, 'Verde'],
    ['BLUE', 0x001F, 'Azul'],
    ['CYAN', 0x07FF, 'Cian'],
    ['YELLOW', 0xFFE0, 'Amarillo'],
    ['ORANGE', 0xFD20, 'Naranja']
  ];

  const BOOL_PRESETS = [
    ['OFF', 'ON'],
    ['PARADO', 'MARCHA'],
    ['CERRADO', 'ABIERTO'],
    ['NO', 'SI'],
    ['FALLA', 'OK']
  ];

  const section = document.getElementById('textFieldControlsSection');
  if (!section) return;

  const $ = (id) => document.getElementById(id);

  function editor() {
    return window.JWPLCHMIEditor || null;
  }

  function selectedField() {
    return editor()?.getSelectedField?.() || null;
  }

  function isManaged(field = selectedField()) {
    return Boolean(field && TYPES.has(field.type));
  }

  function hex565(value) {
    const n = Number(value);
    return `0x${(Number.isFinite(n) ? n & 0xFFFF : 0).toString(16).toUpperCase().padStart(4, '0')}`;
  }

  function rgb565ToCss(value) {
    const v = Number(value) & 0xFFFF;
    const r = ((v >> 11) & 0x1F) * 255 / 31;
    const g = ((v >> 5) & 0x3F) * 255 / 63;
    const b = (v & 0x1F) * 255 / 31;
    const h = (x) => Math.round(x).toString(16).padStart(2, '0');
    return `#${h(r)}${h(g)}${h(b)}`;
  }

  function cssTo565(hex) {
    const m = /^#?([0-9a-f]{6})$/i.exec(String(hex || ''));
    if (!m) return 0;
    const n = parseInt(m[1], 16);
    return (((n >> 19) & 0x1F) << 11) | (((n >> 10) & 0x3F) << 5) | ((n >> 3) & 0x1F);
  }

  function escapeHtml(text) {
    return String(text ?? '').replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));
  }

  // --- Escritura en los controles viejos -------------------------------------

  function proxyValue(id, value, commit) {
    const el = $(id);
    if (!el) return;
    if (el.tagName === 'SELECT' && ![...el.options].some((o) => o.value === String(value))) {
      const opt = document.createElement('option');
      opt.value = String(value);
      opt.textContent = String(value);
      el.appendChild(opt);
    }
    el.value = String(value);
    el.dispatchEvent(new Event(commit ? 'change' : 'input', { bubbles: true }));
  }

  // commit=false: vista previa en vivo mientras se arrastra en el selector.
  function proxyColor(id, value565, commit = true) {
    const match = PALETTE.find(([, v]) => v === (value565 & 0xFFFF));
    proxyValue(id, match ? match[0] : hex565(value565), commit);
  }

  // Propiedades sin control viejo equivalente (color por estado de BOOL):
  // se escriben en el modelo y se registra el historial con la API del editor.
  function setFieldProps(apply, commit) {
    const field = selectedField();
    if (!field) return;
    apply(field);
    editor()?.render?.();
    if (commit) editor()?.commitHistory?.();
  }

  // --- Plantilla --------------------------------------------------------------

  function layoutSvg(stacked) {
    return stacked
      ? '<svg width="34" height="18" viewBox="0 0 34 18"><rect x="1" y="1" width="32" height="16" rx="2" stroke-width="1"/><rect x="4" y="4" width="12" height="3" fill="currentColor" stroke="none"/><rect x="4" y="10" width="22" height="5" fill="currentColor" stroke="none"/></svg>'
      : '<svg width="34" height="18" viewBox="0 0 34 18"><rect x="1" y="1" width="32" height="16" rx="2" stroke-width="1"/><rect x="4" y="7" width="10" height="3" fill="currentColor" stroke="none"/><rect x="17" y="5" width="13" height="7" fill="currentColor" stroke="none"/></svg>';
  }

  function alignSvg(align) {
    const x = align === 'LEFT' ? 4 : align === 'CENTER' ? 11 : 18;
    return `<svg width="34" height="14" viewBox="0 0 34 14"><rect x="1" y="1" width="32" height="12" rx="2" stroke-width="1"/><rect x="${x}" y="4" width="12" height="6" fill="currentColor" stroke="none"/></svg>`;
  }

  function frameSvg(frame) {
    return frame
      ? '<svg width="34" height="14" viewBox="0 0 34 14"><rect x="1.5" y="1.5" width="31" height="11" rx="1" stroke-width="1.5"/><rect x="6" y="5" width="22" height="4" fill="currentColor" stroke="none"/></svg>'
      : '<svg width="34" height="14" viewBox="0 0 34 14"><rect x="1.5" y="1.5" width="31" height="11" rx="1" stroke-width="1" stroke-dasharray="2 2" opacity="0.5"/><rect x="6" y="5" width="22" height="4" fill="currentColor" stroke="none"/></svg>';
  }

  function chips(set, values, suffix = '') {
    return values.map((v) => `<button type="button" class="insp-chip" data-set="${set}" data-value="${v}">${v}${suffix}</button>`).join('');
  }

  function colorRow(key, title) {
    const pills = PALETTE.map(([name, v, label]) =>
      `<button type="button" class="insp-swatch" data-color-key="${key}" data-color="${v}" title="${label} · ${hex565(v)}" style="background:${rgb565ToCss(v)}"></button>`).join('');
    return `
      <div class="insp-color-row" data-color-row="${key}">
        <div class="insp-color-head">
          <span>${title}</span>
          <label class="insp-color-native" title="Color personalizado (se convierte a RGB565)">
            <input type="color" data-color-native="${key}" value="#ffffff" />
            <span class="insp-hex" data-color-hex="${key}">0xFFFF</span>
          </label>
        </div>
        <div class="insp-swatch-row">${pills}</div>
      </div>`;
  }

  const wrap = document.createElement('div');
  wrap.id = 'fieldInspectorV2';
  wrap.className = 'insp-wrap';
  wrap.hidden = true;
  wrap.innerHTML = `
    <div class="bar-nav-header insp-nav-header">
      <div class="bar-main-tabs" role="tablist">
        <button type="button" class="bar-main-tab insp-main-tab active" data-insp-main="design" role="tab">Diseño</button>
        <button type="button" class="bar-main-tab insp-main-tab" data-insp-main="data" role="tab">Datos</button>
        <button type="button" class="bar-main-tab insp-main-tab" data-insp-main="tech" role="tab">Técnico</button>
      </div>
      <div class="bar-sub-tabs" data-insp-subtabs role="tablist">
        <button type="button" class="bar-sub-tab insp-sub-tab active" data-insp-sub="shape" role="tab">Forma</button>
        <button type="button" class="bar-sub-tab insp-sub-tab" data-insp-sub="content" role="tab">Contenido</button>
        <button type="button" class="bar-sub-tab insp-sub-tab" data-insp-sub="appearance" role="tab">Apariencia</button>
        <button type="button" class="bar-sub-tab insp-sub-tab" data-insp-sub="placement" role="tab">Ubicación</button>
      </div>
    </div>

    <div class="insp-body">
      <!-- DISEÑO · FORMA -->
      <div class="insp-panel" data-insp-panel="design:shape">
        <div class="bar-card">
          <div class="bar-card-section-title">Distribución</div>
          <div class="bar-visual-grid-2">
            <button type="button" class="bar-visual-btn" data-set="layout" data-value="INLINE" title="Etiqueta y valor en la misma línea">${layoutSvg(false)}<span>En línea</span></button>
            <button type="button" class="bar-visual-btn" data-set="layout" data-value="STACKED" title="Etiqueta arriba, valor abajo">${layoutSvg(true)}<span>Apilado</span></button>
          </div>
        </div>
        <div class="bar-card">
          <div class="bar-card-section-title">Tamaño del valor <span class="insp-hint" data-readout="valueSizePx"></span></div>
          <div class="insp-chip-row">${chips('valueSize', [1, 2, 3, 4], '×')}</div>
          <div class="bar-card-section-title insp-gap">Tamaño de la etiqueta <span class="insp-hint" data-readout="labelSizePx"></span></div>
          <div class="insp-chip-row">${chips('labelSize', [1, 2, 3, 4], '×')}</div>
          <div class="insp-note">Fuente GLCD 5×7 escalada (celda 6×8 px por 1×).</div>
        </div>
        <div class="bar-card">
          <div class="bar-card-section-title">Marco</div>
          <div class="bar-visual-grid-2">
            <button type="button" class="bar-visual-btn" data-set="frame" data-value="0">${frameSvg(false)}<span>Sin marco</span></button>
            <button type="button" class="bar-visual-btn" data-set="frame" data-value="1">${frameSvg(true)}<span>Con marco</span></button>
          </div>
        </div>
      </div>

      <!-- DISEÑO · CONTENIDO -->
      <div class="insp-panel" data-insp-panel="design:content" hidden>
        <div class="bar-card">
          <div class="insp-two">
            <label class="field-label">Etiqueta visible
              <input class="field-input" data-proxy="fieldLabel" type="text" maxlength="24" />
            </label>
            <label class="field-label" data-only="VALUE BOOL">Unidad
              <input class="field-input" data-proxy="fieldUnit" type="text" maxlength="12" />
            </label>
          </div>
        </div>

        <div class="bar-card" data-only="VALUE">
          <div class="bar-card-section-title">Formato numérico</div>
          <div class="insp-two">
            <div>
              <span class="bar-visual-group-title">Dígitos enteros</span>
              <div class="insp-stepper" data-stepper="integerDigits" data-min="1" data-max="9">
                <button type="button" data-step="-1">−</button><strong data-readout="integerDigits">3</strong><button type="button" data-step="1">+</button>
              </div>
            </div>
            <div>
              <span class="bar-visual-group-title">Decimales</span>
              <div class="insp-chip-row" data-decimal-chips>${chips('decimalDigits', [0, 1, 2, 3])}</div>
            </div>
          </div>
          <div class="bar-visual-grid-2 insp-gap">
            <button type="button" class="bar-visual-btn insp-toggle" data-toggle="signedValue"><strong class="insp-mono">±</strong><span>Con signo</span></button>
            <button type="button" class="bar-visual-btn insp-toggle" data-toggle="leadingZeros"><strong class="insp-mono">007</strong><span>Ceros a la izquierda</span></button>
          </div>
          <div class="insp-format-view">
            <span>Reserva <code data-readout="reserve">888.8</code></span>
            <span>→</span>
            <span>Resultado <code data-readout="formatted">25.6</code></span>
          </div>
        </div>

        <div class="bar-card" data-only="TEXT">
          <div class="bar-card-section-title">Capacidad (caracteres reservados)</div>
          <div class="insp-stepper" data-stepper="capacity" data-min="1" data-max="39">
            <button type="button" data-step="-1">−</button><strong data-readout="capacity">12</strong><button type="button" data-step="1">+</button>
          </div>
          <div class="insp-ruler" title="Ancho que el firmware reserva para el texto">
            <code data-readout="ruler">MMMMMMMMMMMM</code>
            <span class="insp-hint" data-readout="rulerPx"></span>
          </div>
        </div>

        <div class="bar-card" data-only="BOOL">
          <div class="bar-card-section-title">Estados</div>
          <div class="insp-bool-states">
            <label class="insp-state-card"><span>FALSE</span>
              <input class="field-input" data-proxy="fieldFalseText" type="text" maxlength="39" />
            </label>
            <span class="insp-arrow">→</span>
            <label class="insp-state-card is-on"><span>TRUE</span>
              <input class="field-input" data-proxy="fieldTrueText" type="text" maxlength="39" />
            </label>
          </div>
          <span class="bar-visual-group-title insp-gap">Presets rápidos</span>
          <div class="insp-chip-row">
            ${BOOL_PRESETS.map(([f, t]) => `<button type="button" class="insp-chip" data-bool-preset="${f}|${t}">${f}/${t}</button>`).join('')}
          </div>
        </div>

        <div class="bar-card">
          <div class="bar-card-section-title">Alineación del valor</div>
          <div class="bar-visual-grid-3">
            <button type="button" class="bar-visual-btn" data-set="align" data-value="LEFT">${alignSvg('LEFT')}<span>Izquierda</span></button>
            <button type="button" class="bar-visual-btn" data-set="align" data-value="CENTER">${alignSvg('CENTER')}<span>Centro</span></button>
            <button type="button" class="bar-visual-btn" data-set="align" data-value="RIGHT">${alignSvg('RIGHT')}<span>Derecha</span></button>
          </div>
        </div>
      </div>

      <!-- DISEÑO · APARIENCIA -->
      <div class="insp-panel" data-insp-panel="design:appearance" hidden>
        <div class="bar-card">
          ${colorRow('label', 'Etiqueta')}
          <div data-value-color-row>${colorRow('value', 'Valor')}</div>
          ${colorRow('background', 'Fondo')}
          <div data-frame-only>${colorRow('frame', 'Marco')}</div>
          <div class="insp-note">El fondo es obligatorio: el refresco parcial de la TFT borra el valor anterior pintando el fondo.</div>
        </div>
        <div class="bar-card" data-only="BOOL">
          <div class="bar-card-section-title">Color del valor</div>
          <div class="bar-visual-grid-2">
            <button type="button" class="bar-visual-btn" data-bool-colormode="single"><strong>ON</strong><span>Un color</span></button>
            <button type="button" class="bar-visual-btn" data-bool-colormode="state"><strong><span style="color:#ff5050">OFF</span> / <span style="color:#3ce36b">ON</span></strong><span>Por estado</span></button>
          </div>
          <div data-bool-state-colors>
            ${colorRow('false', 'Estado FALSE')}
            ${colorRow('true', 'Estado TRUE')}
            <div class="insp-note">Requiere JWPLC_Display con color por estado (Alpha15); con un package anterior el header compila con un solo color.</div>
          </div>
        </div>
      </div>

      <!-- DISEÑO · UBICACIÓN -->
      <div class="insp-panel" data-insp-panel="design:placement" hidden>
        <div class="bar-card">
          <div class="bar-card-section-title">Página</div>
          <div class="insp-chip-row" data-page-chips></div>
        </div>
        <div class="bar-card">
          <div class="insp-two">
            <label class="field-label">X (px)
              <input class="field-input" data-proxy="fieldX" type="number" min="0" max="319" />
            </label>
            <label class="field-label">Y (px)
              <input class="field-input" data-proxy="fieldY" type="number" min="0" max="169" />
            </label>
          </div>
          <div class="insp-readouts">
            <span>Tamaño calculado <strong data-mirror="fieldBoundsStatus">—</strong></span>
          </div>
        </div>
      </div>

      <!-- DATOS -->
      <div class="insp-panel" data-insp-panel="data" hidden>
        <div class="bar-card">
          <div class="bar-card-section-title">Variable vinculada <span class="insp-mode-chip" title="Modo del proyecto vinculado">Arduino</span></div>
          <label class="field-label">Variable C++
            <input class="field-input code-input" data-proxy="fieldVariable" type="text" maxlength="32" />
          </label>
          <div class="insp-readouts">
            <span>Tipo <code class="insp-type-chip" data-readout="cppType">float</code></span>
          </div>
          <div class="insp-note">Proyecto OpenPLC: aquí se elegirá la variable PLC existente (A15-6).</div>
        </div>

        <div class="bar-card" data-only="VALUE">
          <div class="bar-card-section-title">Valor de prueba <span class="insp-hint">sólo para el lienzo</span></div>
          <div class="insp-two">
            <input class="field-input" data-proxy="fieldPreview" type="text" maxlength="16" />
            <input class="bar-top-slider-input" data-preview-slider type="range" />
          </div>
          <div class="insp-hint" data-readout="sliderRange"></div>
        </div>

        <div class="bar-card" data-only="TEXT">
          <div class="bar-card-section-title">Valor de prueba <span class="insp-hint" data-readout="textCounter">0/12</span></div>
          <input class="field-input" data-proxy="fieldPreview" data-text-preview type="text" maxlength="39" />
        </div>

        <div class="bar-card" data-only="BOOL">
          <div class="bar-card-section-title">Estado de prueba</div>
          <div class="bar-visual-grid-2">
            <button type="button" class="bar-visual-btn" data-bool-preview="0"><strong data-readout="falseText">OFF</strong><span>FALSE</span></button>
            <button type="button" class="bar-visual-btn" data-bool-preview="1"><strong data-readout="trueText">ON</strong><span>TRUE</span></button>
          </div>
        </div>
      </div>

      <!-- TÉCNICO -->
      <div class="insp-panel" data-insp-panel="tech" hidden>
        <div class="bar-card">
          <label class="field-label">Nombre del objeto (sólo Designer)
            <input class="field-input" data-proxy="fieldName" type="text" maxlength="24" />
          </label>
          <label class="field-label">ID C++ (enum HMIFieldId)
            <input class="field-input code-input" data-proxy="fieldId" type="text" maxlength="32" />
          </label>
        </div>
        <div class="bar-card">
          <div class="bar-card-header">
            <span class="bar-card-title">Contrato C++</span>
            <button type="button" class="icon-button" data-copy-contract title="Copiar contrato">⧉</button>
          </div>
          <pre class="insp-contract" data-mirror="inspectorContract"></pre>
        </div>
        <div class="bar-card">
          <div class="bar-card-section-title">Diagnóstico de geometría</div>
          <div class="insp-readouts insp-readouts-grid">
            <span>Ancho/alto <strong data-mirror="fieldBoundsStatus">—</strong></span>
            <span>Región de valor <strong data-mirror="fieldValueBoundsStatus">—</strong></span>
            <span>Valor XY <strong data-mirror="fieldValueXYStatus">—</strong></span>
            <span>Padding <strong data-mirror="fieldPadStatus">—</strong></span>
            <span>Gap <strong>4 px</strong></span>
            <span>Layout <strong data-mirror="fieldLayoutStatus">—</strong></span>
          </div>
        </div>
      </div>
    </div>`;

  section.querySelector('.inspector-title')?.insertAdjacentElement('afterend', wrap);

  // --- Navegación -------------------------------------------------------------

  const state = { main: 'design', sub: 'shape' };

  function applyTabs() {
    wrap.querySelectorAll('[data-insp-main]').forEach((b) => b.classList.toggle('active', b.dataset.inspMain === state.main));
    wrap.querySelectorAll('[data-insp-sub]').forEach((b) => b.classList.toggle('active', b.dataset.inspSub === state.sub));
    wrap.querySelector('[data-insp-subtabs]').hidden = state.main !== 'design';
    const key = state.main === 'design' ? `design:${state.sub}` : state.main;
    wrap.querySelectorAll('[data-insp-panel]').forEach((p) => { p.hidden = p.dataset.inspPanel !== key; });
  }

  wrap.querySelectorAll('[data-insp-main]').forEach((b) => b.addEventListener('click', () => {
    state.main = b.dataset.inspMain;
    applyTabs();
  }));
  wrap.querySelectorAll('[data-insp-sub]').forEach((b) => b.addEventListener('click', () => {
    state.sub = b.dataset.inspSub;
    applyTabs();
  }));

  // --- Eventos de edición -----------------------------------------------------

  const SET_TARGET = {
    layout: 'fieldLayout',
    align: 'fieldAlign',
    frame: 'fieldFrame',
    valueSize: 'fieldValueSize',
    labelSize: 'fieldLabelSize',
    decimalDigits: 'fieldDecimalDigits'
  };

  wrap.addEventListener('click', (e) => {
    const setBtn = e.target.closest('[data-set]');
    if (setBtn && !setBtn.disabled) {
      proxyValue(SET_TARGET[setBtn.dataset.set], setBtn.dataset.value, true);
      return;
    }
    const toggle = e.target.closest('[data-toggle]');
    if (toggle) {
      const field = selectedField();
      const target = toggle.dataset.toggle === 'signedValue' ? 'fieldSigned' : 'fieldLeadingZeros';
      proxyValue(target, field?.[toggle.dataset.toggle] ? '0' : '1', true);
      return;
    }
    const step = e.target.closest('[data-step]');
    if (step) {
      const box = step.closest('[data-stepper]');
      const field = selectedField();
      const prop = box.dataset.stepper;
      const next = Math.min(Number(box.dataset.max), Math.max(Number(box.dataset.min), Number(field?.[prop] || 0) + Number(step.dataset.step)));
      proxyValue(prop === 'capacity' ? 'fieldCapacity' : 'fieldIntegerDigits', next, true);
      return;
    }
    const swatch = e.target.closest('[data-color-key]');
    if (swatch) {
      const key = swatch.dataset.colorKey;
      if (BOOL_STATE_PROP[key]) setFieldProps((f) => { f[BOOL_STATE_PROP[key]] = Number(swatch.dataset.color); }, true);
      else proxyColor(COLOR_TARGET[key], Number(swatch.dataset.color));
      return;
    }
    const colorMode = e.target.closest('[data-bool-colormode]');
    if (colorMode) {
      setFieldProps((f) => {
        f.stateColors = colorMode.dataset.boolColormode === 'state';
        if (!Number.isFinite(Number(f.falseColor))) f.falseColor = 0xF800;
        if (!Number.isFinite(Number(f.trueColor))) f.trueColor = 0x07E0;
      }, true);
      return;
    }
    const preset = e.target.closest('[data-bool-preset]');
    if (preset) {
      const [f, t] = preset.dataset.boolPreset.split('|');
      proxyValue('fieldFalseText', f, false);
      proxyValue('fieldTrueText', t, true);
      return;
    }
    const boolPreview = e.target.closest('[data-bool-preview]');
    if (boolPreview) {
      proxyValue('fieldBoolPreview', boolPreview.dataset.boolPreview, true);
      return;
    }
    const page = e.target.closest('[data-page-id]');
    if (page) {
      editor()?.moveSelectedFieldToPage?.(Number(page.dataset.pageId));
      return;
    }
    const copy = e.target.closest('[data-copy-contract]');
    if (copy) {
      const text = $('inspectorContract')?.textContent || '';
      navigator.clipboard?.writeText(text).then(() => {
        copy.textContent = '✓';
        setTimeout(() => { copy.textContent = '⧉'; }, 900);
      }).catch(() => {});
    }
  });

  wrap.querySelectorAll('[data-proxy]').forEach((input) => {
    input.addEventListener('input', () => proxyValue(input.dataset.proxy, input.value, false));
    input.addEventListener('change', () => proxyValue(input.dataset.proxy, input.value, true));
  });

  const COLOR_TARGET = {
    label: 'fieldLabelColor',
    value: 'fieldValueColor',
    background: 'fieldBackgroundColor',
    frame: 'fieldFrameColor'
  };
  const COLOR_PROP = { label: 'labelColor', value: 'valueColor', background: 'backgroundColor', frame: 'frameColor' };
  const BOOL_STATE_PROP = { false: 'falseColor', true: 'trueColor' };

  // 'input' llega mientras se arrastra en el selector: el lienzo se actualiza
  // en vivo y el historial se registra una sola vez en 'change'.
  wrap.querySelectorAll('[data-color-native]').forEach((input) => {
    const apply = (commit) => {
      const key = input.dataset.colorNative;
      const value = cssTo565(input.value);
      if (BOOL_STATE_PROP[key]) setFieldProps((f) => { f[BOOL_STATE_PROP[key]] = value; }, commit);
      else proxyColor(COLOR_TARGET[key], value, commit);
    };
    input.addEventListener('input', () => apply(false));
    input.addEventListener('change', () => apply(true));
  });

  const slider = wrap.querySelector('[data-preview-slider]');
  slider.addEventListener('input', () => proxyValue('fieldPreview', slider.value, false));
  slider.addEventListener('change', () => proxyValue('fieldPreview', slider.value, true));

  // --- Sincronización desde el modelo ----------------------------------------

  function setInput(input, value) {
    if (document.activeElement === input) return;
    const text = String(value ?? '');
    if (input.value !== text) input.value = text;
  }

  function setActive(selector, value) {
    wrap.querySelectorAll(selector).forEach((b) => b.classList.toggle('active', b.dataset.value === String(value)));
  }

  function readout(name, text) {
    wrap.querySelectorAll(`[data-readout="${name}"]`).forEach((el) => { el.textContent = text; });
  }

  function valueRange(field) {
    const ints = Math.max(1, Number(field.integerDigits) || 1);
    const decs = Math.max(0, Number(field.decimalDigits) || 0);
    const max = Number((10 ** ints - 10 ** -decs).toFixed(decs));
    return { min: field.signedValue ? -max : 0, max, step: decs ? 10 ** -decs : 1, decs };
  }

  function cppTypeFor(field) {
    if (field.type === 'VALUE') return 'float';
    if (field.type === 'BOOL') return 'bool';
    return `char[${Math.max(1, Number(field.capacity) || 1) + 1}]`;
  }

  function syncPages(field) {
    const host = wrap.querySelector('[data-page-chips]');
    const pages = editor()?.getPages?.() || [];
    const current = Number(field.page || 0);
    const signature = pages.map((p) => `${p.id}:${p.name}`).join('|') + `#${current}`;
    if (host.dataset.signature === signature) return;
    host.dataset.signature = signature;
    host.innerHTML = pages.map((p) =>
      `<button type="button" class="insp-chip${Number(p.id) === current ? ' active' : ''}" data-page-id="${p.id}" title="${escapeHtml(p.name)}">${String(Number(p.id) + 1).padStart(2, '0')} · ${escapeHtml(p.name)}</button>`).join('');
  }

  function syncColors(field) {
    const stateColors = field.type === 'BOOL' && Boolean(field.stateColors);
    wrap.querySelectorAll('[data-bool-colormode]').forEach((b) =>
      b.classList.toggle('active', (b.dataset.boolColormode === 'state') === stateColors));
    wrap.querySelector('[data-bool-state-colors]').hidden = !stateColors;
    wrap.querySelector('[data-value-color-row]').hidden = stateColors;
    const props = field.type === 'BOOL' ? { ...COLOR_PROP, ...BOOL_STATE_PROP } : COLOR_PROP;
    Object.entries(props).forEach(([key, prop]) => {
      const raw = field[prop];
      const v = Number.isFinite(Number(raw)) ? Number(raw) & 0xFFFF : 0;
      wrap.querySelectorAll(`[data-color-key="${key}"]`).forEach((b) => b.classList.toggle('active', Number(b.dataset.color) === v));
      const native = wrap.querySelector(`[data-color-native="${key}"]`);
      if (native && document.activeElement !== native) native.value = rgb565ToCss(v);
      const hexEl = wrap.querySelector(`[data-color-hex="${key}"]`);
      if (hexEl) hexEl.textContent = hex565(v);
    });
    wrap.querySelector('[data-frame-only]').hidden = !field.frame;
  }

  function sync() {
    const field = selectedField();
    const managed = isManaged(field);
    section.classList.toggle('insp-v2', managed);
    wrap.hidden = !managed;
    if (!managed) return;

    // El firmware no tiene fondo transparente para campos: normalizar
    // proyectos viejos que lo guardaron (codegen emitía 0xTRANSPARENT).
    if (!Number.isFinite(Number(field.backgroundColor))) {
      proxyColor('fieldBackgroundColor', 0x0000);
      return;
    }

    wrap.querySelectorAll('[data-only]').forEach((el) => {
      el.hidden = !el.dataset.only.split(' ').includes(field.type);
    });

    wrap.querySelectorAll('[data-proxy]').forEach((input) => {
      const source = $(input.dataset.proxy);
      if (source) setInput(input, source.value);
    });

    setActive('[data-set="layout"]', field.layout || 'INLINE');
    setActive('[data-set="align"]', field.align || 'RIGHT');
    setActive('[data-set="frame"]', field.frame ? '1' : '0');
    setActive('[data-set="valueSize"]', Number(field.valueSize) || 1);
    setActive('[data-set="labelSize"]', Number(field.labelSize) || 1);
    readout('valueSizePx', `${8 * (Number(field.valueSize) || 1)} px de alto`);
    readout('labelSizePx', `${8 * (Number(field.labelSize) || 1)} px de alto`);

    if (field.type === 'VALUE') {
      const decs = Number(field.decimalDigits) || 0;
      const decHost = wrap.querySelector('[data-decimal-chips]');
      const options = decs > 3 ? [0, 1, 2, 3, decs] : [0, 1, 2, 3];
      if (decHost.dataset.options !== options.join(',')) {
        decHost.dataset.options = options.join(',');
        decHost.innerHTML = chips('decimalDigits', options);
      }
      setActive('[data-set="decimalDigits"]', decs);
      readout('integerDigits', String(field.integerDigits || 1));
      wrap.querySelectorAll('[data-toggle]').forEach((b) => b.classList.toggle('active', Boolean(field[b.dataset.toggle])));
      readout('reserve', $('valueFormatSampleStatus')?.textContent || '');
      readout('formatted', $('valueFormattedStatus')?.textContent || '');

      const range = valueRange(field);
      slider.min = String(range.min);
      slider.max = String(range.max);
      slider.step = String(range.step);
      if (document.activeElement !== slider) slider.value = String(Number(field.preview) || 0);
      readout('sliderRange', `Rango de la reserva: ${range.min.toFixed(range.decs)} … ${range.max.toFixed(range.decs)}`);
    }

    if (field.type === 'TEXT') {
      const cap = Math.max(1, Number(field.capacity) || 1);
      readout('capacity', String(cap));
      readout('ruler', 'M'.repeat(cap));
      readout('rulerPx', `${cap * 6 * (Number(field.valueSize) || 1)} px`);
      readout('textCounter', `${String(field.preview || '').length}/${cap}`);
      const textPreview = wrap.querySelector('[data-text-preview]');
      textPreview.maxLength = cap;
    }

    if (field.type === 'BOOL') {
      readout('falseText', field.falseText || 'OFF');
      readout('trueText', field.trueText || 'ON');
      wrap.querySelectorAll('[data-bool-preview]').forEach((b) => b.classList.toggle('active', (b.dataset.boolPreview === '1') === Boolean(field.boolValue)));
      wrap.querySelectorAll('[data-bool-preset]').forEach((b) => b.classList.toggle('active', b.dataset.boolPreset === `${field.falseText}|${field.trueText}`));
    }

    readout('cppType', cppTypeFor(field));
    syncColors(field);
    syncPages(field);

    wrap.querySelectorAll('[data-mirror]').forEach((el) => {
      const text = $(el.dataset.mirror)?.textContent ?? '';
      if (el.textContent !== text) el.textContent = text;
    });

    applyTabs();
  }

  window.addEventListener('jwplc:editor-refresh', sync);
  window.JWPLCHMIFieldInspector = { sync, isManaged, tabs: state };
  setTimeout(sync, 0);
})();
