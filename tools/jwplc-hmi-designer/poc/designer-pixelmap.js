(() => {
  'use strict';

  if (window.JWPLCHMIPixelMaps) return;

  const WIDTH = 320;
  const HEIGHT = 170;
  const MAX_PIXEL_MAPS = 16;
  const MAX_BRUSH_SIZE = 16;
  const DEFAULT_ONION_OPACITY = 0.35;
  const PRESETS = [
    ['BLACK', 0x0000], ['WHITE', 0xFFFF], ['RED', 0xF800],
    ['GREEN', 0x07E0], ['BLUE', 0x001F], ['CYAN', 0x07FF],
    ['MAGENTA', 0xF81F], ['YELLOW', 0xFFE0], ['ORANGE', 0xFD20]
  ];

  const displayCanvas = document.getElementById('displayCanvas');
  const previewCanvas = document.getElementById('previewCanvas');
  const geometryCanvas = document.getElementById('geometryCanvas');
  const objectList = document.getElementById('objectsList') || document.getElementById('textObjectItem')?.parentElement;
  const countBadge = document.querySelector('.count-badge');
  const fieldSection = document.getElementById('textFieldControlsSection');
  const rawSection = document.getElementById('rawTextControlsSection');
  const pixelSection = document.querySelector('.pixel-controls');
  const pixelTool = document.querySelector('.tool[data-tool="pixel"]');
  const eraseTool = document.querySelector('.tool[data-tool="erase"]');
  const rawTool = document.querySelector('.tool[data-tool="rawText"]');
  const zoomSelect = document.getElementById('zoomSelect');
  const gridToggle = document.getElementById('gridToggle');
  const geometryToggle = document.getElementById('geometryToggle');
  const codeOutput = document.getElementById('codeOutput');
  const contractTab = document.getElementById('contractTab');
  const generateButton = document.getElementById('generateButton');
  const newProjectButton = document.getElementById('newProjectButton');

  if (!displayCanvas || !previewCanvas || !objectList) return;

  let maps = [];
  let selectedKeys = [];
  let selectedKey = null;
  let serial = 0;
  let mode = 'DRAW';
  let color = 0xFD20;
  let brushSize = 1;
  let eraserSize = 1;
  let pointer = null;
  let lastPoint = null;
  let dragStart = null;
  let dragOrigin = null;
  let dragOrigins = [];
  let rendering = false;
  let codePatching = false;

  const editor = () => window.JWPLCHMIEditor || null;
  const page = () => Number(editor()?.getActivePage?.() || 0);
  const hex565 = (value) => `0x${(Number(value) & 0xFFFF).toString(16).toUpperCase().padStart(4, '0')}`;
  const inside = (p) => p.x >= 0 && p.x < WIDTH && p.y >= 0 && p.y < HEIGHT;
  const clamp = (value, min, max) => Math.min(max, Math.max(min, value));
  const currentZoom = () => editor()?.getZoom?.() || (displayCanvas.width / WIDTH) || 1;

  function parse565(value, fallback = 0) {
    let text = String(value ?? '').trim();
    if (!text) return fallback & 0xFFFF;
    text = text.replace(/^0x/i, '');
    if (!/^[0-9a-f]{1,4}$/i.test(text)) return fallback & 0xFFFF;
    return Number.parseInt(text, 16) & 0xFFFF;
  }

  function rgb565ToRgb(value) {
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

  function css565(value) {
    const c = rgb565ToRgb(value);
    return `rgb(${c.r}, ${c.g}, ${c.b})`;
  }

  function hex888(value) {
    const c = rgb565ToRgb(value);
    return `#${[c.r, c.g, c.b].map((v) => v.toString(16).padStart(2, '0')).join('')}`;
  }

  function hex888To565(value) {
    const match = /^#?([0-9a-f]{6})$/i.exec(String(value || ''));
    if (!match) return color;
    const raw = Number.parseInt(match[1], 16);
    const r = (raw >> 16) & 255;
    const g = (raw >> 8) & 255;
    const b = raw & 255;
    return (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)) & 0xFFFF;
  }

  function selected() {
    return maps.find((map) => map.key === selectedKey || selectedKeys.includes(map.key)) || null;
  }

  function selectedMaps() {
    return maps.filter((map) => selectedKeys.includes(map.key) || map.key === selectedKey);
  }

  function mapsForPage() {
    return maps.filter((map) => Number(map.page || 0) === page());
  }

  function ensureEditorMeta(map) {
    if (!map) return map;
    if (typeof map.editorVisible !== 'boolean') map.editorVisible = true;
    const opacity = Number(map.editorOpacity);
    map.editorOpacity = Number.isFinite(opacity) ? clamp(opacity, 0.1, 1) : 1;
    return map;
  }

  function bounds(map) {
    if (!map?.pixels?.length) return { x: map?.x || 0, y: map?.y || 0, width: 1, height: 1 };
    let maxX = 0;
    let maxY = 0;
    map.pixels.forEach((p) => {
      maxX = Math.max(maxX, Number(p.x) || 0);
      maxY = Math.max(maxY, Number(p.y) || 0);
    });
    return { x: map.x, y: map.y, width: maxX + 1, height: maxY + 1 };
  }

  function pointFromEvent(event) {
    const rect = displayCanvas.getBoundingClientRect();
    if (!rect.width || !rect.height) return { x: 0, y: 0 };
    return {
      x: Math.floor((event.clientX - rect.left) * (WIDTH / rect.width)),
      y: Math.floor((event.clientY - rect.top) * (HEIGHT / rect.height))
    };
  }

  function findPixel(map, localX, localY) {
    return map.pixels.findIndex((p) => p.x === localX && p.y === localY);
  }

  function canonicalize(map) {
    if (!map?.pixels?.length) return;
    let minX = Infinity;
    let minY = Infinity;
    map.pixels.forEach((p) => {
      minX = Math.min(minX, p.x);
      minY = Math.min(minY, p.y);
    });
    if (!Number.isFinite(minX) || !Number.isFinite(minY) || (minX === 0 && minY === 0)) return;
    map.x += minX;
    map.y += minY;
    map.pixels.forEach((p) => { p.x -= minX; p.y -= minY; });
  }

  function putPixel(map, gx, gy, value) {
    if (!map || gx < 0 || gx >= WIDTH || gy < 0 || gy >= HEIGHT) return false;
    if (!map.pixels.length) {
      map.x = gx;
      map.y = gy;
      map.pixels.push({ x: 0, y: 0, color: value & 0xFFFF });
      return true;
    }
    if (gx < map.x) {
      const shift = map.x - gx;
      map.pixels.forEach((p) => { p.x += shift; });
      map.x = gx;
    }
    if (gy < map.y) {
      const shift = map.y - gy;
      map.pixels.forEach((p) => { p.y += shift; });
      map.y = gy;
    }
    const lx = gx - map.x;
    const ly = gy - map.y;
    const index = findPixel(map, lx, ly);
    if (index >= 0) {
      if (map.pixels[index].color === (value & 0xFFFF)) return false;
      map.pixels[index].color = value & 0xFFFF;
      return true;
    }
    map.pixels.push({ x: lx, y: ly, color: value & 0xFFFF });
    return true;
  }

  function brushRange(size) {
    const normalized = clamp(Math.trunc(Number(size) || 1), 1, MAX_BRUSH_SIZE);
    const start = -Math.floor((normalized - 1) / 2);
    return { size: normalized, start, end: start + normalized - 1 };
  }

  function putBrush(map, gx, gy, value, size) {
    const range = brushRange(size);
    let changedAny = false;
    for (let oy = range.start; oy <= range.end; oy += 1) {
      for (let ox = range.start; ox <= range.end; ox += 1) {
        changedAny = putPixel(map, gx + ox, gy + oy, value) || changedAny;
      }
    }
    return changedAny;
  }

  function eraseBrush(map, gx, gy, size) {
    if (!map?.pixels?.length) return false;
    const range = brushRange(size);
    const minX = gx + range.start;
    const maxX = gx + range.end;
    const minY = gy + range.start;
    const maxY = gy + range.end;
    const before = map.pixels.length;
    map.pixels = map.pixels.filter((pixel) => {
      const px = map.x + pixel.x;
      const py = map.y + pixel.y;
      return px < minX || px > maxX || py < minY || py > maxY;
    });
    if (map.pixels.length === before) return false;
    canonicalize(map);
    return true;
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

  function clearFieldSelection() {
    if (editor()?.getSelectedFieldKeys?.()?.length > 0) {
      editor()?.setSelectedFieldKeys?.([]);
    }
  }

  function createMap() {
    if (maps.length >= MAX_PIXEL_MAPS) return null;
    clearFieldSelection();
    serial += 1;
    const map = {
      type: 'PIXELMAP',
      key: `pixelmap-${serial}`,
      name: `Pixel ${serial}`,
      page: page(),
      x: 20,
      y: 20,
      editorVisible: true,
      editorOpacity: 1,
      pixels: []
    };
    editor()?.assignNewLayerOrder?.(map);
    maps.push(map);
    selectedKey = map.key;
    selectedKeys = [map.key];
    mode = 'DRAW';
    refreshBase();
    changed();
    return map;
  }

  function editableOnPage(m) {
    return m && !m.locked && m.editorVisible !== false && Number(m.page || 0) === page();
  }

  // Capa que recibe un relleno: la que rodea la región (el trazo cerrado que
  // se rellena), para que trazo y relleno sigan siendo un solo objeto; si
  // ninguna capa la toca, la seleccionada, una vacía o una nueva.
  function fillTargetMap(points) {
    const owner = new Map();
    mapsForPage().filter(editableOnPage).forEach((m) => {
      m.pixels.forEach((p) => owner.set((m.y + p.y) * WIDTH + (m.x + p.x), m));
    });
    const region = new Set(points.map((pt) => pt.y * WIDTH + pt.x));
    const votes = new Map();
    points.forEach((pt) => {
      [[1, 0], [-1, 0], [0, 1], [0, -1]].forEach(([dx, dy]) => {
        const x = pt.x + dx;
        const y = pt.y + dy;
        if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return;
        const k = y * WIDTH + x;
        if (region.has(k)) return;
        const m = owner.get(k);
        if (m) votes.set(m, (votes.get(m) || 0) + 1);
      });
    });
    let best = null;
    votes.forEach((count, m) => { if (!best || count > votes.get(best)) best = m; });
    if (best) return best;
    const current = selected();
    if (editableOnPage(current)) return current;
    return mapsForPage().find((m) => !m.locked && (!m.pixels || m.pixels.length === 0)) || createMap();
  }

  // Herramienta Relleno de app.js: agrega en bloque los puntos (coordenadas de
  // pantalla) a una capa del color pedido. Devuelve la capa o null.
  function paintPixels(points, value) {
    value = Number(value) & 0xFFFF;
    const map = fillTargetMap(points);
    if (!map) return null;
    const all = new Map();
    map.pixels.forEach((p) => all.set((map.y + p.y) * WIDTH + (map.x + p.x), p.color & 0xFFFF));
    points.forEach((pt) => {
      if (pt.x >= 0 && pt.x < WIDTH && pt.y >= 0 && pt.y < HEIGHT) all.set(pt.y * WIDTH + pt.x, value);
    });
    if (!all.size) return null;
    let minX = Infinity;
    let minY = Infinity;
    all.forEach((_, k) => {
      minX = Math.min(minX, k % WIDTH);
      minY = Math.min(minY, Math.floor(k / WIDTH));
    });
    map.x = minX;
    map.y = minY;
    map.pixels = [...all].map(([k, c]) => ({ x: (k % WIDTH) - minX, y: Math.floor(k / WIDTH) - minY, color: c }));
    selectedKey = map.key;
    selectedKeys = [map.key];
    color = value;
    updatePixelSwatch(color);
    changed();
    refreshBase();
    return map;
  }

  // "Usar en Pincel" del cuentagotas: color del pincel, sin tocar los píxeles.
  function prepareBrush(value) {
    value = Number(value) & 0xFFFF;
    const map = editableOnPage(selected()) ? selected() : ensureActiveMap(true);
    if (!map) return null;
    color = value;
    updatePixelSwatch(color);
    pixelColorEditor?.set(color, 'Píxeles');
    return map;
  }

  function duplicateMap() {
    const source = selected();
    if (!source || maps.length >= MAX_PIXEL_MAPS) return;
    serial += 1;
    const copy = {
      ...source,
      key: `pixelmap-${serial}`,
      name: (editor()?.nextSequentialName?.(source.name) || `${source.name} 2`).slice(0, 24),
      x: Math.min(WIDTH - 1, source.x + 4),
      y: Math.min(HEIGHT - 1, source.y + 4),
      editorVisible: true,
      editorOpacity: 1,
      pixels: source.pixels.map((p) => ({ ...p }))
    };
    delete copy.layerOrder;
    editor()?.assignNewLayerOrder?.(copy);
    const b = bounds(copy);
    copy.x = Math.min(copy.x, Math.max(0, WIDTH - b.width));
    copy.y = Math.min(copy.y, Math.max(0, HEIGHT - b.height));
    maps.push(copy);
    selectedKey = copy.key;
    selectedKeys = [copy.key];
    refreshBase();
    changed();
  }

  function deleteMap() {
    const toDelete = selectedKeys.length > 0 ? selectedKeys : (selectedKey ? [selectedKey] : []);
    if (toDelete.length === 0) return;
    maps = maps.filter((item) => !toDelete.includes(item.key));
    selectedKey = null;
    selectedKeys = [];
    refreshBase();
    changed();
  }

  function hitMap(point, tolerance = 2) {
    const visible = mapsForPage()
      .filter((map) => ensureEditorMeta(map).editorVisible && !map.locked)
      .sort((a, b) => (Number(a.layerOrder) || 0) - (Number(b.layerOrder) || 0));
    const topFieldAtPoint = editor()?.getPixelFieldOrder?.(point.x, point.y) ?? Infinity;
    for (let i = 0; i < visible.length; i += 1) {
      const map = visible[i];
      if (!map.pixels?.length) continue;
      if (topFieldAtPoint < (Number(map.layerOrder) || Infinity)) continue;
      const b = bounds(map);
      if (
        point.x < b.x - tolerance ||
        point.x > b.x + b.width + tolerance ||
        point.y < b.y - tolerance ||
        point.y > b.y + b.height + tolerance
      ) {
        continue;
      }

      const nearPixel = map.pixels.some((p) => {
        const px = map.x + p.x;
        const py = map.y + p.y;
        return Math.abs(px - point.x) <= tolerance && Math.abs(py - point.y) <= tolerance;
      });
      if (nearPixel) return map;
    }
    return null;
  }

  function organizeLeftPanel() {
    // Preserved for toolbar architecture
  }

  function injectStyles() {
    if (document.getElementById('a11-pixelmap-style')) return;
    const style = document.createElement('style');
    style.id = 'a11-pixelmap-style';
    style.textContent = `
      .a11-pixel-row .object-icon{color:#ff9a43;font-weight:800}
      .a11-pixel-row.active{border-color:#ff8a2a!important;background:rgba(255,138,42,.10)!important}
      .a11-pixel-row[data-hidden="1"]{opacity:.62}
      .a11-color-control{display:grid;grid-template-columns:34px 1fr 86px;gap:6px;align-items:center}
      .a11-color-control input[type=color]{width:34px;height:30px;padding:1px;border:1px solid #425764;border-radius:4px;background:#13212a}
      .a11-color-control input[type=text]{min-width:0}
      .a11-color-presets{display:flex;flex-wrap:wrap;gap:5px;margin-top:7px}
      .a11-color-chip{width:22px;height:22px;border:1px solid #5b6f7b;border-radius:4px;padding:0;cursor:pointer}
      .a11-pixel-actions{display:grid;grid-template-columns:repeat(3,1fr);gap:5px}
      .a11-pixel-actions button.active,.a11-pixel-layer-actions button.active{border-color:#ff8a2a;color:#ffc48f;background:rgba(255,138,42,.12)}
      .a11-pixel-size-grid{display:grid;grid-template-columns:1fr 1fr;gap:7px;margin-top:8px}
      .a11-pixel-layer-actions{display:grid;grid-template-columns:1fr 1fr;gap:6px}
      .a11-opacity-row{display:grid;grid-template-columns:1fr 50px;gap:8px;align-items:center;margin-top:8px}
      .a11-opacity-row input[type=range]{width:100%}
      .a11-opacity-value{text-align:right;font-variant-numeric:tabular-nums;color:#cbd9e1}
      .a11-pixel-note{font-size:10px;line-height:1.35;color:#92a7b3;margin:7px 0 0}
      .a11-custom-color{display:grid;gap:5px;margin-top:6px}
      .a11-custom-color .a11-color-control{grid-template-columns:30px 1fr}
      .a11-custom-color .a11-color-control input[type=text]{display:none}
    `;
    document.head.appendChild(style);
  }

  function makeColorControl(root, getValue, setValue, compact = false) {
    if (!root) return () => {};
    root.innerHTML = '';
    root.classList.toggle('a11-custom-color', compact);
    const row = document.createElement('div');
    row.className = 'a11-color-control';
    const visual = document.createElement('input');
    visual.type = 'color';
    visual.title = 'Selector visual; se cuantiza al RGB565 más cercano';
    const hex = document.createElement('input');
    hex.type = 'text';
    hex.className = 'field-input code-input';
    hex.maxLength = 6;
    hex.spellcheck = false;
    const swatch = document.createElement('span');
    swatch.style.cssText = 'height:28px;border:1px solid #526875;border-radius:4px';
    row.append(visual, hex, swatch);
    root.appendChild(row);

    if (!compact) {
      const presets = document.createElement('div');
      presets.className = 'a11-color-presets';
      PRESETS.forEach(([name, value]) => {
        const button = document.createElement('button');
        button.type = 'button';
        button.className = 'a11-color-chip';
        button.title = `${name} · ${hex565(value)}`;
        button.style.background = css565(value);
        button.addEventListener('click', () => { setValue(value); sync(); changed(); });
        presets.appendChild(button);
      });
      root.appendChild(presets);
    }

    function sync() {
      const value = Number(getValue()) & 0xFFFF;
      visual.value = hex888(value);
      hex.value = hex565(value);
      swatch.style.background = css565(value);
      swatch.title = `${hex565(value)} · ${visual.value.toUpperCase()}`;
    }

    visual.addEventListener('input', () => { setValue(hex888To565(visual.value)); sync(); });
    visual.addEventListener('change', () => { setValue(hex888To565(visual.value)); sync(); changed(); });
    hex.addEventListener('change', () => { setValue(parse565(hex.value, getValue())); sync(); changed(); });
    sync();
    return sync;
  }

  let inspector = null;
  let mapName = null;
  let mapX = null;
  let mapY = null;
  let mapPage = null;
  let pixelCount = null;
  let pixelBounds = null;
  let brushSizeInput = null;
  let eraserSizeInput = null;
  let visibilityButton = null;
  let onionButton = null;
  let opacityInput = null;
  let opacityValue = null;
  let pixelActiveMainTab = 'design';
  let pixelActiveSubTab = 'edit';
  let pixelColorEditor = null;
  let syncMainColor = () => {
    ensureColorEditor();
    if (pixelColorEditor) pixelColorEditor.set(color, 'Píxeles');
    updatePixelSwatch(color);
  };

  function sizeOptions() {
    return Array.from({ length: MAX_BRUSH_SIZE }, (_, index) => `<option value="${index + 1}">${index + 1} × ${index + 1} px</option>`).join('');
  }

  let isEnsuringMap = false;
  function ensureActiveMap(createIfNone = true) {
    if (isEnsuringMap) return selected();
    isEnsuringMap = true;
    try {
      let map = selected();
      if (!map || Number(map.page || 0) !== page()) {
        const pageMaps = mapsForPage();
        const emptyMap = pageMaps.find((m) => !m.pixels || m.pixels.length === 0);
        if (emptyMap) {
          selectedKey = emptyMap.key;
          map = emptyMap;
        } else if (createIfNone) {
          map = createMap();
        } else if (pageMaps.length > 0) {
          selectedKey = pageMaps[pageMaps.length - 1].key;
          map = selected();
        }
      }
      return map;
    } finally {
      isEnsuringMap = false;
    }
  }

  function ensureColorEditor() {
    if (pixelColorEditor || !window.JWPLCColorEditor) return;
    const host = document.getElementById('pixelMapColorEditorHost');
    if (!host) return;
    pixelColorEditor = window.JWPLCColorEditor.create({
      paletteAttr: 'data-pixel-palette',
      onInput: (v565) => setPixelColor(v565, false),
      onCommit: (v565) => setPixelColor(v565, true)
    });
    host.appendChild(pixelColorEditor.el);
    pixelColorEditor.set(color, 'Píxeles');
    updatePixelSwatch(color);
  }

  // Color del pincel. Una capa PIXEL puede tener varios colores (el codegen
  // emite un run por color), así que elegir color no recolorea la capa: eso
  // lo hace el botón "Recolorear capa" (recolorSelected).
  function setPixelColor(v565) {
    color = v565 & 0xFFFF;
    updatePixelSwatch(color);
  }

  function recolorSelected() {
    const map = selected();
    if (!map || map.locked || !map.pixels?.length) return;
    map.pixels.forEach((p) => { p.color = color; });
    render();
    refreshBase();
    changed();
  }

  function updatePixelSwatch(v565) {
    const swatch = document.getElementById('pixelMapActiveSwatch');
    if (swatch) {
      swatch.style.background = css565(v565);
      const isLight = ((v565 >> 11) + ((v565 >> 5) & 0x3F) / 2 + (v565 & 0x1F)) > 40;
      swatch.style.color = isLight ? '#000000' : '#ffffff';
    }
  }

  function syncBrushUI() {
    const hint = document.getElementById('pixelBrushSizeHint');
    if (hint) hint.textContent = `· ${brushSize} × ${brushSize} px`;
    const readout = document.getElementById('pixelBrushSizeReadout');
    if (readout) readout.textContent = `${brushSize} × ${brushSize} px`;
    document.querySelectorAll('#pixelBrushChips [data-brush-chip]').forEach((btn) => {
      btn.classList.toggle('active', Number(btn.dataset.brushChip) === brushSize);
    });
    if (brushSizeInput && brushSizeInput.value !== String(brushSize)) {
      brushSizeInput.value = String(brushSize);
    }
  }

  function setBrushSize(size) {
    brushSize = clamp(Number(size) || 1, 1, MAX_BRUSH_SIZE);
    syncBrushUI();
    if (brushSizeInput) {
      brushSizeInput.value = String(brushSize);
      brushSizeInput.dispatchEvent(new Event('change', { bubbles: true }));
    }
  }

  function syncEraserUI() {
    const hint = document.getElementById('pixelEraserSizeHint');
    if (hint) hint.textContent = `· ${eraserSize} × ${eraserSize} px`;
    const readout = document.getElementById('pixelEraserSizeReadout');
    if (readout) readout.textContent = `${eraserSize} × ${eraserSize} px`;
    document.querySelectorAll('#pixelEraserChips [data-eraser-chip]').forEach((btn) => {
      btn.classList.toggle('active', Number(btn.dataset.eraserChip) === eraserSize);
    });
    if (eraserSizeInput && eraserSizeInput.value !== String(eraserSize)) {
      eraserSizeInput.value = String(eraserSize);
    }
  }

  function setEraserSize(size) {
    eraserSize = clamp(Number(size) || 1, 1, MAX_BRUSH_SIZE);
    syncEraserUI();
    if (eraserSizeInput) {
      eraserSizeInput.value = String(eraserSize);
      eraserSizeInput.dispatchEvent(new Event('change', { bubbles: true }));
    }
    if (mainEraserSizeSelect) mainEraserSizeSelect.value = String(eraserSize);
  }

  function syncPixelTabPanes() {
    if (!inspector) return;
    const isDesign = pixelActiveMainTab === 'design';
    inspector.querySelectorAll('#pixelMainTabs [data-pixel-tab]').forEach((btn) => {
      btn.classList.toggle('active', btn.dataset.pixelTab === pixelActiveMainTab);
    });
    const subTabsBar = document.getElementById('pixelSubTabs');
    if (subTabsBar) subTabsBar.style.display = isDesign ? 'flex' : 'none';

    inspector.querySelectorAll('#pixelSubTabs [data-pixel-subtab]').forEach((btn) => {
      btn.classList.toggle('active', isDesign && btn.dataset.pixelSubtab === pixelActiveSubTab);
    });

    const activePanelKey = isDesign ? `design:${pixelActiveSubTab}` : pixelActiveMainTab;
    inspector.querySelectorAll('[data-pixel-panel]').forEach((panel) => {
      panel.hidden = panel.dataset.pixelPanel !== activePanelKey;
    });

    if (isDesign && pixelActiveSubTab === 'appearance') {
      ensureColorEditor();
      if (pixelColorEditor) pixelColorEditor.set(color, 'Píxeles');
      updatePixelSwatch(color);
    }
  }

  function syncPixelPages(currentMap) {
    const host = document.getElementById('pixelPageChipRow');
    if (!host) return;
    const curPage = Number(currentMap?.page ?? page());
    const pages = editor()?.getPages?.() || [{ id: 0, name: 'Principal' }];
    const signature = pages.map((p) => `${p.id}:${p.name}`).join('|') + `#${curPage}`;
    if (host.dataset.signature === signature) return;
    host.dataset.signature = signature;
    host.innerHTML = pages.map((p) => {
      const active = Number(p.id) === curPage;
      const title = p.name ? `Página ${p.id}: ${p.name}` : `Página ${p.id}`;
      return `<button type="button" class="insp-chip${active ? ' active' : ''}" data-pixel-page-id="${p.id}" title="${title}">${String(Number(p.id) + 1).padStart(2, '0')} · ${p.name || ('Página ' + (Number(p.id) + 1))}</button>`;
    }).join('');

    host.querySelectorAll('[data-pixel-page-id]').forEach((btn) => {
      btn.addEventListener('click', () => {
        const targetPage = Number(btn.dataset.pixelPageId);
        const map = selected();
        if (map && Number(map.page || 0) !== targetPage) {
          map.page = targetPage;
          if (mapPage) mapPage.value = String(targetPage);
          host.dataset.signature = '';
          changed();
          refreshBase();
          syncPixelPages(map);
        }
      });
    });
  }

  function getPixelSnippet(map) {
    if (!map) return '// Sin objeto seleccionado';
    const b = bounds(map);
    const count = (map.pixels || []).length;
    const token = cppToken(map.name, 'PIXEL');
    const p = Number(map.page || 0);
    return `// Capa: ${map.name} (${count} píxeles, ${b.width} × ${b.height} px, Pág. ${p})\n` +
      `// Dibuja en jwplcUIPageEnter() al entrar a la página:\n` +
      `// Formato: RGB565 / PACKED_SPAN16 optimizado en Flash\n` +
      `JWPLC_Display.setPixelMapVisible(${token}, ${map.editorVisible !== false ? 'true' : 'false'});`;
  }

  function injectInspector() {
    inspector = document.getElementById('pixelMapControlsSection');
    if (!inspector) {
      inspector = document.createElement('section');
      inspector.id = 'pixelMapControlsSection';
      inspector.className = 'inspector-section bar-inspector-wrap';
      inspector.hidden = true;
      rawSection?.insertAdjacentElement('afterend', inspector);
      if (!inspector.isConnected) fieldSection?.insertAdjacentElement('afterend', inspector);
    }

    mapName = document.getElementById('pixelMapName');
    mapX = document.getElementById('pixelMapX');
    mapY = document.getElementById('pixelMapY');
    mapPage = document.getElementById('pixelMapPage');
    pixelCount = document.getElementById('pixelMapCount');
    pixelBounds = document.getElementById('pixelMapBounds');
    brushSizeInput = document.getElementById('pixelBrushSize');
    eraserSizeInput = document.getElementById('pixelEraserSize');
    visibilityButton = document.getElementById('pixelMapVisibility');
    document.getElementById('pixelMapRecolor')?.addEventListener('click', () => recolorSelected());
    onionButton = document.getElementById('pixelMapOnion');
    opacityInput = document.getElementById('pixelMapOpacity');
    opacityValue = document.getElementById('pixelMapOpacityValue');

    if (brushSizeInput && !brushSizeInput.children.length) {
      brushSizeInput.innerHTML = sizeOptions();
    }
    if (eraserSizeInput && !eraserSizeInput.children.length) {
      eraserSizeInput.innerHTML = sizeOptions();
    }

    // Pestañas principales y subpestañas
    inspector.querySelectorAll('#pixelMainTabs [data-pixel-tab]').forEach((btn) => {
      btn.addEventListener('click', () => {
        pixelActiveMainTab = btn.dataset.pixelTab;
        syncPixelTabPanes();
      });
    });
    inspector.querySelectorAll('#pixelSubTabs [data-pixel-subtab]').forEach((btn) => {
      btn.addEventListener('click', () => {
        pixelActiveSubTab = btn.dataset.pixelSubtab;
        syncPixelTabPanes();
      });
    });

    // Modos de dibujo
    inspector.querySelectorAll('[data-pm-mode]').forEach((button) => button.addEventListener('click', () => {
      const nextMode = button.dataset.pmMode;
      const currentTool = editor()?.getSelectedTool?.() || 'pointer';
      // El editor no expone setSelectedTool: se cambia la herramienta con su
      // botón de la barra, igual que las teclas B / E.
      if (nextMode === 'DRAW' && currentTool !== 'pixel') {
        pixelTool?.click();
      } else if (nextMode === 'ERASE' && currentTool !== 'erase') {
        eraseTool?.click();
      }
      mode = nextMode;
      syncTools();
      syncInspector();
      render();
    }));

    // Tamaño de pincel y borrador (stepper y chips)
    document.getElementById('pixelBrushStepDec')?.addEventListener('click', () => setBrushSize(brushSize - 1));
    document.getElementById('pixelBrushStepInc')?.addEventListener('click', () => setBrushSize(brushSize + 1));
    inspector.querySelectorAll('#pixelBrushChips [data-brush-chip]').forEach((btn) => {
      btn.addEventListener('click', () => setBrushSize(Number(btn.dataset.brushChip) || 1));
    });

    document.getElementById('pixelEraserStepDec')?.addEventListener('click', () => setEraserSize(eraserSize - 1));
    document.getElementById('pixelEraserStepInc')?.addEventListener('click', () => setEraserSize(eraserSize + 1));
    inspector.querySelectorAll('#pixelEraserChips [data-eraser-chip]').forEach((btn) => {
      btn.addEventListener('click', () => setEraserSize(Number(btn.dataset.eraserChip) || 1));
    });

    brushSizeInput?.addEventListener('change', () => {
      brushSize = clamp(Number(brushSizeInput.value) || 1, 1, MAX_BRUSH_SIZE);
      syncBrushUI();
    });
    eraserSizeInput?.addEventListener('change', () => {
      eraserSize = clamp(Number(eraserSizeInput.value) || 1, 1, MAX_BRUSH_SIZE);
      syncEraserUI();
      if (mainEraserSizeSelect) mainEraserSizeSelect.value = String(eraserSize);
    });

    // Editor de color de la casa
    ensureColorEditor();

    // Visibilidad y referencia
    visibilityButton?.addEventListener('click', () => {
      const map = selected();
      if (!map) return;
      ensureEditorMeta(map);
      map.editorVisible = !map.editorVisible;
      syncInspector();
      refreshBase();
      changed();
    });

    onionButton?.addEventListener('click', () => {
      const map = selected();
      if (!map) return;
      ensureEditorMeta(map);
      map.editorOpacity = map.editorOpacity >= 0.99 ? DEFAULT_ONION_OPACITY : 1;
      syncInspector();
      refreshBase();
      changed();
    });

    opacityInput?.addEventListener('input', () => {
      const map = selected();
      if (!map) return;
      ensureEditorMeta(map);
      map.editorOpacity = clamp((Number(opacityInput.value) || 100) / 100, 0.1, 1);
      syncInspector();
      refreshBase();
    });
    opacityInput?.addEventListener('change', changed);

    // Acciones de objeto
    document.getElementById('pixelMapDuplicate')?.addEventListener('click', duplicateMap);
    document.getElementById('pixelMapDelete')?.addEventListener('click', deleteMap);

    // Copiar código C++
    const copyContractBtn = document.getElementById('copyPixelContractBtn');
    copyContractBtn?.addEventListener('click', () => {
      const contractPre = document.getElementById('pixelInspectorContract');
      if (!contractPre || !contractPre.textContent) return;
      navigator.clipboard?.writeText?.(contractPre.textContent).then(() => {
        const orig = copyContractBtn.textContent;
        copyContractBtn.textContent = '✓';
        setTimeout(() => { copyContractBtn.textContent = orig; }, 1200);
      }).catch(() => {});
    });

    // Alineación rápida
    inspector.querySelectorAll('[data-pixel-align]').forEach((btn) => {
      btn.addEventListener('click', () => {
        const map = selected();
        if (!map) return;
        const b = bounds(map);
        const align = btn.dataset.pixelAlign;
        const hCenterX = Math.max(0, Math.round((WIDTH - b.width) / 2));
        const vCenterY = Math.max(0, Math.round((HEIGHT - b.height) / 2));
        const rightX = Math.max(0, WIDTH - b.width);
        const bottomY = Math.max(0, HEIGHT - b.height);

        if (align === 'top-left') { map.x = 0; map.y = 0; }
        else if (align === 'top') { map.x = hCenterX; map.y = 0; }
        else if (align === 'top-right') { map.x = rightX; map.y = 0; }
        else if (align === 'left') { map.x = 0; map.y = vCenterY; }
        else if (align === 'center') { map.x = hCenterX; map.y = vCenterY; }
        else if (align === 'right') { map.x = rightX; map.y = vCenterY; }
        else if (align === 'bottom-left') { map.x = 0; map.y = bottomY; }
        else if (align === 'bottom') { map.x = hCenterX; map.y = bottomY; }
        else if (align === 'bottom-right') { map.x = rightX; map.y = bottomY; }
        else if (align === 'hcenter') { map.x = hCenterX; }
        else if (align === 'vcenter') { map.y = vCenterY; }
        if (mapX) mapX.value = String(map.x);
        if (mapY) mapY.value = String(map.y);
        changed();
        refreshBase();
        syncInspector();
      });
    });

    mapName?.addEventListener('change', () => {
      const map = selected();
      if (!map) return;
      map.name = mapName.value.trim().slice(0, 24) || map.name;
      changed();
      refreshBase();
      syncInspector();
    });

    [mapX, mapY].forEach((input) => input?.addEventListener('change', () => {
      const map = selected();
      if (!map) return;
      const b = bounds(map);
      map.x = Math.max(0, Math.min(WIDTH - b.width, Number(mapX.value) || 0));
      map.y = Math.max(0, Math.min(HEIGHT - b.height, Number(mapY.value) || 0));
      changed();
      refreshBase();
      syncInspector();
    }));

    syncBrushUI();
    syncEraserUI();
    syncPixelTabPanes();
  }

  function syncInspector() {
    let map = selected();
    const currentTool = editor()?.getSelectedTool?.() || 'pointer';
    const isPixelTool = (currentTool === 'pixel' || currentTool === 'erase');

    if (!map && isPixelTool) {
      const pageMaps = mapsForPage();
      if (pageMaps.length > 0) {
        selectedKey = pageMaps[pageMaps.length - 1].key;
        map = selected();
      }
    }

    const hasFields = Boolean(editor()?.hasFieldSelection?.());
    if (fieldSection) fieldSection.hidden = (!hasFields && Boolean(map)) || fieldSection.hidden;
    if (rawSection && map && !hasFields) rawSection.hidden = true;
    if (!inspector) return;
    // Relleno y cuentagotas tienen su propio inspector (app.js).
    const isColorTool = currentTool === 'fill' || currentTool === 'pick';
    inspector.hidden = hasFields || !map || isColorTool;
    if (!map || hasFields || isColorTool) return;

    ensureEditorMeta(map);
    const b = bounds(map);
    const displayName = map.name || 'PIXEL';
    const headerTitle = document.getElementById('pixelMapHeaderTitle');
    if (headerTitle) headerTitle.textContent = displayName.toUpperCase();

    if (mapName && document.activeElement !== mapName) mapName.value = map.name;
    if (mapX && document.activeElement !== mapX) mapX.value = String(map.x);
    if (mapY && document.activeElement !== mapY) mapY.value = String(map.y);
    if (mapPage) mapPage.value = String(Number(map.page || 0));
    if (pixelCount) pixelCount.textContent = String(map.pixels.length);
    if (pixelBounds) pixelBounds.textContent = `${b.width} × ${b.height} px`;

    // Botones de modo
    inspector.querySelectorAll('[data-pm-mode]').forEach((btn) => {
      btn.classList.toggle('active', btn.dataset.pmMode === mode);
    });

    // Tamaños
    syncBrushUI();
    syncEraserUI();

    // Visibilidad y referencia
    const isVisible = map.editorVisible !== false;
    const visText = document.getElementById('pixelMapVisibilityText');
    if (visText) visText.textContent = isVisible ? 'Ocultar' : 'Mostrar';
    if (visibilityButton) visibilityButton.classList.toggle('active', !isVisible);

    const isOnion = map.editorOpacity < 0.99;
    const onionText = document.getElementById('pixelMapOnionText');
    if (onionText) onionText.textContent = isOnion ? 'Opacidad 100%' : 'Referencia 35%';
    if (onionButton) onionButton.classList.toggle('active', isOnion);

    const opPct = Math.round(map.editorOpacity * 100);
    if (opacityInput && document.activeElement !== opacityInput) opacityInput.value = String(opPct);
    if (opacityValue) opacityValue.textContent = `${opPct}%`;

    // Sincronizar celda activa de la matriz 3x3
    const hCenterX = Math.max(0, Math.round((WIDTH - b.width) / 2));
    const vCenterY = Math.max(0, Math.round((HEIGHT - b.height) / 2));
    const rightX = Math.max(0, WIDTH - b.width);
    const bottomY = Math.max(0, HEIGHT - b.height);
    const mx = map.x;
    const my = map.y;

    let activeAlign = null;
    if (mx === 0 && my === 0) activeAlign = 'top-left';
    else if (mx === hCenterX && my === 0) activeAlign = 'top';
    else if (mx === rightX && my === 0) activeAlign = 'top-right';
    else if (mx === 0 && my === vCenterY) activeAlign = 'left';
    else if (mx === hCenterX && my === vCenterY) activeAlign = 'center';
    else if (mx === rightX && my === vCenterY) activeAlign = 'right';
    else if (mx === 0 && my === bottomY) activeAlign = 'bottom-left';
    else if (mx === hCenterX && my === bottomY) activeAlign = 'bottom';
    else if (mx === rightX && my === bottomY) activeAlign = 'bottom-right';

    inspector.querySelectorAll('[data-pixel-align]').forEach((btn) => {
      btn.classList.toggle('active', btn.dataset.pixelAlign === activeAlign);
    });

    // Editor de color y muestra
    ensureColorEditor();
    if (pixelColorEditor) pixelColorEditor.set(color, 'Píxeles');
    updatePixelSwatch(color);

    // Pestañas de página
    syncPixelPages(map);

    // C++ Token y Contrato
    const token = cppToken(map.name, 'PIXEL');
    const pixelMapId = document.getElementById('pixelMapId');
    if (pixelMapId) pixelMapId.value = `HMI_PIXEL_${token}`;

    const contractPre = document.getElementById('pixelInspectorContract');
    if (contractPre) {
      contractPre.textContent = getPixelSnippet(map);
    }

    // Diagnósticos
    const diagLayers = document.getElementById('pixelDiagLayers');
    if (diagLayers) diagLayers.textContent = String(mapsForPage().length);
    const diagOrder = document.getElementById('pixelDiagLayerOrder');
    if (diagOrder) diagOrder.textContent = String(map.layerOrder ?? 0);
    const diagLocked = document.getElementById('pixelDiagLocked');
    if (diagLocked) diagLocked.textContent = map.locked ? 'Sí' : 'No';
    const diagVisible = document.getElementById('pixelDiagVisible');
    if (diagVisible) diagVisible.textContent = isVisible ? 'Sí' : 'No';

    syncPixelTabPanes();
  }

  const SVG_EYE_OPEN = `<svg width="13" height="13" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"/><circle cx="12" cy="12" r="3"/></svg>`;
  const SVG_EYE_OFF = `<svg width="13" height="13" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M17.94 17.94A10.07 10.07 0 0 1 12 20c-7 0-11-8-11-8a18.45 18.45 0 0 1 5.06-5.94M9.9 4.24A9.12 9.12 0 0 1 12 4c7 0 11 8 11 8a18.5 18.5 0 0 1-2.16 3.19m-6.72-1.07a3 3 0 1 1-4.24-4.24"/><line x1="1" y1="1" x2="23" y2="23"/></svg>`;
  const SVG_LOCK_CLOSED = `<svg width="13" height="13" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><rect x="3" y="11" width="18" height="11" rx="2" ry="2"/><path d="M7 11V7a5 5 0 0 1 10 0v4"/></svg>`;
  const SVG_LOCK_OPEN = `<svg width="13" height="13" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><rect x="3" y="11" width="18" height="11" rx="2" ry="2"/><path d="M7 11V7a5 5 0 0 1 9.9-1"/></svg>`;

  function reorderPageMaps(sourceKey, targetKey, insertAbove) {
    if (!sourceKey || !targetKey || sourceKey === targetKey) return;
    if (editor()?.reorderPageObjects) {
      editor().reorderPageObjects(sourceKey, targetKey, insertAbove);
      changed();
      return;
    }
    const activePage = page();
    const pageMaps = mapsForPage();
    const sourceIdx = pageMaps.findIndex((m) => m.key === sourceKey);
    const targetIdx = pageMaps.findIndex((m) => m.key === targetKey);
    if (sourceIdx === -1 || targetIdx === -1 || sourceIdx === targetIdx) return;

    const [moved] = pageMaps.splice(sourceIdx, 1);
    let newTargetIdx = pageMaps.findIndex((m) => m.key === targetKey);
    if (!insertAbove) newTargetIdx += 1;
    pageMaps.splice(newTargetIdx, 0, moved);

    const otherMaps = maps.filter((m) => Number(m.page || 0) !== activePage);
    maps = [...pageMaps, ...otherMaps];

    refreshBase();
    changed();
  }

  function patchObjectList() {
    objectList.querySelectorAll('.a11-pixel-row').forEach((node) => node.remove());
    mapsForPage().forEach((map) => {
      ensureEditorMeta(map);
      editor()?.ensureLayerOrder?.(map);
      const row = document.createElement('div');
      const isAct = selectedKeys.includes(map.key) || map.key === selectedKey;
      row.className = `object-item a11-pixel-row${isAct ? ' active' : ''}${!map.editorVisible ? ' is-hidden' : ''}${map.locked ? ' is-locked' : ''}`;
      row.dataset.pixelKey = map.key;
      row.dataset.layerOrder = String(map.layerOrder || 0);
      row.innerHTML = `
        <span class="object-icon">▦</span>
        <span class="object-type" style="display:none">PIXEL</span>
        <span class="object-name" title="${map.name}">${map.name}</span>
        <div class="object-actions">
          <button type="button" class="object-btn btn-eye" title="${map.editorVisible ? 'Ocultar' : 'Mostrar'}">${map.editorVisible ? SVG_EYE_OPEN : SVG_EYE_OFF}</button>
          <button type="button" class="object-btn btn-lock" title="${map.locked ? 'Desbloquear' : 'Bloquear'}">${map.locked ? SVG_LOCK_CLOSED : SVG_LOCK_OPEN}</button>
        </div>
      `;

      row.draggable = true;
      row.addEventListener('dragstart', (e) => {
        e.dataTransfer.setData('text/plain', map.key);
        e.dataTransfer.effectAllowed = 'move';
        row.classList.add('is-dragging');
      });

      row.addEventListener('dragend', () => {
        row.classList.remove('is-dragging');
        objectList.querySelectorAll('.drop-target-above, .drop-target-below').forEach((el) => {
          el.classList.remove('drop-target-above', 'drop-target-below');
        });
      });

      row.addEventListener('dragover', (e) => {
        e.preventDefault();
        e.dataTransfer.dropEffect = 'move';
        const rect = row.getBoundingClientRect();
        const isAbove = (e.clientY - rect.top) < rect.height / 2;
        row.classList.toggle('drop-target-above', isAbove);
        row.classList.toggle('drop-target-below', !isAbove);
      });

      row.addEventListener('dragleave', () => {
        row.classList.remove('drop-target-above', 'drop-target-below');
      });

      row.addEventListener('drop', (e) => {
        e.preventDefault();
        row.classList.remove('drop-target-above', 'drop-target-below');
        const sourceKey = e.dataTransfer.getData('text/plain');
        if (!sourceKey || sourceKey === map.key) return;
        const rect = row.getBoundingClientRect();
        const isAbove = (e.clientY - rect.top) < rect.height / 2;
        reorderPageMaps(sourceKey, map.key, isAbove);
      });

      const eyeBtn = row.querySelector('.btn-eye');
      eyeBtn.addEventListener('click', (e) => {
        e.stopPropagation();
        e.preventDefault();
        map.editorVisible = !map.editorVisible;
        syncInspector();
        render();
        window.JWPLCHMIInspectorState?.sync?.();
      });

      const lockBtn = row.querySelector('.btn-lock');
      lockBtn.addEventListener('click', (e) => {
        e.stopPropagation();
        e.preventDefault();
        map.locked = !map.locked;
        syncInspector();
        render();
        window.JWPLCHMIInspectorState?.sync?.();
      });

      row.addEventListener('click', (event) => {
        if (event.target.closest('.object-btn')) return;
        event.preventDefault();
        if (!event.shiftKey && !event.ctrlKey) {
          clearFieldSelection();
        }
        if (event.shiftKey || event.ctrlKey) {
          if (selectedKeys.includes(map.key)) {
            selectedKeys = selectedKeys.filter((k) => k !== map.key);
          } else {
            selectedKeys.push(map.key);
          }
        } else {
          selectedKeys = [map.key];
        }
        selectedKey = selectedKeys.length > 0 ? selectedKeys[selectedKeys.length - 1] : null;
        mode = 'MOVE';
        const pointerBtn = document.querySelector('.tool[data-tool="pointer"]');
        if (pointerBtn) pointerBtn.click();
        else window.dispatchEvent(new CustomEvent('jwplc:tool-changed', { detail: { tool: 'pointer' } }));
        syncInspector();
        refreshBase();
        window.JWPLCHMIInspectorState?.sync?.();
      });

      row.addEventListener('dblclick', (event) => {
        if (event.target.closest('.object-btn')) return;
        event.preventDefault();
        clearFieldSelection();
        selectedKeys = [map.key];
        selectedKey = map.key;
        mode = 'MOVE';
        const pointerBtn = document.querySelector('.tool[data-tool="pointer"]');
        if (pointerBtn) pointerBtn.click();
        else window.dispatchEvent(new CustomEvent('jwplc:tool-changed', { detail: { tool: 'pointer' } }));
        syncInspector();
        refreshBase();
        window.JWPLCHMIEditor?.fitSelection?.();
      });

      objectList.appendChild(row);
    });
    editor()?.sortObjectListDOM?.();
    if (countBadge) countBadge.textContent = String((editor()?.getFieldsForPage?.(page()) || []).length + mapsForPage().length);
  }

  function fieldRects(forMap) {
    const compat = window.JWPLCHMIPixelCompat;
    const mapOrder = Number(forMap?.layerOrder) || Infinity;
    return (editor()?.getFieldsForPage?.(page()) || []).filter((field) => {
      if (field.hidden) return false;
      const fOrder = Number(field.layerOrder) || 0;
      return fOrder < mapOrder;
    }).map((field) => {
      const rect = editor()?.getFieldBounds?.(field) || compat?.fieldRect?.(field);
      return rect ? { x: rect.minX ?? rect.x, y: rect.minY ?? rect.y, w: rect.w ?? rect.width, h: rect.h ?? rect.height } : null;
    }).filter(Boolean);
  }

  function render() {
    if (rendering) return;
    rendering = true;
    const zoom = currentZoom();
    const ctx = displayCanvas.getContext('2d');
    const pctx = previewCanvas.getContext('2d');
    const hasPixelOrder = typeof editor()?.getPixelFieldOrder === 'function';
    const pageMaps = mapsForPage();
    pageMaps.forEach((m) => editor()?.ensureLayerOrder?.(m));

    [...pageMaps]
      .sort((a, b) => (Number(b.layerOrder) || 0) - (Number(a.layerOrder) || 0))
      .forEach((map) => {
        ensureEditorMeta(map);
        if (!map.editorVisible) return;
        const mapOrder = Number(map.layerOrder) || Infinity;
        const protectedRects = hasPixelOrder ? null : fieldRects(map);
        const covered = (x, y) => hasPixelOrder
          ? editor().getPixelFieldOrder(x, y) < mapOrder
          : protectedRects.some((r) => x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h);
        ctx.save();
        pctx.save();
        ctx.globalAlpha = map.editorOpacity;
        pctx.globalAlpha = map.editorOpacity;
        map.pixels.forEach((pixel) => {
          const x = map.x + pixel.x;
          const y = map.y + pixel.y;
          if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT || covered(x, y)) return;
          ctx.fillStyle = css565(pixel.color);
          ctx.fillRect(Math.floor(x * zoom), Math.floor(y * zoom), Math.ceil(zoom), Math.ceil(zoom));
          pctx.fillStyle = css565(pixel.color);
          pctx.fillRect(x, y, 1, 1);
        });
        ctx.restore();
        pctx.restore();
      });


    if (gridToggle?.checked && zoom >= 3) {
      const map = selected();
      if (map && Number(map.page || 0) === page() && ensureEditorMeta(map).editorVisible) {
        ctx.save();
        ctx.strokeStyle = 'rgba(255,154,67,.5)';
        ctx.lineWidth = 1;
        map.pixels.forEach((pixel) => ctx.strokeRect((map.x + pixel.x) * zoom + 0.5, (map.y + pixel.y) * zoom + 0.5, zoom, zoom));
        ctx.restore();
      }
    }
    if (geometryCanvas && geometryToggle?.checked) drawGeometry();
    patchObjectList();
    syncInspector();
    rendering = false;
  }

  function drawGeometry() {
    const map = selected();
    if (!map || Number(map.page || 0) !== page() || !ensureEditorMeta(map).editorVisible) return;
    const zoom = currentZoom();
    const ctx = geometryCanvas.getContext('2d');
    const b = bounds(map);
    ctx.save();
    ctx.strokeStyle = '#ff8a2a';
    ctx.lineWidth = 2;
    ctx.setLineDash([5, 3]);
    ctx.strokeRect(b.x * zoom + 0.5, b.y * zoom + 0.5, b.width * zoom, b.height * zoom);
    ctx.restore();
  }

  function refreshBase() {
    editor()?.render?.();
    requestAnimationFrame(render);
  }

  function changed() {
    window.dispatchEvent(new CustomEvent('jwplc:pixelmap-changed', { detail: { count: maps.length } }));
  }



  function normalizeRuns(map) {
    const pixels = map.pixels.map((p) => ({ x: map.x + p.x, y: map.y + p.y, color: p.color & 0xFFFF }))
      .sort((a, b) => a.y - b.y || a.x - b.x || a.color - b.color);
    const runs = [];
    pixels.forEach((p) => {
      const last = runs[runs.length - 1];
      if (last && last.y === p.y && last.color === p.color && last.x + last.width === p.x) last.width += 1;
      else runs.push({ x: p.x, y: p.y, width: 1, color: p.color });
    });
    return runs;
  }

  function cppToken(value, fallback) {
    const cleaned = String(value || '').trim().replace(/[^A-Za-z0-9_]/g, '_').replace(/^([0-9])/, '_$1').toUpperCase();
    return cleaned || fallback;
  }

  function buildCode() {
    const nonEmpty = maps.filter((map) => map.pixels.length);
    if (!nonEmpty.length) return { block: '', registration: '' };
    const arrays = [];
    const entries = [];
    nonEmpty.forEach((map, index) => {
      const name = `HMI_PIXEL_${cppToken(map.name, `PIXEL_${index + 1}`)}_${index + 1}_RUNS`;
      const runs = normalizeRuns(map);
      arrays.push(`static const JWPLC_UIPixelRun ${name}[] =\n{\n${runs.map((run) => `    JWPLC_UIPixelRun(${run.x}, ${run.y}, ${run.width}, ${hex565(run.color)})`).join(',\n')}\n};`);
      entries.push(`    JWPLC_UIPixelMap(${Number(map.page || 0)}, ${name}, sizeof(${name}) / sizeof(${name}[0]))`);
    });
    return {
      block: `// PixelMaps estáticos RGB565 · JWPLC HMI Designer\n${arrays.join('\n\n')}\n\nstatic const JWPLC_UIPixelMap HMI_PIXEL_MAPS[] =\n{\n${entries.join(',\n')}\n};`,
      registration: '    JWPLC_Display.setPixelMaps(HMI_PIXEL_MAPS, sizeof(HMI_PIXEL_MAPS) / sizeof(HMI_PIXEL_MAPS[0]));'
    };
  }

  function patchCode() {
    if (codePatching || !codeOutput?.textContent?.startsWith('// Código generado por JWPLC HMI Designer')) return;
    const generated = buildCode();
    if (!generated.block) return;
    let text = codeOutput.textContent;
    if (text.includes('// PixelMaps estáticos RGB565 · JWPLC HMI Designer')) return;
    const setup = text.indexOf('void jwplcHMISetup()');
    if (setup < 0) return;
    text = `${text.slice(0, setup).trimEnd()}\n\n${generated.block}\n\n${text.slice(setup)}`;
    const marker = text.indexOf('    JWPLC_Display.setUserRefreshMode(', text.indexOf('void jwplcHMISetup()'));
    if (marker >= 0) text = `${text.slice(0, marker)}${generated.registration}\n${text.slice(marker)}`;
    codePatching = true;
    codeOutput.textContent = text;
    codePatching = false;
  }

  function syncTools() {
    const currentTool = editor()?.getSelectedTool?.() || 'pointer';
    if (pixelTool) {
      pixelTool.title = 'Pincel / Lápiz (B)';
      pixelTool.classList.toggle('active', currentTool === 'pixel');
      if (currentTool !== 'pixel') pixelTool.blur();
    }
    if (eraseTool) {
      eraseTool.title = 'Borrador (E)';
      eraseTool.classList.toggle('active', currentTool === 'erase');
      if (currentTool !== 'erase') eraseTool.blur();
    }
  }

  pixelTool?.addEventListener('click', () => {
    clearFieldSelection();
    mode = 'DRAW';
    let map = selected();
    if (!map || Number(map.page || 0) !== page()) {
      const pageMaps = mapsForPage();
      const emptyMap = pageMaps.find((m) => !m.pixels || m.pixels.length === 0);
      if (emptyMap) {
        selectedKey = emptyMap.key;
        map = emptyMap;
      } else {
        map = createMap();
      }
    }
    syncTools();
    render();
    syncInspector();
    window.JWPLCHMIInspectorState?.sync?.();
  });

  const eraserCursorBox = document.getElementById('eraserCursorBox');

  function updateEraserCursor(p) {
    if (!eraserCursorBox) return;
    const currentTool = editor()?.getSelectedTool?.() || 'pointer';
    if (currentTool !== 'erase' || !inside(p)) {
      eraserCursorBox.style.display = 'none';
      return;
    }
    const range = brushRange(eraserSize);
    const zoom = currentZoom();
    const left = (p.x + range.start) * zoom;
    const top = (p.y + range.start) * zoom;
    const w = range.size * zoom;
    const h = range.size * zoom;
    eraserCursorBox.style.left = `${left}px`;
    eraserCursorBox.style.top = `${top}px`;
    eraserCursorBox.style.width = `${w}px`;
    eraserCursorBox.style.height = `${h}px`;
    eraserCursorBox.style.display = 'block';
  }

  displayCanvas.addEventListener('pointerleave', () => {
    if (eraserCursorBox) eraserCursorBox.style.display = 'none';
  });

  displayCanvas.addEventListener('pointermove', (event) => {
    const p = pointFromEvent(event);
    updateEraserCursor(p);
  });

  // Ajustar tamaño del borrador con la rueda del ratón
  window.addEventListener('wheel', (event) => {
    const currentTool = editor()?.getSelectedTool?.() || 'pointer';
    if (currentTool !== 'erase') return;
    const target = event.target;
    if (target?.closest?.('.panel') || target?.closest?.('nav') || target?.closest?.('header')) return;
    event.preventDefault();
    if (event.deltaY < 0) {
      eraserSize = Math.min(MAX_BRUSH_SIZE, eraserSize + 1);
    } else if (event.deltaY > 0) {
      eraserSize = Math.max(1, eraserSize - 1);
    }
    if (eraserSizeInput) eraserSizeInput.value = String(eraserSize);
    const p = pointFromEvent(event);
    updateEraserCursor(p);
  }, { passive: false });

  eraseTool?.addEventListener('click', () => {
    clearFieldSelection();
    mode = 'ERASE';
    ensureActiveMap(false);
    syncTools();
    render();
    syncInspector();
    window.JWPLCHMIInspectorState?.sync?.();
  });

  window.addEventListener('jwplc:tool-changed', (event) => {
    const newTool = event.detail?.tool;
    if (newTool !== 'erase' && eraserCursorBox) eraserCursorBox.style.display = 'none';
    if (newTool === 'pixel' || newTool === 'erase') {
      mode = (newTool === 'erase') ? 'ERASE' : 'DRAW';
      ensureActiveMap(newTool !== 'erase');
      syncTools();
      render();
      syncInspector();
      window.JWPLCHMIInspectorState?.sync?.();
    } else if (newTool) {
      if (newTool !== 'pointer') {
        selectedKey = null;
      }
      syncTools();
      render();
      window.JWPLCHMIInspectorState?.sync?.();
    }
  });

  displayCanvas.addEventListener('pointerdown', (event) => {
    // Alt+clic es el cuentagotas rápido (app.js), con cualquier herramienta.
    if (event.altKey) return;
    const currentTool = editor()?.getSelectedTool?.() || 'pointer';
    const isPixelTool = (currentTool === 'pixel' || currentTool === 'erase');

    // Si la herramienta activa es una figura (line, rect, ellipse, etc.) o campo de texto/var, NO interceptar.
    if (!isPixelTool && currentTool !== 'pointer') {
      return;
    }

    const p = pointFromEvent(event);
    if (!inside(p)) return;

    // Modo Mover del panel PIXEL: arrastra la capa seleccionada (o la que
    // está bajo el puntero) aunque la herramienta activa sea pincel/borrador.
    if (isPixelTool && mode === 'MOVE') {
      const hit = hitMap(p);
      const map = hit || selected();
      if (!map || Number(map.page || 0) !== page()) return;
      if (map.locked) {
        event.preventDefault();
        event.stopImmediatePropagation();
        lockedNotice();
        return;
      }
      selectedKey = map.key;
      selectedKeys = [map.key];
      event.preventDefault();
      event.stopImmediatePropagation();
      pointer = event.pointerId;
      lastPoint = p;
      try { displayCanvas.setPointerCapture(pointer); } catch (_) {}
      dragStart = p;
      dragOrigins = [{ key: map.key, x: map.x, y: map.y }];
      syncInspector();
      refreshBase();
      return;
    }

    if (isPixelTool) {
      let map = null;
      if (currentTool === 'erase') {
        // Erase: prefer map directly under pointer, otherwise active map or last map on page
        const hit = hitMap(p, Math.max(2, Math.floor(eraserSize / 2)));
        if (hit) {
          map = hit;
          selectedKey = map.key;
        } else {
          map = selected();
          if (!map || Number(map.page || 0) !== page()) {
            const pageMaps = mapsForPage();
            map = pageMaps.length > 0 ? pageMaps[pageMaps.length - 1] : null;
            if (map) selectedKey = map.key;
          }
        }
        if (!map) return; // Nothing to erase
      } else {
        let currentMap = selected();
        if (currentMap && Number(currentMap.page || 0) === page()) {
          map = currentMap;
        } else {
          map = ensureActiveMap();
        }
        if (!map) return;
        selectedKey = map.key;
      }
      mode = (currentTool === 'erase') ? 'ERASE' : 'DRAW';
      event.preventDefault();
      event.stopImmediatePropagation();
      if (map.locked) {
        lockedNotice();
        return;
      }
      pointer = event.pointerId;
      lastPoint = p;
      try { displayCanvas.setPointerCapture(pointer); } catch (_) {}
      const drawColor = color & 0xFFFF;
      if (mode === 'ERASE') {
        if (eraseBrush(map, p.x, p.y, eraserSize)) scheduleEraseRefresh();
      } else {
        putBrush(map, p.x, p.y, drawColor, brushSize);
        render();
      }
      return;
    }

    // currentTool === 'pointer'
    if (editor()?.hitTestHandle?.(event)) {
      return;
    }

    // If clicking inside an active selection (multi-selection containing shapes and/or pixelmaps),
    // allow editor (app.js) to handle unified dragging without clearing selection
    if (editor()?.isPointInsideSelection?.(p)) {
      return;
    }

    const hit = hitMap(p);
    if (hit) {
      if (!event.shiftKey && !event.ctrlKey && !selectedKeys.includes(hit.key)) {
        clearFieldSelection();
      }
      if (event.shiftKey || event.ctrlKey) {
        if (selectedKeys.includes(hit.key)) {
          selectedKeys = selectedKeys.filter((k) => k !== hit.key);
        } else {
          selectedKeys.push(hit.key);
        }
      } else if (!selectedKeys.includes(hit.key)) {
        selectedKeys = [hit.key];
      }
      selectedKey = selectedKeys.length > 0 ? selectedKeys[selectedKeys.length - 1] : null;
      mode = 'MOVE';
      event.preventDefault();
      event.stopImmediatePropagation();
      pointer = event.pointerId;
      lastPoint = p;
      try { displayCanvas.setPointerCapture(pointer); } catch (_) {}
      dragStart = p;
      dragOrigins = selectedMaps().filter((m) => !m.locked).map((m) => ({ key: m.key, x: m.x, y: m.y }));
      syncInspector();
      refreshBase();
      return;
    }

    if (!event.shiftKey && !event.ctrlKey) {
      if (selectedKeys.length > 0 || selectedKey) {
        selectedKey = null;
        selectedKeys = [];
        syncInspector();
        refreshBase();
      }
    }
  }, true);

  window.addEventListener('pointermove', (event) => {
    if (pointer === null || pointer !== event.pointerId) return;
    const p = pointFromEvent(event);
    updateEraserCursor(p);
    window.JWPLCHMIEditor?.trackViewportPointer?.(event);
    if (!inside(p)) return;
    const map = selected();
    if (!map) return;
    event.preventDefault();
    event.stopImmediatePropagation();
    if (mode === 'MOVE' && dragStart && dragOrigins && dragOrigins.length > 0) {
      const dx = p.x - dragStart.x;
      const dy = p.y - dragStart.y;
      const snapToggle = document.getElementById('snapToggle');
      const grid = Number(document.getElementById('gridSizeSelect')?.value) || 8;
      dragOrigins.forEach((orig) => {
        const m = maps.find((x) => x.key === orig.key);
        if (m) {
          let nextX = orig.x + dx;
          let nextY = orig.y + dy;
          if (snapToggle && snapToggle.checked) {
            nextX = Math.round(nextX / grid) * grid;
            nextY = Math.round(nextY / grid) * grid;
          }
          nextX = Math.max(0, Math.min(WIDTH - bounds(m).width, nextX));
          nextY = Math.max(0, Math.min(HEIGHT - bounds(m).height, nextY));
          m.x = nextX;
          m.y = nextY;
        }
      });
      syncInspector();
      refreshBase();
    } else if (lastPoint) {
      const drawColor = color & 0xFFFF;
      let erased = false;
      rasterLine(lastPoint, p, (x, y) => {
        if (mode === 'ERASE') erased = eraseBrush(map, x, y, eraserSize) || erased;
        else putBrush(map, x, y, drawColor, brushSize);
      });
      lastPoint = p;
      // render() sólo pinta píxeles encima del lienzo: lo borrado no
      // desaparece hasta repintar la base, así que se repinta (1 vez por frame).
      if (erased) scheduleEraseRefresh();
      else if (mode !== 'ERASE') render();
    }
  }, true);

  let eraseRefreshQueued = false;
  function scheduleEraseRefresh() {
    if (eraseRefreshQueued) return;
    eraseRefreshQueued = true;
    requestAnimationFrame(() => {
      eraseRefreshQueued = false;
      editor()?.render?.();
      render();
    });
  }

  function endPointer(event) {
    if (pointer === null || (event?.pointerId !== undefined && event.pointerId !== pointer)) return;
    event?.preventDefault?.();
    event?.stopImmediatePropagation?.();
    try { displayCanvas.releasePointerCapture(pointer); } catch (_) {}
    pointer = null;
    lastPoint = null;
    dragStart = null;
    dragOrigin = null;

    // Remove empty pixel maps
    const beforeCount = maps.length;
    maps = maps.filter((m) => m.pixels && m.pixels.length > 0);
    if (!maps.some((m) => m.key === selectedKey)) {
      selectedKey = null;
    }

    const currentTool = editor()?.getSelectedTool?.() || 'pointer';
    if (currentTool === 'pixel') {
      // Conservar selectedKey para permitir trazos sucesivos e inspección continua
    }

    changed();
    syncInspector();
    refreshBase();
    render();
  }
  window.addEventListener('pointerup', endPointer, true);
  window.addEventListener('pointercancel', endPointer, true);

  window.addEventListener('keydown', (event) => {
    const target = event.target;
    if (target instanceof HTMLInputElement || target instanceof HTMLTextAreaElement || target instanceof HTMLSelectElement || target?.isContentEditable) return;

    if (event.key === 'Escape') {
      const currentTool = editor()?.getSelectedTool?.() || 'pointer';
      if (currentTool === 'pixel' || currentTool === 'erase' || selectedKey || selectedKeys.length > 0) {
        if (!editor()?.hasFieldSelection?.()) {
          event.preventDefault();
          event.stopImmediatePropagation();
        }
        selectedKey = null;
        selectedKeys = [];
        if (!editor()?.hasFieldSelection?.()) clearFieldSelection();
        if (currentTool === 'pixel' || currentTool === 'erase') {
          const pointerBtn = document.querySelector('.tool[data-tool="pointer"]');
          if (pointerBtn) {
            pointerBtn.click();
          } else {
            window.dispatchEvent(new CustomEvent('jwplc:tool-changed', { detail: { tool: 'pointer' } }));
          }
        }
        if (pixelTool) {
          pixelTool.classList.remove('active');
          pixelTool.blur();
        }
        if (eraseTool) {
          eraseTool.classList.remove('active');
          eraseTool.blur();
        }
        syncTools();
        syncInspector();
        refreshBase();
        window.JWPLCHMIInspectorState?.sync?.();
        return;
      }
    }

    const map = selected();
    if (!map) return;
    if (event.key === 'Delete' || event.key === 'Backspace') {
      if (editor()?.hasFieldSelection?.()) return;
      event.preventDefault();
      event.stopImmediatePropagation();
      if (map.locked) {
        lockedNotice();
        return;
      }
      deleteMap();
      return;
    }
    const step = event.shiftKey ? 10 : 1;
    const delta = { ArrowLeft: [-step, 0], ArrowRight: [step, 0], ArrowUp: [0, -step], ArrowDown: [0, step] }[event.key];
    if (!delta) return;
    if (editor()?.hasFieldSelection?.()) return;
    event.preventDefault();
    event.stopImmediatePropagation();
    if (map.locked) return;
    const b = bounds(map);
    map.x = Math.max(0, Math.min(WIDTH - b.width, map.x + delta[0]));
    map.y = Math.max(0, Math.min(HEIGHT - b.height, map.y + delta[1]));
    refreshBase();
    changed();
  }, true);

  window.addEventListener('jwplc:editor-refresh', () => {
    if (selected() && Number(selected().page || 0) !== page()) {
      selectedKey = null;
      selectedKeys = [];
    }
    requestAnimationFrame(() => { render(); syncTools(); });
  });
  window.addEventListener('jwplc:project-loaded', () => requestAnimationFrame(render));
  newProjectButton?.addEventListener('click', () => {
    maps = [];
    selectedKey = null;
    serial = 0;
    syncInspector();
    requestAnimationFrame(render);
  });
  contractTab?.addEventListener('click', () => setTimeout(patchCode, 50));
  generateButton?.addEventListener('click', () => setTimeout(patchCode, 70));

  function exportMaps() {
    return maps.map((map) => ({
      type: 'PIXELMAP',
      name: map.name,
      page: Number(map.page || 0),
      x: map.x,
      y: map.y,
      editorVisible: ensureEditorMeta(map).editorVisible,
      editorOpacity: ensureEditorMeta(map).editorOpacity,
      pixels: map.pixels.map((p) => ({ x: p.x, y: p.y, color: p.color & 0xFFFF }))
    }));
  }

  function importMaps(input) {
    maps = Array.isArray(input) ? input.slice(0, MAX_PIXEL_MAPS).map((map, index) => ensureEditorMeta({
      type: 'PIXELMAP',
      key: `pixelmap-${index + 1}`,
      name: String(map.name || `Pixel ${index + 1}`).slice(0, 24),
      page: Number(map.page || 0),
      x: Math.max(0, Math.min(WIDTH - 1, Number(map.x) || 0)),
      y: Math.max(0, Math.min(HEIGHT - 1, Number(map.y) || 0)),
      editorVisible: map.editorVisible !== false,
      editorOpacity: Number(map.editorOpacity),
      pixels: Array.isArray(map.pixels)
        ? map.pixels.map((p) => ({ x: Math.max(0, Number(p.x) || 0), y: Math.max(0, Number(p.y) || 0), color: Number(p.color) & 0xFFFF }))
        : []
    })) : [];
    serial = maps.length;
    selectedKey = null;
    refreshBase();
  }

  injectStyles();
  organizeLeftPanel();
  injectInspector();
  if (pixelSection) pixelSection.hidden = true;
  render();

  window.JWPLCHMIPixelMaps = {
    getAll: () => maps,
    getSelected: selected,
    getActiveColor: () => color,
    setActiveColor: (value) => { setPixelColor(Number(value) & 0xFFFF); syncMainColor(); syncInspector(); },
    recolorSelected,
    getBrushSize: () => brushSize,
    paintPixels,
    prepareBrush,
    getEraserSize: () => eraserSize,
    create: createMap,
    duplicate: duplicateMap,
    removeSelected: deleteMap,
    export: exportMaps,
    import: importMaps,
    buildCode,
    refresh: render,
    hitTest: (point) => hitMap(point),
    getSelectedMaps: selectedMaps,
    getSelectedKeys: () => (selectedKeys.length > 0 ? selectedKeys : (selectedKey ? [selectedKey] : [])),
    syncInspector: () => syncInspector(),
    notifyChanged: () => changed(),
    setSelectedKey: (key, clearFields = true) => {
      if (clearFields && key) clearFieldSelection();
      selectedKeys = key ? [key] : [];
      selectedKey = key;
      syncInspector();
      refreshBase();
      window.JWPLCHMIInspectorState?.sync?.();
    },
    setSelectedKeys: (keys, clearFields = true) => {
      if (clearFields && keys && keys.length > 0) clearFieldSelection();
      selectedKeys = Array.isArray(keys) ? [...keys] : (keys ? [keys] : []);
      selectedKey = selectedKeys.length > 0 ? selectedKeys[selectedKeys.length - 1] : null;
      syncInspector();
      refreshBase();
      window.JWPLCHMIInspectorState?.sync?.();
    },
    getBounds: (map) => bounds(map)
  };
})();
