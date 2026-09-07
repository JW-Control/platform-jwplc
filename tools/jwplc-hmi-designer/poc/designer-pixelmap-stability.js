(() => {
  'use strict';

  if (window.JWPLCHMIPixelStability) return;

  const WIDTH = 320;
  const HEIGHT = 170;
  const displayCanvas = document.getElementById('displayCanvas');
  const previewCanvas = document.getElementById('previewCanvas');
  const zoomSelect = document.getElementById('zoomSelect');

  if (!displayCanvas || !previewCanvas) return;

  const editor = () => window.JWPLCHMIEditor || null;
  const pixels = () => window.JWPLCHMIPixelMaps || null;
  const compat = () => window.JWPLCHMIPixelCompat || null;
  const page = () => Number(editor()?.getActivePage?.() || 0);

  let repaintToken = 0;

  function clamp(value, min, max) {
    return Math.min(max, Math.max(min, value));
  }

  function rgb565(value) {
    value = Number(value) & 0xFFFF;
    const r5 = (value >> 11) & 31;
    const g6 = (value >> 5) & 63;
    const b5 = value & 31;
    return {
      r: (r5 << 3) | (r5 >> 2),
      g: (g6 << 2) | (g6 >> 4),
      b: (b5 << 3) | (b5 >> 2)
    };
  }

  // La referencia de edición se representa como color atenuado opaco sobre el
  // fondo lógico negro. Así un único RGB565 conserva un único tono visible y la
  // grilla no altera su intensidad. No cambia el color almacenado ni el codegen.
  function editorCss565(value, opacity) {
    const c = rgb565(value);
    const k = clamp(Number(opacity), 0.1, 1);
    return `rgb(${Math.round(c.r * k)}, ${Math.round(c.g * k)}, ${Math.round(c.b * k)})`;
  }

  function fieldRects() {
    const helper = compat();
    return (editor()?.getFieldsForPage?.(page()) || []).map((field) => {
      const rect = helper?.fieldRect?.(field);
      return rect ? { x: rect.x, y: rect.y, w: rect.width, h: rect.height } : null;
    }).filter(Boolean);
  }

  function stableRepaint() {
    const pm = pixels();
    if (!pm) return;

    const zoom = Math.max(0.01, Number(zoomSelect?.value) || 1);
    const ctx = displayCanvas.getContext('2d');
    const pctx = previewCanvas.getContext('2d');
    const protectedRects = fieldRects();
    const covered = (x, y) => protectedRects.some((r) => x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h);

    ctx.save();
    pctx.save();
    ctx.globalAlpha = 1;
    pctx.globalAlpha = 1;

    (pm.getAll?.() || []).forEach((map) => {
      if (Number(map.page || 0) !== page() || map.editorVisible === false) return;
      const opacity = Number.isFinite(Number(map.editorOpacity)) ? Number(map.editorOpacity) : 1;
      (map.pixels || []).forEach((pixel) => {
        const x = Number(map.x || 0) + Number(pixel.x || 0);
        const y = Number(map.y || 0) + Number(pixel.y || 0);
        if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT || covered(x, y)) return;
        const color = editorCss565(pixel.color, opacity);
        ctx.fillStyle = color;
        ctx.fillRect(x * zoom, y * zoom, zoom, zoom);
        pctx.fillStyle = color;
        pctx.fillRect(x, y, 1, 1);
      });
    });

    ctx.restore();
    pctx.restore();
    compat()?.restoreFields?.();
  }

  function scheduleStableRepaint() {
    const token = ++repaintToken;
    requestAnimationFrame(() => requestAnimationFrame(() => {
      if (token !== repaintToken) return;
      stableRepaint();
    }));
  }

  function isEditingTarget(target) {
    return target instanceof HTMLInputElement ||
      target instanceof HTMLTextAreaElement ||
      target instanceof HTMLSelectElement ||
      target?.isContentEditable;
  }

  // Captura antes del keydown global de app.js. Si hay PIXEL seleccionado,
  // Ctrl+D pertenece al PixelMap y no debe caer en duplicateSelectedField().
  document.addEventListener('keydown', (event) => {
    const pm = pixels();
    if (!pm?.getSelected?.() || isEditingTarget(event.target)) return;
    const ctrl = event.ctrlKey || event.metaKey;
    if (!ctrl || event.key.toLowerCase() !== 'd') return;

    event.preventDefault();
    event.stopImmediatePropagation();
    pm.duplicate?.();
    scheduleStableRepaint();
  }, true);

  // El botón y Ctrl+D usan exactamente la misma operación. Se intercepta en
  // captura para evitar que el listener histórico del inspector la ejecute dos veces.
  document.addEventListener('click', (event) => {
    const button = event.target?.closest?.('#pixelMapDuplicate');
    if (!button || !pixels()?.getSelected?.()) return;
    event.preventDefault();
    event.stopImmediatePropagation();
    pixels()?.duplicate?.();
    scheduleStableRepaint();
  }, true);

  window.addEventListener('jwplc:editor-refresh', scheduleStableRepaint);
  window.addEventListener('jwplc:pixelmap-changed', scheduleStableRepaint);
  window.addEventListener('jwplc:project-loaded', scheduleStableRepaint);
  document.addEventListener('pointerup', scheduleStableRepaint, true);
  document.addEventListener('pointercancel', scheduleStableRepaint, true);
  document.addEventListener('keyup', (event) => {
    if (event.key === 'Escape' || event.key.startsWith('Arrow')) scheduleStableRepaint();
  }, true);

  const readyTimer = setInterval(() => {
    if (!pixels()) return;
    clearInterval(readyTimer);
    scheduleStableRepaint();
  }, 25);
  setTimeout(() => clearInterval(readyTimer), 5000);

  window.JWPLCHMIPixelStability = {
    repaint: stableRepaint,
    schedule: scheduleStableRepaint
  };
})();
