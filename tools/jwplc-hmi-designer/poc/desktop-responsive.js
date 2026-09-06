(() => {
  'use strict';

  if (window.JWPLCHMIDesktopResponsive) return;

  const STORAGE_PREFIX = 'jwplc-hmi-layout.';
  const WIDE_RATIO = 0.70;
  const COMPACT_RATIO = 0.38;
  const ZOOM_CHOICES = [1, 2, 3, 4, 6, 8];

  let mode = null;
  let rightView = localStorage.getItem(`${STORAGE_PREFIX}rightView`) || 'inspector';
  let leftDrawerOpen = false;
  let rightDrawerOpen = false;
  let activeLeftSection = 'pages';
  let applyingZoom = false;

  const qs = (selector, root = document) => root.querySelector(selector);
  const qsa = (selector, root = document) => [...root.querySelectorAll(selector)];

  function toolbar() { return qs('.toolbar'); }
  function statusbar() { return qs('.statusbar'); }
  function workspace() { return qs('.workspace'); }
  function leftPanel() { return qs('.left-panel'); }
  function rightPanel() { return qs('.right-panel'); }

  function screenRatio() {
    const available = Number(window.screen?.availWidth || window.screen?.width || window.innerWidth || 1);
    const current = Number(window.outerWidth || window.innerWidth || available);
    return Math.max(0.10, Math.min(1.25, current / Math.max(1, available)));
  }

  function chooseMode() {
    const ratio = screenRatio();
    if (ratio < COMPACT_RATIO || window.innerWidth < 620) return 'compact';
    if (ratio < WIDE_RATIO) return 'medium';
    return 'wide';
  }

  function parseSketchName() {
    const apiName = window.JWPLCHMIProject?.linkedSketchName?.();
    if (apiName) return apiName;
    const text = qs('#sketchLinkStatus')?.textContent || '';
    const match = text.match(/^Sketch:\s*([^·]+?)(?:\s*·|$)/);
    const name = match?.[1]?.trim();
    return name && name !== 'sin vincular' ? name : null;
  }

  function syncSketchIdentity() {
    const name = parseSketchName();
    const button = qs('#linkSketchButton');
    if (button) {
      button.dataset.linkedSketch = name || '';
      button.textContent = name ? `Sketch: ${name}` : 'Vincular sketch…';
      button.title = name
        ? `Sketch vinculado: ${name}. Clic para cambiar la vinculación.`
        : 'Seleccionar la carpeta del sketch Arduino';
    }
    document.title = name
      ? `${name} · JWPLC HMI Designer — Alpha11`
      : 'JWPLC HMI Designer — Alpha11';
  }

  function ensureGateInStatusbar() {
    const gate = qs('.gate');
    const bar = statusbar();
    const grow = qs('.grow', bar || document);
    if (!gate || !bar || gate.dataset.responsiveMoved === '1') return;
    gate.dataset.responsiveMoved = '1';
    gate.classList.add('responsive-gate-status');
    if (grow) bar.insertBefore(gate, grow);
    else bar.appendChild(gate);
  }

  function ensureLiveStatusInStatusbar() {
    const live = qs('#liveStatus');
    const bar = statusbar();
    const grow = qs('.grow', bar || document);
    if (!live || !bar || live.parentElement === bar) return;
    live.classList.add('responsive-live-status');
    if (grow) bar.insertBefore(live, grow);
    else bar.appendChild(live);
  }

  function markToolbarCommands() {
    const bar = toolbar();
    if (!bar) return;
    qsa('button', bar).forEach((button) => {
      const text = button.textContent.trim();
      if (text === 'Deshacer') button.classList.add('cmd-undo');
      if (text === 'Rehacer') button.classList.add('cmd-redo');
    });
  }

  function proxyTarget(key) {
    const map = {
      new: '#newProjectButton',
      open: '#openProjectButton',
      undo: '.cmd-undo',
      redo: '.cmd-redo',
      fit: '#fitButton',
      sketch: '#linkSketchButton',
      install: '#installAppButton'
    };
    return map[key] ? qs(map[key]) : null;
  }

  function ensureOverflowMenu() {
    const bar = toolbar();
    if (!bar || qs('#responsiveOverflow')) return;

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
        <button type="button" data-proxy="sketch">Vincular / cambiar sketch</button>
        <button type="button" data-proxy="install">Instalar app</button>
      </div>`;
    bar.appendChild(wrap);

    const toggle = qs('#responsiveOverflowButton');
    const menu = qs('#responsiveOverflowMenu');
    toggle.addEventListener('click', (event) => {
      event.stopPropagation();
      const opening = menu.hidden;
      menu.hidden = !opening;
      toggle.setAttribute('aria-expanded', opening ? 'true' : 'false');
      syncOverflowState();
    });
    qsa('[data-proxy]', menu).forEach((button) => {
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

  function syncOverflowState() {
    const menu = qs('#responsiveOverflowMenu');
    if (!menu) return;
    qsa('[data-proxy]', menu).forEach((button) => {
      const source = proxyTarget(button.dataset.proxy);
      const unavailable = !source || source.disabled || source.hidden;
      button.disabled = unavailable;
      if (button.dataset.proxy === 'sketch') {
        const name = parseSketchName();
        button.textContent = name ? `Cambiar sketch · ${name}` : 'Vincular sketch…';
      }
    });
  }

  function ensureRightTabs() {
    const panel = rightPanel();
    if (!panel || qs('#responsiveRightTabs')) return;
    const tabs = document.createElement('div');
    tabs.id = 'responsiveRightTabs';
    tabs.className = 'responsive-right-tabs';
    tabs.innerHTML = `
      <button type="button" data-right-view="inspector">Inspector</button>
      <button type="button" data-right-view="preview">Vista 1:1</button>`;
    panel.prepend(tabs);
    qsa('[data-right-view]', tabs).forEach((button) => {
      button.addEventListener('click', () => setRightView(button.dataset.rightView, true));
    });
  }

  function setRightView(view, persist = false) {
    rightView = view === 'preview' ? 'preview' : 'inspector';
    if (persist) localStorage.setItem(`${STORAGE_PREFIX}rightView`, rightView);
    document.body.classList.toggle('right-view-preview', rightView === 'preview');
    document.body.classList.toggle('right-view-inspector', rightView !== 'preview');
    qsa('[data-right-view]').forEach((button) => {
      button.classList.toggle('active', button.dataset.rightView === rightView);
    });
  }

  function ensureCompactRightButtons() {
    const bar = toolbar();
    if (!bar) return;
    if (!qs('#compactInspectorButton')) {
      const inspector = document.createElement('button');
      inspector.id = 'compactInspectorButton';
      inspector.type = 'button';
      inspector.textContent = 'Inspector';
      inspector.addEventListener('click', () => {
        setRightView('inspector', true);
        rightDrawerOpen = !(rightDrawerOpen && rightView === 'inspector');
        syncDrawers();
      });
      bar.appendChild(inspector);
    }
    if (!qs('#compactPreviewButton')) {
      const preview = document.createElement('button');
      preview.id = 'compactPreviewButton';
      preview.type = 'button';
      preview.textContent = '1:1';
      preview.title = 'Vista previa 1:1';
      preview.addEventListener('click', () => {
        setRightView('preview', true);
        rightDrawerOpen = !(rightDrawerOpen && rightView === 'preview');
        syncDrawers();
      });
      bar.appendChild(preview);
    }
  }

  function leftSectionMap() {
    const blocks = qsa('.left-panel .nav-block');
    return {
      pages: blocks[0] || null,
      objects: blocks[1] || null,
      components: blocks[2] || null,
      tools: blocks[3] || null,
      dev: qs('.left-panel .dev-actions')
    };
  }

  function ensureLeftRail() {
    const root = workspace();
    if (!root || qs('#responsiveLeftRail')) return;
    const rail = document.createElement('nav');
    rail.id = 'responsiveLeftRail';
    rail.className = 'responsive-left-rail';
    rail.setAttribute('aria-label', 'Paneles de diseño');
    rail.innerHTML = `
      <button type="button" data-left-section="pages" title="Páginas">P</button>
      <button type="button" data-left-section="objects" title="Objetos">O</button>
      <button type="button" data-left-section="components" title="Componentes">C</button>
      <button type="button" data-left-section="tools" title="Herramientas">T</button>`;
    root.prepend(rail);
    qsa('[data-left-section]', rail).forEach((button) => {
      button.addEventListener('click', () => {
        activeLeftSection = button.dataset.leftSection;
        leftDrawerOpen = true;
        rightDrawerOpen = false;
        const section = leftSectionMap()[activeLeftSection];
        section?.classList.remove('section-collapsed');
        syncDrawers();
        setTimeout(() => section?.scrollIntoView({ block: 'start' }), 0);
      });
    });
  }

  function ensureScrim() {
    if (qs('#responsiveScrim')) return;
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
    document.body.classList.toggle('compact-left-open', mode === 'compact' && leftDrawerOpen);
    document.body.classList.toggle('compact-right-open', mode === 'compact' && rightDrawerOpen);
    document.body.classList.toggle('responsive-drawer-open', mode === 'compact' && (leftDrawerOpen || rightDrawerOpen));
    qsa('[data-left-section]').forEach((button) => {
      button.classList.toggle('active', leftDrawerOpen && button.dataset.leftSection === activeLeftSection);
    });
  }

  function makeLeftSectionsCollapsible() {
    const map = leftSectionMap();
    ['components', 'tools'].forEach((key) => {
      const section = map[key];
      const heading = section?.querySelector(':scope > h2');
      if (!section || !heading || section.dataset.responsiveCollapsible === '1') return;
      section.dataset.responsiveCollapsible = '1';
      section.classList.add('responsive-collapsible');
      heading.title = 'Expandir o contraer sección';
      heading.addEventListener('click', () => {
        section.classList.toggle('section-collapsed');
        localStorage.setItem(`${STORAGE_PREFIX}left.${key}.collapsed`, section.classList.contains('section-collapsed') ? '1' : '0');
      });
    });

    const dev = map.dev;
    const devHeading = dev?.querySelector(':scope > h2');
    if (dev && devHeading && dev.dataset.responsiveCollapsible !== '1') {
      dev.dataset.responsiveCollapsible = '1';
      dev.classList.add('responsive-collapsible');
      devHeading.title = 'Expandir o contraer herramientas de desarrollo';
      devHeading.addEventListener('click', () => {
        dev.classList.toggle('section-collapsed');
        localStorage.setItem(`${STORAGE_PREFIX}left.dev.collapsed`, dev.classList.contains('section-collapsed') ? '1' : '0');
      });
    }
  }

  function applyLeftDefaults(nextMode) {
    const map = leftSectionMap();
    ['components', 'tools'].forEach((key) => {
      const section = map[key];
      if (!section) return;
      const stored = localStorage.getItem(`${STORAGE_PREFIX}left.${key}.collapsed`);
      const collapse = stored == null ? nextMode !== 'wide' : stored === '1';
      section.classList.toggle('section-collapsed', collapse);
    });
    if (map.dev) {
      const stored = localStorage.getItem(`${STORAGE_PREFIX}left.dev.collapsed`);
      map.dev.classList.toggle('section-collapsed', stored == null ? true : stored === '1');
    }
  }

  function ensureBottomPersistence() {
    const button = qs('#collapseBottomButton');
    if (!button || button.dataset.responsiveBound === '1') return;
    button.dataset.responsiveBound = '1';
    button.addEventListener('click', () => {
      setTimeout(() => {
        localStorage.setItem(`${STORAGE_PREFIX}bottomCollapsed`, document.body.classList.contains('bottom-collapsed') ? '1' : '0');
      }, 0);
    });
    qsa('.bottom-tabs .tab').forEach((tab) => {
      tab.addEventListener('click', () => {
        if (mode === 'compact' && document.body.classList.contains('bottom-collapsed')) button.click();
      });
    });
  }

  function applyBottomDefault(nextMode) {
    const button = qs('#collapseBottomButton');
    if (!button) return;
    const stored = localStorage.getItem(`${STORAGE_PREFIX}bottomCollapsed`);
    const wantCollapsed = stored == null ? nextMode !== 'wide' : stored === '1';
    const isCollapsed = document.body.classList.contains('bottom-collapsed');
    if (wantCollapsed !== isCollapsed) button.click();
  }

  function ensureZoomOne() {
    const select = qs('#zoomSelect');
    if (!select || qsa('option', select).some((option) => option.value === '1')) return;
    const option = document.createElement('option');
    option.value = '1';
    option.textContent = '1×';
    select.insertBefore(option, select.firstChild);
  }

  function bestFitZoom(maxZoom = 8) {
    const viewport = qs('#canvasViewport');
    if (!viewport) return 1;
    const availableW = Math.max(320, viewport.clientWidth - 34);
    const availableH = Math.max(170, viewport.clientHeight - 30);
    const raw = Math.min(availableW / 320, availableH / 170, maxZoom);
    let best = 1;
    ZOOM_CHOICES.forEach((candidate) => { if (candidate <= raw) best = candidate; });
    return best;
  }

  function setZoom(value, persist = true) {
    const select = qs('#zoomSelect');
    if (!select) return;
    ensureZoomOne();
    const normalized = ZOOM_CHOICES.includes(Number(value)) ? Number(value) : 1;
    applyingZoom = true;
    select.value = String(normalized);
    select.dispatchEvent(new Event('change', { bubbles: true }));
    applyingZoom = false;
    if (persist && mode) localStorage.setItem(`${STORAGE_PREFIX}zoom.${mode}`, String(normalized));
  }

  function applyAdaptiveZoom(nextMode) {
    const stored = Number(localStorage.getItem(`${STORAGE_PREFIX}zoom.${nextMode}`));
    if (ZOOM_CHOICES.includes(stored)) {
      setZoom(stored, false);
      return;
    }
    setTimeout(() => {
      const fit = bestFitZoom(nextMode === 'wide' ? 4 : 3);
      if (nextMode === 'wide') setZoom(Math.min(3, fit || 1), false);
      else if (nextMode === 'medium') setZoom(Math.min(2, fit || 1), false);
      else setZoom(fit || 1, false);
    }, 40);
  }

  function bindAdaptiveFit() {
    const fit = qs('#fitButton');
    if (!fit || fit.dataset.responsiveBound === '1') return;
    fit.dataset.responsiveBound = '1';
    fit.addEventListener('click', () => {
      setTimeout(() => setZoom(bestFitZoom(mode === 'wide' ? 8 : 4), true), 0);
    });
    const select = qs('#zoomSelect');
    select?.addEventListener('change', () => {
      if (!applyingZoom && mode) localStorage.setItem(`${STORAGE_PREFIX}zoom.${mode}`, select.value);
    });
  }

  function syncToolbarLabels() {
    const generate = qs('#generateButton');
    const update = qs('#updateHmiButton');
    const connect = qs('#liveConnectButton');
    if (generate) generate.textContent = mode === 'compact' ? 'Generar' : 'Generar C++';
    if (update) update.textContent = mode === 'compact' ? 'HMI' : 'Actualizar HMI';
    if (connect && !connect.classList.contains('live')) connect.textContent = mode === 'compact' ? 'JWPLC' : 'Conectar JWPLC';
  }

  function markStatusItems() {
    const bar = statusbar();
    if (!bar) return;
    qsa(':scope > span', bar).forEach((span) => {
      const text = span.textContent.trim();
      if (text.startsWith('Proyecto:')) span.classList.add('status-project');
      if (text.startsWith('Página:')) span.classList.add('status-page');
      if (text.startsWith('Campos:')) span.classList.add('status-fields');
      if (text.startsWith('Resolución:')) span.classList.add('status-resolution');
      if (text.startsWith('Formato:')) span.classList.add('status-format');
    });
    qs('#selectedObjectStatus')?.classList.add('status-object');
    qs('#selectedGeometryStatus')?.classList.add('status-geometry');
    qs('#zoomStatus')?.classList.add('status-zoom');
    qs('#sketchLinkStatus')?.classList.add('status-sketch');
  }

  function syncOverflowAndLateNodes() {
    markToolbarCommands();
    ensureLiveStatusInStatusbar();
    syncSketchIdentity();
    syncOverflowState();
    markStatusItems();
    syncToolbarLabels();
  }

  function applyMode(force = false) {
    const next = chooseMode();
    if (!force && next === mode) {
      syncOverflowAndLateNodes();
      return;
    }

    mode = next;
    document.body.classList.remove('layout-wide', 'layout-medium', 'layout-compact');
    document.body.classList.add(`layout-${mode}`);
    document.body.dataset.layoutMode = mode;
    document.body.dataset.screenRatio = screenRatio().toFixed(3);

    if (mode !== 'compact') {
      leftDrawerOpen = false;
      rightDrawerOpen = false;
    }
    if (mode === 'wide') setRightView('inspector', false);
    else setRightView(rightView, false);

    applyLeftDefaults(mode);
    applyBottomDefault(mode);
    applyAdaptiveZoom(mode);
    syncDrawers();
    syncToolbarLabels();
    setTimeout(() => window.dispatchEvent(new Event('resize')), 0);
  }

  function init() {
    ensureGateInStatusbar();
    ensureOverflowMenu();
    ensureRightTabs();
    ensureCompactRightButtons();
    ensureLeftRail();
    ensureScrim();
    makeLeftSectionsCollapsible();
    ensureBottomPersistence();
    ensureZoomOne();
    bindAdaptiveFit();
    markStatusItems();
    syncSketchIdentity();
    applyMode(true);

    const toolbarObserver = new MutationObserver(() => {
      syncOverflowAndLateNodes();
      ensureCompactRightButtons();
    });
    if (toolbar()) toolbarObserver.observe(toolbar(), { childList: true, subtree: true, characterData: true });

    const statusObserver = new MutationObserver(() => {
      markStatusItems();
      syncSketchIdentity();
    });
    if (statusbar()) statusObserver.observe(statusbar(), { childList: true, subtree: true, characterData: true });
  }

  let resizeTimer = null;
  window.addEventListener('resize', () => {
    clearTimeout(resizeTimer);
    resizeTimer = setTimeout(() => applyMode(false), 60);
  });
  window.addEventListener('jwplc:editor-refresh', syncOverflowAndLateNodes);
  window.addEventListener('jwplc:header-written', syncSketchIdentity);
  window.addEventListener('jwplc:project-loaded', syncSketchIdentity);

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
