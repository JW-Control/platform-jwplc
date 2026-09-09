(() => {
  'use strict';

  if (window.JWPLCHMIPixelWorkbench) return;

  const WIDTH = 320;
  const HEIGHT = 170;
  const MAX_HISTORY = 80;
  const HISTORY_DEBOUNCE_MS = 45;

  const waitForPixelMaps = () => new Promise((resolve) => {
    const started = performance.now();
    const timer = setInterval(() => {
      if (window.JWPLCHMIPixelMaps) {
        clearInterval(timer);
        resolve(window.JWPLCHMIPixelMaps);
        return;
      }
      if (performance.now() - started > 5000) {
        clearInterval(timer);
        resolve(null);
      }
    }, 25);
  });

  const isEditingTarget = (target) => target instanceof HTMLInputElement ||
    target instanceof HTMLTextAreaElement ||
    target instanceof HTMLSelectElement ||
    target?.isContentEditable;

  function clamp(value, min, max) {
    return Math.min(max, Math.max(min, value));
  }

  function brushRange(size) {
    const normalized = clamp(Math.trunc(Number(size) || 1), 1, 16);
    const start = -Math.floor((normalized - 1) / 2);
    return { size: normalized, start, end: start + normalized - 1 };
  }

  function pointFromEvent(canvas, event) {
    const rect = canvas.getBoundingClientRect();
    const scaleX = canvas.width / rect.width;
    const scaleY = canvas.height / rect.height;
    const zoom = Math.max(0.01, Number(document.getElementById('zoomSelect')?.value) || 1);
    return {
      x: Math.floor(((event.clientX - rect.left) * scaleX) / zoom),
      y: Math.floor(((event.clientY - rect.top) * scaleY) / zoom)
    };
  }

  function mapBounds(map) {
    if (!map?.pixels?.length) {
      return { x: Number(map?.x || 0), y: Number(map?.y || 0), width: 1, height: 1 };
    }
    let maxX = 0;
    let maxY = 0;
    map.pixels.forEach((pixel) => {
      maxX = Math.max(maxX, Number(pixel.x || 0));
      maxY = Math.max(maxY, Number(pixel.y || 0));
    });
    return {
      x: Number(map.x || 0),
      y: Number(map.y || 0),
      width: maxX + 1,
      height: maxY + 1
    };
  }

  function pixelIndex(map, gx, gy) {
    if (!map?.pixels) return -1;
    const lx = gx - Number(map.x || 0);
    const ly = gy - Number(map.y || 0);
    return map.pixels.findIndex((pixel) => Number(pixel.x) === lx && Number(pixel.y) === ly);
  }

  function pixelColor(map, gx, gy) {
    const index = pixelIndex(map, gx, gy);
    return index >= 0 ? (Number(map.pixels[index].color) & 0xFFFF) : null;
  }

  function putPixel(map, gx, gy, value) {
    if (!map || gx < 0 || gx >= WIDTH || gy < 0 || gy >= HEIGHT) return false;
    value = Number(value) & 0xFFFF;

    if (!Array.isArray(map.pixels)) map.pixels = [];
    if (!map.pixels.length) {
      map.x = gx;
      map.y = gy;
      map.pixels.push({ x: 0, y: 0, color: value });
      return true;
    }

    if (gx < map.x) {
      const shift = map.x - gx;
      map.pixels.forEach((pixel) => { pixel.x += shift; });
      map.x = gx;
    }
    if (gy < map.y) {
      const shift = map.y - gy;
      map.pixels.forEach((pixel) => { pixel.y += shift; });
      map.y = gy;
    }

    const lx = gx - map.x;
    const ly = gy - map.y;
    const index = map.pixels.findIndex((pixel) => pixel.x === lx && pixel.y === ly);
    if (index >= 0) {
      if ((Number(map.pixels[index].color) & 0xFFFF) === value) return false;
      map.pixels[index].color = value;
      return true;
    }

    map.pixels.push({ x: lx, y: ly, color: value });
    return true;
  }

  function putBrush(map, gx, gy, value, size) {
    const range = brushRange(size);
    let changed = false;
    for (let oy = range.start; oy <= range.end; oy += 1) {
      for (let ox = range.start; ox <= range.end; ox += 1) {
        changed = putPixel(map, gx + ox, gy + oy, value) || changed;
      }
    }
    return changed;
  }

  function rasterLine(a, b, fn) {
    let x0 = a.x;
    let y0 = a.y;
    const x1 = b.x;
    const y1 = b.y;
    const dx = Math.abs(x1 - x0);
    const sx = x0 < x1 ? 1 : -1;
    const dy = -Math.abs(y1 - y0);
    const sy = y0 < y1 ? 1 : -1;
    let error = dx + dy;

    while (true) {
      fn(x0, y0);
      if (x0 === x1 && y0 === y1) break;
      const e2 = 2 * error;
      if (e2 >= dy) { error += dy; x0 += sx; }
      if (e2 <= dx) { error += dx; y0 += sy; }
    }
  }

  function drawLine(map, a, b, color, size) {
    let changed = false;
    rasterLine(a, b, (x, y) => {
      changed = putBrush(map, x, y, color, size) || changed;
    });
    return changed;
  }

  function drawRect(map, a, b, color, size) {
    const left = Math.min(a.x, b.x);
    const right = Math.max(a.x, b.x);
    const top = Math.min(a.y, b.y);
    const bottom = Math.max(a.y, b.y);
    let changed = false;

    changed = drawLine(map, { x: left, y: top }, { x: right, y: top }, color, size) || changed;
    changed = drawLine(map, { x: right, y: top }, { x: right, y: bottom }, color, size) || changed;
    changed = drawLine(map, { x: right, y: bottom }, { x: left, y: bottom }, color, size) || changed;
    changed = drawLine(map, { x: left, y: bottom }, { x: left, y: top }, color, size) || changed;
    return changed;
  }

  function floodFill(map, gx, gy, replacement) {
    if (!map?.pixels?.length) return false;
    const bounds = mapBounds(map);
    if (gx < bounds.x || gx >= bounds.x + bounds.width || gy < bounds.y || gy >= bounds.y + bounds.height) {
      return false;
    }

    replacement = Number(replacement) & 0xFFFF;
    const target = pixelColor(map, gx, gy);
    if (target === replacement) return false;

    const pixelLookup = new Map();
    map.pixels.forEach((pixel) => {
      const x = Number(map.x) + Number(pixel.x);
      const y = Number(map.y) + Number(pixel.y);
      pixelLookup.set(`${x},${y}`, pixel);
    });

    const sameTarget = (x, y) => {
      const pixel = pixelLookup.get(`${x},${y}`);
      const value = pixel ? (Number(pixel.color) & 0xFFFF) : null;
      return value === target;
    };

    const queue = [{ x: gx, y: gy }];
    const visited = new Set();
    const region = [];
    let cursor = 0;

    while (cursor < queue.length) {
      const point = queue[cursor++];
      const key = `${point.x},${point.y}`;
      if (visited.has(key)) continue;
      visited.add(key);

      if (point.x < bounds.x || point.x >= bounds.x + bounds.width || point.y < bounds.y || point.y >= bounds.y + bounds.height) continue;
      if (!sameTarget(point.x, point.y)) continue;

      region.push(point);
      queue.push(
        { x: point.x + 1, y: point.y },
        { x: point.x - 1, y: point.y },
        { x: point.x, y: point.y + 1 },
        { x: point.x, y: point.y - 1 }
      );
    }

    if (!region.length) return false;

    if (target === null) {
      region.forEach((point) => putPixel(map, point.x, point.y, replacement));
    } else {
      region.forEach((point) => {
        const pixel = pixelLookup.get(`${point.x},${point.y}`);
        if (pixel) pixel.color = replacement;
      });
    }
    return true;
  }

  function rgb565Css(value) {
    value = Number(value) & 0xFFFF;
    const r5 = (value >> 11) & 31;
    const g6 = (value >> 5) & 63;
    const b5 = value & 31;
    const r = (r5 << 3) | (r5 >> 2);
    const g = (g6 << 2) | (g6 >> 4);
    const b = (b5 << 3) | (b5 >> 2);
    return `rgb(${r}, ${g}, ${b})`;
  }

  async function init() {
    const api = await waitForPixelMaps();
    if (!api) return;

    const displayCanvas = document.getElementById('displayCanvas');
    const canvasStage = document.getElementById('canvasStage');
    const zoomSelect = document.getElementById('zoomSelect');
    const undoButton = document.getElementById('undoButton');
    const redoButton = document.getElementById('redoButton');
    const pixelComponent = document.querySelector('.tool[data-tool="pixel"]');

    if (!displayCanvas || !canvasStage) return;

    let extraMode = null;
    let nativeMode = 'DRAW';
    let gesture = null;
    let lastPointerPoint = null;
    let restoringHistory = false;
    let historyTimer = null;
    let history = [];
    let historyIndex = -1;

    function selectedMap() {
      return api.getSelected?.() || null;
    }

    function selectedMapIndex() {
      const selected = selectedMap();
      return selected ? Math.max(0, (api.getAll?.() || []).indexOf(selected)) : -1;
    }

    function snapshot() {
      return {
        maps: api.export?.() || [],
        selectedIndex: selectedMapIndex()
      };
    }

    function sameSnapshot(a, b) {
      if (!a || !b) return false;
      return JSON.stringify(a.maps) === JSON.stringify(b.maps);
    }

    function resetHistory() {
      clearTimeout(historyTimer);
      historyTimer = null;
      history = [snapshot()];
      historyIndex = 0;
      syncHistoryButtons();
    }

    function commitHistoryNow() {
      clearTimeout(historyTimer);
      historyTimer = null;
      if (restoringHistory) return;
      const next = snapshot();
      if (historyIndex >= 0 && sameSnapshot(next, history[historyIndex])) {
        syncHistoryButtons();
        return;
      }
      history.splice(historyIndex + 1);
      history.push(next);
      if (history.length > MAX_HISTORY) history.shift();
      historyIndex = history.length - 1;
      syncHistoryButtons();
    }

    function scheduleHistoryCommit() {
      if (restoringHistory) return;
      clearTimeout(historyTimer);
      historyTimer = setTimeout(commitHistoryNow, HISTORY_DEBOUNCE_MS);
    }

    function rowForIndex(index) {
      const map = (api.getAll?.() || [])[index];
      if (!map) return null;
      return [...document.querySelectorAll('.a11-pixel-row')].find((row) =>
        row.querySelector('.object-name')?.textContent === map.name) || null;
    }

    function restoreSnapshot(state) {
      if (!state) return;
      const keepNativeMode = nativeMode;
      const keepExtraMode = extraMode;
      restoringHistory = true;
      api.import?.(state.maps);

      requestAnimationFrame(() => {
        const row = rowForIndex(state.selectedIndex);
        row?.click();

        if (keepExtraMode) {
          extraMode = keepExtraMode;
          updateToolButtons();
        } else {
          extraMode = null;
          nativeMode = keepNativeMode;
          document.querySelector(`[data-pm-mode="${nativeMode}"]`)?.click();
        }

        api.refresh?.();
        window.JWPLCHMIPixelStability?.schedule?.();
        restoringHistory = false;
        syncHistoryButtons();
        drawCursor(lastPointerPoint);
      });
    }

    function undoPixel() {
      commitHistoryNow();
      if (historyIndex <= 0) return false;
      historyIndex -= 1;
      restoreSnapshot(history[historyIndex]);
      return true;
    }

    function redoPixel() {
      commitHistoryNow();
      if (historyIndex >= history.length - 1) return false;
      historyIndex += 1;
      restoreSnapshot(history[historyIndex]);
      return true;
    }

    function syncHistoryButtons() {
      if (!selectedMap()) return;
      if (undoButton) {
        undoButton.disabled = historyIndex <= 0;
        undoButton.title = 'Deshacer cambio PIXEL (Ctrl+Z)';
      }
      if (redoButton) {
        redoButton.disabled = historyIndex < 0 || historyIndex >= history.length - 1;
        redoButton.title = 'Rehacer cambio PIXEL (Ctrl+Y / Ctrl+Shift+Z)';
      }
    }

    function notifyChanged(source) {
      api.refresh?.();
      window.dispatchEvent(new CustomEvent('jwplc:pixelmap-changed', {
        detail: { count: (api.getAll?.() || []).length, source }
      }));
    }

    function injectTools() {
      const actions = document.querySelector('.a11-pixel-actions');
      if (!actions || actions.dataset.workbenchReady === '1') return false;
      actions.dataset.workbenchReady = '1';

      const tools = [
        ['FILL', '▨ Relleno', 'Rellenar región contigua (G)'],
        ['PICK', '◉ Color', 'Cuentagotas RGB565 (I / Alt+clic)'],
        ['LINE', '╱ Línea', 'Línea pixel-perfect (L)'],
        ['RECT', '□ Rect.', 'Rectángulo sin relleno (R)']
      ];

      tools.forEach(([mode, label, title]) => {
        const button = document.createElement('button');
        button.type = 'button';
        button.dataset.pmwMode = mode;
        button.textContent = label;
        button.title = title;
        button.addEventListener('click', () => {
          extraMode = mode;
          updateToolButtons();
          drawCursor(lastPointerPoint);
        });
        actions.appendChild(button);
      });

      const note = document.createElement('p');
      note.className = 'a11-pixel-note a11-pixel-workbench-note';
      note.innerHTML = '<strong>Atajos:</strong> B pincel · E borrador · G relleno · I color · L línea · R rectángulo · V mover · Alt+clic toma color.';
      actions.insertAdjacentElement('afterend', note);

      updateToolButtons();
      return true;
    }

    function injectStyles() {
      if (document.getElementById('a11-pixel-workbench-style')) return;
      const style = document.createElement('style');
      style.id = 'a11-pixel-workbench-style';
      style.textContent = `
        .a11-pixel-actions[data-workbench-ready="1"]{grid-template-columns:repeat(3,minmax(0,1fr))!important}
        .a11-pixel-actions[data-workbench-ready="1"] button{min-width:0;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
        .a11-pixel-workbench-note{margin-top:7px!important}
        #pixelToolCursorCanvas{position:absolute;inset:0;width:100%;height:100%;pointer-events:none;z-index:40}
      `;
      document.head.appendChild(style);
    }

    function updateToolButtons() {
      document.querySelectorAll('[data-pmw-mode]').forEach((button) => {
        button.classList.toggle('active', button.dataset.pmwMode === extraMode);
      });
      document.querySelectorAll('[data-pm-mode]').forEach((button) => {
        if (extraMode) button.classList.remove('active');
      });
    }

    const cursorCanvas = document.createElement('canvas');
    cursorCanvas.id = 'pixelToolCursorCanvas';
    cursorCanvas.setAttribute('aria-hidden', 'true');
    canvasStage.appendChild(cursorCanvas);
    const cursorCtx = cursorCanvas.getContext('2d');

    function syncCursorCanvas() {
      if (cursorCanvas.width !== displayCanvas.width) cursorCanvas.width = displayCanvas.width;
      if (cursorCanvas.height !== displayCanvas.height) cursorCanvas.height = displayCanvas.height;
    }

    function activeFootprintSize() {
      if (extraMode === 'LINE' || extraMode === 'RECT') return api.getBrushSize?.() || 1;
      if (extraMode === 'FILL' || extraMode === 'PICK') return 1;
      if (nativeMode === 'ERASE') return api.getEraserSize?.() || 1;
      if (nativeMode === 'DRAW') return api.getBrushSize?.() || 1;
      return 1;
    }

    function drawGuideRect(point, size, stroke, dashed = true) {
      const zoom = Math.max(0.01, Number(zoomSelect?.value) || 1);
      const range = brushRange(size);
      const x = (point.x + range.start) * zoom;
      const y = (point.y + range.start) * zoom;
      const w = range.size * zoom;
      const h = range.size * zoom;
      cursorCtx.save();
      cursorCtx.strokeStyle = stroke;
      cursorCtx.lineWidth = 1;
      if (dashed) cursorCtx.setLineDash([4, 3]);
      cursorCtx.strokeRect(x + 0.5, y + 0.5, Math.max(1, w - 1), Math.max(1, h - 1));
      cursorCtx.restore();
    }

    function drawPreviewShape() {
      if (!gesture || !gesture.current) return;
      const zoom = Math.max(0.01, Number(zoomSelect?.value) || 1);
      const color = rgb565Css(api.getActiveColor?.() ?? 0xFFFF);
      cursorCtx.save();
      cursorCtx.strokeStyle = color;
      cursorCtx.globalAlpha = 0.9;
      cursorCtx.lineWidth = Math.max(1, (api.getBrushSize?.() || 1) * zoom);
      cursorCtx.setLineDash([4, 3]);

      if (extraMode === 'LINE') {
        cursorCtx.beginPath();
        cursorCtx.moveTo((gesture.start.x + 0.5) * zoom, (gesture.start.y + 0.5) * zoom);
        cursorCtx.lineTo((gesture.current.x + 0.5) * zoom, (gesture.current.y + 0.5) * zoom);
        cursorCtx.stroke();
      } else if (extraMode === 'RECT') {
        const left = Math.min(gesture.start.x, gesture.current.x);
        const top = Math.min(gesture.start.y, gesture.current.y);
        const right = Math.max(gesture.start.x, gesture.current.x);
        const bottom = Math.max(gesture.start.y, gesture.current.y);
        cursorCtx.strokeRect(left * zoom + 0.5, top * zoom + 0.5, (right - left + 1) * zoom - 1, (bottom - top + 1) * zoom - 1);
      }
      cursorCtx.restore();
    }

    function drawCursor(point) {
      syncCursorCanvas();
      cursorCtx.clearRect(0, 0, cursorCanvas.width, cursorCanvas.height);
      if (!point || !selectedMap() || point.x < 0 || point.x >= WIDTH || point.y < 0 || point.y >= HEIGHT) return;

      if (gesture) drawPreviewShape();

      if (extraMode === 'FILL') {
        drawGuideRect(point, 1, 'rgba(255,211,86,.95)', false);
        return;
      }
      if (extraMode === 'PICK') {
        drawGuideRect(point, 1, 'rgba(116,220,255,.95)', false);
        return;
      }
      if (extraMode === 'LINE' || extraMode === 'RECT') {
        drawGuideRect(point, activeFootprintSize(), 'rgba(255,154,67,.92)');
        return;
      }
      if (nativeMode === 'DRAW') {
        drawGuideRect(point, activeFootprintSize(), 'rgba(255,154,67,.92)');
      } else if (nativeMode === 'ERASE') {
        drawGuideRect(point, activeFootprintSize(), 'rgba(116,220,255,.92)');
      }
    }

    function pickColorAt(point) {
      const maps = (api.getAll?.() || []).filter((map) =>
        Number(map.page || 0) === Number(window.JWPLCHMIEditor?.getActivePage?.() || 0) && map.editorVisible !== false);
      for (let i = maps.length - 1; i >= 0; i -= 1) {
        const value = pixelColor(maps[i], point.x, point.y);
        if (value !== null) {
          api.setActiveColor?.(value);
          return true;
        }
      }
      return false;
    }

    function activateNativeMode(mode) {
      extraMode = null;
      nativeMode = mode;
      document.querySelector(`[data-pm-mode="${mode}"]`)?.click();
      updateToolButtons();
      drawCursor(lastPointerPoint);
    }

    function activateExtraMode(mode) {
      extraMode = mode;
      updateToolButtons();
      drawCursor(lastPointerPoint);
    }

    function handlePointerDown(event) {
      const map = selectedMap();
      if (!map || event.button !== 0) return;
      if (event.target !== displayCanvas && !displayCanvas.contains?.(event.target)) return;

      const point = pointFromEvent(displayCanvas, event);
      lastPointerPoint = point;

      if (event.altKey) {
        event.preventDefault();
        event.stopImmediatePropagation();
        pickColorAt(point);
        drawCursor(point);
        return;
      }

      if (!extraMode) return;

      event.preventDefault();
      event.stopImmediatePropagation();

      if (extraMode === 'PICK') {
        pickColorAt(point);
        drawCursor(point);
        return;
      }

      if (extraMode === 'FILL') {
        if (floodFill(map, point.x, point.y, api.getActiveColor?.() ?? 0xFFFF)) {
          notifyChanged('fill');
        }
        drawCursor(point);
        return;
      }

      if (extraMode === 'LINE' || extraMode === 'RECT') {
        gesture = { pointerId: event.pointerId, start: point, current: point };
        try { displayCanvas.setPointerCapture(event.pointerId); } catch (_) { /* noop */ }
        drawCursor(point);
      }
    }

    function handlePointerMove(event) {
      if (event.target !== displayCanvas && !gesture) return;
      const point = pointFromEvent(displayCanvas, event);
      lastPointerPoint = point;
      if (gesture && event.pointerId === gesture.pointerId) {
        event.preventDefault();
        event.stopImmediatePropagation();
        gesture.current = point;
      }
      drawCursor(point);
    }

    function handlePointerUp(event) {
      if (!gesture || event.pointerId !== gesture.pointerId) return;
      event.preventDefault();
      event.stopImmediatePropagation();

      const map = selectedMap();
      const end = pointFromEvent(displayCanvas, event);
      const start = gesture.start;
      const mode = extraMode;
      gesture = null;
      try { displayCanvas.releasePointerCapture(event.pointerId); } catch (_) { /* noop */ }

      if (!map) {
        drawCursor(end);
        return;
      }

      const color = api.getActiveColor?.() ?? 0xFFFF;
      const size = api.getBrushSize?.() || 1;
      let changed = false;
      if (mode === 'LINE') changed = drawLine(map, start, end, color, size);
      if (mode === 'RECT') changed = drawRect(map, start, end, color, size);
      if (changed) notifyChanged(mode === 'LINE' ? 'line' : 'rect');
      drawCursor(end);
    }

    function handleKeydown(event) {
      if (isEditingTarget(event.target)) return;
      const map = selectedMap();
      if (!map) return;

      const ctrl = event.ctrlKey || event.metaKey;
      const key = event.key.toLowerCase();

      if (ctrl && key === 'z') {
        event.preventDefault();
        event.stopImmediatePropagation();
        if (event.shiftKey) redoPixel();
        else undoPixel();
        return;
      }
      if (ctrl && key === 'y') {
        event.preventDefault();
        event.stopImmediatePropagation();
        redoPixel();
        return;
      }
      if (ctrl || event.altKey || event.metaKey) return;

      const shortcuts = {
        b: () => activateNativeMode('DRAW'),
        e: () => activateNativeMode('ERASE'),
        v: () => activateNativeMode('MOVE'),
        g: () => activateExtraMode('FILL'),
        i: () => activateExtraMode('PICK'),
        l: () => activateExtraMode('LINE'),
        r: () => activateExtraMode('RECT')
      };
      if (shortcuts[key]) {
        event.preventDefault();
        event.stopImmediatePropagation();
        shortcuts[key]();
      }
    }

    function handleToolbarClick(event) {
      const target = event.target?.closest?.('button');
      if (!target || !selectedMap()) return;
      if (target === undoButton) {
        event.preventDefault();
        event.stopImmediatePropagation();
        undoPixel();
      } else if (target === redoButton) {
        event.preventDefault();
        event.stopImmediatePropagation();
        redoPixel();
      }
    }

    injectStyles();
    injectTools();
    resetHistory();

    window.addEventListener('pointerdown', handlePointerDown, true);
    window.addEventListener('pointermove', handlePointerMove, true);
    window.addEventListener('pointerup', handlePointerUp, true);
    window.addEventListener('pointercancel', handlePointerUp, true);
    window.addEventListener('keydown', handleKeydown, true);
    window.addEventListener('click', handleToolbarClick, true);

    displayCanvas.addEventListener('pointerleave', () => {
      if (!gesture) {
        lastPointerPoint = null;
        drawCursor(null);
      }
    });

    document.addEventListener('click', (event) => {
      const nativeButton = event.target?.closest?.('[data-pm-mode]');
      if (nativeButton) {
        nativeMode = nativeButton.dataset.pmMode || nativeMode;
        extraMode = null;
        updateToolButtons();
        drawCursor(lastPointerPoint);
        return;
      }
      if (event.target?.closest?.('.a11-pixel-row')) {
        nativeMode = 'MOVE';
        extraMode = null;
        updateToolButtons();
        requestAnimationFrame(() => {
          syncHistoryButtons();
          drawCursor(lastPointerPoint);
        });
      }
    });

    pixelComponent?.addEventListener('click', () => {
      nativeMode = 'DRAW';
      extraMode = null;
      requestAnimationFrame(() => {
        injectTools();
        updateToolButtons();
        syncHistoryButtons();
        drawCursor(lastPointerPoint);
      });
    });

    zoomSelect?.addEventListener('change', () => drawCursor(lastPointerPoint));
    document.getElementById('pixelBrushSize')?.addEventListener('change', () => drawCursor(lastPointerPoint));
    document.getElementById('pixelEraserSize')?.addEventListener('change', () => drawCursor(lastPointerPoint));

    window.addEventListener('jwplc:pixelmap-changed', scheduleHistoryCommit);
    window.addEventListener('jwplc:project-loaded', () => setTimeout(resetHistory, 0));
    window.addEventListener('jwplc:editor-refresh', () => {
      requestAnimationFrame(() => {
        injectTools();
        syncHistoryButtons();
        drawCursor(lastPointerPoint);
      });
    });

    const newProjectButton = document.getElementById('newProjectButton');
    newProjectButton?.addEventListener('click', () => setTimeout(resetHistory, 80));

    window.JWPLCHMIPixelWorkbench = {
      undo: undoPixel,
      redo: redoPixel,
      resetHistory,
      getMode: () => extraMode || nativeMode,
      setFillMode: () => activateExtraMode('FILL'),
      setPickerMode: () => activateExtraMode('PICK'),
      setLineMode: () => activateExtraMode('LINE'),
      setRectMode: () => activateExtraMode('RECT')
    };
  }

  init().catch((error) => console.error('[JWPLC HMI Pixel Workbench]', error));
})();
