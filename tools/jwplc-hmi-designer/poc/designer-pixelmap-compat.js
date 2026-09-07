(() => {
  'use strict';

  if (window.JWPLCHMIPixelCompat) return;

  const WIDTH = 320;
  const HEIGHT = 170;
  const FIELD_PADDING = 3;
  const FIELD_GAP = 4;
  const BAR_W = 80;
  const BAR_H = 12;
  const display = document.getElementById('displayCanvas');
  const preview = document.getElementById('previewCanvas');
  if (!display || !preview) return;

  const baseDisplay = document.createElement('canvas');
  const basePreview = document.createElement('canvas');
  let haveBase = false;
  let restoreQueued = false;

  const editor = () => window.JWPLCHMIEditor || null;
  const activePage = () => Number(editor()?.getActivePage?.() || 0);

  function textBounds(text, size) {
    text = String(text || '');
    if (!text) return { width: 0, height: 0 };
    const scale = Math.max(1, Math.trunc(Number(size) || 1));
    return { width: text.length * 6 * scale - scale, height: 7 * scale };
  }

  function numericSample(field) {
    const integers = Math.max(1, Math.trunc(Number(field.integerDigits) || 1));
    const decimals = Math.max(0, Math.trunc(Number(field.decimalDigits) || 0));
    return `${field.signedValue ? '-' : ''}${'8'.repeat(integers)}${decimals ? `.${'8'.repeat(decimals)}` : ''}`;
  }

  function barRect(field) {
    const labelText = String(field.barLabel ?? field.label ?? 'Nivel');
    const unitText = String(field.barUnit ?? field.unit ?? '%');
    const pad = Math.max(FIELD_PADDING, Math.max(1, Math.trunc(Number(field.labelSize) || 1)));
    const label = textBounds(labelText, field.labelSize);
    const unit = textBounds(unitText, field.labelSize);
    let valueW = BAR_W;
    let width;
    let height;

    if (field.barAutoWidth === false) {
      width = Math.max(24, Math.min(WIDTH, Math.trunc(Number(field.barWidth) || 110)));
      let available = width - 2 * pad;
      if (field.layout === 'INLINE' && label.width) available -= label.width + FIELD_GAP;
      if (unit.width) available -= unit.width + FIELD_GAP;
      valueW = Math.max(1, available);
    }

    if (field.layout === 'STACKED') {
      if (field.barAutoWidth !== false) width = 2 * pad + Math.max(label.width, valueW + (unit.width ? FIELD_GAP + unit.width : 0));
      height = 2 * pad + label.height + (label.height ? FIELD_GAP : 0) + Math.max(BAR_H, unit.height);
    } else {
      if (field.barAutoWidth !== false) width = 2 * pad + label.width + (label.width ? FIELD_GAP : 0) + valueW + (unit.width ? FIELD_GAP + unit.width : 0);
      height = 2 * pad + Math.max(label.height, BAR_H, unit.height);
    }
    return { x: Number(field.x) || 0, y: Number(field.y) || 0, width, height };
  }

  function fieldRect(field) {
    if (field.type === 'BAR') return barRect(field);
    const pad = Math.max(FIELD_PADDING, Number(field.labelSize) || 1, Number(field.valueSize) || 1);
    const label = textBounds(field.label, field.labelSize);
    const unit = textBounds(field.unit, field.labelSize);
    let valueText;
    if (field.type === 'VALUE') valueText = numericSample(field);
    else if (field.type === 'BOOL') {
      const a = String(field.falseText || 'OFF');
      const b = String(field.trueText || 'ON');
      valueText = a.length >= b.length ? a : b;
    } else valueText = 'W'.repeat(Math.max(1, Number(field.capacity) || 1));
    const value = textBounds(valueText, field.valueSize);
    let width;
    let height;
    if (field.layout === 'STACKED') {
      width = 2 * pad + Math.max(label.width, value.width + (unit.width ? FIELD_GAP + unit.width : 0));
      height = 2 * pad + label.height + (label.height ? FIELD_GAP : 0) + Math.max(value.height, unit.height);
    } else {
      width = 2 * pad + label.width + (label.width ? FIELD_GAP : 0) + value.width + (unit.width ? FIELD_GAP + unit.width : 0);
      height = 2 * pad + Math.max(label.height, value.height, unit.height);
    }
    return { x: Number(field.x) || 0, y: Number(field.y) || 0, width, height };
  }

  function captureBase() {
    baseDisplay.width = display.width;
    baseDisplay.height = display.height;
    baseDisplay.getContext('2d').drawImage(display, 0, 0);
    basePreview.width = preview.width;
    basePreview.height = preview.height;
    basePreview.getContext('2d').drawImage(preview, 0, 0);
    haveBase = true;
  }

  function restoreFields() {
    restoreQueued = false;
    if (!haveBase) return;
    const zoom = display.width / WIDTH;
    const dctx = display.getContext('2d');
    const pctx = preview.getContext('2d');
    (editor()?.getFieldsForPage?.(activePage()) || []).forEach((field) => {
      const rect = fieldRect(field);
      if (!rect || rect.width <= 0 || rect.height <= 0) return;
      const sx = Math.max(0, rect.x);
      const sy = Math.max(0, rect.y);
      const sw = Math.min(WIDTH - sx, rect.width);
      const sh = Math.min(HEIGHT - sy, rect.height);
      if (sw <= 0 || sh <= 0) return;
      dctx.drawImage(baseDisplay, sx * zoom, sy * zoom, sw * zoom, sh * zoom, sx * zoom, sy * zoom, sw * zoom, sh * zoom);
      pctx.drawImage(basePreview, sx, sy, sw, sh, sx, sy, sw, sh);
    });
  }

  function queueRestore() {
    if (restoreQueued) return;
    restoreQueued = true;
    requestAnimationFrame(() => requestAnimationFrame(restoreFields));
  }

  // Se registra antes de designer-pixelmap.js: captura el framebuffer base que
  // acaba de producir app.js. Después del overlay restaura únicamente los fields,
  // reproduciendo el orden físico PixelMaps -> fields del runtime JWPLC_Display.
  window.addEventListener('jwplc:editor-refresh', () => {
    captureBase();
    queueRestore();
  });
  document.addEventListener('pointerdown', queueRestore, true);
  document.addEventListener('pointermove', (event) => { if (event.buttons) queueRestore(); }, true);
  document.addEventListener('click', queueRestore, true);

  function wrapCodegen() {
    const cg = window.JWPLCHMICodegen;
    const pm = window.JWPLCHMIPixelMaps;
    if (!cg || !pm || cg.__a11PixelWrapped) return false;
    const original = cg.refresh?.bind(cg);
    if (!original) return false;
    cg.refresh = () => {
      original();
      const generated = pm.buildCode?.();
      const output = document.getElementById('codeOutput');
      if (!generated?.block || !output?.textContent?.startsWith('// Código generado por JWPLC HMI Designer')) return;
      let text = output.textContent;
      if (text.includes('// PixelMaps estáticos RGB565 · JWPLC HMI Designer')) return;
      const setup = text.indexOf('void jwplcHMISetup()');
      if (setup < 0) return;
      text = `${text.slice(0, setup).trimEnd()}\n\n${generated.block}\n\n${text.slice(setup)}`;
      const marker = text.indexOf('    JWPLC_Display.setUserRefreshMode(', text.indexOf('void jwplcHMISetup()'));
      if (marker >= 0) text = `${text.slice(0, marker)}${generated.registration}\n${text.slice(marker)}`;
      output.textContent = text;
    };
    cg.__a11PixelWrapped = true;
    return true;
  }

  const wrapTimer = setInterval(() => {
    if (wrapCodegen()) clearInterval(wrapTimer);
  }, 50);
  setTimeout(() => clearInterval(wrapTimer), 5000);

  window.addEventListener('jwplc:pixelmap-changed', () => {
    queueRestore();
    setTimeout(() => window.JWPLCHMILive?.sendFrame?.(), 0);
  });

  window.JWPLCHMIPixelCompat = { fieldRect, captureBase, restoreFields, wrapCodegen };
})();
