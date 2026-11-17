(() => {
  'use strict';

  // Módulo I5-I6: Editor, previsualizador pixel-accurate y generador C++ para
  // ventanas emergentes modales (JWPLC_UIPopup).
  // Trabaja tanto en navegador (window.JWPLCHMIPopups + UI) como en node/vm sin DOM.

  const MAX_POPUPS = 12;
  const SCREEN_W = 320;
  const SCREEN_H = 170;
  const HEADER_H = 22;
  const CHAR_W = 6;
  const CHAR_H = 8;
  const LINE_H = 11;

  const PRESETS = Object.freeze(['INFO', 'WARN', 'CRIT', 'CONFIRM']);
  const PRESET_META = Object.freeze({
    INFO: { label: 'Informativo', icon: 'ℹ', dotClass: 'is-info', builder: 'JWPLC_UIPopupInfo' },
    WARN: { label: 'Advertencia', icon: '⚠', dotClass: 'is-warn', builder: 'JWPLC_UIPopupWarn' },
    CRIT: { label: 'Error crítico', icon: '🛑', dotClass: 'is-crit', builder: 'JWPLC_UIPopupCrit' },
    CONFIRM: { label: 'Confirmación', icon: '❓', dotClass: 'is-confirm', builder: 'JWPLC_UIPopupConfirm' }
  });

  const PRESET_COLORS = Object.freeze({
    INFO: Object.freeze({
      background: 0x1082, border: 0x2D7F, headerBg: 0x1B9C, headerText: 0xFFFF,
      text: 0xFFFF, acceptBg: 0x1B9C, acceptText: 0xFFFF, cancelBg: 0x3186, cancelText: 0xFFFF
    }),
    WARN: Object.freeze({
      background: 0x1082, border: 0xFD20, headerBg: 0xFD20, headerText: 0x0000,
      text: 0xFFFF, acceptBg: 0xFD20, acceptText: 0x0000, cancelBg: 0x3186, cancelText: 0xFFFF
    }),
    CRIT: Object.freeze({
      background: 0x1082, border: 0xF945, headerBg: 0xE8E4, headerText: 0xFFFF,
      text: 0xFFFF, acceptBg: 0xE8E4, acceptText: 0xFFFF, cancelBg: 0x3186, cancelText: 0xFFFF
    }),
    CONFIRM: Object.freeze({
      background: 0x1082, border: 0x07FF, headerBg: 0x055A, headerText: 0xFFFF,
      text: 0xFFFF, acceptBg: 0x0568, acceptText: 0xFFFF, cancelBg: 0x3186, cancelText: 0xFFFF
    })
  });

  const COLOR_LABELS = [
    ['headerBg', 'Cabecera · fondo'],
    ['headerText', 'Cabecera · texto'],
    ['background', 'Fondo del cuadro'],
    ['text', 'Cuerpo · texto'],
    ['border', 'Borde exterior'],
    ['acceptBg', 'Botón Aceptar / OK'],
    ['acceptText', 'Texto Aceptar / OK'],
    ['cancelBg', 'Botón Cancelar'],
    ['cancelText', 'Texto Cancelar']
  ];

  const DEFAULT_RECT = Object.freeze({ x: 36, y: 27, width: 248, height: 116, cornerRadius: 6 });

  const clamp = (v, lo, hi) => Math.min(hi, Math.max(lo, v));
  const num = (v, fallback = 0) => (Number.isFinite(Number(v)) ? Number(v) : fallback);

  function sanitizeSymbol(value, fallback) {
    const cleaned = String(value || '').trim()
      .normalize('NFD').replace(/[\u0300-\u036f]/g, '')
      .replace(/[^A-Za-z0-9_]/g, '_');
    if (!cleaned) return fallback;
    return /^[A-Za-z_]/.test(cleaned) ? cleaned : `_${cleaned}`;
  }

  function hex565(value, fallback = 0xFFFF) {
    const v = Number.isFinite(Number(value)) ? (Number(value) & 0xFFFF) : fallback;
    return `0x${v.toString(16).toUpperCase().padStart(4, '0')}`;
  }

  function rgb565ToCss(value) {
    const c = Number(value) & 0xFFFF;
    const r = Math.round((((c >> 11) & 0x1F) * 255) / 31);
    const g = Math.round((((c >> 5) & 0x3F) * 255) / 63);
    const b = Math.round(((c & 0x1F) * 255) / 31);
    return `rgb(${r}, ${g}, ${b})`;
  }

  function isLight565(value) {
    const c = Number(value) & 0xFFFF;
    return (((c >> 11) + ((c >> 5) & 0x3F) / 2 + (c & 0x1F)) > 40);
  }

  function cppString(value) {
    if (typeof window !== 'undefined' && window.JWPLCCp437?.cppEscape) {
      return `"${window.JWPLCCp437.cppEscape(value ?? '')}"`;
    }
    return `"${String(value ?? '').replace(/\\/g, '\\\\').replace(/"/g, '\\"').replace(/\n/g, '\\n')}"`;
  }

  function cppPlainString(value) {
    return `"${String(value ?? '').replace(/\\/g, '\\\\').replace(/"/g, '\\"')}"`;
  }

  function escapeHtml(text) {
    return String(text ?? '')
      .replace(/&/g, '&amp;')
      .replace(/</g, '&lt;')
      .replace(/>/g, '&gt;')
      .replace(/"/g, '&quot;');
  }

  function presetDefaults(preset = 'INFO', id = 0) {
    const key = PRESETS.includes(String(preset).toUpperCase()) ? String(preset).toUpperCase() : 'INFO';
    const base = {
      id: Math.trunc(Number(id) || 0),
      name: `Popup_${Math.trunc(Number(id) || 0)}`,
      preset: key,
      title: 'AVISO',
      code: '',
      message: 'Operación completada.',
      acceptText: key === 'CONFIRM' ? 'ACEPTAR' : 'OK',
      cancelText: 'CANCELAR',
      triggerVar: '',
      confirmOutVar: '',
      errCode: '',
      autoCloseMs: 0,
      showOverIdle: true,
      blocksIdle: key === 'CRIT' || key === 'CONFIRM',
      latched: key === 'CRIT' || key === 'CONFIRM',
      logToEventLog: key === 'CRIT',
      targetPage: -1,
      targetView: -1,
      ...DEFAULT_RECT,
      colors: { ...PRESET_COLORS[key] }
    };
    if (key === 'WARN') {
      base.title = 'PRECAUCION';
      base.code = 'W01';
      base.message = 'Revisar parámetro fuera de rango.';
    } else if (key === 'CRIT') {
      base.title = 'ERROR CRITICO';
      base.code = 'E104';
      base.errCode = 'E104';
      base.message = 'Falla detectada en proceso.';
    } else if (key === 'CONFIRM') {
      base.title = 'CONFIRMAR';
      base.message = '¿Confirmar acción?';
    }
    return base;
  }

  function normalizePopup(raw, fallbackId = 0) {
    const src = (raw && typeof raw === 'object') ? raw : {};
    const preset = PRESETS.includes(String(src.preset || 'INFO').toUpperCase())
      ? String(src.preset || 'INFO').toUpperCase()
      : 'INFO';
    const dflt = presetDefaults(preset, src.id !== undefined ? src.id : fallbackId);
    const merged = {
      ...dflt,
      ...src,
      preset,
      id: clamp(Math.trunc(num(src.id, dflt.id)), 0, 254),
      autoCloseMs: clamp(Math.trunc(num(src.autoCloseMs, 0)), 0, 60000),
      showOverIdle: src.showOverIdle !== undefined ? Boolean(src.showOverIdle) : dflt.showOverIdle,
      blocksIdle: src.blocksIdle !== undefined ? Boolean(src.blocksIdle) : dflt.blocksIdle,
      latched: src.latched !== undefined ? Boolean(src.latched) : dflt.latched,
      logToEventLog: src.logToEventLog !== undefined ? Boolean(src.logToEventLog) : dflt.logToEventLog,
      targetPage: src.targetPage !== undefined ? clamp(Math.trunc(num(src.targetPage, -1)), -1, 15) : -1,
      targetView: src.targetView !== undefined ? clamp(Math.trunc(num(src.targetView, -1)), -1, 7) : -1,
      width: clamp(Math.trunc(num(src.width, DEFAULT_RECT.width)), 120, SCREEN_W - 8),
      height: clamp(Math.trunc(num(src.height, DEFAULT_RECT.height)), 72, SCREEN_H - 8),
      cornerRadius: clamp(Math.trunc(num(src.cornerRadius, DEFAULT_RECT.cornerRadius)), 0, 12)
    };
    merged.x = clamp(Math.trunc(num(src.x, Math.round((SCREEN_W - merged.width) / 2))), 0, SCREEN_W - merged.width);
    merged.y = clamp(Math.trunc(num(src.y, Math.round((SCREEN_H - merged.height) / 2))), 0, SCREEN_H - merged.height);
    merged.name = String(src.name || `Popup_${merged.id}`).slice(0, 32);
    merged.title = String(src.title ?? dflt.title).slice(0, 35);
    merged.code = String(src.code ?? '').slice(0, 8);
    merged.errCode = String(src.errCode ?? '').toUpperCase().slice(0, 4);
    merged.message = String(src.message ?? dflt.message).slice(0, 127);
    merged.acceptText = String(src.acceptText ?? dflt.acceptText).slice(0, 14) || (preset === 'CONFIRM' ? 'ACEPTAR' : 'OK');
    merged.cancelText = String(src.cancelText ?? dflt.cancelText).slice(0, 14) || 'CANCELAR';
    merged.triggerVar = src.triggerVar ? sanitizeSymbol(src.triggerVar, '') : '';
    merged.confirmOutVar = src.confirmOutVar ? sanitizeSymbol(src.confirmOutVar, '') : '';
    merged.colors = { ...PRESET_COLORS[preset], ...(src.colors && typeof src.colors === 'object' ? src.colors : {}) };
    return merged;
  }

  // ---------------------------------------------------------------------------
  // Pintado 320x170 (paridad exacta con JWPLC_UI_Popup.cpp::renderPopup)
  // ---------------------------------------------------------------------------

  function drawRoundRectPath(ctx, x, y, w, h, r) {
    const rr = Math.min(r, Math.floor(w / 2), Math.floor(h / 2));
    ctx.beginPath();
    if (rr <= 0) {
      ctx.rect(x, y, w, h);
      return;
    }
    ctx.moveTo(x + rr, y);
    ctx.lineTo(x + w - rr, y);
    ctx.quadraticCurveTo(x + w, y, x + w, y + rr);
    ctx.lineTo(x + w, y + h - rr);
    ctx.quadraticCurveTo(x + w, y + h, x + w - rr, y + h);
    ctx.lineTo(x + rr, y + h);
    ctx.quadraticCurveTo(x, y + h, x, y + h - rr);
    ctx.lineTo(x, y + rr);
    ctx.quadraticCurveTo(x, y, x + rr, y);
    ctx.closePath();
  }

  function fillRound(ctx, x, y, w, h, r, css) {
    ctx.fillStyle = css;
    drawRoundRectPath(ctx, x, y, w, h, r);
    ctx.fill();
  }

  function strokeRound(ctx, x, y, w, h, r, css) {
    ctx.strokeStyle = css;
    ctx.lineWidth = 1;
    drawRoundRectPath(ctx, x + 0.5, y + 0.5, w - 1, h - 1, r);
    ctx.stroke();
  }

  function drawGlcdText(ctx, text, x, y, css) {
    const font = typeof window !== 'undefined' ? window.JWPLCGfxClassicFont : null;
    if (!font || !text) {
      if (text) {
        ctx.fillStyle = css;
        ctx.font = '8px monospace';
        ctx.textBaseline = 'top';
        ctx.fillText(String(text), x, y);
      }
      return;
    }
    ctx.fillStyle = css;
    let cursorX = Math.round(x);
    const baseY = Math.round(y);
    for (const ch of String(text)) {
      const glyph = font.glyphFor(ch.codePointAt(0));
      for (let col = 0; col < font.cellWidth; col += 1) {
        const bits = col < font.bytesPerGlyph ? glyph[col] : 0;
        for (let row = 0; row < font.cellHeight; row += 1) {
          if ((bits >> row) & 1) ctx.fillRect(cursorX + col, baseY + row, 1, 1);
        }
      }
      cursorX += font.cellWidth;
    }
  }

  function wrapLines(message, maxChars, maxLines) {
    const rawLines = String(message || '').split('\n');
    const out = [];
    for (const para of rawLines) {
      if (out.length >= maxLines) break;
      const trimmed = para.trimStart();
      if (!trimmed) {
        out.push('');
        continue;
      }
      let p = trimmed;
      while (p.length > 0 && out.length < maxLines) {
        if (p.length <= maxChars) {
          out.push(p);
          break;
        }
        let take = p.lastIndexOf(' ', maxChars);
        if (take <= 0) take = maxChars;
        out.push(p.slice(0, take));
        p = p.slice(take).trimStart();
      }
    }
    return out;
  }

  function drawHeaderIcon(ctx, preset, ix, iy, fgCss, bgCss) {
    ctx.strokeStyle = fgCss;
    ctx.lineWidth = 1;
    if (preset === 'INFO') {
      ctx.strokeRect(ix + 1.5, iy + 1.5, 11, 11);
      ctx.fillStyle = fgCss;
      ctx.fillRect(ix + 6, iy + 3, 2, 2);
      ctx.fillRect(ix + 6, iy + 6, 2, 5);
    } else if (preset === 'CONFIRM') {
      ctx.strokeRect(ix + 1.5, iy + 1.5, 11, 11);
      ctx.fillStyle = fgCss;
      ctx.fillRect(ix + 5, iy + 3, 4, 2);
      ctx.fillRect(ix + 7, iy + 5, 2, 2);
      ctx.fillRect(ix + 6, iy + 6, 2, 2);
      ctx.fillRect(ix + 6, iy + 9, 2, 2);
    } else {
      ctx.fillStyle = bgCss;
      ctx.fillRect(ix + 1, iy + 1, 12, 12);
      ctx.strokeRect(ix + 1.5, iy + 1.5, 11, 11);
      ctx.fillStyle = fgCss;
      ctx.fillRect(ix + 6, iy + 3, 2, 5);
      ctx.fillRect(ix + 6, iy + 9, 2, 2);
    }
  }

  function drawButton(ctx, bx, by, bw, bh, label, fillCss, textCss, focused) {
    fillRound(ctx, bx, by, bw, bh, 4, fillCss);
    if (focused) {
      strokeRound(ctx, bx, by, bw, bh, 4, '#ffffff');
      strokeRound(ctx, bx + 1, by + 1, bw - 2, bh - 2, 3, '#ffffff');
    } else {
      strokeRound(ctx, bx, by, bw, bh, 4, textCss);
    }
    const text = String(label || '');
    const tw = text.length * CHAR_W;
    drawGlcdText(ctx, text, Math.round(bx + (bw - tw) / 2), Math.round(by + (bh - CHAR_H) / 2), textCss);
  }

  function paintPopup(ctx, rawPopup, focusIndex = 0) {
    if (!ctx || !rawPopup) return;
    const p = normalizePopup(rawPopup);
    const c = p.colors;
    const { x, y, width: w, height: h, cornerRadius: r } = p;

    const bgCss = rgb565ToCss(c.background);
    const borderCss = rgb565ToCss(c.border);
    const hdrBgCss = rgb565ToCss(c.headerBg);
    const hdrTxtCss = rgb565ToCss(c.headerText);
    const txtCss = rgb565ToCss(c.text);
    const acceptBgCss = rgb565ToCss(c.acceptBg);
    const acceptTxtCss = rgb565ToCss(c.acceptText);
    const cancelBgCss = rgb565ToCss(c.cancelBg);
    const cancelTxtCss = rgb565ToCss(c.cancelText);

    strokeRound(ctx, x - 1, y - 1, w + 2, h + 2, r > 0 ? r + 1 : 0, '#000000');
    fillRound(ctx, x, y, w, h, r, bgCss);
    strokeRound(ctx, x, y, w, h, r, borderCss);
    strokeRound(ctx, x + 1, y + 1, w - 2, h - 2, r > 1 ? r - 1 : 0, borderCss);

    ctx.fillStyle = hdrBgCss;
    ctx.fillRect(x + 2, y + 2, w - 4, HEADER_H);
    drawHeaderIcon(ctx, p.preset, x + 6, y + 6, hdrTxtCss, hdrBgCss);

    const codeStr = p.preset === 'CONFIRM' ? '' : String(p.code || '').slice(0, 8);
    const codeW = codeStr.length * CHAR_W;
    if (codeW > 0) {
      drawGlcdText(ctx, codeStr, x + w - 8 - codeW, y + 9, hdrTxtCss);
    }

    const titleSpace = w - 30 - (codeW > 0 ? codeW + 10 : 8);
    const maxTitleChars = Math.max(1, Math.floor(titleSpace / CHAR_W));
    const titleStr = String(p.title || 'AVISO').slice(0, maxTitleChars);
    drawGlcdText(ctx, titleStr, x + 24, y + 9, hdrTxtCss);

    const bodyX = x + 10;
    const bodyY = y + 2 + HEADER_H + 4;
    const bodyW = w - 20;
    const bodyH = h - HEADER_H - 34;
    const maxChars = Math.max(4, Math.floor(bodyW / CHAR_W));
    const maxLines = Math.max(1, Math.min(6, Math.floor(bodyH / LINE_H)));
    const lines = wrapLines(p.message, maxChars, maxLines);
    const totalH = lines.length * LINE_H;
    const startY = totalH < bodyH ? Math.floor(bodyY + (bodyH - totalH) / 2 + 1) : bodyY;
    lines.forEach((line, idx) => {
      const lw = line.length * CHAR_W;
      const lx = Math.max(bodyX, Math.floor(bodyX + (bodyW - lw) / 2));
      drawGlcdText(ctx, line, lx, startY + idx * LINE_H, txtCss);
    });

    const btnY = y + h - 25;
    const btnH = 18;
    if (p.preset === 'CONFIRM') {
      const btnW = 94;
      const gap = 12;
      const startX = Math.floor(x + (w - (btnW * 2 + gap)) / 2);
      drawButton(ctx, startX, btnY, btnW, btnH, p.cancelText, cancelBgCss, cancelTxtCss, focusIndex === 0);
      drawButton(ctx, startX + btnW + gap, btnY, btnW, btnH, p.acceptText, acceptBgCss, acceptTxtCss, focusIndex === 1);
    } else {
      const btnW = 86;
      const bx = Math.floor(x + (w - btnW) / 2);
      drawButton(ctx, bx, btnY, btnW, btnH, p.acceptText, acceptBgCss, acceptTxtCss, true);
    }
  }

  // ---------------------------------------------------------------------------
  // Validación y Codegen C++
  // ---------------------------------------------------------------------------

  function validate(popups = [], fields = []) {
    const errors = [];
    const warnings = [];
    if (popups.length > MAX_POPUPS) errors.push(`Hay ${popups.length} popups; máximo ${MAX_POPUPS}.`);
    const idsSeen = new Map();
    const nonBoolVars = new Set();
    fields.forEach((f) => {
      if (!f || !f.variable) return;
      if (f.type === 'BOOL' || (f.type === 'ICON' && f.iconVarType !== 'uint8')) return;
      if (['VALUE', 'BAR', 'TREND', 'TEXT'].includes(f.type) || (f.type === 'ICON' && f.iconVarType === 'uint8')) {
        nonBoolVars.add(sanitizeSymbol(f.variable, ''));
      }
      if (f.type === 'TREND') {
        if (f.ch2Enabled && f.ch2Variable) nonBoolVars.add(sanitizeSymbol(f.ch2Variable, ''));
        if (f.ch3Enabled && f.ch3Variable) nonBoolVars.add(sanitizeSymbol(f.ch3Variable, ''));
      }
    });

    const noGlyphWarning = (where, text) => {
      const missing = (typeof window !== 'undefined' && window.JWPLCCp437?.unsupported?.(text)) || [];
      if (missing.length) warnings.push(`${where}: «${missing.join(' ')}» no existe en GLCD (se ve sin tilde o como ?).`);
    };

    popups.forEach((raw, idx) => {
      const p = normalizePopup(raw, idx);
      const where = `Popup #${p.id} (${p.name})`;
      if (idsSeen.has(p.id)) errors.push(`${where}: ID numérico ${p.id} duplicado con ${idsSeen.get(p.id)}.`);
      idsSeen.set(p.id, p.name);
      if (p.triggerVar && nonBoolVars.has(p.triggerVar)) {
        errors.push(`${where}: triggerVar «${p.triggerVar}» ya existe en un campo no booleano.`);
      }
      if (p.confirmOutVar && nonBoolVars.has(p.confirmOutVar)) {
        errors.push(`${where}: confirmOutVar «${p.confirmOutVar}» ya existe en un campo no booleano.`);
      }
      if (p.triggerVar && p.confirmOutVar && p.triggerVar === p.confirmOutVar) {
        errors.push(`${where}: triggerVar y confirmOutVar no pueden usar la misma variable.`);
      }
      noGlyphWarning(where, `${p.title}${p.message}${p.acceptText}${p.cancelText}`);
    });

    return { errors, warnings };
  }

  function popupEnumSymbol(p) {
    const base = sanitizeSymbol(p.name || `POPUP_${p.id}`, `POPUP_${p.id}`).toUpperCase();
    return base.startsWith('POPUP_') ? base : `POPUP_${base}`;
  }

  function colorsDiffer(c, dflt) {
    return Object.keys(dflt).some((k) => (Number(c[k]) & 0xFFFF) !== (Number(dflt[k]) & 0xFFFF));
  }

  function generate(popups = [], context = {}) {
    if (!popups.length) return { errors: [], warnings: [], variables: '', declarations: '', setup: '', enumSymbols: [] };
    const normalized = popups.map((p, i) => normalizePopup(p, i));
    const fields = context.fields || [];
    const pageSymbol = context.pageSymbol || ((id) => String(id));
    const { errors, warnings } = validate(normalized, fields);

    const declaredVars = new Set(Array.isArray(context.existingVars) ? context.existingVars : []);
    fields.forEach((f) => {
      if (!f) return;
      if (f.variable && !['LINE', 'RECT', 'ELLIPSE', 'TRIANGLE', 'POLYGON', 'RAW_TEXT'].includes(f.type)) {
        declaredVars.add(sanitizeSymbol(f.variable, ''));
      }
      if (f.type === 'TREND') {
        if (f.ch2Enabled && f.ch2Variable) declaredVars.add(sanitizeSymbol(f.ch2Variable, ''));
        if (f.ch3Enabled && f.ch3Variable) declaredVars.add(sanitizeSymbol(f.ch3Variable, ''));
      }
    });

    const vars = [];
    normalized.forEach((p) => {
      [p.triggerVar, p.confirmOutVar].forEach((sym) => {
        if (!sym || declaredVars.has(sym)) return;
        declaredVars.add(sym);
        vars.push(`bool ${sym} = false;`);
      });
    });

    const guard =
      `// Ventanas emergentes (I5-I6): requiere JWPLC_Display con soporte de Popups.\n` +
      `#ifndef JWPLC_UI_HAS_POPUP\n` +
      `#error "Las ventanas emergentes requieren JWPLC_Display con soporte I5-I6."\n` +
      `#endif\n` +
      errors.map((e) => `#error ${cppPlainString(`POPUP: ${e}`)}\n`).join('');

    const seenSyms = new Set();
    const enumEntries = normalized.map((p) => {
      let sym = popupEnumSymbol(p);
      while (seenSyms.has(sym)) sym = `${sym}_${p.id}`;
      seenSyms.add(sym);
      return { id: p.id, sym };
    });

    const enumDecl = `enum HMIPopupId : uint8_t\n{\n${enumEntries.map((e) => `    ${e.sym} = ${e.id}`).join(',\n')}\n};`;

    const popupItems = normalized.map((p, idx) => {
      const sym = enumEntries[idx].sym;
      const codeArg = p.code ? cppString(p.code) : 'nullptr';
      let expr;
      if (p.preset === 'WARN') {
        expr = `JWPLC_UIPopupWarn(${sym}, ${cppString(p.title)}, ${cppString(p.message)}, ${codeArg})`;
      } else if (p.preset === 'CRIT') {
        const errArg = p.errCode ? cppString(p.errCode) : 'nullptr';
        expr = `JWPLC_UIPopupCrit(${sym}, ${cppString(p.title)}, ${cppString(p.message)}, ${codeArg}, ${errArg})`;
      } else if (p.preset === 'CONFIRM') {
        const outPtr = p.confirmOutVar ? `&${p.confirmOutVar}` : 'nullptr';
        expr = `JWPLC_UIPopupConfirm(${sym}, ${cppString(p.title)}, ${cppString(p.message)}, ${outPtr}, ${cppString(p.cancelText)}, ${cppString(p.acceptText)})`;
      } else {
        expr = `JWPLC_UIPopupInfo(${sym}, ${cppString(p.title)}, ${cppString(p.message)}, ${codeArg})`;
      }

      const dflt = presetDefaults(p.preset, p.id);
      if (p.triggerVar || p.latched !== dflt.latched || p.showOverIdle !== dflt.showOverIdle) {
        const trigPtr = p.triggerVar ? `&${p.triggerVar}` : 'nullptr';
        expr = `JWPLC_UIPopupTrigger(\n        ${expr},\n        ${trigPtr}, ${p.latched ? 'true' : 'false'}, ${p.showOverIdle ? 'true' : 'false'})`;
      }
      if (p.autoCloseMs > 0 || p.logToEventLog !== dflt.logToEventLog || p.blocksIdle !== dflt.blocksIdle) {
        expr = `JWPLC_UIPopupOptions(\n        ${expr},\n        ${p.autoCloseMs}, ${p.logToEventLog ? 'true' : 'false'}, ${p.blocksIdle ? 'true' : 'false'})`;
      }
      if (p.preset !== 'CONFIRM' && p.acceptText !== 'OK') {
        expr = `JWPLC_UIPopupButtons(${expr}, ${cppString(p.acceptText)})`;
      }
      if (p.preset === 'CONFIRM' && (p.targetPage >= 0 || p.targetView >= 0)) {
        const pageArg = p.targetPage >= 0 ? pageSymbol(p.targetPage) : '0xFFU';
        const viewArg = p.targetView >= 0 ? String(p.targetView) : '0xFFU';
        expr = `JWPLC_UIPopupNavTarget(\n        ${expr},\n        ${pageArg}, ${viewArg})`;
      }
      if (p.x !== DEFAULT_RECT.x || p.y !== DEFAULT_RECT.y || p.width !== DEFAULT_RECT.width || p.height !== DEFAULT_RECT.height || p.cornerRadius !== DEFAULT_RECT.cornerRadius) {
        expr = `JWPLC_UIPopupRect(\n        ${expr},\n        ${p.x}, ${p.y}, ${p.width}, ${p.height}, ${p.cornerRadius})`;
      }
      if (colorsDiffer(p.colors, PRESET_COLORS[p.preset])) {
        const cols = [
          p.colors.background, p.colors.border, p.colors.headerBg, p.colors.headerText,
          p.colors.text, p.colors.acceptBg, p.colors.acceptText, p.colors.cancelBg, p.colors.cancelText
        ].map((v) => hex565(v)).join(', ');
        expr = `JWPLC_UIPopupColorsOverride(\n        ${expr},\n        JWPLC_UIPopupColors(${cols}))`;
      }
      return `    ${expr.replace(/\n/g, '\n    ')}`;
    });

    const decls = `${guard}\n${enumDecl}\n\nstatic const JWPLC_UIPopup HMI_POPUPS[] =\n{\n${popupItems.join(',\n')}\n};`;
    const setup = `    JWPLC_Display.setPopups(\n        HMI_POPUPS,\n        sizeof(HMI_POPUPS) / sizeof(HMI_POPUPS[0]));\n`;

    return {
      errors,
      warnings,
      variables: vars.length ? `// Ventanas emergentes (I5-I6)\n${vars.join('\n')}` : '',
      declarations: decls,
      setup,
      enumSymbols: enumEntries
    };
  }

  // ---------------------------------------------------------------------------
  // Estado vivo en el Designer
  // ---------------------------------------------------------------------------

  let state = [];
  let selectedId = null;
  let openModeState = false;
  let previewFocusIndex = 0; // 0 = CANCELAR, 1 = ACEPTAR
  let activeColorKey = 'headerBg';
  let tabs = { main: 'design', sub: 'content' };

  function nextFreeId() {
    const used = new Set(state.map((p) => Number(p.id)));
    let candidate = 0;
    while (used.has(candidate) && candidate < 255) candidate += 1;
    return candidate < 255 ? candidate : 0;
  }

  function findSelected() {
    if (!state.length) return null;
    const found = state.find((p) => Number(p.id) === Number(selectedId));
    return found || state[0];
  }

  function editor() {
    return typeof window !== 'undefined' ? (window.JWPLCHMIEditor || null) : null;
  }

  function extractDeclaredVarsFromHeader(text) {
    const out = new Set();
    const m = String(text || '').match(/\/\/ Variables HMI\n([\s\S]*?)\n\n\/\/ Definición declarativa/);
    if (!m) return out;
    const lines = m[1].split('\n');
    lines.forEach((line) => {
      const mm = line.trim().match(/^(?:float|bool|uint8_t|int32_t|uint32_t|char)\s+([A-Za-z_][A-Za-z0-9_]*)/);
      if (mm) out.add(mm[1]);
    });
    return out;
  }

  function patchHeader(text) {
    if (!state.length || !String(text || '').includes('void jwplcHMISetup()')) return text;
    const symbols = new Map((window.JWPLCHMICodegen?.getPageSymbols?.() || []).map((p) => [p.id, p.symbol]));
    const existingVars = [...extractDeclaredVarsFromHeader(text)];
    const out = generate(state, {
      fields: editor()?.getAllFields?.() || [],
      existingVars,
      pageSymbol: (id) => symbols.get(id) || String(id)
    });

    if (out.variables && !text.includes(out.variables)) {
      text = text.replace(/(\/\/ Variables HMI\n[\s\S]*?)(\n\n\/\/ Definición declarativa)/, (all, head, tail) => {
        const cleanHead = head.trimEnd();
        return `${cleanHead}\n${cleanHead.endsWith('// Variables HMI') ? '' : '\n'}${out.variables}${tail}`;
      });
    }
    if (!text.includes('static const JWPLC_UIPopup HMI_POPUPS[]')) {
      text = text.replace('void jwplcHMISetup()\n{\n', `${out.declarations}\n\nvoid jwplcHMISetup()\n{\n`);
    }
    if (!text.includes('JWPLC_Display.setPopups(')) {
      text = text.replace('void jwplcHMISetup()\n{\n', `void jwplcHMISetup()\n{\n${out.setup}`);
    }
    return text;
  }

  function setPopups(list) {
    state = Array.isArray(list) ? list.slice(0, MAX_POPUPS).map((p, i) => normalizePopup(p, i)) : [];
    selectedId = state[0]?.id ?? null;
    syncAllUI();
    editor()?.render?.();
  }

  function openMode(id = null) {
    openModeState = true;
    if (id !== null && state.some((p) => Number(p.id) === Number(id))) selectedId = Number(id);
    else if (!findSelected() && state.length) selectedId = state[0].id;
    syncAllUI();
    editor()?.render?.();
    window.JWPLCHMIInspectorState?.sync?.();
    window.dispatchEvent(new CustomEvent('jwplc:editor-refresh'));
  }

  function closeMode() {
    if (!openModeState) return;
    openModeState = false;
    syncAllUI();
    editor()?.render?.();
    window.JWPLCHMIInspectorState?.sync?.();
    window.dispatchEvent(new CustomEvent('jwplc:editor-refresh'));
  }

  function toggleMode() {
    if (openModeState) closeMode();
    else openMode();
  }

  function paintOnLogical(ctx) {
    if (!openModeState) return;
    const p = findSelected();
    ctx.save();
    ctx.fillStyle = 'rgba(4, 9, 14, 0.58)';
    ctx.fillRect(0, 0, SCREEN_W, SCREEN_H);
    if (p) {
      paintPopup(ctx, p, p.preset === 'CONFIRM' ? previewFocusIndex : 0);
    } else {
      drawGlcdText(ctx, 'Sin ventanas: elige un preset a la izquierda', 18, 80, '#d0e2f0');
    }
    ctx.restore();
  }

  // ---------------------------------------------------------------------------
  // DOM: Panel izquierdo + Inspector derecho (solo en navegador)
  // ---------------------------------------------------------------------------

  const hasDom = typeof document !== 'undefined' && Boolean(document.getElementById('textFieldControlsSection'));
  let leftBlock = null;
  let rightSection = null;
  let colorEditor = null;

  function mutateSelected(fn, shouldNotify = true) {
    const p = findSelected();
    if (!p) return;
    fn(p);
    Object.assign(p, normalizePopup(p, p.id));
    syncAllUI();
    editor()?.render?.();
    window.dispatchEvent(new CustomEvent('jwplc:editor-refresh'));
    if (shouldNotify) window.JWPLCHMISaveFlow?.refresh?.();
  }

  function createByPreset(preset) {
    if (state.length >= MAX_POPUPS) {
      window.JWPLCDialog?.alert?.(`Máximo ${MAX_POPUPS} ventanas emergentes en el proyecto.`);
      return null;
    }
    const id = nextFreeId();
    const created = presetDefaults(preset, id);
    state.push(created);
    selectedId = created.id;
    previewFocusIndex = 0;
    if (!openModeState) openMode(created.id);
    else {
      syncAllUI();
      editor()?.render?.();
      window.dispatchEvent(new CustomEvent('jwplc:editor-refresh'));
    }
    return created;
  }

  function removePopup(id) {
    const idx = state.findIndex((p) => Number(p.id) === Number(id));
    if (idx === -1) return;
    state.splice(idx, 1);
    if (Number(selectedId) === Number(id)) selectedId = state[Math.max(0, idx - 1)]?.id ?? null;
    syncAllUI();
    editor()?.render?.();
    window.dispatchEvent(new CustomEvent('jwplc:editor-refresh'));
  }

  function buildLeftPanel() {
    const objectsNav = document.getElementById('objectsNavBlock') || document.getElementById('objectsList')?.closest('.nav-block');
    if (!objectsNav) return;
    leftBlock = document.createElement('section');
    leftBlock.id = 'popupsLeftSection';
    leftBlock.className = 'nav-block popups-left-block';
    leftBlock.hidden = true;
    leftBlock.innerHTML = `
      <div class="nav-header">
        <div class="nav-title">
          <h2>⚠ Ventanas emergentes</h2>
        </div>
        <span class="count-badge" data-popups-count>0/${MAX_POPUPS}</span>
      </div>
      <div class="insp-note" style="margin:0;">Crear rápido desde preset:</div>
      <div class="popups-preset-grid">
        <button type="button" class="popup-preset-btn" data-popup-new="INFO"><span class="popup-preset-dot is-info"></span>+ Info</button>
        <button type="button" class="popup-preset-btn" data-popup-new="WARN"><span class="popup-preset-dot is-warn"></span>+ Aviso</button>
        <button type="button" class="popup-preset-btn" data-popup-new="CRIT"><span class="popup-preset-dot is-crit"></span>+ Crítica</button>
        <button type="button" class="popup-preset-btn" data-popup-new="CONFIRM"><span class="popup-preset-dot is-confirm"></span>+ Confirmar</button>
      </div>
      <div class="popup-list-tree" data-popups-list></div>
      <button type="button" class="bar-visual-btn insp-wide" data-popups-exit>← Volver a página y objetos</button>
    `;
    objectsNav.insertAdjacentElement('afterend', leftBlock);

    leftBlock.addEventListener('click', (e) => {
      const addBtn = e.target.closest('[data-popup-new]');
      if (addBtn) {
        createByPreset(addBtn.dataset.popupNew);
        return;
      }
      if (e.target.closest('[data-popups-exit]')) {
        closeMode();
        return;
      }
      const delBtn = e.target.closest('[data-popup-del]');
      if (delBtn) {
        e.stopPropagation();
        removePopup(Number(delBtn.dataset.popupDel));
        return;
      }
      const row = e.target.closest('[data-popup-select]');
      if (row) {
        selectedId = Number(row.dataset.popupSelect);
        syncAllUI();
        editor()?.render?.();
        window.dispatchEvent(new CustomEvent('jwplc:editor-refresh'));
      }
    });
  }

  function buildRightInspector() {
    const anchor = document.getElementById('textFieldControlsSection');
    if (!anchor) return;
    rightSection = document.createElement('section');
    rightSection.id = 'popupInspectorSection';
    rightSection.className = 'inspector-section inspector-rich';
    rightSection.hidden = true;
    rightSection.innerHTML = `
      <div class="insp-shell">
        <div class="insp-header">
          <div class="insp-header-top">
            <span class="insp-type-badge" data-popup-badge>POPUP</span>
            <button type="button" class="icon-button" data-popups-close-insp title="Volver al lienzo">✕</button>
          </div>
          <div class="insp-tabs-main" role="tablist">
            <button type="button" class="bar-sub-tab insp-tab" data-popup-main="design" role="tab">Diseño</button>
            <button type="button" class="bar-sub-tab insp-tab" data-popup-main="data" role="tab">Datos</button>
            <button type="button" class="bar-sub-tab insp-tab" data-popup-main="tech" role="tab">Técnico</button>
          </div>
          <div class="insp-tabs-sub" data-popup-subtabs role="tablist">
            <button type="button" class="bar-sub-tab insp-sub-tab" data-popup-sub="content" role="tab">Contenido</button>
            <button type="button" class="bar-sub-tab insp-sub-tab" data-popup-sub="appearance" role="tab">Colores y forma</button>
          </div>
        </div>

        <div class="insp-body" data-popup-empty hidden>
          <div class="bar-card">
            <div class="bar-card-section-title">Sin ventanas creadas</div>
            <div class="insp-note">Elige uno de los 4 presets en el panel izquierdo (+ Info, + Aviso, + Crítica o + Confirmar) para diseñar tu ventana modal.</div>
          </div>
        </div>

        <div class="insp-body" data-popup-form>
          <div class="insp-panel" data-popup-panel="design:content">
            <div class="bar-card">
              <div class="bar-card-section-title">Tipo de aviso (preset)</div>
              <div class="insp-chip-row">
                <button type="button" class="insp-chip" data-popup-preset="INFO">ℹ Info</button>
                <button type="button" class="insp-chip" data-popup-preset="WARN">⚠ Aviso</button>
                <button type="button" class="insp-chip" data-popup-preset="CRIT">🛑 Crítica</button>
                <button type="button" class="insp-chip" data-popup-preset="CONFIRM">❓ Confirmar</button>
              </div>
            </div>
            <div class="bar-card">
              <label class="field-label">Título cabecera<input class="field-input" data-popup-prop="title" type="text" maxlength="35" /></label>
              <label class="field-label" data-popup-code-wrap>Código derecha (opcional, ej. E104)<input class="field-input code-input" data-popup-prop="code" type="text" maxlength="8" /></label>
              <label class="field-label">Mensaje (multilínea)<textarea class="field-input popup-textarea" data-popup-prop="message" maxlength="127"></textarea></label>
            </div>
            <div class="bar-card">
              <div class="insp-two">
                <label class="field-label">Botón principal<input class="field-input" data-popup-prop="acceptText" type="text" maxlength="14" /></label>
                <label class="field-label" data-popup-cancel-wrap>Botón cancelar<input class="field-input" data-popup-prop="cancelText" type="text" maxlength="14" /></label>
              </div>
              <div data-popup-confirm-preview hidden>
                <span class="bar-visual-group-title insp-gap">Previsualizar foco de botón</span>
                <div class="bar-visual-grid-2">
                  <button type="button" class="bar-visual-btn" data-popup-focus="0"><strong>[ CANCELAR ]</strong></button>
                  <button type="button" class="bar-visual-btn" data-popup-focus="1"><strong>[ ACEPTAR ]</strong></button>
                </div>
              </div>
            </div>
          </div>

          <div class="insp-panel" data-popup-panel="design:appearance" hidden>
            <div class="bar-card">
              <div class="bar-card-header">
                <span class="bar-card-section-title">Colores del modal</span>
                <button type="button" class="insp-chip" data-popup-reset-colors>Restaurar preset</button>
              </div>
              <div class="insp-chip-row" data-popup-swatch-list></div>
              <div class="insp-gap" data-popup-color-editor></div>
            </div>
            <div class="bar-card">
              <div class="bar-card-header">
                <span class="bar-card-section-title">Geometría (320×170)</span>
                <button type="button" class="insp-chip" data-popup-center>Centrar</button>
              </div>
              <div class="insp-two">
                <label class="field-label">X (px)<input class="field-input" data-popup-prop="x" type="number" min="0" max="200" /></label>
                <label class="field-label">Y (px)<input class="field-input" data-popup-prop="y" type="number" min="0" max="98" /></label>
                <label class="field-label">Ancho (px)<input class="field-input" data-popup-prop="width" type="number" min="120" max="312" /></label>
                <label class="field-label">Alto (px)<input class="field-input" data-popup-prop="height" type="number" min="72" max="162" /></label>
                <label class="field-label">Radio esquinas<input class="field-input" data-popup-prop="cornerRadius" type="number" min="0" max="12" /></label>
              </div>
            </div>
          </div>

          <div class="insp-panel" data-popup-panel="data" hidden>
            <div class="bar-card">
              <div class="bar-card-section-title">Disparo y variables C++ (bool)</div>
              <label class="field-label">Variable trigger automático (0 → 1)<input class="field-input code-input" data-popup-prop="triggerVar" type="text" maxlength="32" placeholder="ej. alarmaSobreTemp" /></label>
              <label class="field-label" data-popup-outvar-wrap hidden>Variable resultado CONFIRM (true/false)<input class="field-input code-input" data-popup-prop="confirmOutVar" type="text" maxlength="32" placeholder="ej. iniciarProceso" /></label>
              <label class="field-label">Código ERR lateral IDLE (4 chars)<input class="field-input code-input" data-popup-prop="errCode" type="text" maxlength="4" placeholder="ej. E104" /></label>
              <label class="field-label">Auto-cierre (ms, 0 = manual)<input class="field-input" data-popup-prop="autoCloseMs" type="number" min="0" max="60000" step="100" /></label>
            </div>
            <div class="bar-card">
              <div class="bar-card-section-title">Comportamiento modal I4/I5</div>
              <button type="button" class="bar-visual-btn insp-toggle insp-wide" data-popup-toggle="showOverIdle"><span>Mostrar sobre pantalla IDLE</span><strong data-popup-bool="showOverIdle">Sí</strong></button>
              <button type="button" class="bar-visual-btn insp-toggle insp-wide insp-gap" data-popup-toggle="blocksIdle"><span>Bloquear retorno a IDLE</span><strong data-popup-bool="blocksIdle">No</strong></button>
              <button type="button" class="bar-visual-btn insp-toggle insp-wide insp-gap" data-popup-toggle="latched"><span>Retener hasta pulsar OK (latched)</span><strong data-popup-bool="latched">No</strong></button>
              <button type="button" class="bar-visual-btn insp-toggle insp-wide insp-gap" data-popup-toggle="logToEventLog"><span>Registrar en Historial EVENT_LOG</span><strong data-popup-bool="logToEventLog">No</strong></button>
            </div>
            <div class="bar-card" data-popup-nav-wrap hidden>
              <div class="bar-card-section-title">Navegación tras aceptar CONFIRM</div>
              <div class="insp-two">
                <label class="field-label">Página destino
                  <select class="field-input" data-popup-prop="targetPage"></select>
                </label>
                <label class="field-label">Subvista destino
                  <select class="field-input" data-popup-prop="targetView"></select>
                </label>
              </div>
            </div>
          </div>

          <div class="insp-panel" data-popup-panel="tech" hidden>
            <div class="bar-card">
              <label class="field-label">Nombre símbolo C++ / Designer<input class="field-input" data-popup-prop="name" type="text" maxlength="32" /></label>
              <label class="field-label">ID numérico (0..254)<input class="field-input" data-popup-prop="id" type="number" min="0" max="254" /></label>
            </div>
            <div class="bar-card">
              <div class="bar-card-header">
                <span class="bar-card-title">Uso desde el sketch</span>
                <button type="button" class="icon-button" data-popup-copy title="Copiar">⧉</button>
              </div>
              <pre class="insp-contract" data-popup-contract></pre>
            </div>
            <div class="bar-card">
              <div class="bar-card-section-title">Diagnóstico</div>
              <div class="menu-issues" data-popup-issues></div>
            </div>
          </div>
        </div>
      </div>
    `;
    anchor.insertAdjacentElement('beforebegin', rightSection);

    const colorHost = rightSection.querySelector('[data-popup-color-editor]');
    if (window.JWPLCColorEditor && colorHost) {
      colorEditor = window.JWPLCColorEditor.create({
        paletteAttr: 'data-popup-palette',
        onInput: (v) => mutateSelected((p) => { p.colors[activeColorKey] = v & 0xFFFF; }, false),
        onCommit: (v) => mutateSelected((p) => { p.colors[activeColorKey] = v & 0xFFFF; }, true)
      });
      if (colorEditor?.element) colorHost.appendChild(colorEditor.element);
    }

    rightSection.addEventListener('click', (e) => {
      const t = e.target;
      if (t.closest('[data-popups-close-insp]')) { closeMode(); return; }
      const main = t.closest('[data-popup-main]');
      if (main) { tabs.main = main.dataset.popupMain; syncRightInspector(); return; }
      const sub = t.closest('[data-popup-sub]');
      if (sub) { tabs.sub = sub.dataset.popupSub; syncRightInspector(); return; }
      const presetBtn = t.closest('[data-popup-preset]');
      if (presetBtn) {
        const nextPreset = presetBtn.dataset.popupPreset;
        mutateSelected((p) => {
          p.preset = nextPreset;
          p.colors = { ...PRESET_COLORS[nextPreset] };
          p.blocksIdle = nextPreset === 'CRIT' || nextPreset === 'CONFIRM';
          p.latched = nextPreset === 'CRIT' || nextPreset === 'CONFIRM';
          p.logToEventLog = nextPreset === 'CRIT';
          if (nextPreset === 'CONFIRM' && p.acceptText === 'OK') p.acceptText = 'ACEPTAR';
          if (nextPreset !== 'CONFIRM' && p.acceptText === 'ACEPTAR') p.acceptText = 'OK';
        });
        return;
      }
      const focusBtn = t.closest('[data-popup-focus]');
      if (focusBtn) {
        previewFocusIndex = Number(focusBtn.dataset.popupFocus) === 1 ? 1 : 0;
        syncRightInspector();
        editor()?.render?.();
        return;
      }
      const sw = t.closest('[data-popup-swatch]');
      if (sw) {
        activeColorKey = sw.dataset.popupSwatch;
        syncRightInspector();
        return;
      }
      if (t.closest('[data-popup-reset-colors]')) {
        mutateSelected((p) => { p.colors = { ...PRESET_COLORS[p.preset] }; });
        return;
      }
      if (t.closest('[data-popup-center]')) {
        mutateSelected((p) => {
          p.x = Math.round((SCREEN_W - p.width) / 2);
          p.y = Math.round((SCREEN_H - p.height) / 2);
        });
        return;
      }
      const toggleBtn = t.closest('[data-popup-toggle]');
      if (toggleBtn) {
        const prop = toggleBtn.dataset.popupToggle;
        mutateSelected((p) => { p[prop] = !p[prop]; });
        return;
      }
      if (t.closest('[data-popup-copy]')) {
        const text = rightSection.querySelector('[data-popup-contract]')?.textContent || '';
        navigator.clipboard?.writeText?.(text).catch(() => {});
      }
    });

    const applyProp = (target, commit) => {
      const prop = target.dataset.popupProp;
      if (!prop) return;
      mutateSelected((p) => {
        if (['id', 'x', 'y', 'width', 'height', 'cornerRadius', 'autoCloseMs', 'targetPage', 'targetView'].includes(prop)) {
          p[prop] = Number(target.value);
        } else if (prop === 'triggerVar' || prop === 'confirmOutVar') {
          p[prop] = target.value;
        } else {
          p[prop] = target.value;
        }
        if (prop === 'id') selectedId = p.id;
      }, commit);
    };

    rightSection.addEventListener('input', (e) => {
      if (e.target.matches('[data-popup-prop]')) applyProp(e.target, false);
    });
    rightSection.addEventListener('change', (e) => {
      if (e.target.matches('[data-popup-prop]')) applyProp(e.target, true);
    });
  }

  function syncLeftPanel() {
    const objectsNav = document.getElementById('objectsNavBlock') || document.getElementById('objectsList')?.closest('.nav-block');
    const pixelSection = document.getElementById('pixelMapLayersSection');
    if (objectsNav) objectsNav.hidden = openModeState;
    if (pixelSection && openModeState) pixelSection.hidden = true;
    if (!leftBlock) return;
    leftBlock.hidden = !openModeState;
    if (!openModeState) return;

    leftBlock.querySelector('[data-popups-count]').textContent = `${state.length}/${MAX_POPUPS}`;
    const listHost = leftBlock.querySelector('[data-popups-list]');
    const cur = findSelected();
    listHost.innerHTML = state.length
      ? state.map((p) => {
          const meta = PRESET_META[p.preset] || PRESET_META.INFO;
          const active = cur && Number(cur.id) === Number(p.id);
          const sub = [p.preset, p.code, p.triggerVar ? `var:${p.triggerVar}` : ''].filter(Boolean).join(' · ');
          return `
            <div class="popup-item-row${active ? ' active' : ''}" data-popup-select="${p.id}">
              <span class="popup-preset-dot ${meta.dotClass}"></span>
              <span class="popup-item-meta">
                <span class="popup-item-title">#${p.id} ${escapeHtml(p.title || p.name)}</span>
                <span class="popup-item-sub">${escapeHtml(sub)}</span>
              </span>
              <button type="button" class="popup-item-del" data-popup-del="${p.id}" title="Eliminar popup">✕</button>
            </div>`;
        }).join('')
      : '<div class="insp-note">Aún no hay ventanas emergentes.</div>';
  }

  function syncRightInspector() {
    if (!rightSection || !openModeState) return;
    const p = findSelected();
    const emptyEl = rightSection.querySelector('[data-popup-empty]');
    const formEl = rightSection.querySelector('[data-popup-form]');
    const badgeEl = rightSection.querySelector('[data-popup-badge]');
    const subTabsEl = rightSection.querySelector('[data-popup-subtabs]');

    if (subTabsEl) subTabsEl.hidden = tabs.main !== 'design';
    rightSection.querySelectorAll('[data-popup-main]').forEach((b) => b.classList.toggle('active', b.dataset.popupMain === tabs.main));
    rightSection.querySelectorAll('[data-popup-sub]').forEach((b) => b.classList.toggle('active', b.dataset.popupSub === tabs.sub));

    const activePanel = tabs.main === 'design' ? `design:${tabs.sub}` : tabs.main;
    rightSection.querySelectorAll('[data-popup-panel]').forEach((panel) => {
      panel.hidden = panel.dataset.popupPanel !== activePanel;
    });

    if (!p) {
      if (emptyEl) emptyEl.hidden = false;
      if (formEl) formEl.hidden = true;
      if (badgeEl) badgeEl.textContent = 'POPUPS';
      return;
    }

    if (emptyEl) emptyEl.hidden = true;
    if (formEl) formEl.hidden = false;
    if (badgeEl) badgeEl.textContent = `POPUP #${p.id} · ${p.preset}`;

    rightSection.querySelectorAll('[data-popup-preset]').forEach((b) => b.classList.toggle('active', b.dataset.popupPreset === p.preset));
    rightSection.querySelector('[data-popup-code-wrap]').hidden = p.preset === 'CONFIRM';
    rightSection.querySelector('[data-popup-cancel-wrap]').hidden = p.preset !== 'CONFIRM';
    rightSection.querySelector('[data-popup-confirm-preview]').hidden = p.preset !== 'CONFIRM';
    rightSection.querySelector('[data-popup-outvar-wrap]').hidden = p.preset !== 'CONFIRM';
    rightSection.querySelector('[data-popup-nav-wrap]').hidden = p.preset !== 'CONFIRM';
    rightSection.querySelectorAll('[data-popup-focus]').forEach((b) => b.classList.toggle('active', Number(b.dataset.popupFocus) === previewFocusIndex));

    const targetPageSel = rightSection.querySelector('[data-popup-prop="targetPage"]');
    const targetViewSel = rightSection.querySelector('[data-popup-prop="targetView"]');
    if (targetPageSel) {
      const pgs = editor()?.getPages?.() || [{ id: 0, name: 'Principal' }];
      const pageOpts = `<option value="-1">Página actual (sin salto)</option>` + pgs.map((pg) => `<option value="${pg.id}">${String(Number(pg.id) + 1).padStart(2, '0')} · ${escapeHtml(pg.name)}</option>`).join('');
      if (targetPageSel.innerHTML !== pageOpts) targetPageSel.innerHTML = pageOpts;
    }
    if (targetViewSel) {
      const pidForViews = p.targetPage >= 0 ? p.targetPage : (editor()?.getActivePage?.() ?? 0);
      const vs = editor()?.getPageViews?.(pidForViews) || [{ id: 0, name: 'Principal' }];
      const viewOpts = `<option value="-1">Sin cambio de subvista</option>` + vs.map((v) => `<option value="${v.id}">V${Number(v.id) + 1} · ${escapeHtml(v.name)}</option>`).join('');
      if (targetViewSel.innerHTML !== viewOpts) targetViewSel.innerHTML = viewOpts;
    }

    rightSection.querySelectorAll('[data-popup-prop]').forEach((input) => {
      if (document.activeElement === input) return;
      const val = p[input.dataset.popupProp];
      input.value = val === undefined || val === null ? '' : String(val);
    });

    ['showOverIdle', 'blocksIdle', 'latched', 'logToEventLog'].forEach((prop) => {
      rightSection.querySelector(`[data-popup-toggle="${prop}"]`)?.classList.toggle('active', Boolean(p[prop]));
      const lbl = rightSection.querySelector(`[data-popup-bool="${prop}"]`);
      if (lbl) lbl.textContent = p[prop] ? 'Sí' : 'No';
    });

    const swHost = rightSection.querySelector('[data-popup-swatch-list]');
    if (swHost) {
      swHost.innerHTML = COLOR_LABELS
        .filter(([k]) => p.preset === 'CONFIRM' || (k !== 'cancelBg' && k !== 'cancelText'))
        .map(([key, label]) => {
          const val = p.colors[key];
          return `<button type="button" class="insp-chip${activeColorKey === key ? ' active' : ''}" data-popup-swatch="${key}" style="border-left: 8px solid ${rgb565ToCss(val)}">${escapeHtml(label)}</button>`;
        }).join('');
    }
    if (colorEditor?.setColor) colorEditor.setColor(p.colors[activeColorKey] ?? 0xFFFF);

    const sym = popupEnumSymbol(p);
    const snippetLines = [
      `// Mostrar desde sketch:`,
      `JWPLC_Display.showPopup(${sym});`
    ];
    if (p.preset === 'CONFIRM') {
      snippetLines.push('', `// Comprobar confirmación una sola vez:`, `if (JWPLC_Display.popupConfirmed(${sym}))`, `{`, `    // Acción aceptada por operador`, `}`);
    } else if (p.preset === 'CRIT') {
      snippetLines.push('', `// o mediante bool automático: ${p.triggerVar || 'alarmaCritica'} = true;`);
    }
    const contractEl = rightSection.querySelector('[data-popup-contract]');
    if (contractEl) contractEl.textContent = snippetLines.join('\n');

    const issuesEl = rightSection.querySelector('[data-popup-issues]');
    if (issuesEl) {
      const { errors, warnings } = validate(state, editor()?.getAllFields?.() || []);
      issuesEl.innerHTML =
        errors.map((e) => `<div class="menu-issue is-error">✕ ${escapeHtml(e)}</div>`).join('') +
        warnings.map((w) => `<div class="menu-issue">! ${escapeHtml(w)}</div>`).join('') ||
        '<div class="menu-issue is-ok">✓ Sin problemas</div>';
    }
  }

  function syncAllUI() {
    syncLeftPanel();
    syncRightInspector();
  }

  if (hasDom) {
    buildLeftPanel();
    buildRightInspector();
    window.addEventListener('jwplc:editor-refresh', syncAllUI);
    window.addEventListener('jwplc:project-loaded', syncAllUI);
  }

  const api = {
    PRESETS,
    PRESET_COLORS,
    normalizePopup,
    presetDefaults,
    validate,
    generate,
    patchHeader,
    paintPopup,
    paintOnLogical,
    all: () => state.map((p) => ({ ...p, colors: { ...p.colors } })),
    serialize: () => state.map((p) => {
      const copy = { ...p, colors: { ...p.colors } };
      return copy;
    }),
    load: setPopups,
    createPreset: createByPreset,
    remove: removePopup,
    select: (id) => { selectedId = Number(id); syncAllUI(); editor()?.render?.(); },
    selected: () => findSelected(),
    isOpen: () => openModeState,
    openMode,
    closeMode,
    toggleMode
  };

  if (typeof window !== 'undefined') window.JWPLCHMIPopups = api;
  if (typeof module !== 'undefined' && module.exports) module.exports = api;
})();
