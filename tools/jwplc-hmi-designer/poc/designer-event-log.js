(() => {
  'use strict';

  // Alpha16: plantilla EVENT_LOG (Historial y registro de eventos).
  // Firmware: JWPLC_UI_EventLog.h / JWPLC_UI_EventLog.cpp.

  const SCREEN_W = 320;
  const SCREEN_H = 170;
  const ROW_HEIGHT = 14;
  const HEADER_HEIGHT = 17;
  const FOOTER_HEIGHT = 13;

  const DEFAULT_COLORS = Object.freeze({
    headerBackground: 0x2124,
    headerText: 0xFFFF,
    rowBackground: 0x0000,
    rowBackgroundAlt: 0x10A2,
    rowText: 0xC618,
    selectBackground: 0x03FF,
    selectText: 0xFFFF,
    sevInfo: 0x07E0,
    sevWarn: 0xFD20,
    sevCrit: 0xF800
  });

  const DEFAULT_DESCRIPTORS = [
    { code: 100, label: 'Arranque de sistema', severity: 'INFO', category: 'SYSTEM' },
    { code: 101, label: 'Enlace Ethernet restablecido', severity: 'INFO', category: 'SYSTEM' },
    { code: 102, label: 'Enlace Ethernet caido', severity: 'WARN', category: 'SYSTEM' },
    { code: 103, label: 'Error bus RS485/Modbus', severity: 'WARN', category: 'SYSTEM' },
    { code: 104, label: 'Bus RS485 normalizado', severity: 'INFO', category: 'SYSTEM' },
    { code: 1, label: 'Falla de proceso', severity: 'CRIT', category: 'ALARM' }
  ];

  function clamp(val, min, max) {
    return Math.max(min, Math.min(max, val));
  }

  function escapeHtml(text) {
    return String(text || '')
      .replace(/&/g, '&amp;')
      .replace(/</g, '&lt;')
      .replace(/>/g, '&gt;')
      .replace(/"/g, '&quot;');
  }

  function sanitizeSymbol(str, fallback) {
    const s = String(str || '').replace(/[^a-zA-Z0-9_]/g, '_');
    return s || fallback;
  }

  function rgb565ToHex(c) {
    const r = ((c >> 11) & 0x1F) * 255 / 31;
    const g = ((c >> 5) & 0x3F) * 255 / 63;
    const b = (c & 0x1F) * 255 / 31;
    const toHex = (n) => Math.round(n).toString(16).padStart(2, '0');
    return `#${toHex(r)}${toHex(g)}${toHex(b)}`;
  }

  function hexToRgb565(hex) {
    if (!hex || hex[0] !== '#') return 0;
    const num = parseInt(hex.slice(1), 16);
    const r = (num >> 16) & 0xFF;
    const g = (num >> 8) & 0xFF;
    const b = num & 0xFF;
    return (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
  }

  function createDefault(keyOrPage, serial, page) {
    let key = keyOrPage;
    let s = serial || 1;
    let p = (typeof page === 'number') ? page : 0;
    if (typeof keyOrPage === 'number') {
      p = keyOrPage;
      s = serial || 1;
      key = `eventlog-${s}`;
    } else if (typeof key !== 'string') {
      key = `eventlog-${s}`;
    }
    return {
      key,
      type: 'EVENT_LOG',
      name: `HISTORIAL ${s}`,
      id: `EVENT_LOG_${s}`,
      title: 'Historial de Eventos',
      page: p,
      x: 10,
      y: 15,
      width: 300,
      height: 140,
      frame: true,
      ramCapacity: 64,
      enableFram: true,
      enableSd: true,
      sdFileName: '/EVENTOS.CSV',
      colors: { ...DEFAULT_COLORS },
      descriptors: JSON.parse(JSON.stringify(DEFAULT_DESCRIPTORS))
    };
  }

  function layout(field) {
    const f = ensureState(field);
    const border = f.frame ? 1 : 0;
    const availH = Math.max(0, f.height - 2 * border - HEADER_HEIGHT - FOOTER_HEIGHT);
    const rows = Math.max(1, Math.trunc(availH / ROW_HEIGHT));
    return { border, availH, rows };
  }

  function ensureState(field) {
    if (!field || field.type !== 'EVENT_LOG') return field;
    if (!Number.isFinite(Number(field.x))) field.x = 10;
    if (!Number.isFinite(Number(field.y))) field.y = 15;
    if (!Number.isFinite(Number(field.width))) field.width = 300;
    if (!Number.isFinite(Number(field.height))) field.height = 140;
    if (typeof field.frame !== 'boolean') field.frame = true;
    if (!Number.isFinite(Number(field.ramCapacity))) field.ramCapacity = 64;
    if (typeof field.enableFram !== 'boolean') field.enableFram = true;
    if (typeof field.enableSd !== 'boolean') field.enableSd = true;
    if (typeof field.sdFileName !== 'string' || !field.sdFileName) field.sdFileName = '/EVENTOS.CSV';
    if (!field.colors || typeof field.colors !== 'object') field.colors = { ...DEFAULT_COLORS };
    if (!Array.isArray(field.descriptors)) field.descriptors = JSON.parse(JSON.stringify(DEFAULT_DESCRIPTORS));
    return field;
  }

  function editor() {
    return window.JWPLCHMIEditor || null;
  }

  // ---------------------------------------------------------------------------
  // Pintor en el lienzo (canvas)
  // ---------------------------------------------------------------------------

  function paint(field, painter) {
    ensureState(field);
    const c = field.colors || DEFAULT_COLORS;
    const x0 = Number(field.x) || 0;
    const y0 = Number(field.y) || 0;
    const w = clamp(Number(field.width) || 300, 40, SCREEN_W);
    const h = clamp(Number(field.height) || 140, 30, SCREEN_H);
    const { border, rows: visibleRows } = layout(field);

    // Marco exterior
    if (field.frame && painter.drawRect) {
      painter.drawRect(x0, y0, w, h, c.headerBackground);
    }

    // Encabezado
    painter.fillRect(x0 + border, y0 + border, w - 2 * border, HEADER_HEIGHT, c.headerBackground);
    if (painter.drawText) {
      const title = (w >= 200) ? (field.title || 'HISTORIAL DE EVENTOS') : 'HISTORIAL';
      painter.drawText(title, x0 + border + 6, y0 + border + 4, c.headerText, c.headerBackground, 1);
      if (w >= 180) {
        painter.drawText('01/06', x0 + border + w - (w >= 240 ? 95 : 55), y0 + border + 4, c.headerText, c.headerBackground, 1);
      }
      if (w >= 240) {
        painter.drawText(field.enableSd ? 'SD:OK' : 'SD:--', x0 + border + w - 45, y0 + border + 4, field.enableSd ? 0x07E0 : 0x632C, c.headerBackground, 1);
      }
    }

    // Filas de muestra dinámicas
    const samples = [
      { time: '14:23:05', code: '#101', trans: 'CES', label: 'Enlace Ethernet restablecido', sev: c.sevInfo },
      { time: '14:22:40', code: '#102', trans: 'ACT', label: 'Enlace Ethernet caido', sev: c.sevWarn },
      { time: '14:15:10', code: '#100', trans: 'ACT', label: 'Arranque de sistema', sev: c.sevInfo },
      { time: '14:02:50', code: '#001', trans: 'CES', label: 'Falla de proceso', sev: c.sevCrit },
      { time: '14:00:15', code: '#001', trans: 'ACT', label: 'Falla de proceso', sev: c.sevCrit },
      { time: '13:58:30', code: '#104', trans: 'CES', label: 'Bus RS485 normalizado', sev: c.sevInfo },
      { time: '13:58:05', code: '#103', trans: 'ACT', label: 'Error bus RS485/Modbus', sev: c.sevWarn }
    ];

    for (let r = 0; r < visibleRows; r++) {
      const y = y0 + border + HEADER_HEIGHT + (r * ROW_HEIGHT);
      const isSelected = (r === 0);
      const rowBg = isSelected ? c.selectBackground : ((r % 2 === 0) ? c.rowBackground : c.rowBackgroundAlt);
      const rowFg = isSelected ? c.selectText : c.rowText;

      painter.fillRect(x0 + border, y, w - 2 * border, ROW_HEIGHT, rowBg);

      if (r < samples.length) {
        const item = samples[r];
        // Barra de severidad
        painter.fillRect(x0 + border, y, 3, ROW_HEIGHT, item.sev);

        if (painter.drawText) {
          painter.drawText(item.time, x0 + border + 6, y + 3, rowFg, rowBg, 1);
          if (w >= 120) {
            painter.drawText(item.code, x0 + border + 60, y + 3, rowFg, rowBg, 1);
          }
          if (w >= 165) {
            painter.drawText(item.trans, x0 + border + 98, y + 3, rowFg, rowBg, 1);
          }
          if (w >= 210) {
            const maxChars = Math.max(1, Math.trunc((w - 145) / 6));
            const truncated = item.label.slice(0, maxChars);
            painter.drawText(truncated, x0 + border + 138, y + 3, rowFg, rowBg, 1);
          }
        }
      }
    }

    // Pie de página
    const footerY = y0 + h - border - FOOTER_HEIGHT;
    painter.fillRect(x0 + border, footerY, w - 2 * border, FOOTER_HEIGHT, 0x10A2);
    if (painter.drawText) {
      if (w >= 250) {
        painter.drawText('[OK] Detalle     [\x1E\x1F] Desplazar     [ESC] Salir', x0 + border + 10, footerY + 3, 0xFFFF, 0x10A2, 1);
      } else {
        painter.drawText('[OK] [\x1E\x1F] [ESC]', x0 + border + 6, footerY + 3, 0xFFFF, 0x10A2, 1);
      }
    }
  }

  // ---------------------------------------------------------------------------
  // Codegen C++
  // ---------------------------------------------------------------------------

  function generate(eventLogs, context = {}) {
    if (!eventLogs || !eventLogs.length) {
      return { declarations: '', setup: '' };
    }

    const field = eventLogs[0];
    ensureState(field);

    const descList = (field.descriptors && field.descriptors.length)
      ? field.descriptors
      : DEFAULT_DESCRIPTORS;

    const descLines = descList.map((d) => {
      const sevConst = d.severity === 'CRIT' ? 'JWPLC_EVENT_SEV_CRIT' : (d.severity === 'WARN' ? 'JWPLC_EVENT_SEV_WARN' : 'JWPLC_EVENT_SEV_INFO');
      const catConst = d.category === 'ALARM' ? 'JWPLC_EVENT_CAT_ALARM' : (d.category === 'INTERLOCK' ? 'JWPLC_EVENT_CAT_INTERLOCK' : (d.category === 'SYSTEM' ? 'JWPLC_EVENT_CAT_SYSTEM' : 'JWPLC_EVENT_CAT_PROCESS'));
      const safeLabel = String(d.label || '').replace(/\\/g, '\\\\').replace(/"/g, '\\"');
      return `    { ${d.code}, "${safeLabel}", ${sevConst}, ${catConst} }`;
    }).join(',\n');

    const c = field.colors || DEFAULT_COLORS;
    const pageId = Number(field.page || 0);

    const declarations = `// =====================================================
// Registro de Eventos (EVENT_LOG) · Alpha16
// =====================================================
#if JWPLC_UI_HAS_EVENT_LOG
static const JWPLC_UIEventDescriptor HMI_EVENT_DESCRIPTORS[] PROGMEM = {
${descLines}
};

static const JWPLC_UIEventLog HMI_EVENT_LOG = {
    /* page */ ${pageId},
    /* x */ ${Number(field.x) || 0},
    /* y */ ${Number(field.y) || 0},
    /* width */ ${Number(field.width) || 300},
    /* height */ ${Number(field.height) || 140},
    /* frame */ ${field.frame ? 'true' : 'false'},
    /* ramCapacity */ ${field.ramCapacity || 64},
    /* enableFram */ ${field.enableFram ? 'true' : 'false'},
    /* enableSd */ ${field.enableSd ? 'true' : 'false'},
    /* sdFileName */ "${field.sdFileName || '/EVENTOS.CSV'}",
    /* descriptors */ HMI_EVENT_DESCRIPTORS,
    /* descriptorCount */ ${descList.length},
    /* colors */ {
        0x${c.headerBackground.toString(16).toUpperCase()},
        0x${c.headerText.toString(16).toUpperCase()},
        0x${c.rowBackground.toString(16).toUpperCase()},
        0x${c.rowBackgroundAlt.toString(16).toUpperCase()},
        0x${c.rowText.toString(16).toUpperCase()},
        0x${c.selectBackground.toString(16).toUpperCase()},
        0x${c.selectText.toString(16).toUpperCase()},
        0x${c.sevInfo.toString(16).toUpperCase()},
        0x${c.sevWarn.toString(16).toUpperCase()},
        0x${c.sevCrit.toString(16).toUpperCase()}
    }
};
#endif`;

    const setup = `    #if JWPLC_UI_HAS_EVENT_LOG
    JWPLC_Display.setEventLog(HMI_EVENT_LOG);
    #endif\n`;

    return { declarations, setup };
  }

  function allEventLogs() {
    return (editor()?.getAllFields?.() || []).filter((f) => f.type === 'EVENT_LOG' && !f.hidden);
  }

  function patchHeader(text, optFields) {
    const eventLogs = (Array.isArray(optFields) && optFields.length) ? optFields : allEventLogs();
    if (!eventLogs.length || !text.includes('void jwplcHMISetup()')) return text;

    const out = generate(eventLogs);
    if (!out) return text;

    const setupAnchor = /void\s+jwplcHMISetup\s*\(\s*\)[\r\n\s]*\{/m;

    if (!text.includes('HMI_EVENT_LOG')) {
      if (setupAnchor.test(text)) {
        text = text.replace(setupAnchor, (m) => `${out.declarations}\n\n${m}`);
      } else {
        text = text.replace('void jwplcHMISetup()', `${out.declarations}\n\nvoid jwplcHMISetup()`);
      }
    }
    if (!text.includes('JWPLC_Display.setEventLog(')) {
      if (setupAnchor.test(text)) {
        text = text.replace(setupAnchor, (m) => `${m}\n${out.setup}`);
      }
    }
    return text;
  }

  // ---------------------------------------------------------------------------
  // Inspector UI
  // ---------------------------------------------------------------------------

  const tabs = { main: 'design', sub: 'shape' };

  function selectedEventLog() {
    const field = editor()?.getSelectedField?.();
    return (field && field.type === 'EVENT_LOG') ? ensureState(field) : null;
  }

  function mutate(fn, commit = true) {
    const field = selectedEventLog();
    if (!field) return;
    if (Array.isArray(field.descriptors)) field.descriptors = field.descriptors.map((d) => ({ ...d }));
    field.colors = { ...field.colors };
    fn(field);
    editor()?.render?.();
    syncInspector();
    if (commit) editor()?.commitHistory?.();
    window.dispatchEvent(new CustomEvent('jwplc:project-changed'));
  }

  function applyTabs(section) {
    if (!section) return;
    section.querySelectorAll('[data-evt-main]').forEach((b) => b.classList.toggle('active', b.dataset.evtMain === tabs.main));
    section.querySelectorAll('[data-evt-sub]').forEach((b) => b.classList.toggle('active', b.dataset.evtSub === tabs.sub));
    const subtabs = section.querySelector('[data-evt-subtabs]');
    if (subtabs) subtabs.hidden = (tabs.main !== 'design');
    const panelKey = (tabs.main === 'design') ? `design:${tabs.sub}` : tabs.main;
    section.querySelectorAll('[data-evt-panel]').forEach((p) => {
      p.hidden = (p.dataset.evtPanel !== panelKey);
    });
  }

  function buildInspectorSection() {
    if (typeof document === 'undefined') return null;

    let section = document.getElementById('eventLogInspectorSection');
    if (section) return section;

    const anchor = document.getElementById('textFieldControlsSection');
    if (!anchor) return null;

    section = document.createElement('section');
    section.id = 'eventLogInspectorSection';
    section.className = 'inspector-section';
    section.hidden = true;
    section.style.display = 'none';

    section.innerHTML = `
      <div class="inspector-title">
        <h2>Inspector — HISTORIAL</h2>
        <span>
          <button type="button" class="icon-button" data-evt-action="duplicate" title="Duplicar">⧉</button>
          <button type="button" class="icon-button" data-evt-action="delete" title="Eliminar">🗑</button>
        </span>
      </div>
      <div class="insp-wrap">
        <div class="bar-nav-header insp-nav-header">
          <div class="bar-main-tabs" role="tablist">
            <button type="button" class="bar-main-tab active" data-evt-main="design">Diseño</button>
            <button type="button" class="bar-main-tab" data-evt-main="storage">Memoria</button>
            <button type="button" class="bar-main-tab" data-evt-main="descriptors">Eventos</button>
            <button type="button" class="bar-main-tab" data-evt-main="tech">Técnico</button>
          </div>
          <div class="bar-sub-tabs" data-evt-subtabs>
            <button type="button" class="bar-sub-tab active" data-evt-sub="shape">Forma</button>
            <button type="button" class="bar-sub-tab" data-evt-sub="appearance">Apariencia</button>
            <button type="button" class="bar-sub-tab" data-evt-sub="placement">Ubicación</button>
          </div>
        </div>

        <div class="insp-body">
          <!-- PANEL: Forma -->
          <div class="insp-panel" data-evt-panel="design:shape">
            <div class="bar-card">
              <label class="field-label">Título de la cabecera
                <input class="field-input" data-evt-prop="title" type="text" maxlength="32" placeholder="Historial de Eventos" />
              </label>
            </div>
            <div class="bar-card">
              <div class="bar-card-section-title">Marco</div>
              <div class="bar-visual-grid-2">
                <button type="button" class="bar-visual-btn" data-evt-set="frame" data-value="0"><span>Sin marco</span></button>
                <button type="button" class="bar-visual-btn" data-evt-set="frame" data-value="1"><span>Con marco</span></button>
              </div>
            </div>
            <div class="bar-card">
              <div class="insp-readouts insp-readouts-grid">
                <span>Filas visibles <strong data-evt-readout="rows">—</strong></span>
                <span>Ancho <strong data-evt-readout="width">—</strong></span>
                <span>Alto <strong data-evt-readout="height">—</strong></span>
                <span>Capacidad RAM <strong data-evt-readout="ram">—</strong></span>
              </div>
            </div>
          </div>

          <!-- PANEL: Apariencia (Colores) -->
          <div class="insp-panel" data-evt-panel="design:appearance" hidden>
            <div class="bar-card">
              <div class="bar-card-section-title">Encabezado</div>
              <div class="insp-two">
                <label class="field-label">Fondo
                  <input type="color" class="field-input" data-evt-color="headerBackground" />
                </label>
                <label class="field-label">Texto
                  <input type="color" class="field-input" data-evt-color="headerText" />
                </label>
              </div>
            </div>
            <div class="bar-card">
              <div class="bar-card-section-title">Filas de eventos</div>
              <div class="insp-two">
                <label class="field-label">Fondo A
                  <input type="color" class="field-input" data-evt-color="rowBackground" />
                </label>
                <label class="field-label">Fondo B
                  <input type="color" class="field-input" data-evt-color="rowBackgroundAlt" />
                </label>
              </div>
              <label class="field-label" style="margin-top:6px;">Texto de fila
                <input type="color" class="field-input" data-evt-color="rowText" />
              </label>
            </div>
            <div class="bar-card">
              <div class="bar-card-section-title">Fila seleccionada (cursor)</div>
              <div class="insp-two">
                <label class="field-label">Fondo
                  <input type="color" class="field-input" data-evt-color="selectBackground" />
                </label>
                <label class="field-label">Texto
                  <input type="color" class="field-input" data-evt-color="selectText" />
                </label>
              </div>
            </div>
            <div class="bar-card">
              <div class="bar-card-section-title">Barras de Severidad</div>
              <div class="insp-two">
                <label class="field-label">INFO
                  <input type="color" class="field-input" data-evt-color="sevInfo" />
                </label>
                <label class="field-label">AVISO
                  <input type="color" class="field-input" data-evt-color="sevWarn" />
                </label>
              </div>
              <label class="field-label" style="margin-top:6px;">CRÍTICA
                <input type="color" class="field-input" data-evt-color="sevCrit" />
              </label>
            </div>
          </div>

          <!-- PANEL: Ubicación -->
          <div class="insp-panel" data-evt-panel="design:placement" hidden>
            <div class="bar-card">
              <div class="bar-card-section-title">Página</div>
              <div class="insp-chip-row" data-evt-pages></div>
            </div>
            <div class="bar-card">
              <div class="insp-two">
                <label class="field-label">X (px)<input class="field-input" data-evt-prop="x" type="number" min="0" max="319" /></label>
                <label class="field-label">Y (px)<input class="field-input" data-evt-prop="y" type="number" min="0" max="169" /></label>
                <label class="field-label">Ancho (px)<input class="field-input" data-evt-prop="width" type="number" min="40" max="320" /></label>
                <label class="field-label">Alto (px)<input class="field-input" data-evt-prop="height" type="number" min="30" max="170" /></label>
              </div>
              <div class="insp-note">Puedes moverlo y estirarlo arrastrando los puntos en el lienzo o ingresando los píxeles aquí.</div>
            </div>
          </div>

          <!-- PANEL: Memoria y Almacenamiento -->
          <div class="insp-panel" data-evt-panel="storage" hidden>
            <div class="bar-card">
              <div class="bar-card-section-title">Buffer circular en RAM</div>
              <div class="prop-row">
                <label class="field-label">Capacidad de eventos:
                  <select class="insp-select" data-evt-prop="ramCapacity">
                    <option value="64">64 eventos (~1 KB RAM)</option>
                    <option value="128">128 eventos (~2 KB RAM)</option>
                  </select>
                </label>
              </div>
              <div class="insp-note">Al llenarse el buffer se descartan los eventos más antiguos en FIFO.</div>
            </div>
            <div class="bar-card">
              <div class="bar-card-section-title">Persistencia en FRAM</div>
              <label class="field-label" style="display:flex; align-items:center; gap:8px;">
                <input type="checkbox" data-evt-prop="enableFram" checked />
                <span>Guardar últimos 16 eventos en FRAM</span>
              </label>
              <div class="insp-note">Bloque del sistema 0x1E00. No se borran al cortar la energía.</div>
            </div>
            <div class="bar-card">
              <div class="bar-card-section-title">Registro en MicroSD</div>
              <label class="field-label" style="display:flex; align-items:center; gap:8px;">
                <input type="checkbox" data-evt-prop="enableSd" checked />
                <span>Exportación CSV automática no bloqueante</span>
              </label>
              <label class="field-label" style="margin-top:6px;">Nombre de archivo CSV
                <input type="text" class="field-input" data-evt-prop="sdFileName" value="/EVENTOS.CSV" />
              </label>
            </div>
          </div>

          <!-- PANEL: Descriptores de eventos -->
          <div class="insp-panel" data-evt-panel="descriptors" hidden>
            <div class="bar-card">
              <div class="bar-card-section-title">Catálogo de eventos conocidos</div>
              <div class="insp-note">Códigos y descripciones resueltos en memoria Flash (PROGMEM):</div>
              <div class="evt-desc-list" data-evt-descriptors-list style="max-height: 240px; overflow-y: auto; margin: 8px 0;"></div>
              <button type="button" class="bar-visual-btn" data-evt-add-desc style="width: 100%; justify-content: center;">
                <strong>＋</strong> <span>Agregar descriptor de evento</span>
              </button>
            </div>
          </div>

          <!-- PANEL: Técnico -->
          <div class="insp-panel" data-evt-panel="tech" hidden>
            <div class="bar-card">
              <label class="field-label">Nombre del objeto (sólo Designer)
                <input class="field-input" data-evt-prop="name" type="text" maxlength="24" />
              </label>
              <label class="field-label">ID C++ (enum HMIEventLogId)
                <input class="field-input code-input" data-evt-prop="id" type="text" maxlength="32" />
              </label>
            </div>
            <div class="bar-card">
              <div class="bar-card-header">
                <span class="bar-card-title">Uso desde el sketch (Arduino / C++)</span>
                <button type="button" class="icon-button" data-evt-action="copy" title="Copiar código">⧉</button>
              </div>
              <pre class="insp-contract" data-evt-contract style="font-size:10px; max-height:160px; overflow-y:auto;"></pre>
            </div>
            <div class="bar-card">
              <div class="bar-card-section-title">Diagnóstico</div>
              <div class="insp-readouts insp-readouts-grid">
                <span>Eventos def. <strong data-evt-readout="descCount">—</strong></span>
                <span>RAM buffer <strong data-evt-readout="ramBytes">—</strong></span>
                <span>FRAM asignada <strong>256 B</strong></span>
                <span>Filas pantalla <strong data-evt-readout="rows2">—</strong></span>
              </div>
            </div>
          </div>
        </div>
      </div>
    `;

    anchor.insertAdjacentElement('beforebegin', section);

    // Event listeners
    section.addEventListener('click', (e) => {
      const field = selectedEventLog();
      if (!field) return;

      const main = e.target.closest('[data-evt-main]');
      if (main) {
        tabs.main = main.dataset.evtMain;
        applyTabs(section);
        return;
      }

      const sub = e.target.closest('[data-evt-sub]');
      if (sub) {
        tabs.sub = sub.dataset.evtSub;
        applyTabs(section);
        return;
      }

      const action = e.target.closest('[data-evt-action]')?.dataset.evtAction;
      if (action === 'duplicate') {
        window.JWPLCDialog?.alert?.('Un solo EVENT_LOG por página: muévelo a otra página si necesitas registrar eventos allí.');
        return;
      }
      if (action === 'delete') {
        editor()?.deleteSelectedField?.();
        return;
      }
      if (action === 'copy') {
        const contract = contractText(field);
        navigator.clipboard?.writeText(contract).catch(() => {});
        return;
      }

      const set = e.target.closest('[data-evt-set]');
      if (set) {
        const prop = set.dataset.evtSet;
        const val = Number(set.dataset.value);
        mutate((f) => { f[prop] = Boolean(val); });
        return;
      }

      const pageChip = e.target.closest('[data-evt-page]');
      if (pageChip) {
        const target = Number(pageChip.dataset.evtPage);
        const busy = (editor()?.getAllFields?.() || []).some((f) => f.type === 'EVENT_LOG' && f !== field && Number(f.page || 0) === target);
        if (busy) {
          window.JWPLCDialog?.alert?.('Esa página ya tiene un HISTORIAL (EVENT_LOG).');
          return;
        }
        editor()?.moveSelectedFieldToPage?.(target);
        return;
      }

      if (e.target.closest('[data-evt-add-desc]')) {
        mutate((f) => {
          if (!Array.isArray(f.descriptors)) f.descriptors = [];
          const nextCode = f.descriptors.length ? Math.max(...f.descriptors.map((d) => Number(d.code) || 0)) + 1 : 100;
          f.descriptors.push({ code: nextCode, label: `Evento #${nextCode}`, severity: 'INFO', category: 'PROCESS' });
        });
        return;
      }

      const delBtn = e.target.closest('[data-evt-del-desc]');
      if (delBtn) {
        const idx = Number(delBtn.dataset.evtDelDesc);
        mutate((f) => {
          if (f.descriptors && f.descriptors[idx]) f.descriptors.splice(idx, 1);
        });
        return;
      }
    });

    const NUMERIC_PROPS = new Set(['x', 'y', 'width', 'height', 'ramCapacity']);

    function applyPropInput(t, commit) {
      const prop = t.dataset.evtProp;
      if (!prop) return;
      mutate((f) => {
        if (prop === 'x') f.x = clamp(Math.trunc(Number(t.value) || 0), 0, SCREEN_W - 1);
        else if (prop === 'y') f.y = clamp(Math.trunc(Number(t.value) || 0), 0, SCREEN_H - 1);
        else if (prop === 'width') f.width = clamp(Math.trunc(Number(t.value) || 40), 40, SCREEN_W);
        else if (prop === 'height') f.height = clamp(Math.trunc(Number(t.value) || 30), 30, SCREEN_H);
        else if (prop === 'id') f.id = sanitizeSymbol(t.value, f.id).toUpperCase();
        else if (prop === 'ramCapacity') f.ramCapacity = Number(t.value) || 64;
        else if (t.type === 'checkbox') f[prop] = t.checked;
        else f[prop] = t.value;
      }, commit);
    }

    section.addEventListener('input', (e) => {
      const t = e.target;
      if (t.matches('[data-evt-prop]')) applyPropInput(t, false);
      if (t.matches('[data-evt-color]')) {
        const colKey = t.dataset.evtColor;
        mutate((f) => {
          f.colors[colKey] = hexToRgb565(t.value);
        }, false);
      }
    });

    section.addEventListener('change', (e) => {
      const t = e.target;
      if (t.matches('[data-evt-prop]')) applyPropInput(t, true);
      if (t.matches('[data-evt-color]')) {
        const colKey = t.dataset.evtColor;
        mutate((f) => {
          f.colors[colKey] = hexToRgb565(t.value);
        }, true);
      }
    });

    return section;
  }

  function contractText(field) {
    if (!field) return '';
    return `// ==========================================
// Registrar eventos en tiempo de ejecución:
// ==========================================

// 1. Registro simple por código numérico:
JWPLC_Display.logEvent(100);

// 2. Registro completo con transición, severidad y categoría:
JWPLC_Display.logEvent(
    /* code */       101,
    /* transition */ JWPLC_EVENT_TRANS_ACTIVE,
    /* category */   JWPLC_EVENT_CAT_SYSTEM,
    /* severity */   JWPLC_EVENT_SEV_INFO,
    /* texto opc. */ "Enlace restablecido"
);

// 3. Consultar cantidad o limpiar buffer:
size_t total = JWPLC_Display.eventCount();
// JWPLC_Display.clearEvents();`;
  }

  function syncInspector() {
    const field = selectedEventLog();
    const section = document.getElementById('eventLogInspectorSection');
    if (!field || !section) return;

    ensureState(field);
    applyTabs(section);

    // Título y Nombre
    const titleIn = section.querySelector('[data-evt-prop="title"]');
    if (titleIn) titleIn.value = field.title || 'Historial de Eventos';
    const nameIn = section.querySelector('[data-evt-prop="name"]');
    if (nameIn) nameIn.value = field.name || '';
    const idIn = section.querySelector('[data-evt-prop="id"]');
    if (idIn) idIn.value = field.id || '';

    // Marco botones
    section.querySelectorAll('[data-evt-set="frame"]').forEach((b) => {
      const val = Number(b.dataset.value) === 1;
      b.classList.toggle('active', field.frame === val);
    });

    // Ubicación
    ['x', 'y', 'width', 'height'].forEach((k) => {
      const input = section.querySelector(`[data-evt-prop="${k}"]`);
      if (input) input.value = String(field[k]);
    });

    // Readouts
    const { rows } = layout(field);
    section.querySelectorAll('[data-evt-readout="rows"], [data-evt-readout="rows2"]').forEach((el) => el.textContent = `${rows} filas`);
    const wEl = section.querySelector('[data-evt-readout="width"]');
    if (wEl) wEl.textContent = `${field.width} px`;
    const hEl = section.querySelector('[data-evt-readout="height"]');
    if (hEl) hEl.textContent = `${field.height} px`;
    const ramEl = section.querySelector('[data-evt-readout="ram"]');
    if (ramEl) ramEl.textContent = `${field.ramCapacity || 64} evts`;
    const descCountEl = section.querySelector('[data-evt-readout="descCount"]');
    if (descCountEl) descCountEl.textContent = `${field.descriptors?.length || 0}`;
    const ramBytesEl = section.querySelector('[data-evt-readout="ramBytes"]');
    if (ramBytesEl) ramBytesEl.textContent = `${(field.ramCapacity || 64) * 16} B`;

    // Páginas chips
    const pagesHost = section.querySelector('[data-evt-pages]');
    if (pagesHost) {
      const pages = editor()?.getPages?.() || [];
      pagesHost.innerHTML = pages.map((p) =>
        `<button type="button" class="insp-chip${Number(p.id) === Number(field.page || 0) ? ' active' : ''}" data-evt-page="${p.id}">${String(Number(p.id) + 1).padStart(2, '0')} · ${escapeHtml(p.name)}</button>`
      ).join('');
    }

    // Colores
    const c = field.colors || DEFAULT_COLORS;
    section.querySelectorAll('[data-evt-color]').forEach((input) => {
      const key = input.dataset.evtColor;
      if (c[key] !== undefined) {
        input.value = rgb565ToHex(c[key]);
      }
    });

    // Almacenamiento
    const ramSelect = section.querySelector('[data-evt-prop="ramCapacity"]');
    if (ramSelect) ramSelect.value = String(field.ramCapacity || 64);
    const framCb = section.querySelector('[data-evt-prop="enableFram"]');
    if (framCb) framCb.checked = Boolean(field.enableFram);
    const sdCb = section.querySelector('[data-evt-prop="enableSd"]');
    if (sdCb) sdCb.checked = Boolean(field.enableSd);
    const sdNameInput = section.querySelector('[data-evt-prop="sdFileName"]');
    if (sdNameInput) sdNameInput.value = field.sdFileName || '/EVENTOS.CSV';

    // Descriptores
    const listContainer = section.querySelector('[data-evt-descriptors-list]');
    if (listContainer) {
      const descList = field.descriptors || [];
      listContainer.innerHTML = descList.map((d, i) => `
        <div style="display: flex; gap: 4px; align-items: center; margin-bottom: 4px; background: rgba(255,255,255,0.04); padding: 4px; border-radius: 4px;">
          <input type="number" style="width: 50px;" class="field-input" value="${d.code}" onchange="window.JWPLCHMIEventLog.updateDescriptor(${i}, 'code', this.value)">
          <input type="text" style="flex: 1;" class="field-input" value="${escapeHtml(d.label)}" onchange="window.JWPLCHMIEventLog.updateDescriptor(${i}, 'label', this.value)">
          <select style="width: 65px;" class="field-input" onchange="window.JWPLCHMIEventLog.updateDescriptor(${i}, 'severity', this.value)">
            <option value="INFO" ${d.severity === 'INFO' ? 'selected' : ''}>INFO</option>
            <option value="WARN" ${d.severity === 'WARN' ? 'selected' : ''}>AVISO</option>
            <option value="CRIT" ${d.severity === 'CRIT' ? 'selected' : ''}>CRIT</option>
          </select>
          <select style="width: 75px;" class="field-input" onchange="window.JWPLCHMIEventLog.updateDescriptor(${i}, 'category', this.value)">
            <option value="SYSTEM" ${d.category === 'SYSTEM' ? 'selected' : ''}>SIST</option>
            <option value="ALARM" ${d.category === 'ALARM' ? 'selected' : ''}>ALARM</option>
            <option value="INTERLOCK" ${d.category === 'INTERLOCK' ? 'selected' : ''}>INTLK</option>
            <option value="PROCESS" ${d.category === 'PROCESS' ? 'selected' : ''}>PROC</option>
          </select>
          <button type="button" class="icon-button" data-evt-del-desc="${i}" style="color: #ff5252; cursor: pointer;">🗑</button>
        </div>
      `).join('');
    }

    // Contrato
    const contractEl = section.querySelector('[data-evt-contract]');
    if (contractEl) contractEl.textContent = contractText(field);
  }

  function updateDescriptor(index, key, val) {
    const field = selectedEventLog();
    if (!field || !field.descriptors || !field.descriptors[index]) return;
    if (key === 'code') field.descriptors[index].code = Number(val) || 0;
    else field.descriptors[index][key] = val;
    editor()?.render?.();
    syncInspector();
    editor()?.commitHistory?.();
    window.dispatchEvent(new CustomEvent('jwplc:project-changed'));
  }

  // Inicialización de la API global
  window.JWPLCHMIEventLog = {
    createDefault,
    ensureState,
    layout,
    paint,
    generate,
    patchHeader,
    buildInspectorSection,
    syncInspector,
    updateDescriptor
  };

  if (typeof document !== 'undefined') {
    if (document.readyState === 'loading') {
      document.addEventListener('DOMContentLoaded', () => {
        buildInspectorSection();
      });
    } else {
      buildInspectorSection();
    }
    window.addEventListener('jwplc:editor-refresh', () => {
      syncInspector();
    });
  }
})();
