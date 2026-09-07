(() => {
  'use strict';

  if (window.JWPLCHMIPixelStability) return;

  const WIDTH = 320;
  const HEIGHT = 170;
  const displayCanvas = document.getElementById('displayCanvas');
  const previewCanvas = document.getElementById('previewCanvas');
  const zoomSelect = document.getElementById('zoomSelect');
  const codeOutput = document.getElementById('codeOutput');
  const generateButton = document.getElementById('generateButton');
  const contractTab = document.getElementById('contractTab');

  if (!displayCanvas || !previewCanvas) return;

  const editor = () => window.JWPLCHMIEditor || null;
  const pixels = () => window.JWPLCHMIPixelMaps || null;
  const compat = () => window.JWPLCHMIPixelCompat || null;
  const page = () => Number(editor()?.getActivePage?.() || 0);

  let repaintToken = 0;
  let buildCodeWrapped = false;

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

  // La referencia de edición usa un color atenuado opaco sobre el fondo lógico
  // negro. No existe alpha runtime: sólo es una ayuda visual del Designer.
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
    const helper = compat();
    if (!pm || !helper?.restoreBase?.()) return;

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
    helper.restoreFields?.();
  }

  // Un único RAF: designer-pixelmap.js ya agenda su render antes que este
  // listener. El compositor final corre en el mismo frame y no deja visible un
  // frame intermedio con globalAlpha o bordes naranjas de selección.
  function scheduleStableRepaint() {
    const token = ++repaintToken;
    requestAnimationFrame(() => {
      if (token !== repaintToken) return;
      stableRepaint();
    });
  }

  function isEditingTarget(target) {
    return target instanceof HTMLInputElement ||
      target instanceof HTMLTextAreaElement ||
      target instanceof HTMLSelectElement ||
      target?.isContentEditable;
  }

  function sanitizeToken(value, fallback) {
    const normalized = String(value || '')
      .normalize('NFD')
      .replace(/[\u0300-\u036f]/g, '')
      .trim()
      .replace(/[^A-Za-z0-9_]+/g, '_')
      .replace(/^_+|_+$/g, '')
      .toUpperCase();
    let token = normalized || fallback;
    if (!token.startsWith('PIXEL_')) token = `PIXEL_${token}`;
    if (!/^[A-Z_]/.test(token)) token = `PIXEL_${token}`;
    return token;
  }

  function runtimeMaps() {
    return (pixels()?.getAll?.() || []).filter((map) => Array.isArray(map.pixels) && map.pixels.length);
  }

  function pixelMapEnumBlock() {
    const used = new Set();
    const lines = runtimeMaps().map((map, index) => {
      let symbol = sanitizeToken(map.name, `PIXEL_${index + 1}`);
      if (used.has(symbol)) symbol = `${symbol}_${index + 1}`;
      used.add(symbol);
      return `    ${symbol} = ${index}`;
    });
    if (!lines.length) return '';
    return `enum HMIPixelMapId : uint8_t\n{\n${lines.join(',\n')}\n};`;
  }

  function wrapBuildCode() {
    const pm = pixels();
    if (!pm?.buildCode || buildCodeWrapped || pm.__a11RuntimeIdsWrapped) return Boolean(pm?.__a11RuntimeIdsWrapped);
    const original = pm.buildCode.bind(pm);
    pm.buildCode = () => {
      const generated = original();
      if (!generated?.block) return generated;
      const enumBlock = pixelMapEnumBlock();
      return {
        ...generated,
        block: enumBlock ? `${enumBlock}\n\n${generated.block}` : generated.block
      };
    };
    pm.__a11RuntimeIdsWrapped = true;
    buildCodeWrapped = true;
    return true;
  }

  function pageSymbols() {
    const symbols = window.JWPLCHMICodegen?.getPageSymbols?.() || [];
    if (symbols.length) return symbols;
    const maxMapPage = runtimeMaps().reduce((max, map) => Math.max(max, Number(map.page || 0)), 0);
    return Array.from({ length: maxMapPage + 1 }, (_, index) => ({
      id: index,
      name: index === 0 ? 'Principal' : `Página ${index + 1}`,
      symbol: index === 0 ? 'PAGE_PRINCIPAL' : `PAGE_PAGINA_${index + 1}`
    }));
  }

  function pixelOnlyHeader(generated) {
    const pages = pageSymbols();
    const firstPage = pages[0]?.symbol || 'PAGE_PRINCIPAL';
    const pageEnum = `enum HMIPageId : uint8_t\n{\n${pages.map((item) => `    ${item.symbol} = ${Number(item.id || 0)}`).join(',\n')}\n};`;

    return `// Código generado por JWPLC HMI Designer\n` +
      `// API pública JWPLC_UI · Alpha11 A11-7B\n` +
      `// Proyecto Pixel-only: 0 fields, PixelMaps RGB565 declarativos.\n` +
      `// Destino recomendado: JWPLC_HMI_Generated.h\n` +
      `// Archivo autogenerado: regenerar desde el Designer en lugar de editar a mano.\n\n` +
      `#pragma once\n\n` +
      `#include <JWPLC_Display.h>\n\n` +
      `${pageEnum}\n\n` +
      `${generated.block}\n\n` +
      `void jwplcHMISetup()\n` +
      `{\n` +
      `${generated.registration}\n` +
      `    JWPLC_Display.setUserPageCount(${Math.max(1, pages.length)});\n` +
      `    JWPLC_Display.setUserPage(${firstPage});\n` +
      `    JWPLC_Display.setUserRefreshMode(USER_REFRESH_ON_DEMAND);\n` +
      `}\n\n` +
      `void jwplcUIUpdate()\n` +
      `{\n` +
      `    // Los PixelMaps son estáticos. Para frames/estados use:\n` +
      `    // JWPLC_Display.setPixelMapVisible(PIXEL_1, true/false);\n` +
      `}\n`;
  }

  function ensurePixelOnlyCode() {
    wrapBuildCode();
    const pm = pixels();
    const generated = pm?.buildCode?.();
    if (!generated?.block || !codeOutput) return;

    const fields = editor()?.getAllFields?.() || [];
    if (fields.length !== 0) return;

    codeOutput.textContent = pixelOnlyHeader(generated);
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

  // El botón y Ctrl+D usan exactamente la misma operación.
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

  // El input del slider ocurre después de que el handler PIXEL agenda su render;
  // nuestro RAF queda después y compone el estado definitivo en el mismo frame.
  document.addEventListener('input', (event) => {
    if (event.target?.id === 'pixelMapOpacity') scheduleStableRepaint();
  });
  document.addEventListener('change', (event) => {
    if (event.target?.id === 'pixelMapOpacity') scheduleStableRepaint();
  });
  document.addEventListener('click', (event) => {
    if (event.target?.closest?.('.a11-pixel-row, #pixelMapVisibility, #pixelMapOnion, [data-pm-mode]')) {
      scheduleStableRepaint();
    }
  });
  document.addEventListener('pointerup', scheduleStableRepaint, true);
  document.addEventListener('pointercancel', scheduleStableRepaint, true);
  document.addEventListener('keyup', (event) => {
    if (event.key === 'Escape' || event.key.startsWith('Arrow')) scheduleStableRepaint();
  }, true);

  generateButton?.addEventListener('click', () => setTimeout(ensurePixelOnlyCode, 90));
  contractTab?.addEventListener('click', () => setTimeout(ensurePixelOnlyCode, 90));

  const readyTimer = setInterval(() => {
    if (!pixels()) return;
    wrapBuildCode();
    clearInterval(readyTimer);
    scheduleStableRepaint();
  }, 25);
  setTimeout(() => clearInterval(readyTimer), 5000);

  window.JWPLCHMIPixelStability = {
    repaint: stableRepaint,
    schedule: scheduleStableRepaint,
    ensurePixelOnlyCode,
    pixelMapEnumBlock
  };
})();
