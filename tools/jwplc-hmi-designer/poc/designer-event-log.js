(() => {
  'use strict';

  // Alpha16: plantilla EVENT_LOG (Historial y registro de eventos).
  // Firmware: JWPLC_UI_EventLog.h / JWPLC_UI_EventLog.cpp.

  const SCREEN_W = 320;
  const SCREEN_H = 170;
  const VISIBLE_ROWS = 10;
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
      page: p,
      x: 0,
      y: 0,
      width: SCREEN_W,
      height: SCREEN_H,
      ramCapacity: 64,
      enableFram: true,
      enableSd: true,
      sdFileName: '/EVENTOS.CSV',
      colors: { ...DEFAULT_COLORS },
      descriptors: JSON.parse(JSON.stringify(DEFAULT_DESCRIPTORS))
    };
  }

  function ensureState(field) {
    if (!field || field.type !== 'EVENT_LOG') return field;
    if (!Number.isFinite(Number(field.ramCapacity))) field.ramCapacity = 64;
    if (typeof field.enableFram !== 'boolean') field.enableFram = true;
    if (typeof field.enableSd !== 'boolean') field.enableSd = true;
    if (typeof field.sdFileName !== 'string' || !field.sdFileName) field.sdFileName = '/EVENTOS.CSV';
    if (!field.colors || typeof field.colors !== 'object') field.colors = { ...DEFAULT_COLORS };
    if (!Array.isArray(field.descriptors)) field.descriptors = JSON.parse(JSON.stringify(DEFAULT_DESCRIPTORS));
    field.x = 0;
    field.y = 0;
    field.width = SCREEN_W;
    field.height = SCREEN_H;
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

    // Encabezado
    painter.fillRect(0, 0, SCREEN_W, HEADER_HEIGHT, c.headerBackground);
    if (painter.drawText) {
      painter.drawText('HISTORIAL DE EVENTOS', 6, 4, c.headerText, c.headerBackground, 1);
      painter.drawText('01/06', 215, 4, c.headerText, c.headerBackground, 1);
      painter.drawText(field.enableSd ? 'SD:OK' : 'SD:--', 275, 4, field.enableSd ? 0x07E0 : 0x632C, c.headerBackground, 1);
    }

    // Filas de muestra (10 filas)
    const samples = [
      { time: '14:23:05', code: '#101', trans: 'CES', label: 'Enlace Ethernet restablecido', sev: c.sevInfo },
      { time: '14:22:40', code: '#102', trans: 'ACT', label: 'Enlace Ethernet caido', sev: c.sevWarn },
      { time: '14:15:10', code: '#100', trans: 'ACT', label: 'Arranque de sistema', sev: c.sevInfo },
      { time: '14:02:50', code: '#001', trans: 'CES', label: 'Falla de proceso', sev: c.sevCrit },
      { time: '14:00:15', code: '#001', trans: 'ACT', label: 'Falla de proceso', sev: c.sevCrit },
      { time: '13:58:30', code: '#104', trans: 'CES', label: 'Bus RS485 normalizado', sev: c.sevInfo },
      { time: '13:58:05', code: '#103', trans: 'ACT', label: 'Error bus RS485/Modbus', sev: c.sevWarn }
    ];

    for (let r = 0; r < VISIBLE_ROWS; r++) {
      const y = HEADER_HEIGHT + (r * ROW_HEIGHT);
      const isSelected = (r === 0);
      const rowBg = isSelected ? c.selectBackground : ((r % 2 === 0) ? c.rowBackground : c.rowBackgroundAlt);
      const rowFg = isSelected ? c.selectText : c.rowText;

      painter.fillRect(0, y, SCREEN_W, ROW_HEIGHT, rowBg);

      if (r < samples.length) {
        const item = samples[r];
        // Barra de severidad
        painter.fillRect(0, y, 3, ROW_HEIGHT, item.sev);

        if (painter.drawText) {
          painter.drawText(item.time, 6, y + 3, rowFg, rowBg, 1);
          painter.drawText(item.code, 65, y + 3, rowFg, rowBg, 1);
          painter.drawText(item.trans, 105, y + 3, rowFg, rowBg, 1);
          painter.drawText(item.label, 145, y + 3, rowFg, rowBg, 1);
        }
      }
    }

    // Pie de página
    painter.fillRect(0, 157, SCREEN_W, FOOTER_HEIGHT, 0x10A2);
    if (painter.drawText) {
      painter.drawText('[OK] Detalle     [\x1E\x1F] Desplazar     [ESC] Salir', 12, 160, 0xFFFF, 0x10A2, 1);
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

  function selectedEventLog() {
    const field = editor()?.getSelectedField?.();
    return (field && field.type === 'EVENT_LOG') ? field : null;
  }

  function buildInspectorSection() {
    if (typeof document === 'undefined') return null;

    let section = document.getElementById('eventLogInspectorSection');
    if (section) return section;

    section = document.createElement('section');
    section.id = 'eventLogInspectorSection';
    section.className = 'inspector-section';
    section.style.display = 'none';

    section.innerHTML = `
      <div class="inspector-title"><h2>Historial de Eventos</h2></div>
      <div class="insp-body">
        <div class="bar-card">
          <div class="bar-card-section-title">Almacenamiento y memoria</div>
          <div class="prop-row">
            <label>Buffer en RAM:</label>
            <select class="insp-select" data-evt-prop="ramCapacity">
              <option value="64">64 eventos (~1 KB)</option>
              <option value="128">128 eventos (~2 KB)</option>
            </select>
          </div>
          <div class="prop-row">
            <label>Persistencia FRAM:</label>
            <input type="checkbox" data-evt-prop="enableFram" checked>
            <span class="insp-note">Últimos 16 eventos guardados en 0x1E00</span>
          </div>
          <div class="prop-row">
            <label>Registro en microSD:</label>
            <input type="checkbox" data-evt-prop="enableSd" checked>
          </div>
          <div class="prop-row" data-evt-sd-name-row>
            <label>Archivo CSV:</label>
            <input type="text" class="insp-input" data-evt-prop="sdFileName" value="/EVENTOS.CSV">
          </div>
        </div>

        <div class="bar-card">
          <div class="bar-card-section-title">Descriptores de Eventos</div>
          <div class="insp-note">El texto y la severidad se resuelven por código en la Flash del PLC:</div>
          <div class="evt-desc-list" data-evt-descriptors-list style="max-height: 180px; overflow-y: auto; margin-bottom: 8px;"></div>
          <button type="button" class="bar-visual-btn" data-evt-add-desc style="width: 100%; justify-content: center;">
            <strong>＋</strong> <span>Agregar descriptor de evento</span>
          </button>
        </div>
      </div>
    `;

    const container = document.getElementById('rightSidebar') || document.querySelector('.inspector-panel') || document.body;
    container.appendChild(section);

    // Event listeners
    section.addEventListener('change', (e) => {
      const field = selectedEventLog();
      if (!field) return;

      const prop = e.target.dataset.evtProp;
      if (prop) {
        if (e.target.type === 'checkbox') {
          field[prop] = e.target.checked;
        } else if (prop === 'ramCapacity') {
          field[prop] = Number(e.target.value);
        } else {
          field[prop] = e.target.value;
        }
        editor()?.render?.();
        window.dispatchEvent(new CustomEvent('jwplc:project-changed'));
      }
    });

    section.addEventListener('click', (e) => {
      const field = selectedEventLog();
      if (!field) return;

      if (e.target.closest('[data-evt-add-desc]')) {
        if (!Array.isArray(field.descriptors)) field.descriptors = [];
        const nextCode = field.descriptors.length ? Math.max(...field.descriptors.map((d) => Number(d.code) || 0)) + 1 : 1;
        field.descriptors.push({ code: nextCode, label: `Evento #${nextCode}`, severity: 'INFO', category: 'PROCESS' });
        syncInspector();
        editor()?.render?.();
        window.dispatchEvent(new CustomEvent('jwplc:project-changed'));
        return;
      }

      const delBtn = e.target.closest('[data-evt-del-desc]');
      if (delBtn) {
        const idx = Number(delBtn.dataset.evtDelDesc);
        if (field.descriptors && field.descriptors[idx]) {
          field.descriptors.splice(idx, 1);
          syncInspector();
          editor()?.render?.();
          window.dispatchEvent(new CustomEvent('jwplc:project-changed'));
        }
      }
    });

    return section;
  }

  function syncInspector() {
    const field = selectedEventLog();
    const section = document.getElementById('eventLogInspectorSection');
    if (!field || !section) return;

    ensureState(field);

    const ramSelect = section.querySelector('[data-evt-prop="ramCapacity"]');
    if (ramSelect) ramSelect.value = String(field.ramCapacity || 64);

    const framCb = section.querySelector('[data-evt-prop="enableFram"]');
    if (framCb) framCb.checked = Boolean(field.enableFram);

    const sdCb = section.querySelector('[data-evt-prop="enableSd"]');
    if (sdCb) sdCb.checked = Boolean(field.enableSd);

    const sdNameInput = section.querySelector('[data-evt-prop="sdFileName"]');
    if (sdNameInput) sdNameInput.value = field.sdFileName || '/EVENTOS.CSV';

    const listContainer = section.querySelector('[data-evt-descriptors-list]');
    if (listContainer) {
      const descList = field.descriptors || [];
      listContainer.innerHTML = descList.map((d, i) => `
        <div style="display: flex; gap: 4px; align-items: center; margin-bottom: 4px; background: rgba(255,255,255,0.04); padding: 4px; border-radius: 4px;">
          <input type="number" style="width: 48px;" value="${d.code}" onchange="window.JWPLCHMIEventLog.updateDescriptor(${i}, 'code', this.value)">
          <input type="text" style="flex: 1;" value="${d.label}" onchange="window.JWPLCHMIEventLog.updateDescriptor(${i}, 'label', this.value)">
          <select style="width: 65px;" onchange="window.JWPLCHMIEventLog.updateDescriptor(${i}, 'severity', this.value)">
            <option value="INFO" ${d.severity === 'INFO' ? 'selected' : ''}>INFO</option>
            <option value="WARN" ${d.severity === 'WARN' ? 'selected' : ''}>AVISO</option>
            <option value="CRIT" ${d.severity === 'CRIT' ? 'selected' : ''}>CRIT</option>
          </select>
          <button type="button" data-evt-del-desc="${i}" style="background: none; border: none; color: #ff5252; cursor: pointer;">✕</button>
        </div>
      `).join('');
    }
  }

  function updateDescriptor(index, key, val) {
    const field = selectedEventLog();
    if (!field || !field.descriptors || !field.descriptors[index]) return;
    if (key === 'code') field.descriptors[index].code = Number(val) || 0;
    else field.descriptors[index][key] = val;
    editor()?.render?.();
    window.dispatchEvent(new CustomEvent('jwplc:project-changed'));
  }

  // Inicialización de la API global
  window.JWPLCHMIEventLog = {
    createDefault,
    ensureState,
    paint,
    generate,
    patchHeader,
    buildInspectorSection,
    syncInspector,
    updateDescriptor
  };

  if (typeof document !== 'undefined') {
    document.addEventListener('DOMContentLoaded', () => {
      buildInspectorSection();
    });
    window.addEventListener('jwplc:editor-refresh', () => {
      syncInspector();
    });
  }
})();
