(() => {
  'use strict';

  // Alpha16 · Iconos con estado (objeto ICON).
  //
  // - Panel «Iconos» en la barra izquierda: biblioteca por categorías
  //   (poc/designer-icon-catalog.js, generado desde icons/library.cjs). Clic o
  //   arrastrar al lienzo agrega un ICON.
  // - Inspector propio (#iconInspectorSection): icono, tamaño, estados,
  //   etiqueta, colores, posición y variable (bool o uint8_t).
  // - El dibujo en el lienzo y el codegen están en app.js + designer-icons-core.js
  //   (gemelo de JWPLC_UI.cpp:drawIcon; tests/icon-host-sim.cjs, icon-smoke.cjs).
  // - El .jwhmi guarda una copia de los iconos usados (iconAssets): el proyecto
  //   se abre igual aunque el catálogo cambie en otra versión.

  if (window.JWPLCHMIIcons) return;

  const core = window.JWPLCIconsCore;
  const catalog = window.JWPLCIconCatalog;
  const leftPanel = document.querySelector('.left-panel');
  const anchor = document.getElementById('textFieldControlsSection');
  if (!core || !catalog || !leftPanel || !anchor) return;

  const SCREEN_W = 320;
  const SCREEN_H = 170;
  const SIZES = [24, 32, 48, 64];
  const projectAssets = new Map(); // 'id@size' -> { name, category, stateNames, entry }

  const editor = () => window.JWPLCHMIEditor || null;
  const selectedIcon = () => {
    const field = editor()?.getSelectedField?.();
    return field?.type === 'ICON' ? field : null;
  };
  const clamp = (v, min, max) => Math.min(max, Math.max(min, v));
  const num = (v, d = 0) => (Number.isFinite(Number(v)) ? Number(v) : d);
  const escapeHtml = (s) => String(s ?? '').replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));
  const sanitizeSymbol = (value, fallback) => {
    const cleaned = String(value || '').trim().replace(/[^A-Za-z0-9_]/g, '_');
    if (!cleaned) return fallback;
    return /^[A-Za-z_]/.test(cleaned) ? cleaned : `_${cleaned}`;
  };
  const css565 = (v) => `rgb(${((v >> 11) & 0x1F) * 255 / 31 | 0}, ${((v >> 5) & 0x3F) * 255 / 63 | 0}, ${(v & 0x1F) * 255 / 31 | 0})`;

  // ---------------------------------------------------------------------------
  // Datos: catálogo + copia del proyecto
  // ---------------------------------------------------------------------------

  function iconInfo(id) {
    const icon = core.findIcon(catalog, id);
    if (icon) return icon;
    for (const [key, asset] of projectAssets) {
      if (key.startsWith(`${id}@`)) return { id, name: asset.name, category: asset.category, stateNames: asset.stateNames, sizes: {} };
    }
    return null;
  }

  function entryFor(field) {
    if (!field?.iconId) return null;
    const fromCatalog = core.entryFor(catalog, field.iconId, field.iconSize);
    if (fromCatalog && fromCatalog.width === Number(field.iconSize)) return fromCatalog;
    const saved = projectAssets.get(`${field.iconId}@${Number(field.iconSize)}`);
    return saved?.entry || fromCatalog || null;
  }

  function sizesFor(id) {
    const icon = core.findIcon(catalog, id);
    const sizes = new Set(core.sizesOf(icon));
    for (const key of projectAssets.keys()) if (key.startsWith(`${id}@`)) sizes.add(Number(key.split('@')[1]));
    return [...sizes].sort((a, b) => a - b);
  }

  function symbolForField(field) {
    if (!field) return '';
    const entry = entryFor(field);
    const base = core.symbolFor(field.iconId, entry ? entry.width : (Number(field.iconSize) || 32));
    const hasCustom = Boolean(field.bodyColor || field.stateColors || field.tierColors || field.tierCount || field.customColors || field.animIntervalMs);
    if (!hasCustom) return base;
    const cleanId = String(field.id || field.variable || 'CUSTOM').replace(/[^A-Za-z0-9]+/g, '_').toUpperCase();
    return `${base}_${cleanId}`;
  }

  function assetsFor(fields) {
    const out = {};
    (fields || []).filter((f) => f.type === 'ICON').forEach((f) => {
      const entry = entryFor(f);
      const info = iconInfo(f.iconId);
      if (!entry || !info) return;
      const symbol = symbolForField(f);
      const key = `${f.iconId}@${entry.width}:${symbol}`;
      const override = (f.bodyColor || f.stateColors || f.tierColors || f.tierCount || f.customColors || f.animIntervalMs) ? f : null;
      out[key] = { id: f.iconId, name: info.name, category: info.category, stateNames: f.stateNames || info.stateNames, entry, symbol, override };
    });
    return Object.keys(out).length ? out : undefined;
  }

  function loadAssets(map) {
    if (!map || typeof map !== 'object') return;
    Object.entries(map).forEach(([key, asset]) => {
      const entry = asset?.entry;
      if (!entry || !Array.isArray(entry.images) || !Array.isArray(entry.states)) return;
      if (!(entry.width > 0 && entry.width <= core.MAX_SIZE && entry.height > 0 && entry.height <= core.MAX_SIZE)) return;
      projectAssets.set(key, asset);
    });
  }

  function stateCount(field) {
    if (field?.iconId === 'pilotos/piloto' && field?.iconVarType === 'uint8') {
      return Math.max(2, Math.min(8, Number(field.stateCount || field.stateColors?.length || 2)));
    }
    return entryFor(field)?.states.length || iconInfo(field?.iconId)?.stateNames?.length || 0;
  }

  // ---------------------------------------------------------------------------
  // Miniaturas
  // ---------------------------------------------------------------------------

  function drawThumb(canvas, entry, state, background = 0x0000, frame = 0, field = null) {
    if (!canvas || !entry) return;
    const img = core.decode(entry, state, frame, field);
    canvas.width = img.width;
    canvas.height = img.height;
    const ctx = canvas.getContext('2d');
    const data = ctx.createImageData(img.width, img.height);
    for (let i = 0; i < img.pixels.length; i += 1) {
      const c = img.pixels[i] >= 0 ? img.pixels[i] : background;
      data.data[i * 4] = ((c >> 11) & 0x1F) * 255 / 31;
      data.data[i * 4 + 1] = ((c >> 5) & 0x3F) * 255 / 63;
      data.data[i * 4 + 2] = (c & 0x1F) * 255 / 31;
      data.data[i * 4 + 3] = img.pixels[i] >= 0 ? 255 : 0;
    }
    ctx.putImageData(data, 0, 0);
  }

  // ---------------------------------------------------------------------------
  // Estilos
  // ---------------------------------------------------------------------------

  const style = document.createElement('style');
  style.id = 'a16-icons-style';
  style.textContent = `
    .icon-dropdown-container { position: relative; display: flex; }
    .icon-tool-btn { width: 44px; min-width: 44px; height: 32px; padding: 0 4px; display: flex; align-items: center; justify-content: center; gap: 3px; }
    .icon-tool-btn .dropdown-chevron { opacity: .65; transition: transform .18s ease; pointer-events: none; flex-shrink: 0; }
    .icon-dropdown-container.open .dropdown-chevron { transform: rotate(180deg); }
    .icon-dropdown-menu.icons-block { width: 344px; max-width: calc(100vw - 24px); padding: 0; }
    .icons-block .icons-body { display: flex; flex-direction: column; gap: 6px; padding: 6px 8px 8px; min-height: 0; }
    .icons-block .icons-cats { display: flex; flex-wrap: wrap; gap: 4px; }
    .icons-block .icons-cats .insp-chip { font-size: 10.5px; padding: 2px 7px; }
    .icons-block .icons-tools { display: flex; gap: 6px; align-items: center; }
    .icons-block .icons-tools input { flex: 1; min-width: 0; }
    .icons-block .icons-size { display: flex; gap: 3px; }
    .icons-block .icons-size .insp-chip { font-size: 10px; padding: 2px 5px; }
    .icons-grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(60px, 1fr)); gap: 4px; align-content: start; overflow-y: auto; overscroll-behavior: contain; padding-right: 2px; }
    .icon-anim-badge { display: inline-block; font-size: 8px; line-height: 1; padding: 1.5px 3.5px; border-radius: 3px; background: rgba(82,201,255,.2); color: #52c9ff; font-weight: 700; margin-left: 3px; vertical-align: middle; }
    .icon-tile { position: relative; display: flex; flex-direction: column; align-items: center; gap: 2px; padding: 5px 2px 3px; border: 1px solid rgba(255,255,255,.08); border-radius: 6px; background: #05080b; color: inherit; cursor: grab; font: inherit; }
    .icon-tile:hover { border-color: rgba(82,201,255,.6); }
    .icon-tile .icon-tile-anim { position: absolute; top: 2px; right: 3px; font-size: 7.5px; line-height: 1; color: #52c9ff; opacity: .85; }
    .icon-tile canvas { width: 32px; height: 32px; image-rendering: pixelated; }
    .icon-tile span { font-size: 9.5px; line-height: 1.15; text-align: center; opacity: .8; max-width: 100%; overflow: hidden; text-overflow: ellipsis; display: -webkit-box; -webkit-line-clamp: 2; -webkit-box-orient: vertical; }
    .icons-empty { font-size: 11px; opacity: .7; padding: 6px 2px; }
    #iconInspectorSection .icon-current { display: flex; gap: 10px; align-items: center; }
    #iconInspectorSection .icon-current canvas { width: 48px; height: 48px; image-rendering: pixelated; background: #000; border-radius: 4px; }
    #iconInspectorSection .icon-states { display: grid; grid-template-columns: repeat(auto-fill, minmax(70px, 1fr)); gap: 6px; }
    #iconInspectorSection .icon-state { display: flex; flex-direction: column; align-items: center; gap: 3px; padding: 6px 4px; }
    #iconInspectorSection .icon-state canvas { width: 32px; height: 32px; image-rendering: pixelated; }
    #iconInspectorSection .icon-state small { opacity: .7; font-size: 10px; }
    #iconInspectorSection .icon-color-row { display: flex; gap: 6px; flex-wrap: wrap; }
    #iconInspectorSection .icon-color-row .insp-chip i { display: inline-block; width: 10px; height: 10px; border-radius: 2px; margin-right: 5px; vertical-align: -1px; border: 1px solid rgba(255,255,255,.35); }
    #iconInspectorSection .icon-map { width: 100%; border-collapse: collapse; font-size: 11.5px; }
    #iconInspectorSection .icon-map td { padding: 3px 4px; border-bottom: 1px solid rgba(255,255,255,.06); }
    #iconInspectorSection .icon-map canvas { width: 16px; height: 16px; image-rendering: pixelated; vertical-align: middle; }
    .pilot-states-control { display: flex; align-items: center; gap: 8px; margin: 6px 0; }
    .pilot-states-btn { width: 28px; height: 28px; border-radius: 4px; border: 1px solid rgba(255,255,255,.2); background: #1a2530; color: #fff; font-weight: bold; cursor: pointer; }
    .pilot-states-btn:hover { background: #26384a; }
    .baliza-tier-row { display: flex; align-items: center; justify-content: space-between; gap: 8px; padding: 4px 6px; background: rgba(255,255,255,.03); border-radius: 4px; margin-bottom: 4px; border: 1px solid rgba(255,255,255,.06); }
    .baliza-tier-info { display: flex; align-items: center; gap: 6px; font-size: 11.5px; }
    .baliza-tier-info i { display: inline-block; width: 12px; height: 12px; border-radius: 2px; }
    .baliza-tier-toggle { padding: 2px 8px; font-size: 11px; border-radius: 3px; cursor: pointer; border: 1px solid rgba(255,255,255,.15); background: #131c24; color: #bbb; }
    .baliza-tier-toggle.active { background: #007acc; color: #fff; border-color: #38a5ff; font-weight: 700; }
    .color-swatch-chip { display: inline-flex; align-items: center; gap: 6px; padding: 4px 8px; border-radius: 4px; border: 1px solid rgba(255,255,255,.15); background: #121820; font-size: 11px; cursor: pointer; color: #fff; }
    .color-swatch-chip i { width: 12px; height: 12px; border-radius: 2px; border: 1px solid rgba(255,255,255,.3); }
    .color-swatch-chip.active { border-color: #52c9ff; background: rgba(82,201,255,.15); }
    .color-presets-row { display: flex; gap: 4px; flex-wrap: wrap; margin-top: 4px; }
    .color-preset-pill { font-size: 10px; padding: 2px 6px; border-radius: 3px; border: 1px solid rgba(255,255,255,.12); background: #18222c; color: #ccc; cursor: pointer; }
    .color-preset-pill:hover { background: #22303e; border-color: rgba(255,255,255,.3); color: #fff; }
    .jwplc-tier-var-row { margin-bottom: 6px; }
    #iconInspectorSection .icon-warn { color: #ffcf5a; font-size: 11px; }
    #iconInspectorSection pre { white-space: pre-wrap; font-size: 11px; }
  `;
  document.head.appendChild(style);

  // ---------------------------------------------------------------------------
  // Biblioteca: botón «Iconos» de la barra del lienzo, junto a Variables y
  // Texto; el menú se abre hacia abajo y se cierra al agregar un icono.
  // ---------------------------------------------------------------------------

  const lib = { category: catalog.categories[0]?.id || '', query: '', size: 32 };
  try {
    const saved = JSON.parse(localStorage.getItem('jwplc.icons.library') || 'null');
    if (saved && typeof saved === 'object') Object.assign(lib, { category: saved.category ?? lib.category, size: SIZES.includes(saved.size) ? saved.size : lib.size });
  } catch (_) { /* sin almacenamiento */ }
  const remember = () => {
    try { localStorage.setItem('jwplc.icons.library', JSON.stringify({ category: lib.category, size: lib.size })); } catch (_) { /* sin almacenamiento */ }
  };

  const container = document.createElement('div');
  container.className = 'icon-dropdown-container';
  container.id = 'iconDropdownContainer';
  container.innerHTML = `
    <button class="canvas-tool icon-tool-btn" id="iconToolBtn" type="button" title="Iconos con estado (motores, pilotos, válvulas…)">
      <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="9"/><path d="M8 15V9l4 4 4-4v6"/></svg>
      <svg class="dropdown-chevron" width="7" height="6" viewBox="0 0 8 6" fill="currentColor"><path d="M0 1l4 4 4-4z"/></svg>
    </button>
    <div class="text-dropdown-menu icon-dropdown-menu icons-block" id="iconDropdownMenu" style="display: none;">
      <div class="var-menu-header">Iconos <span class="count-badge" data-icons-count>${catalog.icons.length}</span></div>
      <div class="icons-body">
        <div class="icons-cats" data-icons-cats></div>
        <div class="icons-tools">
          <input class="field-input" data-icons-search type="search" placeholder="Buscar (motor, válvula…)" />
          <div class="icons-size" data-icons-size title="Tamaño del icono nuevo"></div>
        </div>
        <div class="icons-grid" data-icons-grid></div>
        <div class="insp-note" style="font-size:10.5px;opacity:.75">Clic o arrastrar al lienzo. Los iconos cambian de imagen según su variable y animan en marcha.</div>
      </div>
    </div>`;
  const block = container.querySelector('#iconDropdownMenu');
  const toolBtn = container.querySelector('#iconToolBtn');
  const textContainer = document.getElementById('textDropdownContainer');
  if (textContainer) textContainer.insertAdjacentElement('afterend', container);
  else (document.querySelector('.canvas-toolbar') || leftPanel).appendChild(container);

  // Alto fijo de la grilla (no cambia con la categoría) que cabe sobre el
  // borde inferior del lienzo, sin bajar de una fila ni pasar de 300 px.
  const GRID_MIN = 72;
  const GRID_MAX = 300;
  function fitMenu() {
    if (block.style.display === 'none') return;
    const grid = block.querySelector('[data-icons-grid]');
    grid.style.height = `${GRID_MIN}px`;
    const chrome = block.offsetHeight - grid.offsetHeight;
    const viewportBox = document.getElementById('canvasViewport')?.getBoundingClientRect();
    const bottom = Math.min(window.innerHeight, viewportBox ? viewportBox.bottom : window.innerHeight) - 12;
    const free = bottom - block.getBoundingClientRect().top - chrome;
    grid.style.height = `${Math.max(GRID_MIN, Math.min(GRID_MAX, Math.floor(free)))}px`;
  }
  window.addEventListener('resize', fitMenu);

  function setMenuOpen(open) {
    block.style.display = open ? 'flex' : 'none';
    container.classList.toggle('open', open);
    if (open) {
      fitMenu();
      block.querySelector('[data-icons-search]')?.focus({ preventScroll: true });
    }
  }

  // Con el menú abierto la rueda nunca mueve el lienzo: dentro del menú
  // desplaza la grilla y fuera de él no hace nada (el lienzo está tapado).
  block.addEventListener('wheel', (e) => e.stopPropagation(), { passive: true });
  document.getElementById('canvasViewport')?.addEventListener('wheel', (e) => {
    if (block.style.display === 'none' || e.composedPath().includes(block)) return;
    e.preventDefault();
    e.stopPropagation();
  }, { capture: true, passive: false });
  toolBtn.addEventListener('click', (e) => {
    e.stopPropagation();
    // Cierra los otros menús de la barra (Variables, Texto).
    document.querySelectorAll('.var-dropdown-menu, .text-dropdown-menu').forEach((m) => { if (m !== block) m.style.display = 'none'; });
    document.querySelectorAll('.var-dropdown-container.open, .text-dropdown-container.open').forEach((c) => c.classList.remove('open'));
    setMenuOpen(block.style.display === 'none');
  });
  document.addEventListener('click', (e) => {
    if (!e.composedPath().includes(container)) setMenuOpen(false);
  }, { capture: true });
  document.addEventListener('keydown', (e) => { if (e.key === 'Escape' && block.style.display !== 'none') setMenuOpen(false); });
  // Botón activo mientras hay un ICON seleccionado (como Variables y Texto).
  const syncToolBtn = () => toolBtn.classList.toggle('active', editor()?.getSelectedField?.()?.type === 'ICON' && Boolean(editor()?.hasFieldSelection?.()));
  window.addEventListener('jwplc:editor-refresh', syncToolBtn);
  window.addEventListener('jwplc:tool-changed', () => {
    setMenuOpen(false);
    setTimeout(syncToolBtn, 0);
  });

  function renderLibrary() {
    const cats = block.querySelector('[data-icons-cats]');
    const all = [{ id: '', name: 'Todos' }, ...catalog.categories];
    cats.innerHTML = all.map((c) =>
      `<button type="button" class="insp-chip${c.id === lib.category ? ' active' : ''}" data-icons-cat="${c.id}">${escapeHtml(c.name)}</button>`).join('');
    block.querySelector('[data-icons-size]').innerHTML = SIZES.map((s) =>
      `<button type="button" class="insp-chip${s === lib.size ? ' active' : ''}" data-icons-pick-size="${s}">${s}</button>`).join('');
    const q = lib.query.trim().toLowerCase().normalize('NFD').replace(/[̀-ͯ]/g, '');
    const list = catalog.icons.filter((icon) => {
      if (q) {
        const hay = `${icon.name} ${icon.id} ${icon.stateNames.join(' ')}`.toLowerCase().normalize('NFD').replace(/[̀-ͯ]/g, '');
        return hay.includes(q);
      }
      return !lib.category || icon.category === lib.category;
    });
    const grid = block.querySelector('[data-icons-grid]');
    grid.innerHTML = list.length ? list.map((icon) => {
      const entry = icon.sizes[core.resolveSize(icon, 32)];
      const isAnim = entry?.states?.some((s) => s.frameCount > 1);
      return `<button type="button" class="icon-tile" draggable="true" data-icon-id="${icon.id}" title="${escapeHtml(icon.name)} · ${escapeHtml(icon.stateNames.join(' / '))}${isAnim ? ' · animación en marcha' : ''} · ${core.sizesOf(icon).join('/')} px">
        <canvas data-icon-thumb="${icon.id}"></canvas><span>${escapeHtml(icon.name)}</span>
        ${isAnim ? '<span class="icon-tile-anim" title="Animado en marcha">▶</span>' : ''}
      </button>`;
    }).join('') : '<div class="icons-empty">Sin resultados.</div>';
    grid.querySelectorAll('.icon-tile').forEach((tile) => {
      const icon = core.findIcon(catalog, tile.dataset.iconId);
      if (!icon) return;
      const entry = icon.sizes[core.resolveSize(icon, 32)];
      const canvas = tile.querySelector('canvas');
      drawThumb(canvas, entry, 1);
      const st = entry?.states?.[1];
      if (st && st.frameCount > 1) {
        let hoverTimer = null;
        let frame = 0;
        tile.addEventListener('mouseenter', () => {
          if (hoverTimer) clearInterval(hoverTimer);
          frame = 0;
          hoverTimer = setInterval(() => {
            frame = (frame + 1) % st.frameCount;
            drawThumb(canvas, entry, 1, 0x0000, frame);
          }, st.frameIntervalMs || 120);
        });
        tile.addEventListener('mouseleave', () => {
          if (hoverTimer) clearInterval(hoverTimer);
          hoverTimer = null;
          frame = 0;
          drawThumb(canvas, entry, 1, 0x0000, 0);
        });
      }
    });
  }

  function addIcon(id, at = null) {
    setMenuOpen(false);
    const field = editor()?.addIconField?.(id, lib.size, at);
    if (field) setTimeout(() => { sync(); syncToolBtn(); }, 0);
    return field;
  }

  block.addEventListener('click', (e) => {
    const t = e.target;
    const cat = t.closest('[data-icons-cat]');
    if (cat) { lib.category = cat.dataset.iconsCat; lib.query = ''; block.querySelector('[data-icons-search]').value = ''; remember(); renderLibrary(); return; }
    const size = t.closest('[data-icons-pick-size]');
    if (size) { lib.size = Number(size.dataset.iconsPickSize); remember(); renderLibrary(); return; }
    const tile = t.closest('[data-icon-id]');
    if (tile) addIcon(tile.dataset.iconId);
  });
  block.querySelector('[data-icons-search]').addEventListener('input', (e) => { lib.query = e.target.value; renderLibrary(); });

  // Arrastrar al lienzo: el icono queda con su esquina donde se suelta.
  block.addEventListener('dragstart', (e) => {
    const tile = e.target.closest?.('[data-icon-id]');
    if (!tile) return;
    e.dataTransfer.setData('application/x-jwplc-icon', tile.dataset.iconId);
    e.dataTransfer.effectAllowed = 'copy';
    // El menú tapa el lienzo: se vuelve transparente al arrastrar (ocultarlo cancelaría el arrastre).
    requestAnimationFrame(() => { block.style.opacity = '0.15'; block.style.pointerEvents = 'none'; });
  });
  block.addEventListener('dragend', () => { block.style.opacity = ''; block.style.pointerEvents = ''; setMenuOpen(false); });
  const viewport = document.getElementById('canvasViewport');
  const displayCanvas = document.getElementById('displayCanvas');
  viewport?.addEventListener('dragover', (e) => {
    if (e.dataTransfer?.types?.includes('application/x-jwplc-icon')) { e.preventDefault(); e.dataTransfer.dropEffect = 'copy'; }
  });
  viewport?.addEventListener('drop', (e) => {
    const id = e.dataTransfer?.getData('application/x-jwplc-icon');
    if (!id || !displayCanvas) return;
    e.preventDefault();
    const r = displayCanvas.getBoundingClientRect();
    const x = clamp(Math.round((e.clientX - r.left) / r.width * SCREEN_W), 0, SCREEN_W - 1);
    const y = clamp(Math.round((e.clientY - r.top) / r.height * SCREEN_H), 0, SCREEN_H - 1);
    addIcon(id, { x, y });
  });

  renderLibrary();

  // ---------------------------------------------------------------------------
  // Inspector
  // ---------------------------------------------------------------------------

  const section = document.createElement('section');
  section.id = 'iconInspectorSection';
  section.className = 'inspector-section';
  section.hidden = true;
  section.style.display = 'none';
  section.innerHTML = `
    <div class="inspector-title">
      <h2>Inspector — ICON</h2>
      <span>
        <button type="button" class="icon-button" data-icon-action="duplicate" title="Duplicar (Ctrl+D)">⧉</button>
        <button type="button" class="icon-button" data-icon-action="delete" title="Eliminar">🗑</button>
      </span>
    </div>
    <div class="insp-wrap">
      <div class="bar-nav-header insp-nav-header">
        <div class="bar-main-tabs" role="tablist">
          <button type="button" class="bar-main-tab active" data-icon-main="design">Diseño</button>
          <button type="button" class="bar-main-tab" data-icon-main="data">Datos</button>
          <button type="button" class="bar-main-tab" data-icon-main="tech">Técnico</button>
        </div>
        <div class="bar-sub-tabs" data-icon-subtabs>
          <button type="button" class="bar-sub-tab active" data-icon-sub="shape">Forma</button>
          <button type="button" class="bar-sub-tab" data-icon-sub="content">Contenido</button>
          <button type="button" class="bar-sub-tab" data-icon-sub="appearance">Apariencia</button>
          <button type="button" class="bar-sub-tab" data-icon-sub="placement">Ubicación</button>
        </div>
      </div>
      <div class="insp-body">
        <div class="insp-panel" data-icon-panel="design:shape">
          <div class="bar-card">
            <div class="icon-current">
              <canvas data-icon-current></canvas>
              <div>
                <strong data-icon-readout="name">—</strong>
                <div class="insp-note" data-icon-readout="category"></div>
              </div>
            </div>
            <div class="bar-card-section-title insp-gap" style="margin-top:6px" data-icon-quick-states-title>Estado actual <small style="opacity:.7">· clic para probar</small></div>
            <div class="insp-chip-row" data-icon-quick-states></div>
            <label class="field-label insp-gap">Icono
              <select class="field-input" data-icon-prop="iconId"></select>
            </label>
          </div>
          <!-- Configuración del Piloto Modular (solo si es piloto) -->
          <div class="bar-card" data-icon-pilot-card hidden>
            <div class="bar-card-section-title">Configuración del Piloto</div>
            <span class="bar-visual-group-title">Modo</span>
            <div class="bar-visual-grid-2">
              <button type="button" class="bar-visual-btn" data-pilot-mode="bool"><strong>ON / OFF</strong><span>Variable bool (2 estados)</span></button>
              <button type="button" class="bar-visual-btn" data-pilot-mode="uint8"><strong>Multi-estado</strong><span>Variable uint8_t</span></button>
            </div>
            <div data-pilot-states-section class="insp-gap" hidden>
              <span class="bar-visual-group-title">Cantidad de estados</span>
              <div class="insp-chip-row" data-pilot-states-quick>
                <button type="button" class="insp-chip" data-pilot-states-set="2">2</button>
                <button type="button" class="insp-chip" data-pilot-states-set="3">3</button>
                <button type="button" class="insp-chip" data-pilot-states-set="4">4</button>
                <button type="button" class="insp-chip" data-pilot-states-set="5">5</button>
                <button type="button" class="insp-chip" data-pilot-states-set="6">6</button>
                <button type="button" class="insp-chip" data-pilot-states-set="8">8</button>
              </div>
            </div>
            <span class="bar-visual-group-title insp-gap">Estados configurados</span>
            <div data-pilot-states-list class="pilot-states-list"></div>
            <div class="color-presets-row insp-gap">
              <span style="font-size:10px;opacity:.7;line-height:20px">Paletas rápidas:</span>
              <button type="button" class="color-preset-pill" data-apply-color="0x07E0">Verde</button>
              <button type="button" class="color-preset-pill" data-apply-color="0xF800">Rojo</button>
              <button type="button" class="color-preset-pill" data-apply-color="0xFDE0">Ámbar</button>
              <button type="button" class="color-preset-pill" data-apply-color="0x041F">Azul</button>
              <button type="button" class="color-preset-pill" data-apply-color="0xFFFF">Blanco</button>
              <button type="button" class="color-preset-pill" data-apply-color="0x4208">Apagado</button>
            </div>
          </div>

          <!-- Configuración de la Torre de Señalización (Baliza) -->
          <div class="bar-card" data-icon-baliza-card hidden>
            <div class="bar-card-section-title">Torre de Señalización (Baliza)</div>
            <span class="bar-visual-group-title">Cantidad de pisos</span>
            <div class="insp-chip-row" data-baliza-tier-count-group>
              <button type="button" class="insp-chip" data-baliza-tier-count="1">1 Piso</button>
              <button type="button" class="insp-chip" data-baliza-tier-count="2">2 Pisos</button>
              <button type="button" class="insp-chip" data-baliza-tier-count="3">3 Pisos</button>
              <button type="button" class="insp-chip active" data-baliza-tier-count="4">4 Pisos</button>
            </div>
            <span class="bar-visual-group-title insp-gap">Pisos (prueba en vivo y color)</span>
            <div data-baliza-tiers-list></div>
          </div>
          <!-- Velocidad de animación (solo para iconos animados) -->
          <div class="bar-card" data-icon-anim-card hidden>
            <div class="bar-card-section-title">Velocidad de animación</div>
            <div class="insp-note">Ajusta el tiempo de cada fotograma (menor tiempo = más rápido).</div>
            <div class="insp-chip-row insp-gap" data-anim-presets-group>
              <button type="button" class="insp-chip" data-anim-preset="250">250 ms · Muy lento</button>
              <button type="button" class="insp-chip" data-anim-preset="180">180 ms · Lento</button>
              <button type="button" class="insp-chip" data-anim-preset="120">120 ms · Normal</button>
              <button type="button" class="insp-chip" data-anim-preset="80">80 ms · Rápido</button>
              <button type="button" class="insp-chip" data-anim-preset="50">50 ms · Muy rápido</button>
            </div>
            <div style="display:flex;align-items:center;gap:8px;margin-top:6px">
              <label class="field-label" style="margin:0;flex:1">Ms por cuadro
                <input class="field-input" data-icon-anim-input type="number" min="30" max="1000" step="10" placeholder="120" />
              </label>
              <button type="button" class="insp-chip" data-anim-preset="reset" style="margin-top:16px">Original</button>
            </div>
          </div>

          <div class="bar-card">
            <div class="bar-card-section-title">Tamaño</div>
            <div class="insp-chip-row" data-icon-sizes></div>
            <div class="insp-note">Tamaños dibujados para este icono. Cambiar el tamaño no deforma: usa otra imagen.</div>
          </div>
          <div class="bar-card">
            <div class="bar-card-section-title">Marco</div>
            <div class="bar-visual-grid-2">
              <button type="button" class="bar-visual-btn" data-icon-set="frame" data-value="0"><span>Sin marco</span></button>
              <button type="button" class="bar-visual-btn" data-icon-set="frame" data-value="1"><span>Con marco</span></button>
            </div>
          </div>
          <div class="bar-card">
            <div class="bar-card-section-title">Fondo</div>
            <div class="bar-visual-grid-2">
              <button type="button" class="bar-visual-btn" data-icon-set="transparentBackground" data-value="0"><span>Con color de fondo</span></button>
              <button type="button" class="bar-visual-btn" data-icon-set="transparentBackground" data-value="1"><span>Sin fondo</span></button>
            </div>
            <div class="insp-note">Sin fondo se ve lo que hay debajo (figuras, texto fijo). Lo que cambia entre estados sigue usando el color de fondo para no dejar restos en la TFT.</div>
          </div>
        </div>

        <div class="insp-panel" data-icon-panel="design:content" hidden>
          <div class="bar-card" data-icon-states-card>
            <div class="bar-card-section-title">Estados <small style="opacity:.7">· clic para verlo en el lienzo</small></div>
            <div class="icon-states" data-icon-states></div>
          </div>
          <div class="bar-card">
            <div class="bar-card-section-title">Etiqueta (opcional)</div>
            <label class="field-label">Texto<input class="field-input" data-icon-prop="label" type="text" maxlength="24" placeholder="Sin etiqueta" /></label>
            <span class="bar-visual-group-title insp-gap">Posición</span>
            <div class="bar-visual-grid-2">
              <button type="button" class="bar-visual-btn" data-icon-set="layout" data-value="STACKED"><span>Arriba del icono</span></button>
              <button type="button" class="bar-visual-btn" data-icon-set="layout" data-value="INLINE"><span>A la izquierda</span></button>
            </div>
            <span class="bar-visual-group-title insp-gap">Tamaño de texto</span>
            <div class="bar-visual-grid-2">
              <button type="button" class="bar-visual-btn" data-icon-set="labelSize" data-value="1"><strong>Aa</strong><span>1×</span></button>
              <button type="button" class="bar-visual-btn" data-icon-set="labelSize" data-value="2"><strong style="font-size:15px">Aa</strong><span>2×</span></button>
            </div>
          </div>
        </div>

        <div class="insp-panel" data-icon-panel="design:appearance" hidden>
          <div class="menu-case">
            <div class="bar-card-section-title">Vista</div>
            <div class="menu-case-body field-case-body zone-host" data-icon-case-body="single">
              <canvas class="menu-case-canvas field-case-canvas" data-icon-case="single"></canvas>
              <div class="menu-case-legend" data-icon-case-legend></div>
            </div>
          </div>
          <div class="menu-palette">
            <div class="insp-note">Clic en una fila (texto o cuadro) para editar ese color: el contorno y el ▶ marcan lo que cambia en la vista.</div>
            <div class="color-editing-header" style="display:flex;align-items:center;justify-content:space-between;margin:10px 0 6px">
              <span style="font-weight:600;font-size:12px">Editando <strong data-icon-editing-name>Etiqueta</strong> <small style="opacity:.7" data-icon-editing-desc>· texto</small></span>
              <span class="color-current-swatch" data-icon-editing-swatch style="display:inline-block;width:36px;height:16px;border-radius:3px;border:1px solid rgba(255,255,255,.3)"></span>
            </div>
            <div data-icon-color-editor></div>
            <div class="bar-card insp-gap" data-icon-body-presets style="margin-top:10px">
              <div class="bar-card-section-title">Tonos industriales para el cuerpo / equipo</div>
              <div class="color-presets-row" style="margin-top:4px">
                <button type="button" class="color-preset-pill" data-body-color-preset="#0066cc">Azul eléctrico</button>
                <button type="button" class="color-preset-pill" data-body-color-preset="#005544">Verde petróleo</button>
                <button type="button" class="color-preset-pill" data-body-color-preset="#505860">Gris fundición</button>
                <button type="button" class="color-preset-pill" data-body-color-preset="#ff6600">Naranja</button>
                <button type="button" class="color-preset-pill" data-body-color-preset="#ffcc00">Amarillo</button>
                <button type="button" class="color-preset-pill" data-body-color-preset="#cc1100">Rojo máquina</button>
                <button type="button" class="insp-chip" data-body-color-preset="reset" style="margin-left:auto">Restablecer original</button>
              </div>
            </div>
          </div>
        </div>

        <div class="insp-panel" data-icon-panel="design:placement" hidden>
          <div class="bar-card">
            <div class="insp-two">
              <label class="field-label">X<input class="field-input" data-icon-prop="x" type="number" min="0" max="319" /></label>
              <label class="field-label">Y<input class="field-input" data-icon-prop="y" type="number" min="0" max="169" /></label>
            </div>
            <div class="insp-readouts insp-readouts-grid insp-gap">
              <span>Recuadro <strong data-icon-readout="box">—</strong></span>
            </div>
          </div>
          <div class="bar-card">
            <div class="bar-card-section-title">Página</div>
            <div class="insp-chip-row" data-icon-pages></div>
          </div>
        </div>

        <div class="insp-panel" data-icon-panel="data" hidden>
          <!-- Baliza control mode -->
          <div class="bar-card" data-icon-baliza-data hidden>
            <div class="bar-card-section-title">Modo de control de la Torre</div>
            <div class="bar-visual-grid-2">
              <button type="button" class="bar-visual-btn" data-baliza-mode="bitmask"><strong>Máscara de bits</strong><span>uint8_t (0..15)</span></button>
              <button type="button" class="bar-visual-btn" data-baliza-mode="independent"><strong>Pisos independientes</strong><span>Variables bool por piso</span></button>
            </div>
            <div data-baliza-tier-vars-container class="insp-gap" hidden>
              <span class="bar-visual-group-title">Variables por piso (OpenPLC / C++)</span>
              <div data-baliza-tier-vars-list></div>
            </div>
          </div>
          <div class="bar-card" data-icon-standard-data>
            <label class="field-label">Variable C++<input class="field-input code-input" data-icon-prop="variable" type="text" maxlength="32" /></label>
            <span class="bar-visual-group-title insp-gap">Tipo</span>
            <div class="bar-visual-grid-2">
              <button type="button" class="bar-visual-btn" data-icon-set="iconVarType" data-value="bool"><strong>bool</strong><span>2 estados: false / true</span></button>
              <button type="button" class="bar-visual-btn" data-icon-set="iconVarType" data-value="uint8"><strong>uint8_t</strong><span>0..N-1 (entero)</span></button>
            </div>
            <div class="icon-warn insp-gap" data-icon-warn hidden></div>
          </div>
          <div class="bar-card">
            <div class="bar-card-section-title">Valor → imagen</div>
            <table class="icon-map" data-icon-map></table>
            <div class="insp-note">Un valor mayor que el último muestra el último estado.</div>
          </div>
        </div>

        <div class="insp-panel" data-icon-panel="tech" hidden>
          <div class="bar-card">
            <label class="field-label">ID C++<input class="field-input code-input" data-icon-prop="id" type="text" maxlength="40" /></label>
            <div class="insp-readouts insp-readouts-grid insp-gap">
              <span>Flash del icono <strong data-icon-readout="flash">—</strong></span>
              <span>Campos usados <strong data-icon-readout="fields">—</strong></span>
            </div>
          </div>
          <div class="bar-card">
            <div class="bar-card-section-title">En el sketch</div>
            <pre class="code-block" data-icon-contract></pre>
          </div>
        </div>
      </div>
    </div>`;
  anchor.insertAdjacentElement('beforebegin', section);

  const tabs = { main: 'design', sub: 'shape' };
  let activeColorKey = 'labelColor';
  const COLOR_KEYS = [['labelColor', 'Etiqueta'], ['backgroundColor', 'Fondo'], ['frameColor', 'Marco']];

  function handleColorEdit(v, commit) {
    mutate((f) => {
      if (activeColorKey === 'bodyColor') {
        f.bodyColor = v;
      } else if (activeColorKey.startsWith('pilotState:')) {
        const idx = Number(activeColorKey.split(':')[1]);
        if (!Array.isArray(f.stateColors)) f.stateColors = [0x4208, 0x07E0];
        f.stateColors[idx] = v;
      } else if (activeColorKey.startsWith('balizaTier:')) {
        const idx = Number(activeColorKey.split(':')[1]);
        if (!Array.isArray(f.tierColors)) f.tierColors = [0x07E0, 0xFDE0, 0xF800, 0x041F];
        f.tierColors[idx] = v;
      } else {
        f[activeColorKey] = v;
      }
    }, commit);
  }

  const colorEditor = window.JWPLCColorEditor?.create?.({
    paletteAttr: 'data-icon-palette',
    onInput: (v) => handleColorEdit(v, false),
    onCommit: (v) => handleColorEdit(v, true)
  });
  if (colorEditor) section.querySelector('[data-icon-color-editor]').appendChild(colorEditor.el);

  // Selector de icono agrupado por categoría.
  section.querySelector('[data-icon-prop="iconId"]').innerHTML = catalog.categories.map((c) =>
    `<optgroup label="${escapeHtml(c.name)}">${catalog.icons.filter((i) => i.category === c.id)
      .map((i) => `<option value="${i.id}">${escapeHtml(i.name)}</option>`).join('')}</optgroup>`).join('');

  function mutate(fn, commit = true) {
    const field = selectedIcon();
    if (!field) return;
    fn(field);
    editor()?.render?.();
    if (commit) editor()?.commitHistory?.();
  }

  function applyTabs() {
    section.querySelectorAll('[data-icon-main]').forEach((b) => b.classList.toggle('active', b.dataset.iconMain === tabs.main));
    section.querySelectorAll('[data-icon-sub]').forEach((b) => b.classList.toggle('active', b.dataset.iconSub === tabs.sub));
    section.querySelector('[data-icon-subtabs]').hidden = tabs.main !== 'design';
    const key = tabs.main === 'design' ? `design:${tabs.sub}` : tabs.main;
    section.querySelectorAll('[data-icon-panel]').forEach((p) => { p.hidden = p.dataset.iconPanel !== key; });
  }

  function contractText(field) {
    const id = sanitizeSymbol(field.id, 'FIELD_ICON');
    const variable = sanitizeSymbol(field.variable, 'icono');
    const isPilot = field.iconId === 'pilotos/piloto';
    const effectiveCount = isPilot
      ? (field.iconVarType === 'uint8' ? Math.max(2, Math.min(8, Number(field.stateCount || field.stateColors?.length || 2))) : 2)
      : (entryFor(field)?.states?.length || iconInfo(field.iconId)?.stateNames?.length || 2);
    const rawNames = field.stateNames || iconInfo(field.iconId)?.stateNames || [];
    const names = rawNames.slice(0, effectiveCount);
    const legend = field.iconVarType === 'uint8'
      ? names.map((n, i) => `//   ${i} = ${n}`).join('\n')
      : `//   false = ${names[0] || 'estado 0'}\n//   true  = ${names[1] || 'estado 1'}`;
    return `// JWPLC_HMI_Generated.h declara:\n${core.iconDeclaration(field, variable, names)}\n\n` +
      `// En el loop() o la lógica del PLC:\n${field.balizaMode === 'independent' ? '// Actualizar las variables booleanas de cada piso' : `${variable} = ...;\n${legend}`}\n\n` +
      `// jwplcUIUpdate() (generado) llama:\n${core.iconSetter(id, variable, field)}`;
  }

  let cachedZones = null;

  function drawAppearance() {
    const field = selectedIcon();
    if (!field || tabs.main !== 'design' || tabs.sub !== 'appearance') return;
    const canvas = section.querySelector('[data-icon-case="single"]');
    if (!canvas) return;
    const body = canvas.parentElement;
    const preview = { ...field, x: 0, y: 0 };
    const out = editor()?.renderFieldPreview?.(preview);
    if (!out?.g) return;

    const w = Math.max(1, Math.min(out.width, Math.ceil(out.g.fieldW)));
    const h = Math.max(1, Math.min(out.height, Math.ceil(out.g.fieldH)));
    const legend = body.querySelector('.menu-case-legend');
    const gutter = (window.JWPLCZoneIndicator?.CHANNEL ?? 12) + 14;
    const bodyW = body.clientWidth || 300;
    const fit = (avail) => Math.max(0, Math.min(3, Math.floor(avail / w)));
    const rightScale = fit(bodyW - (legend?.offsetWidth || 110) - gutter);
    const belowScale = fit(bodyW - gutter);
    const below = rightScale < 2 && belowScale > rightScale;
    body.classList.toggle('is-below', below);
    body.dataset.placement = below ? 'below' : 'right';
    const scale = Math.max(1, below ? belowScale : rightScale);
    if (canvas.width !== w * scale) canvas.width = w * scale;
    if (canvas.height !== h * scale) canvas.height = h * scale;
    canvas.dataset.logicalW = String(w);
    canvas.dataset.logicalH = String(h);
    canvas.style.width = `${Math.min(w * scale, bodyW - gutter)}px`;
    const ctx = canvas.getContext('2d');
    const img = ctx.createImageData(canvas.width, canvas.height);
    for (let y = 0; y < canvas.height; y++) {
      const row = Math.floor(y / scale) * out.width;
      for (let x = 0; x < canvas.width; x++) {
        const v = out.buffer[row + Math.floor(x / scale)];
        const o = (y * canvas.width + x) * 4;
        img.data[o] = ((v >> 11) & 0x1F) * 255 / 31;
        img.data[o + 1] = ((v >> 5) & 0x3F) * 255 / 63;
        img.data[o + 2] = (v & 0x1F) * 255 / 31;
        img.data[o + 3] = 255;
      }
    }
    ctx.putImageData(img, 0, 0);

    const g = out.g;
    const inset = field.frame ? 1 : 0;
    const labelRects = [];
    if (g.labelBounds && g.labelBounds.width > 0) {
      labelRects.push({ x: 3, y: 3, w: g.labelBounds.width, h: g.labelBounds.height });
    }
    const iconRect = { x: Math.round(g.valueX), y: Math.round(g.valueY), w: Math.max(1, Math.round(g.valueW)), h: Math.max(1, Math.round(g.valueH)) };
    const background = { rects: [{ x: inset, y: inset, w: w - 2 * inset, h: h - 2 * inset, inset: true }] };

    cachedZones = {
      frameColor: { rects: [{ x: 0, y: 0, w, h }] },
      labelColor: { rects: labelRects.length ? labelRects : [iconRect] },
      bodyColor: { rects: [iconRect] },
      backgroundColor: background
    };
    if (activeColorKey.startsWith('pilotState:')) {
      cachedZones[activeColorKey] = { rects: [iconRect] };
    }
    if (activeColorKey.startsWith('balizaTier:')) {
      cachedZones[activeColorKey] = { rects: [iconRect] };
    }

    drawAppearanceArrows();
  }

  function drawAppearanceArrows() {
    const canvas = section.querySelector('[data-icon-case="single"]');
    if (!canvas || !cachedZones) return;
    const body = canvas.parentElement;
    let rects = [];
    body.querySelectorAll('[data-legend-key]').forEach((row) => {
      const active = row.dataset.legendKey === activeColorKey && Boolean(cachedZones[activeColorKey]);
      row.classList.toggle('is-active', active);
      if (active) rects = cachedZones[activeColorKey].rects;
    });
    window.JWPLCZoneIndicator?.draw(body, canvas, rects);
  }

  function sync() {
    const field = selectedIcon();
    if (!field) return;
    applyTabs();
    const info = iconInfo(field.iconId);
    const entry = entryFor(field);

    const isPilot = field.iconId === 'pilotos/piloto';
    const isBaliza = field.iconId === 'pilotos/baliza';

    let effectiveCount;
    if (isPilot) {
      const isUint = field.iconVarType === 'uint8';
      effectiveCount = isUint ? Math.max(2, Math.min(8, Number(field.stateCount || field.stateColors?.length || 2))) : 2;
      field.stateCount = effectiveCount;
      if (!Array.isArray(field.stateColors)) field.stateColors = [0x4208, 0x07E0];
      const defaultPalette = [0x4208, 0x07E0, 0xF800, 0xFDE0, 0x041F, 0xFFFF, 0x07FF, 0x8010];
      while (field.stateColors.length < effectiveCount) {
        field.stateColors.push(defaultPalette[field.stateColors.length] || 0x07E0);
      }
      if (field.stateColors.length > effectiveCount) field.stateColors.splice(effectiveCount);
      if (!Array.isArray(field.stateNames)) field.stateNames = ['Apagado', 'Encendido'];
      while (field.stateNames.length < effectiveCount) {
        field.stateNames.push(`Estado ${field.stateNames.length}`);
      }
      if (field.stateNames.length > effectiveCount) field.stateNames.splice(effectiveCount);
    } else {
      effectiveCount = entry?.states?.length || info?.stateNames?.length || 0;
    }

    const names = isPilot
      ? field.stateNames.slice(0, effectiveCount)
      : ((field.stateNames && field.stateNames.length >= effectiveCount)
          ? field.stateNames.slice(0, effectiveCount)
          : (info?.stateNames ? info.stateNames.slice(0, effectiveCount) : []));

    const curState = core.clampState(entry, field.iconState, effectiveCount);
    field.iconState = curState;

    section.querySelectorAll('[data-icon-prop]').forEach((input) => {
      if (document.activeElement !== input) input.value = field[input.dataset.iconProp] ?? '';
    });
    const set = (name, text) => section.querySelectorAll(`[data-icon-readout="${name}"]`).forEach((el) => { el.textContent = text; });
    set('name', info?.name || field.iconId || '—');
    const hasAnim = entry?.states?.some((s) => s.frameCount > 1 || (s.frames && s.frames.length > 1));
    set('category', `${catalog.categories.find((c) => c.id === info?.category)?.name || ''} · ${names.length} estados${hasAnim ? ' · animado' : ''}${core.findIcon(catalog, field.iconId) ? '' : ' · copia del proyecto'}`);
    set('flash', entry ? `${core.flashBytes(entry)} B` : '—');
    set('fields', `${(editor()?.getAllFields?.() || []).filter((f) => !['LINE', 'RECT', 'ELLIPSE', 'TRIANGLE', 'POLYGON', 'RAW_TEXT', 'NAV_BAR', 'MENU'].includes(f.type)).length}/32`);
    const g = editor()?.getFieldBounds?.(field);
    set('box', g ? `${g.width} × ${g.height} px` : '—');
    drawThumb(section.querySelector('[data-icon-current]'), entry, field.iconState, num(field.backgroundColor), field._animFrame || 0, field);

    section.querySelector('[data-icon-sizes]').innerHTML = sizesFor(field.iconId).map((s) =>
      `<button type="button" class="insp-chip${s === Number(field.iconSize) ? ' active' : ''}" data-icon-size="${s}">${s} px</button>`).join('');
    section.querySelectorAll('[data-icon-set]').forEach((b) => {
      const prop = b.dataset.iconSet;
      const value = prop === 'frame' ? (b.dataset.value === '1') === Boolean(field.frame)
        : prop === 'transparentBackground' ? (b.dataset.value === '1') === Boolean(field.transparentBackground)
        : prop === 'labelSize' ? Number(b.dataset.value) === Math.max(1, num(field.labelSize, 1))
          : prop === 'layout' ? b.dataset.value === (field.layout === 'INLINE' ? 'INLINE' : 'STACKED')
            : b.dataset.value === (field.iconVarType === 'uint8' ? 'uint8' : 'bool');
      b.classList.toggle('active', value);
    });

    // Baliza vs otros: ocultar "Estado actual" en baliza porque tiene sus toggles en vivo abajo
    const quickTitle = section.querySelector('[data-icon-quick-states-title]');
    const quickStates = section.querySelector('[data-icon-quick-states]');
    const statesCard = section.querySelector('[data-icon-states-card]');
    if (quickTitle) quickTitle.hidden = isBaliza;
    if (quickStates) quickStates.hidden = isBaliza;
    if (statesCard) statesCard.hidden = isBaliza;

    if (!isBaliza && quickStates) {
      quickStates.innerHTML = names.map((n, i) => {
        const st = entry?.states?.[i];
        const isAnim = (st?.frameCount > 1) || (st?.frames?.length > 1);
        const badge = isAnim ? `<span class="icon-anim-badge" title="Animado">▶</span>` : '';
        return `<button type="button" class="insp-chip${i === curState ? ' active' : ''}" data-icon-state="${i}">
          <canvas data-icon-quick-state-thumb="${i}" style="width:14px;height:14px;vertical-align:-2px;margin-right:4px;image-rendering:pixelated;"></canvas>
          ${escapeHtml(n)}${badge}
        </button>`;
      }).join('');
      quickStates.querySelectorAll('[data-icon-quick-state-thumb]').forEach((c) => {
        const idx = Number(c.dataset.iconQuickStateThumb);
        drawThumb(c, entry, idx, num(field.backgroundColor), idx === curState ? (field._animFrame || 0) : 0, field);
      });
    }

    if (!isBaliza && statesCard) {
      const states = statesCard.querySelector('[data-icon-states]');
      if (states) {
        states.innerHTML = names.map((n, i) => {
          const st = entry?.states?.[i];
          const isAnim = st && ((st.frameCount > 1) || (st.frames && st.frames.length > 1));
          const count = st?.frameCount || st?.frames?.length || 1;
          const badge = isAnim ? `<span class="icon-anim-badge" title="${count} cuadros · ${field.animIntervalMs || st.frameIntervalMs || 120} ms">▶ ${count}f</span>` : '';
          return `<button type="button" class="bar-visual-btn icon-state${i === curState ? ' active' : ''}" data-icon-state="${i}">
            <canvas data-icon-state-thumb="${i}"></canvas><span>${escapeHtml(n)}${badge}</span><small>${field.iconVarType === 'uint8' ? i : (i === 0 ? 'false' : i === 1 ? 'true' : `${i} (no alcanza)`)}</small>
          </button>`;
        }).join('');
        states.querySelectorAll('[data-icon-state-thumb]').forEach((c) => {
          const idx = Number(c.dataset.iconStateThumb);
          drawThumb(c, entry, idx, num(field.backgroundColor), idx === curState ? (field._animFrame || 0) : 0, field);
        });
      }
    }

    // Datos: tabla Valor -> imagen
    const mapTable = section.querySelector('[data-icon-map]');
    if (mapTable) {
      mapTable.innerHTML = names.map((n, i) =>
        `<tr><td><code>${field.iconVarType === 'uint8' ? i : (i === 0 ? 'false' : i === 1 ? 'true' : '—')}</code></td><td><canvas data-icon-map-thumb="${i}"></canvas></td><td>${escapeHtml(n)}</td></tr>`).join('');
      mapTable.querySelectorAll('[data-icon-map-thumb]').forEach((c) => drawThumb(c, entry, Number(c.dataset.iconMapThumb), num(field.backgroundColor), 0, field));
    }
    const warn = section.querySelector('[data-icon-warn]');
    if (warn) {
      warn.hidden = !(field.iconVarType !== 'uint8' && names.length > 2);
      warn.textContent = `Con bool sólo se ven «${names[0]}» y «${names[1]}». Usa uint8_t para los ${names.length} estados.`;
    }

    // --- Piloto ---
    const pilotCard = section.querySelector('[data-icon-pilot-card]');
    if (pilotCard) {
      pilotCard.hidden = !isPilot;
      if (isPilot) {
        const isUint = field.iconVarType === 'uint8';
        pilotCard.querySelectorAll('[data-pilot-mode]').forEach((b) => {
          b.classList.toggle('active', (b.dataset.pilotMode === 'uint8') === isUint);
        });
        const statesSec = pilotCard.querySelector('[data-pilot-states-section]');
        if (statesSec) statesSec.hidden = !isUint;
        pilotCard.querySelectorAll('[data-pilot-states-set]').forEach((b) => {
          b.classList.toggle('active', Number(b.dataset.pilotStatesSet) === effectiveCount);
        });

        const pilotList = pilotCard.querySelector('[data-pilot-states-list]');
        if (pilotList) {
          pilotList.innerHTML = Array.from({ length: effectiveCount }, (_, idx) => {
            const cVal = core.to565(field.stateColors[idx] !== undefined ? field.stateColors[idx] : (idx === 0 ? 0x4208 : 0x07E0));
            const isActColor = activeColorKey === `pilotState:${idx}`;
            return `<div class="pilot-state-row" style="display:flex;align-items:center;gap:6px;margin-bottom:6px">
              <span style="font-size:11px;font-weight:700;width:18px;text-align:center;opacity:.8">${idx}</span>
              <input class="field-input" data-pilot-state-name="${idx}" value="${escapeHtml(names[idx] || `Estado ${idx}`)}" placeholder="Nombre" style="flex:1" maxlength="20" />
              <button type="button" class="insp-chip${isActColor ? ' active' : ''}" data-pilot-state-idx="${idx}" title="Cambiar color del estado ${idx}" style="padding:4px 8px">
                <i style="display:inline-block;width:12px;height:12px;border-radius:2px;background:${css565(cVal)};border:1px solid rgba(255,255,255,.3)"></i>
              </button>
              <button type="button" class="insp-chip${idx === curState ? ' active' : ''}" data-icon-state="${idx}" title="Probar estado en lienzo">Probar</button>
            </div>`;
          }).join('');
        }
      }
    }

    // --- Baliza ---
    const balizaCard = section.querySelector('[data-icon-baliza-card]');
    if (balizaCard) {
      balizaCard.hidden = !isBaliza;
      if (isBaliza) {
        const tierCount = Math.max(1, Math.min(4, Number(field.tierCount || 4)));
        field.tierCount = tierCount;
        if (!Array.isArray(field.tierColors)) field.tierColors = [0x07E0, 0xFDE0, 0xF800, 0x041F];
        balizaCard.querySelectorAll('[data-baliza-tier-count]').forEach((b) => {
          b.classList.toggle('active', Number(b.dataset.balizaTierCount) === tierCount);
        });

        const tierLabels = ['Verde', 'Ámbar', 'Rojo', 'Azul'];
        const listEl = balizaCard.querySelector('[data-baliza-tiers-list]');
        if (listEl) {
          let listHtml = '';
          for (let t = tierCount - 1; t >= 0; t -= 1) {
            const cVal = core.to565(field.tierColors[t] !== undefined ? field.tierColors[t] : 0xFFFF);
            const isTierOn = Boolean((Number(field.iconState || 0) >> t) & 1);
            const isColorActive = activeColorKey === `balizaTier:${t}`;
            listHtml += `<div class="baliza-tier-row">
              <div class="baliza-tier-info">
                <i style="background:${css565(cVal)}"></i>
                <strong>Piso ${t + 1} (${tierLabels[t]})</strong>
              </div>
              <div style="display:flex;align-items:center;gap:6px">
                <button type="button" class="baliza-tier-toggle${isTierOn ? ' active' : ''}" data-baliza-toggle-tier="${t}">${isTierOn ? 'ENCENDIDO' : 'APAGADO'}</button>
                <button type="button" class="insp-chip${isColorActive ? ' active' : ''}" data-baliza-tier-color-idx="${t}">Color</button>
              </div>
            </div>`;
          }
          listEl.innerHTML = listHtml;
        }
      }
    }

    // --- Baliza en panel Datos ---
    const balizaData = section.querySelector('[data-icon-baliza-data]');
    const stdData = section.querySelector('[data-icon-standard-data]');
    if (balizaData) {
      balizaData.hidden = !isBaliza;
      if (stdData) stdData.hidden = isBaliza && field.balizaMode === 'independent';
      if (isBaliza) {
        const isIndep = field.balizaMode === 'independent';
        balizaData.querySelectorAll('[data-baliza-mode]').forEach((b) => {
          b.classList.toggle('active', (b.dataset.balizaMode === 'independent') === isIndep);
        });
        const tierVarsCont = balizaData.querySelector('[data-baliza-tier-vars-container]');
        if (tierVarsCont) {
          tierVarsCont.hidden = !isIndep;
          if (isIndep) {
            const tierCount = Math.max(1, Math.min(4, Number(field.tierCount || 4)));
            const tierLabels = ['Piso 1 (Verde)', 'Piso 2 (Ámbar)', 'Piso 3 (Rojo)', 'Piso 4 (Azul)'];
            if (!Array.isArray(field.tierVariables)) field.tierVariables = [];
            const tierList = balizaData.querySelector('[data-baliza-tier-vars-list]');
            if (tierList) {
              tierList.innerHTML = Array.from({ length: tierCount }, (_, t) => {
                const varName = field.tierVariables[t] || `${field.variable || 'torre'}_p${t + 1}`;
                field.tierVariables[t] = varName;
                return `<div class="jwplc-tier-var-row">
                  <label class="field-label">${tierLabels[t]}
                    <input class="field-input code-input" data-tier-prop="variable" data-tier-index="${t}" value="${escapeHtml(varName)}" maxlength="32" />
                  </label>
                </div>`;
              }).join('');
            }
          }
        }
      }
    }

    // --- Velocidad de animación ---
    const animCard = section.querySelector('[data-icon-anim-card]');
    if (animCard) {
      animCard.hidden = !hasAnim;
      if (hasAnim) {
        const curInterval = Number(field.animIntervalMs) || 120;
        animCard.querySelectorAll('[data-anim-preset]').forEach((b) => {
          const v = b.dataset.animPreset;
          if (v === 'reset') b.classList.toggle('active', !field.animIntervalMs);
          else b.classList.toggle('active', Number(v) === curInterval && Boolean(field.animIntervalMs));
        });
        const animInput = animCard.querySelector('[data-icon-anim-input]');
        if (animInput && document.activeElement !== animInput) {
          animInput.value = field.animIntervalMs || '';
        }
      }
    }

    // --- Apariencia (Estilo Imagen 2 con Vista interactiva, leyenda y recuadro A1) ---
    const legendRows = [
      { key: 'labelColor', label: 'Etiqueta', letter: 'T', name: 'Etiqueta', desc: 'texto', color: num(field.labelColor) & 0xFFFF }
    ];
    if (isPilot) {
      const pColor = core.to565(field.stateColors?.[curState] !== undefined ? field.stateColors[curState] : 0x07E0);
      legendRows.push({
        key: `pilotState:${curState}`,
        label: `Piloto (Estado ${curState})`,
        letter: 'C',
        name: `Piloto (${names[curState] || `Estado ${curState}`})`,
        desc: 'luz del estado actual',
        color: pColor
      });
    } else {
      const bColor = field.bodyColor !== undefined ? (core.to565(field.bodyColor) || 0) : 0x5555;
      legendRows.push({
        key: 'bodyColor',
        label: 'Icono / Cuerpo',
        letter: 'C',
        name: 'Icono / Cuerpo',
        desc: field.bodyColor !== undefined ? 'color personalizado' : 'color original',
        color: bColor
      });
    }
    legendRows.push({ key: 'backgroundColor', label: 'Fondo', letter: 'F', name: 'Fondo', desc: 'interior del campo', color: num(field.backgroundColor) & 0xFFFF });
    if (field.frame) {
      legendRows.push({ key: 'frameColor', label: 'Marco', letter: 'M', name: 'Marco', desc: 'borde exterior', color: num(field.frameColor) & 0xFFFF });
    }

    if (!legendRows.some((r) => r.key === activeColorKey)) {
      activeColorKey = legendRows[0].key;
    }

    const legendEl = section.querySelector('[data-icon-case-legend]');
    if (legendEl) {
      legendEl.innerHTML = legendRows.map((r) => {
        const isActive = r.key === activeColorKey;
        const v = r.color;
        const textColor = ((v >> 11) + ((v >> 5) & 0x3F) / 2 + (v & 0x1F)) > 40 ? '#000' : '#fff';
        return `<div class="menu-legend-row${isActive ? ' is-active' : ''}" data-legend-key="${r.key}">
          <span class="menu-legend-label">${escapeHtml(r.label)}</span>
          <button type="button" class="menu-swatch${isActive ? ' active' : ''}" data-icon-swatch="${r.key}" title="${escapeHtml(r.name)}" style="background:${css565(v)};color:${textColor}">
            <span>${r.letter}</span>
          </button>
        </div>`;
      }).join('');
    }

    const activeRow = legendRows.find((r) => r.key === activeColorKey) || legendRows[0];
    const nameEl = section.querySelector('[data-icon-editing-name]');
    const descEl = section.querySelector('[data-icon-editing-desc]');
    const swatchEl = section.querySelector('[data-icon-editing-swatch]');
    if (nameEl) nameEl.textContent = activeRow.name;
    if (descEl) descEl.textContent = `· ${activeRow.desc}`;
    if (swatchEl) swatchEl.style.background = css565(activeRow.color);
    colorEditor?.set(activeRow.color, activeRow.name);

    drawAppearance();

    const pages = editor()?.getPages?.() || [];
    section.querySelector('[data-icon-pages]').innerHTML = pages.map((p) =>
      `<button type="button" class="insp-chip${Number(p.id) === Number(field.page || 0) ? ' active' : ''}" data-icon-page="${p.id}">${String(Number(p.id) + 1).padStart(2, '0')} · ${escapeHtml(p.name)}</button>`).join('');
    section.querySelector('[data-icon-contract]').textContent = contractText(field);
  }

  section.addEventListener('click', (e) => {
    const t = e.target;
    const field = selectedIcon();
    if (!field) return;
    const main = t.closest('[data-icon-main]');
    if (main) { tabs.main = main.dataset.iconMain; applyTabs(); return; }
    const sub = t.closest('[data-icon-sub]');
    if (sub) { tabs.sub = sub.dataset.iconSub; applyTabs(); return; }
    const action = t.closest('[data-icon-action]')?.dataset.iconAction;
    if (action === 'duplicate') { editor()?.duplicateSelectedField?.(); return; }
    if (action === 'delete') { editor()?.deleteSelectedField?.(); return; }
    const setBtn = t.closest('[data-icon-set]');
    if (setBtn) {
      const prop = setBtn.dataset.iconSet;
      const v = setBtn.dataset.value;
      mutate((f) => {
        if (prop === 'frame') f.frame = v === '1';
        else if (prop === 'transparentBackground') f.transparentBackground = v === '1';
        else if (prop === 'labelSize') f.labelSize = Number(v);
        else if (prop === 'layout') f.layout = v;
        else if (prop === 'iconVarType') f.iconVarType = v;
      });
      return;
    }
    const size = t.closest('[data-icon-size]');
    if (size) { mutate((f) => { f.iconSize = Number(size.dataset.iconSize); }); return; }
    const state = t.closest('[data-icon-state]');
    if (state) { mutate((f) => { f.iconState = Number(state.dataset.iconState); f._animFrame = 0; f._lastAnimTick = 0; }); return; }
    const color = t.closest('[data-icon-color]');
    if (color) { activeColorKey = color.dataset.iconColor; sync(); return; }

    // Apariencia: clic en fila de leyenda o swatch
    const legendRowEl = t.closest('[data-legend-key]');
    if (legendRowEl) {
      activeColorKey = legendRowEl.dataset.legendKey;
      sync();
      return;
    }
    const swatchEl = t.closest('[data-icon-swatch]');
    if (swatchEl) {
      activeColorKey = swatchEl.dataset.iconSwatch;
      sync();
      return;
    }

    // Piloto mode
    const pilotModeBtn = t.closest('[data-pilot-mode]');
    if (pilotModeBtn) {
      const mode = pilotModeBtn.dataset.pilotMode;
      mutate((f) => {
        f.iconVarType = mode;
        if (mode === 'bool') f.stateCount = 2;
        else f.stateCount = Math.max(2, f.stateColors?.length || 2);
      });
      sync();
      return;
    }

    // Piloto states quick set
    const pilotStatesSetBtn = t.closest('[data-pilot-states-set]');
    if (pilotStatesSetBtn) {
      const count = Number(pilotStatesSetBtn.dataset.pilotStatesSet);
      mutate((f) => {
        f.iconVarType = 'uint8';
        f.stateCount = count;
        if (!Array.isArray(f.stateColors)) f.stateColors = [0x4208, 0x07E0];
        const defaultPaletteColors = [0x4208, 0x07E0, 0xF800, 0xFDE0, 0x041F, 0xFFFF, 0x07FF, 0x8010];
        while (f.stateColors.length < count) {
          f.stateColors.push(defaultPaletteColors[f.stateColors.length] || 0x07E0);
        }
        if (f.stateColors.length > count) f.stateColors.splice(count);
        if (!Array.isArray(f.stateNames)) f.stateNames = ['Apagado', 'Encendido'];
        while (f.stateNames.length < count) {
          f.stateNames.push(`Estado ${f.stateNames.length}`);
        }
        if (f.stateNames.length > count) f.stateNames.splice(count);
        if (f.iconState >= count) f.iconState = count - 1;
      });
      sync();
      return;
    }

    // Anim preset button
    const animPresetBtn = t.closest('[data-anim-preset]');
    if (animPresetBtn) {
      const val = animPresetBtn.dataset.animPreset;
      mutate((f) => {
        if (val === 'reset') delete f.animIntervalMs;
        else f.animIntervalMs = Number(val);
      });
      sync();
      return;
    }

    // Piloto state chip (select active state for color editing)
    const pilotStateChip = t.closest('[data-pilot-state-idx]');
    if (pilotStateChip) {
      activeColorKey = `pilotState:${pilotStateChip.dataset.pilotStateIdx}`;
      sync();
      return;
    }

    // Baliza tier count
    const balizaTierCountBtn = t.closest('[data-baliza-tier-count]');
    if (balizaTierCountBtn) {
      const count = Number(balizaTierCountBtn.dataset.balizaTierCount);
      mutate((f) => {
        f.tierCount = count;
        const maxMask = (1 << count) - 1;
        f.iconState = Number(f.iconState || 0) & maxMask;
      });
      sync();
      return;
    }

    // Baliza toggle tier (ON/OFF in live preview)
    const balizaToggleBtn = t.closest('[data-baliza-toggle-tier]');
    if (balizaToggleBtn) {
      const tierIdx = Number(balizaToggleBtn.dataset.balizaToggleTier);
      mutate((f) => {
        const cur = Number(f.iconState || 0);
        const count = Math.max(1, Math.min(4, Number(f.tierCount || 4)));
        const maxMask = (1 << count) - 1;
        f.iconState = (cur ^ (1 << tierIdx)) & maxMask;
      });
      sync();
      return;
    }

    // Baliza tier color chip
    const balizaTierColorBtn = t.closest('[data-baliza-tier-color-idx]');
    if (balizaTierColorBtn) {
      activeColorKey = `balizaTier:${balizaTierColorBtn.dataset.balizaTierColorIdx}`;
      sync();
      return;
    }

    // Baliza mode (bitmask vs independent)
    const balizaModeBtn = t.closest('[data-baliza-mode]');
    if (balizaModeBtn) {
      const mode = balizaModeBtn.dataset.balizaMode;
      mutate((f) => { f.balizaMode = mode; });
      sync();
      return;
    }

    // Quick palette apply color
    const applyColorBtn = t.closest('[data-apply-color]');
    if (applyColorBtn) {
      const val = applyColorBtn.dataset.applyColor;
      handleColorEdit(val, true);
      sync();
      return;
    }

    // Body color activate chip
    const bodyColorAct = t.closest('[data-body-color-activate]');
    if (bodyColorAct) {
      activeColorKey = 'bodyColor';
      sync();
      return;
    }

    // Body color preset
    const bodyPresetBtn = t.closest('[data-body-color-preset]');
    if (bodyPresetBtn) {
      const preset = bodyPresetBtn.dataset.bodyColorPreset;
      mutate((f) => {
        if (preset === 'reset') delete f.bodyColor;
        else f.bodyColor = preset;
      });
      activeColorKey = 'bodyColor';
      sync();
      return;
    }

    const page = t.closest('[data-icon-page]');
    if (page) { editor()?.moveSelectedFieldToPage?.(Number(page.dataset.iconPage)); }
  });

  function applyInput(t, commit) {
    if (t.matches('[data-pilot-state-name]')) {
      const idx = Number(t.dataset.pilotStateName);
      mutate((f) => {
        if (!Array.isArray(f.stateNames)) f.stateNames = ['Apagado', 'Encendido'];
        f.stateNames[idx] = t.value;
      }, commit);
      return;
    }
    if (t.matches('[data-icon-anim-input]')) {
      const val = Math.max(30, Math.min(1000, Number(t.value) || 120));
      mutate((f) => { f.animIntervalMs = val; }, commit);
      return;
    }
    if (t.matches('[data-tier-prop="variable"]')) {
      const tierIdx = Number(t.dataset.tierIndex) || 0;
      mutate((f) => {
        if (!Array.isArray(f.tierVariables)) f.tierVariables = [];
        f.tierVariables[tierIdx] = sanitizeSymbol(t.value, `torre_p${tierIdx + 1}`);
      }, commit);
      return;
    }
    if (!t.matches('[data-icon-prop]')) return;
    const prop = t.dataset.iconProp;
    mutate((f) => {
      if (prop === 'x') f.x = clamp(Math.trunc(num(t.value, f.x)), 0, SCREEN_W - 1);
      else if (prop === 'y') f.y = clamp(Math.trunc(num(t.value, f.y)), 0, SCREEN_H - 1);
      else if (prop === 'id') f.id = sanitizeSymbol(t.value, f.id);
      else if (prop === 'variable') f.variable = sanitizeSymbol(t.value, f.variable);
      else if (prop === 'iconId') {
        const icon = core.findIcon(catalog, t.value);
        if (!icon) return;
        f.iconId = icon.id;
        f.iconSize = core.resolveSize(icon, f.iconSize);
        f.iconState = Math.min(num(f.iconState), icon.stateNames.length - 1); f._animFrame = 0; f._lastAnimTick = 0;
        if (icon.stateNames.length > 2) f.iconVarType = 'uint8';
      } else f[prop] = t.value;
    }, commit);
  }
  section.addEventListener('input', (e) => { if (e.target.tagName !== 'SELECT') applyInput(e.target, false); });
  section.addEventListener('change', (e) => applyInput(e.target, true));

  window.addEventListener('jwplc:editor-refresh', sync);


  // ---------------------------------------------------------------------------
  // Bucle de animación en vivo de iconos multi-cuadro (Alpha16)
  // ---------------------------------------------------------------------------
  function updateAnimationLoop() {
    const ed = editor();
    if (!ed) return;
    const page = ed.getActivePage?.() ?? 0;
    const fields = ed.getFieldsForPage?.(page) || [];
    let needsRedraw = false;
    const now = performance.now();

    for (const f of fields) {
      if (f.type !== 'ICON' || f.hidden) continue;
      const entry = entryFor(f);
      if (!entry) continue;
      const stateIndex = core.clampState(entry, f.iconState);
      const st = entry.states[stateIndex];
      const count = st?.frameCount || (Array.isArray(st?.frames) ? st.frames.length : 1);
      if (count > 1) {
        const interval = Number(f.animIntervalMs) || st.frameIntervalMs || st.intervalMs || 120;
        if (!f._lastAnimTick) f._lastAnimTick = now;
        if (now - f._lastAnimTick >= interval) {
          f._animFrame = ((f._animFrame || 0) + 1) % count;
          f._lastAnimTick = now;
          needsRedraw = true;
        }
      } else if (f._animFrame) {
        f._animFrame = 0;
        needsRedraw = true;
      }
    }

    if (needsRedraw) {
      if (ed.redrawCanvasOnly) ed.redrawCanvasOnly();
      else ed.render?.();

      const sel = selectedIcon();
      if (sel) {
        const selEntry = entryFor(sel);
        const selState = core.clampState(selEntry, sel.iconState);
        const selSt = selEntry?.states?.[selState];
        const selCount = selSt?.frameCount || (Array.isArray(selSt?.frames) ? selSt.frames.length : 1);
        if (selCount > 1) {
          const curr = section.querySelector('[data-icon-current]');
          if (curr) drawThumb(curr, selEntry, sel.iconState, num(sel.backgroundColor), sel._animFrame || 0, sel);
          const stThumb = section.querySelector(`[data-icon-state-thumb="${selState}"]`);
          if (stThumb) drawThumb(stThumb, selEntry, selState, num(sel.backgroundColor), sel._animFrame || 0, sel);
          const qThumb = section.querySelector(`[data-icon-quick-state-thumb="${selState}"]`);
          if (qThumb) drawThumb(qThumb, selEntry, selState, num(sel.backgroundColor), sel._animFrame || 0, sel);
        }
      }
    }
  }

  const animTimer = setInterval(updateAnimationLoop, 40);
  window.addEventListener('beforeunload', () => clearInterval(animTimer));

  window.JWPLCHMIIcons = {
    catalog,
    iconInfo,
    entryFor,
    sizesFor,
    stateCount,
    assetsFor,
    loadAssets,
    addIcon,
    symbolForField,
    sync
  };
})();
