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

  // --- Apariencia: mini-modelos con la leyenda de colores ------------------
  // Igual que el MENU: una vista real del campo (renderFieldPreview de app.js,
  // mismo dibujo que el lienzo) y a su lado los colores de cada parte. BOOL
  // "por estado" muestra los dos casos con el texto y el fondo de cada uno.
  const SWATCH_LETTER = { label: 'T', value: 'T', background: 'F', frame: 'M', false: 'T', true: 'T', falseBg: 'F', trueBg: 'F' };
  const SWATCH_NAME = {
    label: 'Etiqueta · texto', value: 'Valor · texto', background: 'Fondo del campo', frame: 'Marco',
    false: 'FALSE · texto', true: 'TRUE · texto', falseBg: 'FALSE · fondo', trueBg: 'TRUE · fondo'
  };

  function swatchHtml(key) {
    return `<button type="button" class="menu-swatch" data-field-swatch="${key}" title="${SWATCH_NAME[key]}">` +
      `<span>${SWATCH_LETTER[key]}</span></button>`;
  }

  function legendRow(label, keys) {
    return `<div class="menu-legend-row" data-legend-key="${keys[0]}"><span class="menu-legend-label">${label}</span>${keys.map(swatchHtml).join('')}</div>`;
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
          <div class="bar-card-section-title">Tamaño de la etiqueta <span class="insp-hint" data-readout="labelSizePx"></span></div>
          <div class="insp-chip-row">${chips('labelSize', [1, 2, 3, 4], '×')}</div>
          <div class="bar-card-section-title insp-gap">Tamaño del valor <span class="insp-hint" data-readout="valueSizePx"></span></div>
          <div class="insp-chip-row">${chips('valueSize', [1, 2, 3, 4], '×')}</div>
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
          <div class="bar-card-section-title">Formato</div>
          <div class="bar-visual-grid-3">
            <button type="button" class="bar-visual-btn" data-vkind="NUMBER"><strong class="insp-mono">123.4</strong><span>Número</span></button>
            <button type="button" class="bar-visual-btn" data-vkind="DURATION"><strong class="insp-mono">05:03</strong><span>Duración</span></button>
            <button type="button" class="bar-visual-btn" data-vkind="DATETIME"><strong class="insp-mono">14:35</strong><span>Fecha/hora</span></button>
          </div>

          <div data-vkind-panel="NUMBER">
            <div class="insp-two insp-gap">
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
            <span class="bar-visual-group-title insp-gap">Separador de miles</span>
            <div class="bar-visual-grid-4">
              <button type="button" class="bar-visual-btn" data-vsep=""><strong class="insp-mono">1234</strong><span>Ninguno</span></button>
              <button type="button" class="bar-visual-btn" data-vsep=","><strong class="insp-mono">1,234</strong><span>Coma</span></button>
              <button type="button" class="bar-visual-btn" data-vsep="."><strong class="insp-mono">1.234</strong><span>Punto</span></button>
              <button type="button" class="bar-visual-btn" data-vsep=" "><strong class="insp-mono">1 234</strong><span>Espacio</span></button>
            </div>
            <div class="insp-note" data-vsep-note hidden>Con punto como separador, el decimal se muestra con coma (1.234,5).</div>
          </div>

          <div data-vkind-panel="DURATION" hidden>
            <span class="bar-visual-group-title insp-gap">Cómo se ve</span>
            <div class="bar-visual-grid-2">
              <button type="button" class="bar-visual-btn" data-vpattern="SS"><strong class="insp-mono">45</strong><span>Segundos</span></button>
              <button type="button" class="bar-visual-btn" data-vpattern="MM_SS"><strong class="insp-mono">05:03</strong><span>Min : seg</span></button>
              <button type="button" class="bar-visual-btn" data-vpattern="HH_MM_SS"><strong class="insp-mono">01:05:03</strong><span>Hora : min : seg</span></button>
              <button type="button" class="bar-visual-btn" data-vpattern="COMPACT"><strong class="insp-mono">1h05m</strong><span>Compacto</span></button>
            </div>
            <div class="insp-two insp-gap">
              <div>
                <span class="bar-visual-group-title" data-readout="leadTitle">Dígitos de los minutos</span>
                <div class="insp-stepper" data-stepper="integerDigits" data-min="1" data-max="6">
                  <button type="button" data-step="-1">−</button><strong data-readout="integerDigits">2</strong><button type="button" data-step="1">+</button>
                </div>
              </div>
              <div>
                <span class="bar-visual-group-title">La variable trae</span>
                <div class="insp-chip-row">
                  <button type="button" class="insp-chip" data-vunit="S">segundos</button>
                  <button type="button" class="insp-chip" data-vunit="MS">milisegundos</button>
                </div>
              </div>
            </div>
            <div class="bar-visual-grid-2 insp-gap">
              <button type="button" class="bar-visual-btn insp-toggle" data-toggle="signedValue"><strong class="insp-mono">−01:15</strong><span>Admite negativos</span></button>
            </div>
            <div class="insp-note">El programa calcula el tiempo (transcurrido o restante) y VALUE sólo lo muestra, truncado a segundos. Si no cabe en los dígitos elegidos se ve <code data-readout="overflow">--:--</code>.</div>
          </div>

          <div data-vkind-panel="DATETIME" hidden>
            <span class="bar-visual-group-title insp-gap">Qué muestra</span>
            <div class="bar-visual-grid-2">
              <button type="button" class="bar-visual-btn" data-vdt="HH_MM"><strong class="insp-mono">14:35</strong><span>Hora</span></button>
              <button type="button" class="bar-visual-btn" data-vdt="HH_MM_SS"><strong class="insp-mono">14:35:07</strong><span>Hora con segundos</span></button>
              <button type="button" class="bar-visual-btn" data-vdt="DD_MM"><strong class="insp-mono">18/10</strong><span>Día y mes</span></button>
              <button type="button" class="bar-visual-btn" data-vdt="DD_MM_YYYY"><strong class="insp-mono">18/10/2026</strong><span>Fecha</span></button>
              <button type="button" class="bar-visual-btn" data-vdt="DD_MM_YYYY_HH_MM"><strong class="insp-mono">18/10/2026 14:35</strong><span>Fecha y hora</span></button>
              <button type="button" class="bar-visual-btn insp-toggle" data-rtc-invalid><strong class="insp-mono">--:--</strong><span>Ver sin reloj válido</span></button>
            </div>
            <div class="insp-note">Lee el reloj de la placa (RTC): no lleva variable. La TFT se repinta sólo cuando cambia lo que se ve (el minuto o el segundo). Sin hora válida (sin pila o sin ajustar) se ve <code data-readout="overflow">--:--</code>. En el lienzo se ve la hora del PC.</div>
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
          <div class="bar-card-section-title">Representación</div>
          <div class="bar-visual-grid-2">
            <button type="button" class="bar-visual-btn" data-bool-style="TEXT"><strong class="insp-mono">OFF / ON</strong><span>Texto</span></button>
            <button type="button" class="bar-visual-btn" data-bool-style="PILOT"><strong><span class="insp-pilot-dot is-off"></span> <span class="insp-pilot-dot is-on"></span></strong><span>Piloto</span></button>
          </div>
          <div class="insp-note" data-bool-pilot-note hidden>Un círculo de 7 px por escala, a la altura del texto: aro del color de la etiqueta y relleno del color de cada estado (Apariencia).</div>
        </div>

        <div class="bar-card" data-only="BOOL" data-bool-text-only>
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

        <div class="bar-card" data-bool-text-only>
          <div class="bar-card-section-title">Alineación del valor</div>
          <div class="bar-visual-grid-3">
            <button type="button" class="bar-visual-btn" data-set="align" data-value="LEFT">${alignSvg('LEFT')}<span>Izquierda</span></button>
            <button type="button" class="bar-visual-btn" data-set="align" data-value="CENTER">${alignSvg('CENTER')}<span>Centro</span></button>
            <button type="button" class="bar-visual-btn" data-set="align" data-value="RIGHT">${alignSvg('RIGHT')}<span>Derecha</span></button>
          </div>
          <div class="insp-note" data-align-unit-hint hidden>El valor ocupa la reserva más larga (<code data-readout="reserveShort"></code>) y la unidad va después de ella. Con «Derecha» el valor queda pegado a la unidad.</div>
        </div>
      </div>

      <!-- DISEÑO · APARIENCIA -->
      <div class="insp-panel" data-insp-panel="design:appearance" hidden>
        <div class="bar-card" data-only="BOOL" data-bool-text-only>
          <div class="bar-card-section-title">Color del valor</div>
          <div class="bar-visual-grid-2">
            <button type="button" class="bar-visual-btn" data-bool-colormode="single"><strong>ON</strong><span>Un color</span></button>
            <button type="button" class="bar-visual-btn" data-bool-colormode="state"><strong><span style="color:#ff5050">OFF</span> / <span style="color:#3ce36b">ON</span></strong><span>Por estado (texto y fondo)</span></button>
          </div>
        </div>
        <div class="bar-card" data-field-appearance></div>
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
        <div class="bar-card" data-clock-only hidden>
          <div class="bar-card-section-title">Reloj de la placa</div>
          <div class="insp-note">Este VALUE muestra la fecha/hora del RTC y no lleva variable ni setter: el firmware lo actualiza solo. Para cambiarlo a un número, elige «Número» en Diseño · Contenido · Formato.</div>
        </div>

        <div class="bar-card" data-not-clock>
          <div class="bar-card-section-title">Variable vinculada <span class="insp-mode-chip" title="Modo del proyecto vinculado">Arduino</span></div>
          <label class="field-label">Variable C++
            <input class="field-input code-input" data-proxy="fieldVariable" type="text" maxlength="32" />
          </label>
          <div class="insp-readouts">
            <span>Tipo <code class="insp-type-chip" data-readout="cppType">float</code></span>
          </div>
          <div class="insp-note">Proyecto OpenPLC: aquí se elegirá la variable PLC existente (A15-6).</div>
        </div>

        <div class="bar-card" data-only="VALUE" data-not-clock>
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
    syncAppearance(selectedField());
  }));
  wrap.querySelectorAll('[data-insp-sub]').forEach((b) => b.addEventListener('click', () => {
    state.sub = b.dataset.inspSub;
    applyTabs();
    syncAppearance(selectedField());
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
    const kindBtn = e.target.closest('[data-vkind]');
    if (kindBtn) {
      const kind = kindBtn.dataset.vkind;
      setFieldProps((f) => {
        if ((f.valueKind || 'NUMBER') === kind) return;
        f.valueKind = kind;
        if (kind === 'DATETIME') {
          f.dateTimePattern = f.dateTimePattern || 'HH_MM';
          f.integerDigits = 2;
          f.decimalDigits = 0;
          f.leadingZeros = false;
          f.signedValue = false;
        } else if (kind === 'DURATION') {
          f.durationPattern = f.durationPattern || 'MM_SS';
          f.durationUnit = f.durationUnit || 'S';
          f.integerDigits = 2;
          f.decimalDigits = 0;
          f.leadingZeros = false;
          f.preview = f.durationUnit === 'MS' ? '303000' : '303';
        } else {
          f.integerDigits = 3;
          f.preview = '25';
        }
      }, true);
      return;
    }
    const dtBtn = e.target.closest('[data-vdt]');
    if (dtBtn) {
      setFieldProps((f) => { f.dateTimePattern = dtBtn.dataset.vdt; }, true);
      return;
    }
    const rtcInvalid = e.target.closest('[data-rtc-invalid]');
    if (rtcInvalid) {
      setFieldProps((f) => { f.rtcPreview = f.rtcPreview === 'INVALID' ? '' : 'INVALID'; }, true);
      return;
    }
    const sepBtn = e.target.closest('[data-vsep]');
    if (sepBtn) {
      setFieldProps((f) => { f.thousandsSep = sepBtn.dataset.vsep; }, true);
      return;
    }
    const patternBtn = e.target.closest('[data-vpattern]');
    if (patternBtn) {
      setFieldProps((f) => { f.durationPattern = patternBtn.dataset.vpattern; }, true);
      return;
    }
    const unitBtn = e.target.closest('[data-vunit]');
    if (unitBtn) {
      setFieldProps((f) => {
        const next = unitBtn.dataset.vunit;
        if ((f.durationUnit || 'S') === next) return;
        // Misma duración en la otra unidad: la vista previa no cambia.
        const v = Number(f.preview) || 0;
        f.preview = String(next === 'MS' ? Math.round(v * 1000) : Math.floor(v / 1000));
        f.durationUnit = next;
      }, true);
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
    // Clic en el cuadro o en el texto de la fila: elige qué color edita el
    // editor fijo de abajo (no abre ninguna ventana).
    const swatch = e.target.closest('[data-field-swatch]');
    const legendRow = e.target.closest('[data-field-appearance] [data-legend-key]');
    if (swatch || legendRow) {
      activeColorKey = swatch ? swatch.dataset.fieldSwatch : legendRow.dataset.legendKey;
      syncAppearance(selectedField());
      return;
    }
    const colorMode = e.target.closest('[data-bool-colormode]');
    if (colorMode) {
      setFieldProps((f) => {
        f.stateColors = colorMode.dataset.boolColormode === 'state';
        if (!Number.isFinite(Number(f.falseColor))) f.falseColor = 0xF800;
        if (!Number.isFinite(Number(f.trueColor))) f.trueColor = 0x07E0;
        if (!Number.isFinite(Number(f.falseBackground))) f.falseBackground = Number(f.backgroundColor) || 0;
        if (!Number.isFinite(Number(f.trueBackground))) f.trueBackground = Number(f.backgroundColor) || 0;
      }, true);
      if (!KEYS_FOR(selectedField()).includes(activeColorKey)) activeColorKey = 'label';
      return;
    }
    const boolStyle = e.target.closest('[data-bool-style]');
    if (boolStyle) {
      setFieldProps((f) => {
        f.boolStyle = boolStyle.dataset.boolStyle;
        if (f.boolStyle === 'PILOT') {
          // El piloto siempre tiene color por estado.
          f.stateColors = true;
          if (!Number.isFinite(Number(f.falseColor))) f.falseColor = 0xF800;
          if (!Number.isFinite(Number(f.trueColor))) f.trueColor = 0x07E0;
          if (!Number.isFinite(Number(f.falseBackground))) f.falseBackground = Number(f.backgroundColor) || 0;
          if (!Number.isFinite(Number(f.trueBackground))) f.trueBackground = Number(f.backgroundColor) || 0;
        }
      }, true);
      if (!KEYS_FOR(selectedField()).includes(activeColorKey)) activeColorKey = 'label';
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
  const BOOL_STATE_PROP = { false: 'falseColor', true: 'trueColor', falseBg: 'falseBackground', trueBg: 'trueBackground' };

  // Colores con control viejo (#fieldLabelColor...) pasan por proxyColor; los
  // del BOOL por estado no tienen control viejo y van directo al modelo.
  function applyColor(key, value565, commit) {
    if (BOOL_STATE_PROP[key]) setFieldProps((f) => { f[BOOL_STATE_PROP[key]] = value565 & 0xFFFF; }, commit);
    else if (COLOR_TARGET[key]) proxyColor(COLOR_TARGET[key], value565, commit);
  }

  // Editor de color fijo bajo la leyenda (designer-color-editor.js): en vivo
  // mientras se arrastra y al historial al soltar.
  const colorEditor = window.JWPLCColorEditor?.create({
    paletteAttr: 'data-field-palette',
    onInput: (v) => applyColor(activeColorKey, v, false),
    onCommit: (v) => applyColor(activeColorKey, v, true)
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

  const LEAD_SECONDS = { SS: 1, MM_SS: 60, HH_MM_SS: 3600, COMPACT: 3600 };
  const LEAD_TITLE = { SS: 'Dígitos de los segundos', MM_SS: 'Dígitos de los minutos', HH_MM_SS: 'Dígitos de las horas', COMPACT: 'Dígitos de las horas' };

  function valueRange(field) {
    if (field.valueKind === 'DURATION') {
      const lead = Math.max(1, Number(field.integerDigits) || 1);
      const scale = field.durationUnit === 'MS' ? 1000 : 1;
      const max = (10 ** lead * (LEAD_SECONDS[field.durationPattern] || 60) - 1) * scale;
      return { min: field.signedValue ? -max : 0, max, step: scale, decs: 0 };
    }
    const ints = Math.max(1, Number(field.integerDigits) || 1);
    const decs = Math.max(0, Number(field.decimalDigits) || 0);
    const max = Number((10 ** ints - 10 ** -decs).toFixed(decs));
    return { min: field.signedValue ? -max : 0, max, step: decs ? 10 ** -decs : 1, decs };
  }

  function cppTypeFor(field) {
    if (field.type === 'VALUE') return window.JWPLCHMICodegenCore?.valueCppType?.(field) || 'float';
    if (field.type === 'BOOL') return 'bool';
    return `char[${window.JWPLCHMICodegenCore?.textBufferSize?.(field) ?? (Math.max(1, Number(field.capacity) || 1) + 1)}]`;
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

  // --- Apariencia ------------------------------------------------------------

  let activeColorKey = 'label';
  const PREVIEW_MAX_W = 150;
  const FIELD_PADDING_PX = 3; // JWPLC_UI.cpp FIELD_PADDING: margen superior/izquierdo
  const FIELD_GAP_PX = 4;     // JWPLC_UI.cpp FIELD_GAP: valor -> unidad
  const caseZones = new Map();

  function stateMode(field) {
    return field?.type === 'BOOL' && Boolean(field.stateColors);
  }

  function KEYS_FOR(field) {
    if (!field) return [];
    const keys = ['label'];
    keys.push(...(stateMode(field) ? ['false', 'falseBg', 'true', 'trueBg'] : ['value', 'background']));
    if (field.frame) keys.push('frame');
    return keys;
  }

  function colorOf(field, key) {
    const prop = COLOR_PROP[key] || BOOL_STATE_PROP[key];
    let raw = field[prop];
    if ((key === 'falseBg' || key === 'trueBg') && !Number.isFinite(Number(raw))) raw = field.backgroundColor;
    return Number.isFinite(Number(raw)) ? Number(raw) & 0xFFFF : 0;
  }

  // Un caso = una vista del campo con su leyenda, de arriba abajo como se ve
  // el campo: marco, etiqueta, valor y fondo. En BOOL por estado hay dos
  // casos (FALSE y TRUE), cada uno con el texto y el fondo de su estado; la
  // etiqueta y el marco son comunes y aparecen en ambos.
  function casesFor(field) {
    const pilot = field.type === 'BOOL' && field.boolStyle === 'PILOT';
    // BOOL por estado: etiqueta y marco son comunes a los dos casos.
    const common = stateMode(field) ? ' (común)' : '';
    const rowsFor = (valueKey, backgroundKey) => [
      ...(field.frame ? [{ label: `Marco${common}`, keys: ['frame'] }] : []),
      { label: `${pilot ? 'Etiqueta y aro' : 'Etiqueta'}${common}`, keys: ['label'] },
      { label: pilot ? 'Piloto' : 'Valor', keys: [valueKey] },
      { label: 'Fondo', keys: [backgroundKey] }
    ];
    if (!stateMode(field)) {
      return [{ id: 'single', title: 'Vista', boolValue: field.boolValue, rows: rowsFor('value', 'background') }];
    }
    return [
      { id: 'false', title: pilot ? 'Caso FALSE · apagado' : `Caso FALSE · «${field.falseText || 'OFF'}»`, boolValue: false, rows: rowsFor('false', 'falseBg') },
      { id: 'true', title: pilot ? 'Caso TRUE · encendido' : `Caso TRUE · «${field.trueText || 'ON'}»`, boolValue: true, rows: rowsFor('true', 'trueBg') }
    ];
  }

  function buildAppearance(field) {
    const host = wrap.querySelector('[data-field-appearance]');
    const cases = casesFor(field);
    const signature = cases.map((c) => `${c.id}:${c.rows.map((r) => `${r.label}=${r.keys.join('+')}`).join(',')}`).join('|');
    if (host.dataset.signature === signature) return cases;
    host.dataset.signature = signature;
    host.innerHTML = cases.map((c) => `
      <div class="menu-case">
        <div class="bar-card-section-title">${escapeHtml(c.title)}</div>
        <div class="menu-case-body field-case-body zone-host" data-field-case-body="${c.id}">
          <canvas class="menu-case-canvas field-case-canvas" data-field-case="${c.id}"></canvas>
          <div class="menu-case-legend">${c.rows.map((r) => legendRow(r.label, r.keys)).join('')}</div>
        </div>
      </div>`).join('') + `
      <div class="menu-palette">
        <div class="insp-note">Clic en una fila (texto o cuadro) para editar ese color: el contorno y el ▶ marcan lo que cambia en la vista.</div>
        <div data-field-color-editor></div>
      </div>`;
    if (colorEditor) host.querySelector('[data-field-color-editor]').appendChild(colorEditor.el);
    return cases;
  }

  // Caja de los píxeles de un color dentro de un rectángulo del framebuffer;
  // si no hay (color igual al fondo), el rectángulo entero.
  function colorBox(out, rect, color) {
    let minX = Infinity; let minY = Infinity; let maxX = -1; let maxY = -1;
    for (let y = Math.max(0, rect.y); y < Math.min(out.height, rect.y + rect.h); y++) {
      for (let x = Math.max(0, rect.x); x < Math.min(out.width, rect.x + rect.w); x++) {
        if (out.buffer[y * out.width + x] !== color) continue;
        if (x < minX) minX = x;
        if (x > maxX) maxX = x;
        if (y < minY) minY = y;
        if (y > maxY) maxY = y;
      }
    }
    return maxX < 0 ? rect : { x: minX, y: minY, w: maxX - minX + 1, h: maxY - minY + 1 };
  }

  // Zonas de cada color en coordenadas del mini-modelo (rectángulos que
  // enmarca el indicador A1):
  //   Etiqueta = etiqueta + unidad (la unidad usa colors.label en el firmware)
  //              y, en el piloto, el aro;
  //   Valor    = sólo los píxeles del valor;
  //   Fondo    = el interior del campo (contorno hacia adentro, en el margen);
  //   Marco    = el borde.
  function zonesFor(field, preview, out) {
    const g = out.g;
    const w = Math.ceil(g.fieldW);
    const h = Math.ceil(g.fieldH);
    const inset = field.frame ? 1 : 0;
    const labelRects = [];
    if (g.labelBounds.width > 0) labelRects.push({ x: FIELD_PADDING_PX, y: FIELD_PADDING_PX, w: g.labelBounds.width, h: g.labelBounds.height });
    if (g.unitBounds.width > 0) labelRects.push({ x: Math.round(g.valueX + g.valueW + FIELD_GAP_PX), y: Math.round(g.valueY), w: g.unitBounds.width, h: g.unitBounds.height });
    const valueRect = { x: Math.round(g.valueX), y: Math.round(g.valueY), w: Math.max(1, Math.round(g.valueW)), h: Math.max(1, Math.round(g.valueH)) };
    if (field.type === 'BOOL' && field.boolStyle === 'PILOT') labelRects.push(colorBox(out, valueRect, colorOf(field, 'label')));
    const valueColor = stateMode(field) ? colorOf(field, preview.boolValue ? 'true' : 'false') : colorOf(field, 'value');
    const value = { rects: [colorBox(out, valueRect, valueColor)] };
    const background = { rects: [{ x: inset, y: inset, w: w - 2 * inset, h: h - 2 * inset, inset: true }] };
    return {
      frame: { rects: [{ x: 0, y: 0, w, h }] },
      label: { rects: labelRects.length ? labelRects : [valueRect] },
      value, false: value, true: value,
      background, falseBg: background, trueBg: background
    };
  }

  // Mismo dibujo que el lienzo (app.js:drawField) sobre un framebuffer propio,
  // recortado al rectángulo del campo y escalado a un entero (1..3).
  function drawCase(canvas, field, c) {
    const preview = { ...field, x: 0, y: 0 };
    if (field.type === 'BOOL') {
      preview.boolValue = Boolean(c.boolValue);
      preview.preview = preview.boolValue ? (field.trueText || 'ON') : (field.falseText || 'OFF');
    }
    const out = editor()?.renderFieldPreview?.(preview);
    if (!out?.g) return;
    const w = Math.max(1, Math.min(out.width, Math.ceil(out.g.fieldW)));
    const h = Math.max(1, Math.min(out.height, Math.ceil(out.g.fieldH)));
    // Leyenda a la derecha si el mini-modelo cabe a ×2 (o no mejora debajo);
    // si no, la leyenda va debajo y el mini-modelo usa todo el ancho.
    const body = canvas.parentElement;
    const legend = body.querySelector('.menu-case-legend');
    // Canal del ▶ a la izquierda + separación con la leyenda + margen derecho.
    const gutter = (window.JWPLCZoneIndicator?.CHANNEL ?? 12) + 14;
    const bodyW = body.clientWidth || 300;
    const fit = (avail) => Math.max(0, Math.min(3, Math.floor(avail / w)));
    const rightScale = fit(bodyW - legend.offsetWidth - gutter);
    const belowScale = fit(bodyW - gutter);
    const below = rightScale < 2 && belowScale > rightScale;
    body.classList.toggle('is-below', below);
    body.dataset.placement = below ? 'below' : 'right';
    const scale = Math.max(1, below ? belowScale : rightScale);
    if (canvas.width !== w * scale) canvas.width = w * scale;
    if (canvas.height !== h * scale) canvas.height = h * scale;
    canvas.dataset.logicalW = String(w);
    canvas.dataset.logicalH = String(h);
    // Un campo más ancho que el panel se reduce por CSS.
    canvas.style.width = `${Math.min(w * scale, bodyW - gutter)}px`;
    const ctx = canvas.getContext('2d');
    const img = ctx.createImageData(canvas.width, canvas.height);
    for (let y = 0; y < canvas.height; y++) {
      const row = Math.floor(y / scale) * out.width;
      for (let x = 0; x < canvas.width; x++) {
        const v = out.buffer[row + Math.floor(x / scale)];
        const o = (y * canvas.width + x) * 4;
        img.data[o] = ((v >> 11) & 0x1F) * 255 / 31;
        img.data[o + 1] = ((v >> 5) & 0x3F) * 255 / 63;
        img.data[o + 2] = (v & 0x1F) * 255 / 31;
        img.data[o + 3] = 255;
      }
    }
    ctx.putImageData(img, 0, 0);
    caseZones.set(c.id, zonesFor(field, preview, out));
  }

  // Indicador A1: contorno y ▶ en la zona del color activo, sólo en los casos
  // que tienen ese cuadro (en BOOL por estado, lo común sale en los dos).
  function drawArrows() {
    wrap.querySelectorAll('[data-field-case-body]').forEach((body) => {
      const zones = caseZones.get(body.dataset.fieldCaseBody);
      const canvas = body.querySelector('canvas');
      if (!zones || !canvas) return;
      let rects = [];
      body.querySelectorAll('[data-legend-key]').forEach((row) => {
        const active = row.dataset.legendKey === activeColorKey && Boolean(zones[activeColorKey]);
        row.classList.toggle('is-active', active);
        if (active) rects = zones[activeColorKey].rects;
      });
      window.JWPLCZoneIndicator?.draw(body, canvas, rects);
    });
  }

  let arrowsFrame = 0;
  function scheduleArrows() {
    if (arrowsFrame) return;
    arrowsFrame = requestAnimationFrame(() => {
      arrowsFrame = 0;
      drawArrows();
    });
  }

  if (window.ResizeObserver) new ResizeObserver(scheduleArrows).observe(wrap);

  function syncAppearance(field) {
    if (!isManaged(field)) return;
    const perState = stateMode(field);
    wrap.querySelectorAll('[data-bool-colormode]').forEach((b) =>
      b.classList.toggle('active', (b.dataset.boolColormode === 'state') === perState));
    if (!KEYS_FOR(field).includes(activeColorKey)) activeColorKey = 'label';
    const cases = buildAppearance(field);
    // Sólo si la pestaña está a la vista: no repintar mini-modelos ocultos.
    if (!wrap.querySelector('[data-insp-panel="design:appearance"]').hidden) {
      cases.forEach((c) => {
        const canvas = wrap.querySelector(`[data-field-case="${c.id}"]`);
        if (canvas) drawCase(canvas, field, c);
      });
      scheduleArrows();
    }
    wrap.querySelectorAll('[data-field-swatch]').forEach((sw) => {
      const key = sw.dataset.fieldSwatch;
      const v = colorOf(field, key);
      sw.style.background = rgb565ToCss(v);
      sw.style.color = ((v >> 11) + ((v >> 5) & 0x3F) / 2 + (v & 0x1F)) > 40 ? '#000' : '#fff';
      sw.classList.toggle('active', key === activeColorKey);
    });
    colorEditor?.set(colorOf(field, activeColorKey), SWATCH_NAME[activeColorKey]);
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
    // VALUE de fecha/hora (I2): sin variable ni valor de prueba.
    const clock = Boolean(window.JWPLCHMICodegenCore?.isClockField?.(field));
    wrap.querySelectorAll('[data-clock-only]').forEach((el) => { el.hidden = !clock; });
    wrap.querySelectorAll('[data-not-clock]').forEach((el) => {
      if (clock) el.hidden = true;
      else if (!el.dataset.only) el.hidden = false;
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
      const kind = ['DURATION', 'DATETIME'].includes(field.valueKind) ? field.valueKind : 'NUMBER';
      wrap.querySelectorAll('[data-vdt]').forEach((b) => b.classList.toggle('active', b.dataset.vdt === (field.dateTimePattern || 'HH_MM')));
      wrap.querySelector('[data-rtc-invalid]').classList.toggle('active', field.rtcPreview === 'INVALID');
      wrap.querySelectorAll('[data-vkind]').forEach((b) => b.classList.toggle('active', b.dataset.vkind === kind));
      wrap.querySelectorAll('[data-vkind-panel]').forEach((p) => { p.hidden = p.dataset.vkindPanel !== kind; });
      const sep = [',', '.', ' '].includes(field.thousandsSep) ? field.thousandsSep : '';
      wrap.querySelectorAll('[data-vsep]').forEach((b) => b.classList.toggle('active', b.dataset.vsep === sep));
      wrap.querySelector('[data-vsep-note]').hidden = sep !== '.';
      const pattern = field.durationPattern || 'MM_SS';
      wrap.querySelectorAll('[data-vpattern]').forEach((b) => b.classList.toggle('active', b.dataset.vpattern === pattern));
      wrap.querySelectorAll('[data-vunit]').forEach((b) => b.classList.toggle('active', b.dataset.vunit === (field.durationUnit || 'S')));
      readout('leadTitle', LEAD_TITLE[pattern] || LEAD_TITLE.MM_SS);
      readout('overflow', window.JWPLCValueFormat?.overflow(field) || '--:--');
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
      const pilot = field.boolStyle === 'PILOT';
      wrap.querySelectorAll('[data-bool-style]').forEach((b) => b.classList.toggle('active', b.dataset.boolStyle === (pilot ? 'PILOT' : 'TEXT')));
      wrap.querySelector('[data-bool-pilot-note]').hidden = !pilot;
      wrap.querySelectorAll('[data-bool-text-only]').forEach((el) => { el.hidden = pilot; });
    } else {
      wrap.querySelectorAll('[data-bool-text-only]').forEach((el) => {
        if (!el.dataset.only) el.hidden = false;
      });
    }

    if (field.type === 'BOOL') {
      readout('falseText', field.falseText || 'OFF');
      readout('trueText', field.trueText || 'ON');
      wrap.querySelectorAll('[data-bool-preview]').forEach((b) => b.classList.toggle('active', (b.dataset.boolPreview === '1') === Boolean(field.boolValue)));
      wrap.querySelectorAll('[data-bool-preset]').forEach((b) => b.classList.toggle('active', b.dataset.boolPreset === `${field.falseText}|${field.trueText}`));
    }

    const alignHint = wrap.querySelector('[data-align-unit-hint]');
    alignHint.hidden = !(field.unit && field.type !== 'BOOL' && (field.align || 'RIGHT') !== 'RIGHT');
    if (!alignHint.hidden) readout('reserveShort', field.type === 'VALUE' ? ($('valueFormatSampleStatus')?.textContent || '') : 'M'.repeat(Math.max(1, Number(field.capacity) || 1)));

    readout('cppType', cppTypeFor(field));
    syncPages(field);

    wrap.querySelectorAll('[data-mirror]').forEach((el) => {
      const text = $(el.dataset.mirror)?.textContent ?? '';
      if (el.textContent !== text) el.textContent = text;
    });

    applyTabs();
    syncAppearance(field);
  }

  window.addEventListener('jwplc:editor-refresh', sync);
  window.JWPLCHMIFieldInspector = { sync, isManaged, tabs: state };
  setTimeout(sync, 0);
})();
