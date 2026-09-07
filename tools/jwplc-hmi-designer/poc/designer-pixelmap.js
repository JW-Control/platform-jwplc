(() => {
  'use strict';

  if (window.JWPLCHMIPixelMaps) return;

  const WIDTH = 320;
  const HEIGHT = 170;
  const MAX_PIXEL_MAPS = 16;
  const PRESETS = [
    ['BLACK', 0x0000], ['WHITE', 0xFFFF], ['RED', 0xF800],
    ['GREEN', 0x07E0], ['BLUE', 0x001F], ['CYAN', 0x07FF],
    ['MAGENTA', 0xF81F], ['YELLOW', 0xFFE0], ['ORANGE', 0xFD20]
  ];

  const displayCanvas = document.getElementById('displayCanvas');
  const previewCanvas = document.getElementById('previewCanvas');
  const geometryCanvas = document.getElementById('geometryCanvas');
  const objectList = document.getElementById('textObjectItem')?.parentElement;
  const countBadge = document.querySelector('.count-badge');
  const fieldSection = document.getElementById('textFieldControlsSection');
  const rawSection = document.getElementById('rawTextControlsSection');
  const pixelSection = document.querySelector('.pixel-controls');
  const pixelTool = document.querySelector('.tool[data-tool="pixel"]');
  const eraseTool = document.querySelector('.tool[data-tool="erase"]');
  const zoomSelect = document.getElementById('zoomSelect');
  const gridToggle = document.getElementById('gridToggle');
  const geometryToggle = document.getElementById('geometryToggle');
  const codeOutput = document.getElementById('codeOutput');
  const contractTab = document.getElementById('contractTab');
  const generateButton = document.getElementById('generateButton');
  const newProjectButton = document.getElementById('newProjectButton');

  if (!displayCanvas || !previewCanvas || !objectList || !pixelSection) return;

  let maps = [];
  let selectedKey = null;
  let serial = 0;
  let mode = 'DRAW';
  let color = 0xFD20;
  let pointer = null;
  let lastPoint = null;
  let dragStart = null;
  let dragOrigin = null;
  let rendering = false;
  let codePatching = false;

  const editor = () => window.JWPLCHMIEditor || null;
  const page = () => Number(editor()?.getActivePage?.() || 0);
  const hex565 = (value) => `0x${(Number(value) & 0xFFFF).toString(16).toUpperCase().padStart(4, '0')}`;
  const inside = (p) => p.x >= 0 && p.x < WIDTH && p.y >= 0 && p.y < HEIGHT;

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
    return maps.find((map) => map.key === selectedKey) || null;
  }

  function visibleMaps() {
    return maps.filter((map) => Number(map.page || 0) === page());
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
    const scaleX = displayCanvas.width / rect.width;
    const scaleY = displayCanvas.height / rect.height;
    const zoom = Math.max(0.01, Number(zoomSelect?.value) || 1);
    return {
      x: Math.floor(((event.clientX - rect.left) * scaleX) / zoom),
      y: Math.floor(((event.clientY - rect.top) * scaleY) / zoom)
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

  function erasePixel(map, gx, gy) {
    if (!map?.pixels?.length) return false;
    const index = findPixel(map, gx - map.x, gy - map.y);
    if (index < 0) return false;
    map.pixels.splice(index, 1);
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
    if (!editor()?.hasFieldSelection?.()) return;
    document.dispatchEvent(new KeyboardEvent('keydown', { key: 'Escape', bubbles: true }));
  }

  function createMap() {
    if (maps.length >= MAX_PIXEL_MAPS) return null;
    clearFieldSelection();
    serial += 1;
    const map = {
      type: 'PIXELMAP', key: `pixelmap-${serial}`, name: `Pixel ${serial}`,
      page: page(), x: 20, y: 20, pixels: []
    };
    maps.push(map);
    selectedKey = map.key;
    mode = 'DRAW';
    refreshBase();
    changed();
    return map;
  }

  function duplicateMap() {
    const source = selected();
    if (!source || maps.length >= MAX_PIXEL_MAPS) return;
    serial += 1;
    const copy = {
      ...source,
      key: `pixelmap-${serial}`,
      name: `${source.name} copia`.slice(0, 24),
      x: Math.min(WIDTH - 1, source.x + 4),
      y: Math.min(HEIGHT - 1, source.y + 4),
      pixels: source.pixels.map((p) => ({ ...p }))
    };
    const b = bounds(copy);
    copy.x = Math.min(copy.x, Math.max(0, WIDTH - b.width));
    copy.y = Math.min(copy.y, Math.max(0, HEIGHT - b.height));
    maps.push(copy);
    selectedKey = copy.key;
    refreshBase();
    changed();
  }

  function deleteMap() {
    const map = selected();
    if (!map) return;
    maps = maps.filter((item) => item.key !== map.key);
    selectedKey = null;
    refreshBase();
    changed();
  }

  function hitMap(point) {
    const fieldHit = document.elementFromPoint?.(point.clientX, point.clientY);
    void fieldHit;
    const visible = visibleMaps();
    for (let i = visible.length - 1; i >= 0; i -= 1) {
      const map = visible[i];
      if (findPixel(map, point.x - map.x, point.y - map.y) >= 0) return map;
    }
    return null;
  }

  function injectStyles() {
    if (document.getElementById('a11-pixelmap-style')) return;
    const style = document.createElement('style');
    style.id = 'a11-pixelmap-style';
    style.textContent = `
      .a11-pixel-row .object-icon{color:#ff9a43;font-weight:800}
      .a11-pixel-row.active{border-color:#ff8a2a!important;background:rgba(255,138,42,.10)!important}
      .a11-color-control{display:grid;grid-template-columns:34px 1fr 86px;gap:6px;align-items:center}
      .a11-color-control input[type=color]{width:34px;height:30px;padding:1px;border:1px solid #425764;border-radius:4px;background:#13212a}
      .a11-color-control input[type=text]{min-width:0}
      .a11-color-presets{display:flex;flex-wrap:wrap;gap:5px;margin-top:7px}
      .a11-color-chip{width:22px;height:22px;border:1px solid #5b6f7b;border-radius:4px;padding:0;cursor:pointer}
      .a11-pixel-actions{display:grid;grid-template-columns:repeat(3,1fr);gap:5px}
      .a11-pixel-actions button.active{border-color:#ff8a2a;color:#ffc48f;background:rgba(255,138,42,.12)}
      .a11-pixel-note{font-size:10px;line-height:1.35;color:#92a7b3;margin:7px 0 0}
      .a11-custom-color{display:grid;gap:5px;margin-top:6px}
      .a11-custom-color .a11-color-control{grid-template-columns:30px 1fr}
      .a11-custom-color .a11-color-control input[type=text]{display:none}
    `;
    document.head.appendChild(style);
  }

  function makeColorControl(root, getValue, setValue, compact = false) {
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
        button.addEventListener('click', () => { setValue(value); sync(); });
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
  let syncMainColor = () => {};

  function injectInspector() {
    inspector = document.createElement('section');
    inspector.id = 'pixelMapControlsSection';
    inspector.className = 'inspector-section';
    inspector.hidden = true;
    inspector.innerHTML = `
      <div class="inspector-title"><h2>Inspector · PIXEL</h2></div>
      <details open><summary>Identidad</summary><div class="inspector-body two-cols">
        <label class="field-label full">Nombre del objeto<input id="pixelMapName" class="field-input" maxlength="24"></label>
        <label class="field-label">Página<input id="pixelMapPage" class="field-input" disabled></label>
      </div></details>
      <details open><summary>Geometría</summary><div class="inspector-body two-cols">
        <label class="field-label">X<input id="pixelMapX" class="field-input" type="number" min="0" max="319"></label>
        <label class="field-label">Y<input id="pixelMapY" class="field-input" type="number" min="0" max="169"></label>
        <div class="readout"><span>Bounds</span><strong id="pixelMapBounds">—</strong></div>
        <div class="readout"><span>Píxeles</span><strong id="pixelMapCount">0</strong></div>
      </div></details>
      <details open><summary>Edición</summary><div class="inspector-body">
        <div class="a11-pixel-actions"><button data-pm-mode="DRAW">✎ Pincel</button><button data-pm-mode="ERASE">◇ Borrador</button><button data-pm-mode="MOVE">✥ Mover</button></div>
        <p class="a11-pixel-note">Pincel y borrador sólo modifican este objeto PIXEL. Un objeto puede contener múltiples colores RGB565.</p>
      </div></details>
      <details open><summary>Color RGB565</summary><div class="inspector-body"><div id="pixelMapColor"></div><p class="a11-pixel-note">Rango completo: 0x0000…0xFFFF. Los presets son sólo atajos.</p></div></details>
      <details open><summary>Objeto</summary><div class="inspector-body two-cols"><button id="pixelMapDuplicate">⧉ Duplicar</button><button id="pixelMapDelete">🗑 Eliminar</button></div></details>`;
    rawSection?.insertAdjacentElement('afterend', inspector);
    mapName = document.getElementById('pixelMapName');
    mapX = document.getElementById('pixelMapX');
    mapY = document.getElementById('pixelMapY');
    mapPage = document.getElementById('pixelMapPage');
    pixelCount = document.getElementById('pixelMapCount');
    pixelBounds = document.getElementById('pixelMapBounds');
    syncMainColor = makeColorControl(document.getElementById('pixelMapColor'), () => color, (value) => { color = value & 0xFFFF; });

    inspector.querySelectorAll('[data-pm-mode]').forEach((button) => button.addEventListener('click', () => {
      mode = button.dataset.pmMode;
      syncInspector();
      render();
    }));
    document.getElementById('pixelMapDuplicate').addEventListener('click', duplicateMap);
    document.getElementById('pixelMapDelete').addEventListener('click', deleteMap);
    mapName.addEventListener('change', () => { const m = selected(); if (m) { m.name = mapName.value.trim().slice(0, 24) || m.name; changed(); refreshBase(); } });
    [mapX, mapY].forEach((input) => input.addEventListener('change', () => {
      const m = selected(); if (!m) return;
      const b = bounds(m);
      m.x = Math.max(0, Math.min(WIDTH - b.width, Number(mapX.value) || 0));
      m.y = Math.max(0, Math.min(HEIGHT - b.height, Number(mapY.value) || 0));
      changed(); refreshBase();
    }));
  }

  function syncInspector() {
    const map = selected();
    if (fieldSection) fieldSection.hidden = Boolean(map) || fieldSection.hidden;
    if (rawSection && map) rawSection.hidden = true;
    inspector.hidden = !map;
    if (!map) return;
    const b = bounds(map);
    mapName.value = map.name;
    mapX.value = map.x;
    mapY.value = map.y;
    mapPage.value = String(Number(map.page || 0));
    pixelCount.textContent = String(map.pixels.length);
    pixelBounds.textContent = `${b.width} × ${b.height} px`;
    inspector.querySelectorAll('[data-pm-mode]').forEach((button) => button.classList.toggle('active', button.dataset.pmMode === mode));
    syncMainColor();
  }

  function patchObjectList() {
    objectList.querySelectorAll('.a11-pixel-row').forEach((node) => node.remove());
    visibleMaps().forEach((map) => {
      const row = document.createElement('button');
      row.type = 'button';
      row.className = `object-item a11-pixel-row${map.key === selectedKey ? ' active' : ''}`;
      row.innerHTML = '<span class="object-icon">▦</span><span class="object-type">PIXEL</span><span class="object-name"></span><span class="object-id"></span><span class="object-eye">●</span>';
      row.querySelector('.object-name').textContent = map.name;
      row.querySelector('.object-id').textContent = `${map.pixels.length} px`;
      row.addEventListener('click', (event) => {
        event.preventDefault();
        clearFieldSelection();
        selectedKey = map.key;
        mode = 'MOVE';
        syncInspector();
        render();
      });
      objectList.appendChild(row);
    });
    if (countBadge) countBadge.textContent = String((editor()?.getFieldsForPage?.(page()) || []).length + visibleMaps().length);
  }

  function fieldRects() {
    const zoom = Math.max(0.01, Number(zoomSelect?.value) || 1);
    const nodes = [...document.querySelectorAll('.object-item:not(.a11-pixel-row)')];
    void zoom; void nodes;
    const fields = (editor()?.getFieldsForPage?.(page()) || []);
    return fields.map((field) => {
      const g = editor()?.getSelectedField?.() === field ? editor()?.computeSelectedGeometry?.() : null;
      if (g) return { x: field.x, y: field.y, w: g.fieldW, h: g.fieldH };
      return null;
    }).filter(Boolean);
  }

  function render() {
    if (rendering) return;
    rendering = true;
    const zoom = Math.max(0.01, Number(zoomSelect?.value) || 1);
    const ctx = displayCanvas.getContext('2d');
    const pctx = previewCanvas.getContext('2d');
    const protectedRects = fieldRects();
    const covered = (x, y) => protectedRects.some((r) => x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h);

    visibleMaps().forEach((map) => map.pixels.forEach((pixel) => {
      const x = map.x + pixel.x;
      const y = map.y + pixel.y;
      if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT || covered(x, y)) return;
      ctx.fillStyle = css565(pixel.color);
      ctx.fillRect(x * zoom, y * zoom, zoom, zoom);
      pctx.fillStyle = css565(pixel.color);
      pctx.fillRect(x, y, 1, 1);
    }));

    if (gridToggle?.checked && zoom >= 3) {
      const map = selected();
      if (map && Number(map.page || 0) === page()) {
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
    if (!map || Number(map.page || 0) !== page()) return;
    const zoom = Math.max(0.01, Number(zoomSelect?.value) || 1);
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

  function injectGlobalPicker() {
    pixelSection.innerHTML = '<details open><summary>Color RGB565 activo</summary><div class="inspector-body"><div id="a11Global565"></div></div></details>';
    makeColorControl(document.getElementById('a11Global565'), () => color, (value) => { color = value & 0xFFFF; syncInspector(); });
  }

  function ensureSelectOption(select, value) {
    const token = hex565(value);
    let option = [...select.options].find((item) => item.value === token);
    if (!option) {
      option = document.createElement('option');
      option.value = token;
      option.textContent = `${token} · personalizado`;
      select.appendChild(option);
    }
    select.value = token;
  }

  function injectAppearancePickers() {
    const specs = [
      ['fieldLabelColor', 'labelColor'], ['fieldValueColor', 'valueColor'],
      ['fieldBackgroundColor', 'backgroundColor'], ['fieldFrameColor', 'frameColor']
    ];
    specs.forEach(([id, property]) => {
      const select = document.getElementById(id);
      const label = select?.closest('label');
      if (!select || !label || label.dataset.a11Rgb565 === '1') return;
      label.dataset.a11Rgb565 = '1';
      const host = document.createElement('div');
      select.insertAdjacentElement('afterend', host);
      const sync = makeColorControl(host,
        () => Number(editor()?.getSelectedField?.()?.[property] ?? 0),
        (value) => {
          const field = editor()?.getSelectedField?.();
          if (!field) return;
          field[property] = value & 0xFFFF;
          ensureSelectOption(select, field[property]);
          refreshBase();
        }, true);
      window.addEventListener('jwplc:editor-refresh', () => {
        const field = editor()?.getSelectedField?.();
        if (!field) return;
        const value = Number(field[property]) & 0xFFFF;
        ensureSelectOption(select, value);
        sync();
      });
    });
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
    if (pixelTool) {
      pixelTool.title = 'Crear un nuevo objeto PIXEL';
      const small = pixelTool.querySelector('small');
      if (small) small.textContent = 'Crear capa Pixel';
      pixelTool.classList.toggle('active', Boolean(selected()) && mode === 'DRAW');
    }
    if (eraseTool) {
      eraseTool.title = selected() ? 'Borrar del objeto PIXEL seleccionado' : 'Selecciona primero un objeto PIXEL';
      eraseTool.classList.toggle('active', Boolean(selected()) && mode === 'ERASE');
    }
  }

  pixelTool?.addEventListener('click', (event) => {
    event.preventDefault(); event.stopImmediatePropagation(); createMap(); syncTools();
  }, true);
  eraseTool?.addEventListener('click', (event) => {
    event.preventDefault(); event.stopImmediatePropagation();
    if (!selected()) return;
    mode = 'ERASE'; syncInspector(); syncTools(); render();
  }, true);

  displayCanvas.addEventListener('pointerdown', (event) => {
    const p = pointFromEvent(event);
    if (!inside(p)) return;
    let map = selected();
    if (!map) {
      map = hitMap(p);
      if (!map) return;
      clearFieldSelection();
      selectedKey = map.key;
      mode = 'MOVE';
    }
    if (Number(map.page || 0) !== page()) return;
    event.preventDefault(); event.stopImmediatePropagation();
    pointer = event.pointerId;
    lastPoint = p;
    displayCanvas.setPointerCapture?.(pointer);
    if (mode === 'MOVE') { dragStart = p; dragOrigin = { x: map.x, y: map.y }; }
    else if (mode === 'ERASE') { erasePixel(map, p.x, p.y); refreshBase(); }
    else { putPixel(map, p.x, p.y, color); refreshBase(); }
  }, true);

  displayCanvas.addEventListener('pointermove', (event) => {
    if (pointer !== event.pointerId) return;
    const p = pointFromEvent(event);
    if (!inside(p)) return;
    const map = selected();
    if (!map) return;
    event.preventDefault(); event.stopImmediatePropagation();
    if (mode === 'MOVE' && dragStart && dragOrigin) {
      const b = bounds(map);
      map.x = Math.max(0, Math.min(WIDTH - b.width, dragOrigin.x + p.x - dragStart.x));
      map.y = Math.max(0, Math.min(HEIGHT - b.height, dragOrigin.y + p.y - dragStart.y));
    } else if (lastPoint) {
      rasterLine(lastPoint, p, (x, y) => mode === 'ERASE' ? erasePixel(map, x, y) : putPixel(map, x, y, color));
      lastPoint = p;
    }
    refreshBase();
  }, true);

  function endPointer(event) {
    if (pointer === null || (event?.pointerId !== undefined && event.pointerId !== pointer)) return;
    event?.preventDefault?.(); event?.stopImmediatePropagation?.();
    pointer = null; lastPoint = null; dragStart = null; dragOrigin = null; changed(); syncInspector();
  }
  displayCanvas.addEventListener('pointerup', endPointer, true);
  displayCanvas.addEventListener('pointercancel', endPointer, true);

  document.addEventListener('keydown', (event) => {
    const map = selected();
    if (!map) return;
    const target = event.target;
    if (target instanceof HTMLInputElement || target instanceof HTMLTextAreaElement || target instanceof HTMLSelectElement || target?.isContentEditable) return;
    if (event.key === 'Delete' || event.key === 'Backspace') {
      event.preventDefault(); event.stopImmediatePropagation(); deleteMap(); return;
    }
    if (event.key === 'Escape') {
      event.preventDefault(); event.stopImmediatePropagation(); selectedKey = null; syncInspector(); refreshBase(); return;
    }
    const step = event.shiftKey ? 10 : 1;
    const delta = { ArrowLeft: [-step, 0], ArrowRight: [step, 0], ArrowUp: [0, -step], ArrowDown: [0, step] }[event.key];
    if (!delta) return;
    event.preventDefault(); event.stopImmediatePropagation();
    const b = bounds(map);
    map.x = Math.max(0, Math.min(WIDTH - b.width, map.x + delta[0]));
    map.y = Math.max(0, Math.min(HEIGHT - b.height, map.y + delta[1]));
    refreshBase(); changed();
  }, true);

  window.addEventListener('jwplc:editor-refresh', () => {
    if (editor()?.hasFieldSelection?.()) selectedKey = null;
    if (selected() && Number(selected().page || 0) !== page()) selectedKey = null;
    requestAnimationFrame(() => { render(); syncTools(); });
  });
  window.addEventListener('jwplc:project-loaded', () => requestAnimationFrame(render));
  newProjectButton?.addEventListener('click', () => { maps = []; selectedKey = null; serial = 0; syncInspector(); requestAnimationFrame(render); });
  contractTab?.addEventListener('click', () => setTimeout(patchCode, 50));
  generateButton?.addEventListener('click', () => setTimeout(patchCode, 70));

  function exportMaps() {
    return maps.map((map) => ({
      type: 'PIXELMAP', name: map.name, page: Number(map.page || 0), x: map.x, y: map.y,
      pixels: map.pixels.map((p) => ({ x: p.x, y: p.y, color: p.color & 0xFFFF }))
    }));
  }

  function importMaps(input) {
    maps = Array.isArray(input) ? input.slice(0, MAX_PIXEL_MAPS).map((map, index) => ({
      type: 'PIXELMAP', key: `pixelmap-${index + 1}`, name: String(map.name || `Pixel ${index + 1}`).slice(0, 24),
      page: Number(map.page || 0), x: Math.max(0, Math.min(WIDTH - 1, Number(map.x) || 0)),
      y: Math.max(0, Math.min(HEIGHT - 1, Number(map.y) || 0)),
      pixels: Array.isArray(map.pixels) ? map.pixels.map((p) => ({ x: Math.max(0, Number(p.x) || 0), y: Math.max(0, Number(p.y) || 0), color: Number(p.color) & 0xFFFF })) : []
    })) : [];
    serial = maps.length;
    selectedKey = null;
    refreshBase();
  }

  injectStyles();
  injectInspector();
  injectGlobalPicker();
  injectAppearancePickers();
  pixelSection.querySelector('#palette')?.remove();
  render();

  window.JWPLCHMIPixelMaps = {
    getAll: () => maps,
    getSelected: selected,
    getActiveColor: () => color,
    setActiveColor: (value) => { color = Number(value) & 0xFFFF; syncMainColor(); syncInspector(); },
    create: createMap,
    duplicate: duplicateMap,
    removeSelected: deleteMap,
    export: exportMaps,
    import: importMaps,
    buildCode,
    refresh: render
  };
})();
