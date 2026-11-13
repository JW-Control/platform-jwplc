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
    frame: 0xFFFF,
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
      headerFont: 'GLCD',
      rowFont: 'GLCD',
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
    if (typeof field.headerFont !== 'string') field.headerFont = 'GLCD';
    if (typeof field.rowFont !== 'string') field.rowFont = 'GLCD';
    if (!Number.isFinite(Number(field.ramCapacity))) field.ramCapacity = 64;
    if (typeof field.enableFram !== 'boolean') field.enableFram = true;
    if (typeof field.enableSd !== 'boolean') field.enableSd = true;
    if (typeof field.sdFileName !== 'string' || !field.sdFileName) field.sdFileName = '/EVENTOS.CSV';
    if (!field.colors || typeof field.colors !== 'object') field.colors = { ...DEFAULT_COLORS };
    if (field.colors.frame === undefined) field.colors.frame = 0xFFFF;
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

    // Marco exterior visible (blanco o color configurado)
    if (field.frame && painter.drawRect) {
      painter.drawRect(x0, y0, w, h, (c.frame !== undefined ? c.frame : 0xFFFF));
    }

    // Encabezado
    painter.fillRect(x0 + border, y0 + border, w - 2 * border, HEADER_HEIGHT, c.headerBackground);
    if (painter.drawText) {
      const title = (w >= 200) ? (field.title || 'HISTORIAL DE EVENTOS') : 'HISTORIAL';
      painter.drawText(title, x0 + border + 6, y0 + border + 4, c.headerText, c.headerBackground, 1, field.headerFont);
      // Mismas posiciones que JWPLC_UI_EventLog.cpp; con varias páginas, el indicador
      // NN/TT tapa la esquina y el firmware corre el contador y "SD" a su izquierda.
      let shift = 0;
      const pagesApi = window.JWPLCHMIPages;
      const pageCount = window.JWPLCHMIEditor?.getPages?.()?.length || 1;
      if (pagesApi?.indicatorRect && pageCount > 1) {
        const r = pagesApi.indicatorRect();
        const hx0 = x0 + border, hy0 = y0 + border, hx1 = x0 + border + w, hy1 = hy0 + HEADER_HEIGHT;
        if (r.x < hx1 && r.x + r.w > hx0 && r.y < hy1 && r.y + r.h > hy0) shift = Math.max(0, hx1 - (r.x - 4));
      }
      if (w >= 180) {
        painter.drawText('01/06', x0 + border + w - (w >= 240 ? 110 : 70) - shift, y0 + border + 4, c.headerText, c.headerBackground, 1, field.headerFont);
      }
      if (w >= 240) {
        painter.drawText(field.enableSd ? 'SD:OK' : 'SD:--', x0 + border + w - 50 - shift, y0 + border + 4, field.enableSd ? 0x07E0 : 0x632C, c.headerBackground, 1, field.headerFont);
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
          painter.drawText(item.time, x0 + border + 6, y + 3, rowFg, rowBg, 1, field.rowFont);
          if (w >= 120) {
            painter.drawText(item.code, x0 + border + 60, y + 3, rowFg, rowBg, 1, field.rowFont);
          }
          if (w >= 165) {
            painter.drawText(item.trans, x0 + border + 98, y + 3, rowFg, rowBg, 1, field.rowFont);
          }
          if (w >= 210) {
            const maxChars = Math.max(1, Math.trunc((w - 145) / 6));
            const truncated = item.label.slice(0, maxChars);
            painter.drawText(truncated, x0 + border + 138, y + 3, rowFg, rowBg, 1, field.rowFont);
          }
        }
      }
    }

    // Pie de página
    const footerY = y0 + h - border - FOOTER_HEIGHT;
    painter.fillRect(x0 + border, footerY, w - 2 * border, FOOTER_HEIGHT, 0x10A2);
    if (painter.drawText) {
      if (w >= 250) {
        painter.drawText('[OK] Detalle     [\x1E\x1F] Desplazar     [ESC] Salir', x0 + border + 10, footerY + 3, 0xFFFF, 0x10A2, 1, field.rowFont);
      } else {
        painter.drawText('[OK] [\x1E\x1F] [ESC]', x0 + border + 6, footerY + 3, 0xFFFF, 0x10A2, 1, field.rowFont);
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
        0x${(c.headerBackground !== undefined ? c.headerBackground : 0x2124).toString(16).toUpperCase()},
        0x${(c.headerText !== undefined ? c.headerText : 0xFFFF).toString(16).toUpperCase()},
        0x${(c.rowBackground !== undefined ? c.rowBackground : 0x0000).toString(16).toUpperCase()},
        0x${(c.rowBackgroundAlt !== undefined ? c.rowBackgroundAlt : 0x10A2).toString(16).toUpperCase()},
        0x${(c.rowText !== undefined ? c.rowText : 0xC618).toString(16).toUpperCase()},
        0x${(c.selectBackground !== undefined ? c.selectBackground : 0x03FF).toString(16).toUpperCase()},
        0x${(c.selectText !== undefined ? c.selectText : 0xFFFF).toString(16).toUpperCase()},
        0x${(c.sevInfo !== undefined ? c.sevInfo : 0x07E0).toString(16).toUpperCase()},
        0x${(c.sevWarn !== undefined ? c.sevWarn : 0xFD20).toString(16).toUpperCase()},
        0x${(c.sevCrit !== undefined ? c.sevCrit : 0xF800).toString(16).toUpperCase()},
        0x${((c.frame !== undefined ? c.frame : 0xFFFF) & 0xFFFF).toString(16).toUpperCase()}
    }${(() => {
      const evtFonts = (typeof module === 'object' && module.exports)
        ? require('./designer-fonts.js')
        : (typeof window !== 'undefined' ? window.JWPLCDesignerFonts : null);
      const getEvtFontSym = (fId) => {
        if (!fId || fId === 'GLCD') return null;
        if (evtFonts?.cppSymbol) return evtFonts.cppSymbol(fId);
        if (String(fId).startsWith('JWPLC_FONT_')) return fId;
        if (fId === 'FONT_16' || fId === 'FONT2') return 'JWPLC_FONT_16';
        if (fId === 'FONT_26' || fId === 'FONT4') return 'JWPLC_FONT_26';
        if (fId === '7SEG_48' || fId === 'FONT_7SEG_48') return 'JWPLC_FONT_7SEG_48';
        return `JWPLC_FONT_${fId}`;
      };
      const headerFontSym = getEvtFontSym(field.headerFont);
      const rowFontSym = getEvtFontSym(field.rowFont);
      if (!headerFontSym && !rowFontSym) return '';
      return `,\n    /* headerFont */ ${headerFontSym ? `&${headerFontSym}` : 'nullptr'},\n    /* rowFont */ ${rowFontSym ? `&${rowFontSym}` : 'nullptr'}`;
    })()}
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
  const EVT_PREVIEW_W = 148;
  const EVT_PREVIEW_ROWS = 4;
  const COLOR_META = Object.freeze({
    frame:            { name: 'Marco exterior',         letter: 'M' },
    headerBackground: { name: 'Cabecera · Fondo',       letter: 'F' },
    headerText:       { name: 'Cabecera · Texto',       letter: 'T' },
    selectBackground: { name: 'Selección · Fondo',      letter: 'F' },
    selectText:       { name: 'Selección · Texto',      letter: 'T' },
    rowBackgroundAlt: { name: 'Fila B (impar) · Fondo', letter: 'F' },
    rowBackground:    { name: 'Fila A (par) · Fondo',   letter: 'F' },
    rowText:          { name: 'Filas · Texto',          letter: 'T' },
    sevInfo:          { name: 'Severidad · INFO',       letter: 'I' },
    sevWarn:          { name: 'Severidad · AVISO',      letter: 'A' },
    sevCrit:          { name: 'Severidad · CRÍTICA',    letter: 'C' }
  });
  const EVT_LEGEND_ROWS = Object.freeze([
    { label: 'Cabecera',  keys: ['headerBackground', 'headerText'] },
    { label: 'Selección', keys: ['selectBackground', 'selectText'] },
    { label: 'Fila B',    keys: ['rowBackgroundAlt'] },
    { label: 'Fila A/Tx', keys: ['rowBackground', 'rowText'] },
    { label: 'Severidad', keys: ['sevInfo', 'sevWarn', 'sevCrit'] }
  ]);
  let activeColorKey = 'headerBackground';
  let evtColorEditor = null;
  let lastPreviewFb = null;
  let lastPreviewPitch = 0;

  function evtPreviewHeight(field) {
    const border = field?.frame ? 1 : 0;
    return 2 * border + HEADER_HEIGHT + EVT_PREVIEW_ROWS * ROW_HEIGHT + FOOTER_HEIGHT;
  }

  function scanColorBounds(fb, pitch, color565, x0, y0, w, h) {
    if (!fb || !pitch || w <= 0 || h <= 0) return null;
    const target = Number(color565) & 0xFFFF;
    let minX = Infinity;
    let minY = Infinity;
    let maxX = -1;
    let maxY = -1;
    for (let y = y0; y < y0 + h; y++) {
      for (let x = x0; x < x0 + w; x++) {
        if ((fb[y * pitch + x] & 0xFFFF) !== target) continue;
        if (x < minX) minX = x;
        if (x > maxX) maxX = x;
        if (y < minY) minY = y;
        if (y > maxY) maxY = y;
      }
    }
    return maxX >= 0 ? { x: minX, y: minY, w: maxX - minX + 1, h: maxY - minY + 1 } : null;
  }

  function evtZoneRectsFor(field, fb, pitch) {
    const w = EVT_PREVIEW_W;
    const h = evtPreviewHeight(field);
    const border = field?.frame ? 1 : 0;
    const innerW = w - 2 * border;
    const c = field?.colors || DEFAULT_COLORS;
    const rowBand = (r) => ({
      x: border + 3,
      y: border + HEADER_HEIGHT + r * ROW_HEIGHT,
      w: Math.max(1, innerW - 3),
      h: ROW_HEIGHT
    });
    const sevBar = (r) => ({
      x: border,
      y: border + HEADER_HEIGHT + r * ROW_HEIGHT,
      w: 3,
      h: ROW_HEIGHT
    });
    const headerBand = { x: border, y: border, w: innerW, h: HEADER_HEIGHT };
    const headerTextFallback = { x: border + 5, y: border + 3, w: 56, h: 10 };
    const selBand = rowBand(0);
    const row1Band = rowBand(1);
    const row2Band = rowBand(2);

    const headerBg = Number(c.headerBackground) & 0xFFFF;
    const headerFg = Number(c.headerText) & 0xFFFF;
    const headerTextBox = headerFg !== headerBg
      ? (scanColorBounds(fb, pitch, headerFg, headerBand.x, headerBand.y, headerBand.w, headerBand.h) || headerTextFallback)
      : headerBand;

    const selBg = Number(c.selectBackground) & 0xFFFF;
    const selFg = Number(c.selectText) & 0xFFFF;
    const selectTextBox = selFg !== selBg
      ? (scanColorBounds(fb, pitch, selFg, selBand.x, selBand.y, selBand.w, selBand.h) || selBand)
      : selBand;

    const rowFg = Number(c.rowText) & 0xFFFF;
    const row1Text = rowFg !== (Number(c.rowBackgroundAlt) & 0xFFFF)
      ? scanColorBounds(fb, pitch, rowFg, row1Band.x, row1Band.y, row1Band.w, row1Band.h)
      : row1Band;
    const row2Text = rowFg !== (Number(c.rowBackground) & 0xFFFF)
      ? scanColorBounds(fb, pitch, rowFg, row2Band.x, row2Band.y, row2Band.w, row2Band.h)
      : row2Band;
    const rowTextRects = [row1Text, row2Text].filter(Boolean);

    return {
      frame: field?.frame ? [{ x: 0, y: 0, w, h, inset: true }] : [],
      headerBackground: [headerBand],
      headerText: [headerTextBox],
      selectBackground: [selBand],
      selectText: [selectTextBox],
      rowBackgroundAlt: [row1Band],
      rowBackground: [row2Band],
      rowText: rowTextRects.length ? rowTextRects : [row1Band],
      sevInfo: [sevBar(0), sevBar(2)],
      sevWarn: [sevBar(1)],
      sevCrit: [sevBar(3)]
    };
  }

  function pickColorKeyAt(field, lx, ly, w, h) {
    const border = field?.frame ? 1 : 0;
    if (field?.frame && (lx <= 0 || ly <= 0 || lx >= w - 1 || ly >= h - 1)) return 'frame';
    const fbPix = (lastPreviewFb && lastPreviewPitch && lastPreviewFb[ly * lastPreviewPitch + lx] !== undefined)
      ? (lastPreviewFb[ly * lastPreviewPitch + lx] & 0xFFFF)
      : null;
    const c = field?.colors || DEFAULT_COLORS;
    if (ly < border + HEADER_HEIGHT) {
      const fg = Number(c.headerText) & 0xFFFF;
      const bg = Number(c.headerBackground) & 0xFFFF;
      return (fbPix !== null && fbPix === fg && fg !== bg) ? 'headerText' : 'headerBackground';
    }
    if (ly < border + HEADER_HEIGHT + EVT_PREVIEW_ROWS * ROW_HEIGHT) {
      const r = clamp(Math.floor((ly - border - HEADER_HEIGHT) / ROW_HEIGHT), 0, EVT_PREVIEW_ROWS - 1);
      if (lx < border + 4) {
        if (r === 1) return 'sevWarn';
        if (r === 3) return 'sevCrit';
        return 'sevInfo';
      }
      if (r === 0) {
        const fg = Number(c.selectText) & 0xFFFF;
        const bg = Number(c.selectBackground) & 0xFFFF;
        return (fbPix !== null && fbPix === fg && fg !== bg) ? 'selectText' : 'selectBackground';
      }
      const fg = Number(c.rowText) & 0xFFFF;
      const bg = (r % 2 === 1 ? Number(c.rowBackgroundAlt) : Number(c.rowBackground)) & 0xFFFF;
      if (fbPix !== null && fbPix === fg && fg !== bg) return 'rowText';
      return (r % 2 === 1) ? 'rowBackgroundAlt' : 'rowBackground';
    }
    return 'rowBackground';
  }

  function drawEvtZones(section, field) {
    if (!section || !field || typeof window === 'undefined') return;
    const body = section.querySelector('[data-evt-preview-body]');
    const canvas = section.querySelector('[data-evt-preview-canvas]');
    if (!body || !canvas) return;
    const zones = evtZoneRectsFor(field, lastPreviewFb, lastPreviewPitch);
    const rects = zones[activeColorKey] || [];
    const draw = () => window.JWPLCZoneIndicator?.draw?.(body, canvas, rects);
    if (typeof requestAnimationFrame === 'function') requestAnimationFrame(draw);
    else draw();
  }

  function buildAppearanceLegend(section) {
    const legend = section.querySelector('[data-evt-legend]');
    if (!legend || legend.dataset.built) return;
    legend.dataset.built = '1';
    legend.innerHTML = EVT_LEGEND_ROWS.map((r) => `
      <div class="menu-legend-row" data-evt-legend-row data-keys="${r.keys.join(',')}">
        <span class="menu-legend-label">${r.label}</span>${r.keys.map((key) => {
          const meta = COLOR_META[key];
          return `<button type="button" class="menu-swatch" data-evt-swatch="${key}" title="${meta.name}"><span>${meta.letter}</span></button>`;
        }).join('')}
      </div>`).join('');
  }

  function syncEvtAppearance(section, field) {
    if (!section || !field) return;
    buildAppearanceLegend(section);
    if (!field.frame && activeColorKey === 'frame') activeColorKey = 'headerBackground';

    const frameRow = section.querySelector('[data-evt-frame-row]');
    if (frameRow) {
      frameRow.hidden = !field.frame;
      frameRow.classList.toggle('is-active', activeColorKey === 'frame');
    }

    const canvas = section.querySelector('[data-evt-preview-canvas]');
    const h = evtPreviewHeight(field);
    const previewField = { ...field, x: 0, y: 0, width: EVT_PREVIEW_W, height: h };
    const out = editor()?.renderFieldPreview?.(previewField);
    if (canvas && out?.buffer) {
      lastPreviewFb = out.buffer;
      lastPreviewPitch = out.width;
      if (canvas.width !== EVT_PREVIEW_W) canvas.width = EVT_PREVIEW_W;
      if (canvas.height !== h) canvas.height = h;
      canvas.dataset.logicalW = String(EVT_PREVIEW_W);
      canvas.dataset.logicalH = String(h);
      const ctx = canvas.getContext('2d');
      const img = ctx.createImageData(EVT_PREVIEW_W, h);
      for (let y = 0; y < h; y++) {
        const row = y * out.width;
        for (let x = 0; x < EVT_PREVIEW_W; x++) {
          const v = out.buffer[row + x];
          const o = (y * EVT_PREVIEW_W + x) * 4;
          img.data[o] = Math.round(((v >> 11) & 0x1F) * 255 / 31);
          img.data[o + 1] = Math.round(((v >> 5) & 0x3F) * 255 / 63);
          img.data[o + 2] = Math.round((v & 0x1F) * 255 / 31);
          img.data[o + 3] = 255;
        }
      }
      ctx.putImageData(img, 0, 0);
    } else if (canvas) {
      canvas.dataset.logicalW = String(EVT_PREVIEW_W);
      canvas.dataset.logicalH = String(h);
    }

    const c = field.colors || DEFAULT_COLORS;
    Object.keys(DEFAULT_COLORS).forEach((key) => {
      const v = (Number(c[key] ?? DEFAULT_COLORS[key]) || 0) & 0xFFFF;
      const lum = ((v >> 11) + ((v >> 5) & 0x3F) / 2 + (v & 0x1F));
      section.querySelectorAll(`[data-evt-swatch="${key}"]`).forEach((sw) => {
        sw.style.background = rgb565ToHex(v);
        sw.style.color = lum > 40 ? '#000' : '#fff';
        sw.classList.toggle('active', key === activeColorKey);
      });
    });

    section.querySelectorAll('[data-evt-legend-row]').forEach((row) => {
      const keys = (row.dataset.keys || '').split(',');
      row.classList.toggle('is-active', keys.includes(activeColorKey));
    });

    const activeVal = (Number(c[activeColorKey] ?? DEFAULT_COLORS[activeColorKey]) || 0) & 0xFFFF;
    evtColorEditor?.set(activeVal, COLOR_META[activeColorKey]?.name || activeColorKey);

    if (tabs.main === 'design' && tabs.sub === 'appearance') {
      drawEvtZones(section, field);
    }
  }

  function descriptorMeta() {
    return {
      sev: {
        INFO: { label: 'INFO', short: 'INFO', cls: 'is-info' },
        WARN: { label: 'AVISO', short: 'AVISO', cls: 'is-warn' },
        CRIT: { label: 'CRÍTICA', short: 'CRIT', cls: 'is-crit' }
      },
      cat: {
        SYSTEM: { label: 'Sistema', short: 'SIST' },
        ALARM: { label: 'Alarma', short: 'ALARM' },
        INTERLOCK: { label: 'Interlock', short: 'INTLK' },
        PROCESS: { label: 'Proceso', short: 'PROC' }
      }
    };
  }
  let selectedDescIndex = 0;

  function renderDescriptorList(section, field) {
    const listContainer = section.querySelector('[data-evt-descriptors-list]');
    if (!listContainer) return;
    const descList = field.descriptors || [];
    if (descList.length === 0) selectedDescIndex = -1;
    else selectedDescIndex = clamp(selectedDescIndex < 0 ? 0 : selectedDescIndex, 0, descList.length - 1);

    const { sev, cat } = descriptorMeta();
    const signature = JSON.stringify([descList.map((d) => [d.code, d.label, d.severity, d.category]), selectedDescIndex]);
    if (listContainer.dataset.signature === signature) return;
    listContainer.dataset.signature = signature;

    if (!descList.length) {
      listContainer.innerHTML = '<div class="insp-note">Catálogo vacío: pulsa «＋ Agregar» para definir un evento.</div>';
      return;
    }
    listContainer.innerHTML = descList.map((d, i) => {
      const s = sev[d.severity] || sev.INFO;
      const cLabel = (cat[d.category] || cat.PROCESS).short;
      return `
        <div class="menu-tree-row${i === selectedDescIndex ? ' active' : ''}" data-evt-desc-row="${i}">
          <span class="evt-sev-pill ${s.cls}">${s.short}</span>
          <span class="evt-desc-code">#${Number(d.code) || 0}</span>
          <span class="menu-tree-label">${escapeHtml(d.label || '')}</span>
          <span class="menu-tree-var">${cLabel}</span>
          <span class="menu-tree-btns">
            <button type="button" data-evt-del-desc="${i}" title="Eliminar evento">✕</button>
          </span>
        </div>`;
    }).join('');
  }

  function renderDescriptorEditor(section, field) {
    const host = section.querySelector('[data-evt-desc-editor]');
    if (!host) return;
    const descList = field.descriptors || [];
    const d = (selectedDescIndex >= 0 && selectedDescIndex < descList.length) ? descList[selectedDescIndex] : null;
    if (!d) {
      host.dataset.signature = 'none';
      host.innerHTML = '<div class="insp-note">Elige un evento de la lista para editar su código, texto, severidad y categoría.</div>';
      return;
    }
    const { sev, cat } = descriptorMeta();
    const sig = `${selectedDescIndex}|${d.severity}|${d.category}`;
    if (host.dataset.signature !== sig) {
      host.dataset.signature = sig;
      host.innerHTML = `
        <div class="bar-card-section-title" data-evt-desc-heading>Evento #${Number(d.code) || 0} · ${(cat[d.category] || cat.PROCESS).label}</div>
        <div class="evt-desc-fields">
          <label class="field-label">Código (#)
            <input type="number" class="field-input code-input" data-evt-desc-prop="code" min="0" max="65535" />
          </label>
          <label class="field-label">Descripción en HMI
            <input type="text" class="field-input" data-evt-desc-prop="label" maxlength="48" />
          </label>
        </div>
        <span class="bar-visual-group-title insp-gap">Severidad</span>
        <div class="insp-chip-row">
          ${Object.entries(sev).map(([k, info]) =>
            `<button type="button" class="insp-chip${d.severity === k ? ' active' : ''}" data-evt-desc-sev="${k}">${info.label}</button>`
          ).join('')}
        </div>
        <span class="bar-visual-group-title insp-gap">Categoría</span>
        <div class="insp-chip-row">
          ${Object.entries(cat).map(([k, info]) =>
            `<button type="button" class="insp-chip${d.category === k ? ' active' : ''}" data-evt-desc-cat="${k}">${info.label}</button>`
          ).join('')}
        </div>
      `;
    }
    const heading = host.querySelector('[data-evt-desc-heading]');
    if (heading) heading.textContent = `Evento #${Number(d.code) || 0} · ${(cat[d.category] || cat.PROCESS).label}`;
    const codeIn = host.querySelector('[data-evt-desc-prop="code"]');
    if (codeIn && document.activeElement !== codeIn) codeIn.value = String(Number(d.code) || 0);
    const labelIn = host.querySelector('[data-evt-desc-prop="label"]');
    if (labelIn && document.activeElement !== labelIn) labelIn.value = d.label || '';
  }

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
            <div class="bar-card" data-evt-font-card>
              <div class="insp-font-head">
                <span class="bar-card-section-title" style="margin:0;">Tipografía</span>
                <span class="insp-type-chip">Clásica 5×7 (GLCD · 8 px)</span>
              </div>
              <div class="insp-note">Fuente única fija optimizada para alinear columnas de hora, código y severidad.</div>
            </div>
            <div class="bar-card">
              <div class="bar-card-section-title">Marco exterior</div>
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

          <!-- PANEL: Apariencia (Vista previa + zonas + editor fijo sin scroll) -->
          <div class="insp-panel" data-evt-panel="design:appearance" hidden>
            <div class="bar-card" data-evt-colors>
              <div class="evt-case">
                <div class="bar-card-section-title">Vista previa y zonas</div>
                <div class="menu-case-body zone-host evt-case-body" data-evt-preview-body>
                  <canvas class="menu-case-canvas evt-case-canvas" data-evt-preview-canvas width="148" height="88"></canvas>
                  <div class="menu-case-legend" data-evt-legend></div>
                </div>
              </div>
              <div class="menu-legend-row menu-frame-row" data-evt-frame-row data-keys="frame">
                <span class="menu-legend-label">Marco exterior</span>
                <button type="button" class="menu-swatch" data-evt-swatch="frame" title="Marco exterior"><span>M</span></button>
              </div>
              <div class="menu-palette evt-palette">
                <div class="insp-note">Clic en un cuadro, fila o en la vista previa para editar su color.</div>
                <div data-evt-color-editor></div>
              </div>
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
              <div class="insp-font-head">
                <span class="bar-card-section-title" style="margin:0;">Buffer circular en RAM</span>
                <span class="insp-hint" data-evt-readout="ramStorageHint">64 evts · 1 KB</span>
              </div>
              <div class="bar-visual-grid-2 insp-gap">
                <button type="button" class="bar-visual-btn" data-evt-ram="64"><strong>64 evts</strong><span>~1 KB RAM</span></button>
                <button type="button" class="bar-visual-btn" data-evt-ram="128"><strong>128 evts</strong><span>~2 KB RAM</span></button>
              </div>
              <div class="insp-note">Política FIFO: al llenarse el buffer se descartan los registros más antiguos.</div>
            </div>
            <div class="bar-card">
              <div class="bar-card-section-title">Persistencia en FRAM (sin batería)</div>
              <div class="bar-visual-grid-2">
                <button type="button" class="bar-visual-btn" data-evt-set="enableFram" data-value="0"><span>Solo RAM</span></button>
                <button type="button" class="bar-visual-btn" data-evt-set="enableFram" data-value="1"><strong>FRAM</strong><span>16 últimos</span></button>
              </div>
              <div class="insp-readouts insp-readouts-grid insp-gap">
                <span>Bloque sistema <strong>0x1E00</strong></span>
                <span>Memoria <strong>256 B</strong></span>
              </div>
            </div>
            <div class="bar-card">
              <div class="bar-card-section-title">Exportación a MicroSD</div>
              <div class="bar-visual-grid-2">
                <button type="button" class="bar-visual-btn" data-evt-set="enableSd" data-value="0"><span>Desactivada</span></button>
                <button type="button" class="bar-visual-btn" data-evt-set="enableSd" data-value="1"><span>CSV automático</span></button>
              </div>
              <div class="evt-sd-file insp-gap" data-evt-sd-file>
                <label class="field-label">Archivo CSV en microSD
                  <input type="text" class="field-input code-input" data-evt-prop="sdFileName" value="/EVENTOS.CSV" maxlength="32" />
                </label>
              </div>
            </div>
          </div>

          <!-- PANEL: Descriptores de eventos -->
          <div class="insp-panel" data-evt-panel="descriptors" hidden>
            <div class="bar-card">
              <div class="bar-card-header">
                <span class="bar-card-title">Catálogo Flash (PROGMEM) <span class="insp-hint" data-evt-readout="descListCount"></span></span>
                <button type="button" class="insp-chip" data-evt-add-desc>＋ Agregar</button>
              </div>
              <div class="menu-tree evt-desc-tree" data-evt-descriptors-list></div>
            </div>
            <div class="bar-card" data-evt-desc-editor></div>
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

    if (!evtColorEditor && window.JWPLCColorEditor) {
      evtColorEditor = window.JWPLCColorEditor.create({
        paletteAttr: 'data-evt-palette',
        onInput: (v) => mutate((f) => { f.colors[activeColorKey] = v; }, false),
        onCommit: (v) => mutate((f) => { f.colors[activeColorKey] = v; }, true)
      });
    }
    const editorHost = section.querySelector('[data-evt-color-editor]');
    if (editorHost && evtColorEditor && !editorHost.contains(evtColorEditor.el)) {
      editorHost.appendChild(evtColorEditor.el);
    }

    // Event listeners
    section.addEventListener('click', (e) => {
      const field = selectedEventLog();
      if (!field) return;

      const main = e.target.closest('[data-evt-main]');
      if (main) {
        tabs.main = main.dataset.evtMain;
        applyTabs(section);
        if (tabs.main === 'design' && tabs.sub === 'appearance') drawEvtZones(section, field);
        return;
      }

      const sub = e.target.closest('[data-evt-sub]');
      if (sub) {
        tabs.sub = sub.dataset.evtSub;
        applyTabs(section);
        if (tabs.sub === 'appearance') syncEvtAppearance(section, field);
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

      const ramBtn = e.target.closest('[data-evt-ram]');
      if (ramBtn) {
        const cap = Number(ramBtn.dataset.evtRam) === 128 ? 128 : 64;
        mutate((f) => { f.ramCapacity = cap; });
        return;
      }

      const swatch = e.target.closest('[data-evt-swatch]');
      const legendRow = e.target.closest('[data-evt-legend-row], [data-evt-frame-row]');
      if (swatch || legendRow) {
        const keys = swatch
          ? [swatch.dataset.evtSwatch]
          : (legendRow.dataset.keys || '').split(',').filter(Boolean);
        if (keys.length) {
          activeColorKey = swatch ? keys[0] : (keys.includes(activeColorKey) ? activeColorKey : keys[0]);
          syncEvtAppearance(section, field);
        }
        return;
      }

      const previewCanvas = e.target.closest('[data-evt-preview-canvas]');
      if (previewCanvas) {
        const rect = previewCanvas.getBoundingClientRect();
        const logicalW = Number(previewCanvas.dataset.logicalW) || EVT_PREVIEW_W;
        const logicalH = Number(previewCanvas.dataset.logicalH) || evtPreviewHeight(field);
        if (rect.width > 0 && rect.height > 0) {
          const lx = clamp(Math.floor(((e.clientX - rect.left) / rect.width) * logicalW), 0, logicalW - 1);
          const ly = clamp(Math.floor(((e.clientY - rect.top) / rect.height) * logicalH), 0, logicalH - 1);
          activeColorKey = pickColorKeyAt(field, lx, ly, logicalW, logicalH);
          syncEvtAppearance(section, field);
        }
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
          selectedDescIndex = f.descriptors.length - 1;
        });
        return;
      }

      const delBtn = e.target.closest('[data-evt-del-desc]');
      if (delBtn) {
        const idx = Number(delBtn.dataset.evtDelDesc);
        mutate((f) => {
          if (f.descriptors && f.descriptors[idx]) {
            f.descriptors.splice(idx, 1);
            if (selectedDescIndex >= f.descriptors.length) selectedDescIndex = f.descriptors.length - 1;
          }
        });
        return;
      }

      const descRow = e.target.closest('[data-evt-desc-row]');
      if (descRow) {
        selectedDescIndex = Number(descRow.dataset.evtDescRow);
        renderDescriptorList(section, field);
        renderDescriptorEditor(section, field);
        return;
      }

      const sevChip = e.target.closest('[data-evt-desc-sev]');
      if (sevChip) {
        const val = sevChip.dataset.evtDescSev;
        mutate((f) => {
          if (f.descriptors && f.descriptors[selectedDescIndex]) f.descriptors[selectedDescIndex].severity = val;
        });
        return;
      }

      const catChip = e.target.closest('[data-evt-desc-cat]');
      if (catChip) {
        const val = catChip.dataset.evtDescCat;
        mutate((f) => {
          if (f.descriptors && f.descriptors[selectedDescIndex]) f.descriptors[selectedDescIndex].category = val;
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

    function applyDescInput(t, commit) {
      const key = t.dataset.evtDescProp;
      if (!key) return;
      mutate((f) => {
        const d = f.descriptors && f.descriptors[selectedDescIndex];
        if (!d) return;
        if (key === 'code') d.code = clamp(Math.trunc(Number(t.value) || 0), 0, 65535);
        else d[key] = t.value;
      }, commit);
    }

    section.addEventListener('input', (e) => {
      const t = e.target;
      if (t.matches('[data-evt-prop]')) applyPropInput(t, false);
      else if (t.matches('[data-evt-desc-prop]')) applyDescInput(t, false);
    });

    section.addEventListener('change', (e) => {
      const t = e.target;
      if (t.matches('[data-evt-prop]')) applyPropInput(t, true);
      else if (t.matches('[data-evt-desc-prop]')) applyDescInput(t, true);
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
    if (field.headerFont !== 'GLCD' || field.rowFont !== 'GLCD') {
      field.headerFont = 'GLCD';
      field.rowFont = 'GLCD';
      editor()?.render?.();
    }
    applyTabs(section);

    // Título y Nombre
    const titleIn = section.querySelector('[data-evt-prop="title"]');
    if (titleIn && document.activeElement !== titleIn) titleIn.value = field.title || 'Historial de Eventos';
    const nameIn = section.querySelector('[data-evt-prop="name"]');
    if (nameIn && document.activeElement !== nameIn) nameIn.value = field.name || '';
    const idIn = section.querySelector('[data-evt-prop="id"]');
    if (idIn && document.activeElement !== idIn) idIn.value = field.id || '';

    // Botones booleanos (marco, FRAM, MicroSD)
    section.querySelectorAll('[data-evt-set]').forEach((b) => {
      const prop = b.dataset.evtSet;
      const val = Number(b.dataset.value) === 1;
      b.classList.toggle('active', Boolean(field[prop]) === val);
    });

    // Ubicación
    ['x', 'y', 'width', 'height'].forEach((k) => {
      const input = section.querySelector(`[data-evt-prop="${k}"]`);
      if (input && document.activeElement !== input) input.value = String(field[k]);
    });

    // Readouts
    const ramCap = Number(field.ramCapacity) === 128 ? 128 : 64;
    const { rows } = layout(field);
    section.querySelectorAll('[data-evt-readout="rows"], [data-evt-readout="rows2"]').forEach((el) => el.textContent = `${rows} filas`);
    const wEl = section.querySelector('[data-evt-readout="width"]');
    if (wEl) wEl.textContent = `${field.width} px`;
    const hEl = section.querySelector('[data-evt-readout="height"]');
    if (hEl) hEl.textContent = `${field.height} px`;
    const ramEl = section.querySelector('[data-evt-readout="ram"]');
    if (ramEl) ramEl.textContent = `${ramCap} evts`;
    const ramHintEl = section.querySelector('[data-evt-readout="ramStorageHint"]');
    if (ramHintEl) ramHintEl.textContent = `${ramCap} evts · ~${ramCap === 128 ? 2 : 1} KB`;
    const descCount = field.descriptors?.length || 0;
    const descCountEl = section.querySelector('[data-evt-readout="descCount"]');
    if (descCountEl) descCountEl.textContent = `${descCount}`;
    const descListCountEl = section.querySelector('[data-evt-readout="descListCount"]');
    if (descListCountEl) descListCountEl.textContent = `(${descCount})`;
    const ramBytesEl = section.querySelector('[data-evt-readout="ramBytes"]');
    if (ramBytesEl) ramBytesEl.textContent = `${ramCap * 16} B`;

    // Páginas chips
    const pagesHost = section.querySelector('[data-evt-pages]');
    if (pagesHost) {
      const pages = editor()?.getPages?.() || [];
      pagesHost.innerHTML = pages.map((p) =>
        `<button type="button" class="insp-chip${Number(p.id) === Number(field.page || 0) ? ' active' : ''}" data-evt-page="${p.id}">${String(Number(p.id) + 1).padStart(2, '0')} · ${escapeHtml(p.name)}</button>`
      ).join('');
    }

    // Apariencia: vista previa + swatches + indicador de zona + editor fijo
    syncEvtAppearance(section, field);

    // Almacenamiento
    section.querySelectorAll('[data-evt-ram]').forEach((b) => {
      b.classList.toggle('active', Number(b.dataset.evtRam) === ramCap);
    });
    const sdFileWrap = section.querySelector('[data-evt-sd-file]');
    if (sdFileWrap) sdFileWrap.hidden = !field.enableSd;
    const sdNameInput = section.querySelector('[data-evt-prop="sdFileName"]');
    if (sdNameInput && document.activeElement !== sdNameInput) sdNameInput.value = field.sdFileName || '/EVENTOS.CSV';

    // Descriptores (lista + editor de detalle)
    renderDescriptorList(section, field);
    renderDescriptorEditor(section, field);

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
