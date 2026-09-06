(() => {
  'use strict';

  if (window.JWPLCHMIDesktopResponsive) return;

  const STORAGE = 'jwplc-hmi-layout.';
  const WIDE_RATIO = 0.70;
  const COMPACT_RATIO = 0.38;
  const ZOOMS = [1, 2, 3, 4, 6, 8];

  let mode = null;
  let rightView = localStorage.getItem(`${STORAGE}rightView`) || 'inspector';
  let leftDrawerOpen = false;
  let rightDrawerOpen = false;
  let activeLeftSection = 'pages';
  let applyingZoom = false;

  const $ = (selector, root = document) => root.querySelector(selector);
  const $$ = (selector, root = document) => [...root.querySelectorAll(selector)];

  const toolbar = () => $('.toolbar');
  const statusbar = () => $('.statusbar');
  const workspace = () => $('.workspace');
  const leftPanel = () => $('.left-panel');
  const rightPanel = () => $('.right-panel');

  function screenRatio() {
    const available = Number(screen?.availWidth || screen?.width || innerWidth || 1);
    const current = Number(outerWidth || innerWidth || available);
    return Math.max(0.10, Math.min(1.25, current / Math.max(1, available)));
  }

  function chooseMode() {
    const ratio = screenRatio();
    if (ratio < COMPACT_RATIO || innerWidth < 620) return 'compact';
    if (ratio < WIDE_RATIO) return 'medium';
    return 'wide';
  }

  function linkedSketchName() {
    const apiName = window.JWPLCHMIProject?.linkedSketchName?.();
    if (apiName) return apiName;
    const text = $('#sketchLinkStatus')?.textContent || '';
    const match = text.match(/^Sketch:\s*([^·]+?)(?:\s*·|$)/);
    const name = match?.[1]?.trim();
    return name && name !== 'sin vincular' ? name : null;
  }

  function setTextIfChanged(node, text) {
    if (node && node.textContent !== text) node.textContent = text;
  }

  function syncSketchIdentity() {
    const name = linkedSketchName();
    const button = $('#linkSketchButton');
    if (button) {
      const label = name ? `Sketch: ${name}` : 'Vincular sketch…';
      setTextIfChanged(button, label);
      button.title = name
        ? `Sketch vinculado: ${name}. Clic para cambiar la vinculación.`
        : 'Seleccionar la carpeta del sketch Arduino';
    }
    const title = name ? `${name} · JWPLC HMI Designer — Alpha11` : 'JWPLC HMI Designer — Alpha11';
    if (document.title !== title) document.title = title;
  }

  function moveGateToStatusbar() {
    const gate = $('.gate');
    const bar = statusbar();
    const grow = bar && $('.grow', bar);
    if (!gate || !bar || gate.parentElement === bar) return;
    gate.classList.add('responsive-gate-status');
    if (grow) bar.insertBefore(gate, grow);
    else bar.appendChild(gate);
  }

  function moveLiveStatusToStatusbar() {
    const live = $('#liveStatus');
    const bar = statusbar();
    const grow = bar && $('.grow', bar);
    if (!live || !bar || live.parentElement === bar) return;
    live.classList.add('responsive-live-status');
    if (grow) bar.insertBefore(live, grow);
    else bar.appendChild(live);
  }

  function markToolbarCommands() {
    const bar = toolbar();
    if (!bar) return;
    $$('button', bar).forEach((button) => {
      const text = button.textContent.trim();
      if (text === 'Deshacer') button.classList.add('cmd-undo');
      if (text === 'Rehacer') button.classList.add('cmd-redo');
    });
  }

  function proxyTarget(key) {
    const selectors = {
      new: '#newProjectButton', open: '#openProjectButton', undo: '.cmd-undo',
      redo: '.cmd-redo', fit: '#fitButton', sketch: '#linkSketchButton', install: '#installAppButton'
    };
    return selectors[key] ? $(selectors[key]) : null;
  }

  function ensureOverflow() {
    const bar = toolbar();
    if (!bar || $('#responsiveOverflow')) return;
    const wrap = document.createElement('div');
    wrap.id = 'responsiveOverflow';
    wrap.className = 'responsive-overflow';
    wrap.innerHTML = `
      <button id="responsiveOverflowButton" type="button" aria-expanded="false" title="Más acciones">⋯</button>
      <div id="responsiveOverflowMenu" class="responsive-overflow-menu" hidden>
        <button type="button" data-proxy="new">Nuevo proyecto</button>
        <button type="button" data-proxy="open">Abrir proyecto</button>
        <button type="button" data-proxy="undo">Deshacer</button>
        <button type="button" data-proxy="redo">Rehacer</button>
        <button type="button" data-proxy="fit">Ajustar canvas</button>
        <button type="button" data-proxy="sketch">Vincular sketch…</button>
        <button type="button" data-proxy="install">Instalar app</button>
      </div>`;
    bar.appendChild(wrap);

    const toggle = $('#responsiveOverflowButton');
    const menu = $('#responsiveOverflowMenu');
    toggle.addEventListener('click', (event) => {
      event.stopPropagation();
      menu.hidden = !menu.hidden;
      toggle.setAttribute('aria-expanded', menu.hidden ? 'false' : 'true');
      syncOverflow();
    });
    $$('[data-proxy]', menu).forEach((button) => {
      button.addEventListener('click', () => {
        const source = proxyTarget(button.dataset.proxy);
        if (source && !source.disabled && !source.hidden) source.click();
        menu.hidden = true;
        toggle.setAttribute('aria-expanded', 'false');
      });
    });
    document.addEventListener('click', () => {
      if (!menu.hidden) {
        menu.hidden = true;
        toggle.setAttribute('aria-expanded', 'false');
      }
    });
  }

  function syncOverflow() {
    const menu = $('#responsiveOverflowMenu');
    if (!menu) return;
    $$('[data-proxy]', menu).forEach((button) => {
      const source = proxyTarget(button.dataset.proxy);
      button.disabled = !source || source.disabled || source.hidden;
      if (button.dataset.proxy === 'sketch') {
        const name = linkedSketchName();
        setTextIfChanged(button, name ? `Cambiar sketch · ${name}` : 'Vincular sketch…');
      }
    });
  }

  function ensureRightTabs() {
    const panel = rightPanel();
    if (!panel || $('#responsiveRightTabs')) return;
    const tabs = document.createElement('div');
    tabs.id = 'responsiveRightTabs';
    tabs.className = 'responsive-right-tabs';
    tabs.innerHTML = '<button type="button" data-right-view="inspector">Inspector</button><button type="button" data-right-view="preview">Vista 1:1</button>';
    panel.prepend(tabs);
    $$('[data-right-view]', tabs).forEach((button) => button.addEventListener('click', () => setRightView(button.dataset.rightView, true)));
  }

  function setRightView(view, persist = false) {
    rightView = view === 'preview' ? 'preview' : 'inspector';
    if (persist) localStorage.setItem(`${STORAGE}rightView`, rightView);
    document.body.classList.toggle('right-view-preview', rightView === 'preview');
    document.body.classList.toggle('right-view-inspector', rightView === 'inspector');
    $$('[data-right-view]').forEach((button) => button.classList.toggle('active', button.dataset.rightView === rightView));
  }

  function ensureCompactRightButtons() {
    const bar = toolbar();
    if (!bar) return;
    if (!$('#compactInspectorButton')) {
      const button = document.createElement('button');
      button.id = 'compactInspectorButton';
      button.type = 'button';
      button.textContent = 'Inspector';
      button.addEventListener('click', () => {
        const sameOpenView = rightDrawerOpen && rightView === 'inspector';
        setRightView('inspector', true);
        rightDrawerOpen = !sameOpenView;
        leftDrawerOpen = false;
        syncDrawers();
      });
      bar.appendChild(button);
    }
    if (!$('#compactPreviewButton')) {
      const button = document.createElement('button');
      button.id = 'compactPreviewButton';
      button.type = 'button';
      button.textContent = '1:1';
      button.title = 'Vista previa 1:1';
      button.addEventListener('click', () => {
        const sameOpenView = rightDrawerOpen && rightView === 'preview';
        setRightView('preview', true);
        rightDrawerOpen = !sameOpenView;
        leftDrawerOpen = false;
        syncDrawers();
      });
      bar.appendChild(button);
    }
  }

  function leftSections() {
    const blocks = $$('.left-panel .nav-block');
    return { pages: blocks[0], objects: blocks[1], components: blocks[2], tools: blocks[3], dev: $('.left-panel .dev-actions') };
  }

  function ensureLeftRail() {
    const root = workspace();
    if (!root || $('#responsiveLeftRail')) return;
    const rail = document.createElement('nav');
    rail.id = 'responsiveLeftRail';
    rail.className = 'responsive-left-rail';
    rail.innerHTML = `
      <button type="button" data-left-section="pages" title="Páginas">P</button>
      <button type="button" data-left-section="objects" title="Objetos">O</button>
      <button type="button" data-left-section="components" title="Componentes">C</button>
      <button type="button" data-left-section="tools" title="Herramientas">T</button>`;
    root.prepend(rail);
    $$('[data-left-section]', rail).forEach((button) => button.addEventListener('click', () => {
      activeLeftSection = button.dataset.leftSection;
      leftDrawerOpen = true;
      rightDrawerOpen = false;
      const section = leftSections()[activeLeftSection];
      section?.classList.remove('section-collapsed');
      syncDrawers();
      setTimeout(() => section?.scrollIntoView({ block: 'start' }), 0);
    }));
  }

  function ensureScrim() {
    if ($('#responsiveScrim')) return;
    const scrim = document.createElement('button');
    scrim.id = 'responsiveScrim';
    scrim.className = 'responsive-scrim';
    scrim.type = 'button';
    scrim.title = 'Cerrar panel';
    scrim.addEventListener('click', () => {
      leftDrawerOpen = false;
      rightDrawerOpen = false;
      syncDrawers();
    });
    document.body.appendChild(scrim);
  }

  function syncDrawers() {
    const compact = mode === 'compact';
    document.body.classList.toggle('compact-left-open', compact && leftDrawerOpen);
    document.body.classList.toggle('compact-right-open', compact && rightDrawerOpen);
    document.body.classList.toggle('responsive-drawer-open', compact && (leftDrawerOpen || rightDrawerOpen));
    $$('[data-left-section]').forEach((button) => button.classList.toggle('active', leftDrawerOpen && button.dataset.leftSection === activeLeftSection));
    $('#compactInspectorButton')?.classList.toggle('active', compact && rightDrawerOpen && rightView === 'inspector');
    $('#compactPreviewButton')?.classList.toggle('active', compact && rightDrawerOpen && rightView === 'preview');
  }

  function bindCollapsibleSections() {
    const map = leftSections();
    ['components', 'tools', 'dev'].forEach((key) => {
      const section = map[key];
      const heading = section?.querySelector(':scope > h2');
      if (!section || !heading || section.dataset.responsiveCollapsible === '1') return;
      section.dataset.responsiveCollapsible = '1';
      section.classList.add('responsive-collapsible');
      heading.title = 'Expandir o contraer sección';
      heading.addEventListener('click', () => {
        section.classList.toggle('section-collapsed');
        localStorage.setItem(`${STORAGE}left.${key}.collapsed`, section.classList.contains('section-collapsed') ? '1' : '0');
      });
    });
  }

  function applyLeftDefaults(nextMode) {
    const map = leftSections();
    ['components', 'tools'].forEach((key) => {
      const stored = localStorage.getItem(`${STORAGE}left.${key}.collapsed`);
      map[key]?.classList.toggle('section-collapsed', stored == null ? nextMode !== 'wide' : stored === '1');
    });
    const storedDev = localStorage.getItem(`${STORAGE}left.dev.collapsed`);
    map.dev?.classList.toggle('section-collapsed', storedDev == null ? true : storedDev === '1');
  }

  function bindBottomPersistence() {
    const button = $('#collapseBottomButton');
    if (!button || button.dataset.responsiveBound === '1') return;
    button.dataset.responsiveBound = '1';
    button.addEventListener('click', () => setTimeout(() => {
      localStorage.setItem(`${STORAGE}bottomCollapsed`, document.body.classList.contains('bottom-collapsed') ? '1' : '0');
    }, 0));
    $$('.bottom-tabs .tab').forEach((tab) => tab.addEventListener('click', () => {
      if (mode === 'compact' && document.body.classList.contains('bottom-collapsed')) button.click();
    }));
  }

  function applyBottomDefault(nextMode) {
    const button = $('#collapseBottomButton');
    if (!button) return;
    const stored = localStorage.getItem(`${STORAGE}bottomCollapsed`);
    const wantCollapsed = stored == null ? nextMode !== 'wide' : stored === '1';
    const isCollapsed = document.body.classList.contains('bottom-collapsed');
    if (wantCollapsed !== isCollapsed) button.click();
  }

  function ensureZoomOne() {
    const select = $('#zoomSelect');
    if (!select || $$('option', select).some((option) => option.value === '1')) return;
    const option = document.createElement('option');
    option.value = '1';
    option.textContent = '1×';
    select.insertBefore(option, select.firstChild);
  }

  function bestFitZoom(maxZoom = 8) {
    const viewport = $('#canvasViewport');
    if (!viewport) return 1;
    const raw = Math.min(
      Math.max(320, viewport.clientWidth - 34) / 320,
      Math.max(170, viewport.clientHeight - 30) / 170,
      maxZoom
    );
    let best = 1;
    ZOOMS.forEach((candidate) => { if (candidate <= raw) best = candidate; });
    return best;
  }

  function setZoom(value, persist = true) {
    const select = $('#zoomSelect');
    if (!select) return;
    ensureZoomOne();
    const normalized = ZOOMS.includes(Number(value)) ? Number(value) : 1;
    applyingZoom = true;
    select.value = String(normalized);
    select.dispatchEvent(new Event('change', { bubbles: true }));
    applyingZoom = false;
    if (persist && mode) localStorage.setItem(`${STORAGE}zoom.${mode}`, String(normalized));
  }

  function applyAdaptiveZoom(nextMode) {
    const stored = Number(localStorage.getItem(`${STORAGE}zoom.${nextMode}`));
    if (ZOOMS.includes(stored)) return setZoom(stored, false);
    setTimeout(() => {
      const fit = bestFitZoom(nextMode === 'wide' ? 4 : 3);
      if (nextMode === 'wide') setZoom(Math.min(3, fit), false);
      else if (nextMode === 'medium') setZoom(Math.min(2, fit), false);
      else setZoom(fit, false);
    }, 50);
  }

  function bindAdaptiveFit() {
    const fit = $('#fitButton');
    if (fit && fit.dataset.responsiveBound !== '1') {
      fit.dataset.responsiveBound = '1';
      fit.addEventListener('click', () => setTimeout(() => setZoom(bestFitZoom(mode === 'wide' ? 8 : 4), true), 0));
    }
    const select = $('#zoomSelect');
    if (select && select.dataset.responsiveBound !== '1') {
      select.dataset.responsiveBound = '1';
      select.addEventListener('change', () => {
        if (!applyingZoom && mode) localStorage.setItem(`${STORAGE}zoom.${mode}`, select.value);
      });
    }
  }

  function syncToolbarLabels() {
    const generate = $('#generateButton');
    const update = $('#updateHmiButton');
    const connect = $('#liveConnectButton');
    setTextIfChanged(generate, mode === 'compact' ? 'Generar' : 'Generar C++');
    setTextIfChanged(update, mode === 'compact' ? 'HMI' : 'Actualizar HMI');
    if (connect && !connect.classList.contains('live')) setTextIfChanged(connect, mode === 'compact' ? 'JWPLC' : 'Conectar JWPLC');
  }

  function markStatusItems() {
    const bar = statusbar();
    if (!bar) return;
    $$(':scope > span', bar).forEach((span) => {
      const text = span.textContent.trim();
      if (text.startsWith('Proyecto:')) span.classList.add('status-project');
      if (text.startsWith('Página:')) span.classList.add('status-page');
      if (text.startsWith('Campos:')) span.classList.add('status-fields');
      if (text.startsWith('Resolución:')) span.classList.add('status-resolution');
      if (text.startsWith('Formato:')) span.classList.add('status-format');
    });
    $('#selectedObjectStatus')?.classList.add('status-object');
    $('#selectedGeometryStatus')?.classList.add('status-geometry');
    $('#zoomStatus')?.classList.add('status-zoom');
    $('#sketchLinkStatus')?.classList.add('status-sketch');
  }

  function syncLateNodes() {
    markToolbarCommands();
    moveGateToStatusbar();
    moveLiveStatusToStatusbar();
    syncSketchIdentity();
    syncOverflow();
    markStatusItems();
    syncToolbarLabels();
  }

  function applyMode(force = false) {
    const next = chooseMode();
    if (!force && next === mode) return syncLateNodes();
    mode = next;
    document.body.classList.remove('layout-wide', 'layout-medium', 'layout-compact');
    document.body.classList.add(`layout-${mode}`);
    document.body.dataset.layoutMode = mode;
    document.body.dataset.screenRatio = screenRatio().toFixed(3);

    if (mode !== 'compact') {
      leftDrawerOpen = false;
      rightDrawerOpen = false;
    }
    setRightView(mode === 'wide' ? 'inspector' : rightView, false);
    applyLeftDefaults(mode);
    applyBottomDefault(mode);
    applyAdaptiveZoom(mode);
    syncDrawers();
    syncToolbarLabels();
    setTimeout(() => dispatchEvent(new Event('resize')), 0);
  }

  function init() {
    moveGateToStatusbar();
    ensureOverflow();
    ensureRightTabs();
    ensureCompactRightButtons();
    ensureLeftRail();
    ensureScrim();
    bindCollapsibleSections();
    bindBottomPersistence();
    ensureZoomOne();
    bindAdaptiveFit();
    markStatusItems();
    syncSketchIdentity();
    applyMode(true);

    // designer-live.js se carga después de ux-foundation.js. Un sondeo liviano
    // evita observers recursivos y captura los controles tardíos sin tocar el runtime.
    setInterval(syncLateNodes, 500);
  }

  let resizeTimer = null;
  addEventListener('resize', () => {
    clearTimeout(resizeTimer);
    resizeTimer = setTimeout(() => applyMode(false), 70);
  });
  addEventListener('jwplc:editor-refresh', syncLateNodes);
  addEventListener('jwplc:header-written', syncSketchIdentity);
  addEventListener('jwplc:project-loaded', syncSketchIdentity);

  setTimeout(init, 0);

  window.JWPLCHMIDesktopResponsive = {
    mode: () => mode,
    screenRatio,
    setRightView,
    fit: () => setZoom(bestFitZoom(), true),
    closeDrawers: () => {
      leftDrawerOpen = false;
      rightDrawerOpen = false;
      syncDrawers();
    }
  };
})();
