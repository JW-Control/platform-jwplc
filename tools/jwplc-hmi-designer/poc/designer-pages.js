(() => {
  'use strict';

  const INDICATOR = {
    x: 282,
    y: 3,
    w: 36,
    h: 12,
    textX: 286,
    textY: 5
  };

  // Paleta de colores ST7789 para páginas (inspirada en la referencia física)
  const PRESET_PAGE_COLORS = [
    { name: 'Azul', hex565: 0x03FF, css: '#007aff' },      // CFG
    { name: 'Blanco', hex565: 0xFFFF, css: '#ffffff' },    // GEN
    { name: 'Lavanda', hex565: 0xF81F, css: '#ea75f5' },   // T_CD
    { name: 'Amarillo', hex565: 0xFFE0, css: '#ffe600' },  // COD
    { name: 'Cian', hex565: 0x07FF, css: '#00e5ff' },      // BOD
    { name: 'Verde', hex565: 0x07E0, css: '#00e676' },     // TOC
    { name: 'Naranja', hex565: 0xFD20, css: '#ff9100' },   // TUR
    { name: 'Rojo', hex565: 0xFA60, css: '#ff5252' }       // Extra
  ];

  const gate = document.querySelector('.page-tabs .gate');
  const bottomSummary = document.querySelector('.bottom-summary');
  const pageTabs = document.querySelector('.page-tabs');
  const displayCanvas = document.getElementById('displayCanvas');
  const previewCanvas = document.getElementById('previewCanvas');
  const zoomSelect = document.getElementById('zoomSelect');
  const fieldSection = document.getElementById('textFieldControlsSection');
  const codeOutput = document.getElementById('codeOutput');
  const contractTab = document.getElementById('contractTab');
  const statusTab = document.getElementById('statusTab');
  const generateButton = document.getElementById('generateButton');
  const statusPage = [...document.querySelectorAll('.statusbar span')]
    .find((span) => span.textContent.trim().startsWith('Página:'));

  if (!pageTabs || !displayCanvas || !previewCanvas) return;

  let navigationMode = 'SELECT';
  let pageSelect = null;
  let navPopoverElement = null;

  function editor() {
    return window.JWPLCHMIEditor || null;
  }

  function pages() {
    return editor()?.getPages?.() || [{ id: 0, name: 'Principal' }];
  }

  function activePage() {
    return Number(editor()?.getActivePage?.() ?? 0);
  }

  function selectedField() {
    return editor()?.getSelectedField?.() || null;
  }

  function pageLabel(page) {
    return `${String(page.id + 1).padStart(2, '0')} · ${page.name}`;
  }

  function defaultPageColor(index) {
    return PRESET_PAGE_COLORS[index % PRESET_PAGE_COLORS.length].hex565;
  }

  function pageColorCss(color565) {
    if (typeof color565 === 'string' && color565.startsWith('#')) return color565;
    const num = Number(color565);
    const item = PRESET_PAGE_COLORS.find((c) => c.hex565 === num);
    if (item) return item.css;
    const r = ((num >> 11) & 0x1F) * 255 / 31;
    const g = ((num >> 5) & 0x3F) * 255 / 63;
    const b = (num & 0x1F) * 255 / 31;
    return `rgb(${Math.round(r)}, ${Math.round(g)}, ${Math.round(b)})`;
  }

  function computeSmartShortName(pageName, index) {
    if (!pageName) return `P${index + 1}`;
    const clean = String(pageName).trim().toUpperCase();
    if (clean.length <= 4) return clean;
    if (clean.includes('_') || clean.includes(' ') || clean.includes('-')) {
      const parts = clean.split(/[\s_\-]+/).filter(Boolean);
      if (parts.length >= 2) {
        return (parts[0].slice(0, 2) + '_' + parts[1].slice(0, 2)).slice(0, 4);
      }
    }
    const vowelsRemoved = clean[0] + clean.slice(1).replace(/[AEIOU]/g, '');
    if (vowelsRemoved.length >= 3) return vowelsRemoved.slice(0, 4);
    return clean.slice(0, 4);
  }

  function getNavBarConfig() {
    const navBarField = editor()?.getAllFields?.()?.find((f) => f.type === 'NAV_BAR');
    const fromEditor = editor()?.getNavBarConfig?.() || {};
    return {
      showIndicator: true,
      showArrows: true,
      ...fromEditor,
      enabled: Boolean(navBarField && !navBarField.hidden),
      preset: navBarField?.preset || fromEditor.preset || 'industrial-color',
      position: navBarField?.position || fromEditor.position || 'bottom',
      height: Number(navBarField?.height || fromEditor.height || 22)
    };
  }

  function updateNavBarConfig(next) {
    const current = getNavBarConfig();
    const updated = { ...current, ...next };
    const navBarField = editor()?.getAllFields?.()?.find((f) => f.type === 'NAV_BAR');
    if (navBarField) {
      if (next.preset) navBarField.preset = next.preset;
      if (next.position) {
        navBarField.position = next.position;
        navBarField.y = next.position === 'top' ? 2 : (170 - (navBarField.height || 22) - 1);
      }
      if (next.height) {
        navBarField.height = next.height;
        navBarField.y = (navBarField.position || 'bottom') === 'top' ? 2 : (170 - next.height - 1);
      }
      if (next.enabled !== undefined) navBarField.hidden = !next.enabled;
    }
    editor()?.setNavBarConfig?.(updated);
    patchUI();
  }

  function addPage() {
    const api = editor();
    if (!api) return;
    const nextNumber = pages().length + 1;
    const created = api.addPage?.(`Página ${nextNumber}`);
    if (created) navigationMode = 'SELECT';
    rebuildTopTabs(true);
    patchUI();
  }

  // --- Algoritmo de Distribución Adaptativo para la Barra HMI ---
  function computeNavBarLayout(pageList, curActivePage) {
    const total = pageList.length;
    const cfg = getNavBarConfig();
    const width = 320;
    const h = cfg.height || 22;
    const y = cfg.position === 'top' ? 2 : (170 - h - 1);

    if (!cfg.enabled || total < 1) {
      return { visible: false, hasOverflow: false, tabs: [], leftArrow: null, rightArrow: null, counter: null, y, h, bounds: { x: 0, y, w: width, h } };
    }

    const minTabW = 34;
    const gap = 3;
    const margin = 4;
    const availWidth = width - (2 * margin);
    const neededWidth = (total * minTabW) + ((total - 1) * gap);

    // Caso 1: Todas las páginas caben en el ancho horizontal
    if (neededWidth <= availWidth) {
      const tabW = Math.min(total <= 3 ? 90 : 80, Math.floor((availWidth - (total - 1) * gap) / total));
      const totalSpan = (total * tabW) + ((total - 1) * gap);
      const startX = Math.floor((width - totalSpan) / 2);

      const tabs = pageList.map((page, idx) => {
        const x = startX + idx * (tabW + gap);
        const short = page.shortName || computeSmartShortName(page.name, idx);
        const col = page.color ?? defaultPageColor(idx);
        return {
          pageId: page.id,
          pageIndex: idx,
          name: page.name,
          shortName: short,
          color: col,
          x,
          y,
          w: tabW,
          h,
          isActive: page.id === curActivePage
        };
      });

      return {
        visible: true,
        hasOverflow: false,
        tabs,
        leftArrow: null,
        rightArrow: null,
        counter: null,
        y,
        h,
        bounds: { x: 0, y, w: width, h }
      };
    }

    // Caso 2: Desbordamiento horizontal (Muchas páginas: flechas ◄ ► + contador NN/TT)
    const arrowW = 16;
    const counterW = 34;
    const reserved = (2 * margin) + (2 * arrowW) + counterW + (4 * gap);
    const tabsSpace = width - reserved;

    const tabW = 40;
    const visibleCount = Math.max(3, Math.min(total, Math.floor((tabsSpace + gap) / (tabW + gap))));
    const activeIdx = Math.max(0, pageList.findIndex((p) => p.id === curActivePage));

    const half = Math.floor(visibleCount / 2);
    let startIdx = activeIdx - half;
    if (startIdx < 0) startIdx = 0;
    if (startIdx + visibleCount > total) startIdx = Math.max(0, total - visibleCount);
    const endIdx = Math.min(total - 1, startIdx + visibleCount - 1);

    let cursorX = margin;
    const leftArrow = {
      x: cursorX,
      y,
      w: arrowW,
      h,
      enabled: startIdx > 0,
      targetPage: Math.max(0, activeIdx - 1)
    };
    cursorX += arrowW + gap;

    const tabs = [];
    for (let i = startIdx; i <= endIdx; i++) {
      const page = pageList[i];
      const short = page.shortName || computeSmartShortName(page.name, i);
      const col = page.color ?? defaultPageColor(i);
      tabs.push({
        pageId: page.id,
        pageIndex: i,
        name: page.name,
        shortName: short,
        color: col,
        x: cursorX,
        y,
        w: tabW,
        h,
        isActive: page.id === curActivePage
      });
      cursorX += tabW + gap;
    }

    const rightArrow = {
      x: cursorX,
      y,
      w: arrowW,
      h,
      enabled: endIdx < total - 1,
      targetPage: Math.min(total - 1, activeIdx + 1)
    };
    cursorX += arrowW + gap;

    const counter = {
      x: cursorX,
      y,
      w: counterW,
      h,
      text: `${String(activeIdx + 1).padStart(2, '0')}/${String(total).padStart(2, '0')}`
    };

    return {
      visible: true,
      hasOverflow: true,
      tabs,
      leftArrow,
      rightArrow,
      counter,
      y,
      h,
      bounds: { x: 0, y, w: width, h }
    };
  }

  // --- Renderizado Gráfico de la Barra de Navegación HMI ---
  function drawHMINavigationBar(ctx, scale) {
    const cfg = getNavBarConfig();
    if (!cfg.enabled || pages().length < 1) return;

    const layout = computeNavBarLayout(pages(), activePage());
    if (!layout.visible) return;

    ctx.save();

    // Franja de fondo de la barra
    ctx.fillStyle = '#000000';
    ctx.fillRect(0, layout.y * scale, 320 * scale, layout.h * scale);

    const preset = cfg.preset || 'industrial-color';

    function fillRounded(x, y, w, h, r, col) {
      ctx.fillStyle = col;
      ctx.beginPath();
      const rx = r * scale;
      if (typeof ctx.roundRect === 'function') {
        ctx.roundRect(x * scale, y * scale, w * scale, h * scale, rx);
      } else {
        ctx.rect(x * scale, y * scale, w * scale, h * scale);
      }
      ctx.fill();
    }

    function strokeRounded(x, y, w, h, r, col, lineWidth = 1) {
      ctx.strokeStyle = col;
      ctx.lineWidth = Math.max(1, lineWidth * scale);
      ctx.beginPath();
      const rx = r * scale;
      if (typeof ctx.roundRect === 'function') {
        ctx.roundRect(x * scale + 0.5, y * scale + 0.5, w * scale - 1, h * scale - 1, rx);
      } else {
        ctx.strokeRect(x * scale + 0.5, y * scale + 0.5, w * scale - 1, h * scale - 1);
      }
      ctx.stroke();
    }

    function drawCenteredText(text, x, y, w, h, color) {
      const charCount = String(text).length;
      const textW = charCount * 6 - 1;
      const tx = x + Math.floor((w - textW) / 2);
      const ty = y + Math.floor((h - 7) / 2);
      drawClassicText(ctx, text, tx, ty, scale, color);
    }

    // Dibujo de Pestañas según el Prediseño
    layout.tabs.forEach((tab) => {
      const colCss = pageColorCss(tab.color);

      if (preset === 'industrial-color') {
        // IDÉNTICO A LA REFERENCIA FÍSICA
        if (tab.isActive) {
          fillRounded(tab.x, tab.y, tab.w, tab.h, 3, '#ffffff');
          strokeRounded(tab.x, tab.y, tab.w, tab.h, 3, colCss, 1.5);
          drawCenteredText(tab.shortName, tab.x, tab.y, tab.w, tab.h, colCss === '#ffffff' ? '#9c27b0' : colCss);
        } else {
          fillRounded(tab.x, tab.y, tab.w, tab.h, 3, '#000000');
          strokeRounded(tab.x, tab.y, tab.w, tab.h, 3, colCss, 1);
          drawCenteredText(tab.shortName, tab.x, tab.y, tab.w, tab.h, colCss);
        }
      } else if (preset === 'dark-pill') {
        if (tab.isActive) {
          fillRounded(tab.x, tab.y, tab.w, tab.h, 4, '#00c8ff');
          drawCenteredText(tab.shortName, tab.x, tab.y, tab.w, tab.h, '#000000');
        } else {
          fillRounded(tab.x, tab.y, tab.w, tab.h, 4, '#151e2a');
          strokeRounded(tab.x, tab.y, tab.w, tab.h, 4, '#27384c', 1);
          drawCenteredText(tab.shortName, tab.x, tab.y, tab.w, tab.h, '#8ca2b6');
        }
      } else if (preset === 'segmented-bar') {
        if (tab.isActive) {
          fillRounded(tab.x, tab.y, tab.w, tab.h, 2, '#1f2e42');
          ctx.fillStyle = '#52c9ff';
          ctx.fillRect(tab.x * scale, tab.y * scale, tab.w * scale, 2 * scale);
          drawCenteredText(tab.shortName, tab.x, tab.y, tab.w, tab.h, '#ffffff');
        } else {
          fillRounded(tab.x, tab.y, tab.w, tab.h, 2, '#0e1520');
          strokeRounded(tab.x, tab.y, tab.w, tab.h, 2, '#1e2b3c', 1);
          drawCenteredText(tab.shortName, tab.x, tab.y, tab.w, tab.h, '#768d9f');
        }
      } else if (preset === 'compact-dots') {
        if (tab.isActive) {
          fillRounded(tab.x + Math.floor((tab.w - 18) / 2), tab.y + Math.floor((tab.h - 6) / 2), 18, 6, 3, '#00c8ff');
        } else {
          fillRounded(tab.x + Math.floor((tab.w - 6) / 2), tab.y + Math.floor((tab.h - 6) / 2), 6, 6, 3, '#2a3a4c');
        }
      }
    });

    // Dibujo de Controles de Desbordamiento (◄, ►, NN/TT)
    if (layout.hasOverflow) {
      if (layout.leftArrow) {
        const la = layout.leftArrow;
        const color = la.enabled ? '#52c9ff' : '#283848';
        fillRounded(la.x, la.y, la.w, la.h, 3, '#0b1118');
        strokeRounded(la.x, la.y, la.w, la.h, 3, color, 1);
        drawCenteredText('<', la.x, la.y, la.w, la.h, color);
      }
      if (layout.rightArrow) {
        const ra = layout.rightArrow;
        const color = ra.enabled ? '#52c9ff' : '#283848';
        fillRounded(ra.x, ra.y, ra.w, ra.h, 3, '#0b1118');
        strokeRounded(ra.x, ra.y, ra.w, ra.h, 3, color, 1);
        drawCenteredText('>', ra.x, ra.y, ra.w, ra.h, color);
      }
      if (layout.counter) {
        const cnt = layout.counter;
        fillRounded(cnt.x, cnt.y, cnt.w, cnt.h, 3, '#0b1118');
        strokeRounded(cnt.x, cnt.y, cnt.w, cnt.h, 3, '#3b4e62', 1);
        drawCenteredText(cnt.text, cnt.x, cnt.y, cnt.w, cnt.h, '#ffffff');
      }
    }

    ctx.restore();
  }

  // --- Popover / Modal de Configuración de la Barra HMI ---
  function openNavBarPopover(anchorBtn) {
    if (navPopoverElement) {
      navPopoverElement.remove();
      navPopoverElement = null;
      anchorBtn.classList.remove('active');
      return;
    }

    anchorBtn.classList.add('active');
    const popover = document.createElement('div');
    popover.className = 'hmi-nav-popover';

    const cfg = getNavBarConfig();
    const curPages = pages();

    popover.innerHTML = `
      <div class="hmi-nav-header">
        <span class="hmi-nav-title">
          <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
            <rect x="2" y="7" width="20" height="13" rx="2"></rect>
            <path d="M2 11h20"></path>
          </svg>
          Barra de Navegación HMI
        </span>
        <button type="button" class="hmi-nav-close" title="Cerrar">&times;</button>
      </div>
      <div class="hmi-nav-body">
        <label class="hmi-nav-toggle-row">
          <span>Mostrar en Pantalla (320&times;170)</span>
          <input type="checkbox" id="hmiNavEnabled" ${cfg.enabled ? 'checked' : ''} />
        </label>
        
        <div class="hmi-nav-row">
          <span class="hmi-nav-label">Estilo / Prediseño</span>
          <select id="hmiNavPreset" class="hmi-nav-select">
            <option value="industrial-color" ${cfg.preset === 'industrial-color' ? 'selected' : ''}>Industrial Multi-Color (Ref. Física)</option>
            <option value="dark-pill" ${cfg.preset === 'dark-pill' ? 'selected' : ''}>Modern Dark Pill</option>
            <option value="segmented-bar" ${cfg.preset === 'segmented-bar' ? 'selected' : ''}>Segmented Bar (Continua)</option>
            <option value="compact-dots" ${cfg.preset === 'compact-dots' ? 'selected' : ''}>Compact Dots & Badges</option>
          </select>
        </div>

        <div class="hmi-nav-row-two-cols">
          <div class="hmi-nav-row">
            <span class="hmi-nav-label">Posición</span>
            <select id="hmiNavPosition" class="hmi-nav-select">
              <option value="bottom" ${cfg.position === 'bottom' ? 'selected' : ''}>Inferior (Bottom)</option>
              <option value="top" ${cfg.position === 'top' ? 'selected' : ''}>Superior (Top)</option>
            </select>
          </div>
          <div class="hmi-nav-row">
            <span class="hmi-nav-label">Altura</span>
            <select id="hmiNavHeight" class="hmi-nav-select">
              <option value="18" ${cfg.height === 18 ? 'selected' : ''}>18 px</option>
              <option value="22" ${cfg.height === 22 ? 'selected' : ''}>22 px (Estándar)</option>
              <option value="26" ${cfg.height === 26 ? 'selected' : ''}>26 px</option>
            </select>
          </div>
        </div>

        <div class="hmi-nav-row">
          <span class="hmi-nav-label">Páginas del Proyecto (${curPages.length})</span>
          <div class="hmi-nav-pages-list" id="hmiNavPagesList"></div>
        </div>
      </div>
    `;

    // Llenar lista de páginas con campos de abreviatura y color
    const pagesContainer = popover.querySelector('#hmiNavPagesList');
    curPages.forEach((page, idx) => {
      const row = document.createElement('div');
      row.className = 'hmi-nav-page-row';
      const short = page.shortName || computeSmartShortName(page.name, idx);
      const colHex = page.color ?? defaultPageColor(idx);
      const colCss = pageColorCss(colHex);

      row.innerHTML = `
        <span class="hmi-nav-page-num">${String(idx + 1).padStart(2, '0')}</span>
        <span class="hmi-nav-page-name" title="${page.name}">${page.name}</span>
        <input type="text" class="hmi-nav-short-input" maxlength="6" value="${short}" title="Abreviatura para el botón (máx 6 letras)" />
        <button type="button" class="hmi-nav-color-btn" style="background:${colCss}" title="Clic para alternar color"></button>
      `;

      const shortInput = row.querySelector('.hmi-nav-short-input');
      shortInput.addEventListener('change', () => {
        editor()?.setPageMetadata?.(page.id, { shortName: shortInput.value.trim().toUpperCase() });
        patchUI();
      });

      const colorBtn = row.querySelector('.hmi-nav-color-btn');
      colorBtn.addEventListener('click', () => {
        const curIdx = PRESET_PAGE_COLORS.findIndex((c) => c.hex565 === colHex);
        const nextIdx = (curIdx + 1) % PRESET_PAGE_COLORS.length;
        const nextCol = PRESET_PAGE_COLORS[nextIdx].hex565;
        editor()?.setPageMetadata?.(page.id, { color: nextCol });
        colorBtn.style.background = PRESET_PAGE_COLORS[nextIdx].css;
        patchUI();
      });

      pagesContainer.appendChild(row);
    });

    // Wire-up controls
    popover.querySelector('.hmi-nav-close').addEventListener('click', () => {
      popover.remove();
      navPopoverElement = null;
      anchorBtn.classList.remove('active');
    });

    popover.querySelector('#hmiNavEnabled').addEventListener('change', (e) => {
      updateNavBarConfig({ enabled: e.target.checked });
    });

    popover.querySelector('#hmiNavPreset').addEventListener('change', (e) => {
      updateNavBarConfig({ preset: e.target.value });
    });

    popover.querySelector('#hmiNavPosition').addEventListener('change', (e) => {
      updateNavBarConfig({ position: e.target.value });
    });

    popover.querySelector('#hmiNavHeight').addEventListener('change', (e) => {
      updateNavBarConfig({ height: Number(e.target.value) });
    });

    pageTabs.appendChild(popover);
    navPopoverElement = popover;
  }

  // --- Renombrado Interactivo con Doble Clic en Pestañas Superiores ---
  let activeInlineRenameId = null;

  function rebuildTopTabs(force = false) {
    const curPages = pages();
    const curActive = activePage();
    const gateNode = pageTabs.querySelector('.gate');

    // Si hay un renombrado en curso, no destruir el DOM
    if (activeInlineRenameId !== null && !force) return;

    const existingPills = [...pageTabs.querySelectorAll('.page-tab-pill')];
    const existingIds = existingPills.map((p) => Number(p.dataset.pageId));
    const currentIds = curPages.map((p) => p.id);

    const matches = existingPills.length === curPages.length &&
      existingIds.every((id, idx) => id === currentIds[idx]);

    if (matches && !force) {
      existingPills.forEach((tab, idx) => {
        const page = curPages[idx];
        const isActive = page.id === curActive;
        tab.classList.toggle('active', isActive);
        const label = tab.querySelector('.page-tab-label');
        if (label && label.style.display !== 'none') {
          label.textContent = page.name;
          label.title = `${pageLabel(page)} (Doble clic para renombrar)`;
        }
      });
      return;
    }

    // Reconstrucción completa si cambiaron las páginas
    pageTabs.querySelectorAll('.tab').forEach((node) => node.remove());
    if (navPopoverElement) {
      navPopoverElement.remove();
      navPopoverElement = null;
    }

    const totalPages = curPages.length;

    curPages.forEach((page) => {
      const tab = document.createElement('div');
      const isActive = page.id === curActive;
      tab.className = `tab page-tab-pill${isActive ? ' active' : ''}`;
      tab.dataset.pageId = String(page.id);

      const label = document.createElement('span');
      label.className = 'page-tab-label';
      label.textContent = page.name;
      label.title = `${pageLabel(page)} (Doble clic para renombrar)`;

      const editIcon = document.createElement('span');
      editIcon.className = 'page-tab-edit-icon';
      editIcon.textContent = '✎';
      editIcon.title = 'Renombrar página';

      function startInlineRename(e) {
        if (e) {
          e.stopPropagation();
          e.preventDefault();
        }
        if (tab.querySelector('.page-tab-rename-input')) return;

        activeInlineRenameId = page.id;
        const currentName = page.name;
        label.style.display = 'none';
        editIcon.style.display = 'none';

        const input = document.createElement('input');
        input.type = 'text';
        input.className = 'page-tab-rename-input';
        input.value = currentName;
        input.maxLength = 24;
        input.spellcheck = false;

        let committed = false;

        function commit() {
          if (committed) return;
          committed = true;
          activeInlineRenameId = null;
          const nextName = input.value.trim().slice(0, 24);
          if (nextName && nextName !== currentName) {
            editor()?.renamePage?.(page.id, nextName);
          }
          input.remove();
          label.style.display = '';
          editIcon.style.display = '';
          label.textContent = nextName || currentName;
          patchUI();
        }

        function cancel() {
          if (committed) return;
          committed = true;
          activeInlineRenameId = null;
          input.remove();
          label.style.display = '';
          editIcon.style.display = '';
        }

        input.addEventListener('keydown', (evt) => {
          if (evt.key === 'Enter') {
            evt.preventDefault();
            commit();
          } else if (evt.key === 'Escape') {
            evt.preventDefault();
            cancel();
          }
        });

        input.addEventListener('blur', () => {
          commit();
        });

        input.addEventListener('click', (evt) => evt.stopPropagation());
        input.addEventListener('dblclick', (evt) => evt.stopPropagation());
        input.addEventListener('pointerdown', (evt) => evt.stopPropagation());

        tab.insertBefore(input, label);
        setTimeout(() => {
          input.focus();
          input.select();
        }, 10);
      }

      // Detector de doble clic con latch timer y soporte para clic simple de cambio de página
      let clickTimer = null;
      let clickCount = 0;

      tab.addEventListener('click', (e) => {
        if (e.target.closest('.page-tab-rename-input') || e.target.closest('.page-delete-btn')) return;

        clickCount += 1;
        if (clickCount === 1) {
          clickTimer = setTimeout(() => {
            clickCount = 0;
            clickTimer = null;
            navigationMode = 'SELECT';
            if (activePage() !== page.id) {
              editor()?.setActivePage?.(page.id);
              pageTabs.querySelectorAll('.page-tab-pill').forEach((p) => {
                p.classList.toggle('active', p.dataset.pageId === String(page.id));
              });
              patchCanvases();
              patchStatus();
            }
          }, 250);
        } else if (clickCount >= 2) {
          if (clickTimer) {
            clearTimeout(clickTimer);
            clickTimer = null;
          }
          clickCount = 0;
          startInlineRename(e);
        }
      });

      tab.addEventListener('dblclick', (e) => {
        if (e.target.closest('.page-tab-rename-input') || e.target.closest('.page-delete-btn')) return;
        startInlineRename(e);
      });
      label.addEventListener('dblclick', startInlineRename);
      editIcon.addEventListener('click', startInlineRename);

      tab.appendChild(label);
      tab.appendChild(editIcon);

      // Botón de eliminar con confirmación de mantener pulsado (Hold to delete)
      if (totalPages > 1) {
        const delBtn = document.createElement('button');
        delBtn.type = 'button';
        delBtn.className = 'page-delete-btn';
        delBtn.title = 'Mantén presionado para borrar página';
        delBtn.innerHTML = '<span class="del-icon">✕</span><span class="del-hold-bar"></span>';

        let holdTimer = null;
        let startHoldTime = 0;
        const HOLD_DURATION = 1200;

        function endHold() {
          if (holdTimer) {
            cancelAnimationFrame(holdTimer);
            holdTimer = null;
          }
          delBtn.classList.remove('is-holding');
          const bar = delBtn.querySelector('.del-hold-bar');
          if (bar) bar.style.width = '0%';
        }

        delBtn.addEventListener('pointerdown', (e) => {
          e.stopPropagation();
          e.preventDefault();
          delBtn.classList.add('is-holding');
          startHoldTime = performance.now();

          function stepHold(now) {
            const elapsed = now - startHoldTime;
            const progress = Math.min(1, elapsed / HOLD_DURATION);
            const bar = delBtn.querySelector('.del-hold-bar');
            if (bar) bar.style.width = `${progress * 100}%`;

            if (progress >= 1) {
              endHold();
              editor()?.deletePage?.(page.id);
              rebuildTopTabs(true);
              patchUI();
            } else {
              holdTimer = requestAnimationFrame(stepHold);
            }
          }
          holdTimer = requestAnimationFrame(stepHold);
        });

        delBtn.addEventListener('pointerup', endHold);
        delBtn.addEventListener('pointerleave', endHold);
        delBtn.addEventListener('pointercancel', endHold);
        delBtn.addEventListener('click', (e) => e.stopPropagation());

        tab.appendChild(delBtn);
      }

      pageTabs.insertBefore(tab, gateNode);
    });

    // Botón Nueva Página
    const add = document.createElement('button');
    add.className = 'tab add-tab';
    add.type = 'button';
    add.textContent = '＋';
    add.disabled = totalPages >= (editor()?.getMaxPages?.() || 16);
    add.title = 'Nueva página';
    add.addEventListener('click', addPage);
    pageTabs.insertBefore(add, gateNode);

    // Botón Configuración de Barra HMI
    const navBtn = document.createElement('button');
    navBtn.className = 'tab nav-bar-config-btn';
    navBtn.type = 'button';
    navBtn.title = 'Configuración de Barra de Navegación HMI en Pantalla';
    navBtn.innerHTML = `
      <svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
        <circle cx="12" cy="12" r="3"></circle>
        <path d="M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 0 1 0 2.83 2 2 0 0 1-2.83 0l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 0 1-2 2 2 2 0 0 1-2-2v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 0 1-2.83 0 2 2 0 0 1 0-2.83l.06-.06a1.65 1.65 0 0 0 .33-1.82 1.65 1.65 0 0 0-1.51-1H3a2 2 0 0 1-2-2 2 2 0 0 1 2-2h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 0 1 0-2.83 2 2 0 0 1 2.83 0l.06.06a1.65 1.65 0 0 0 1.82.33H9a1.65 1.65 0 0 0 1-1.51V3a2 2 0 0 1 2-2 2 2 0 0 1 2 2v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 0 1 2.83 0 2 2 0 0 1 0 2.83l-.06.06a1.65 1.65 0 0 0-.33 1.82V9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 0 1 2 2 2 2 0 0 1-2 2h-.09a1.65 1.65 0 0 0-1.51 1z"></path>
      </svg>
      <span>Menú HMI</span>
    `;
    navBtn.addEventListener('click', (e) => {
      e.stopPropagation();
      document.getElementById('navBarToolBtn')?.click();
    });
    pageTabs.insertBefore(navBtn, gateNode);
  }

  function ensurePageSelect() {
    if (pageSelect?.isConnected) return pageSelect;
    const label = [...fieldSection.querySelectorAll('.field-label')]
      .find((node) => node.childNodes[0]?.textContent?.trim() === 'Página');
    if (!label) return null;

    const oldInput = label.querySelector('input, select');
    const select = document.createElement('select');
    select.id = 'fieldPageSelect';
    select.className = 'field-input';
    if (oldInput) oldInput.replaceWith(select);
    else label.appendChild(select);

    select.addEventListener('change', () => {
      editor()?.moveSelectedFieldToPage?.(Number(select.value));
      navigationMode = 'SELECT';
      patchUI();
    });
    pageSelect = select;
    return pageSelect;
  }

  function syncInspectorPage() {
    const select = ensurePageSelect();
    if (!select) return;
    const field = selectedField();
    select.innerHTML = '';
    pages().forEach((page) => {
      const option = document.createElement('option');
      option.value = String(page.id);
      option.textContent = pageLabel(page);
      option.selected = Number(field?.page || 0) === page.id;
      select.appendChild(option);
    });
    select.disabled = !field;
  }

  function drawClassicText(ctx, text, x, y, scale, foreground) {
    const font = window.JWPLCGfxClassicFont;
    if (!font || !text) return;
    ctx.fillStyle = foreground;
    let cursorX = x;
    for (const character of String(text)) {
      const glyph = font.glyphFor(character.codePointAt(0));
      for (let column = 0; column < font.cellWidth; column += 1) {
        const bits = column < font.bytesPerGlyph ? glyph[column] : 0;
        for (let row = 0; row < font.cellHeight; row += 1) {
          if (column < font.bytesPerGlyph && ((bits >> row) & 0x01) !== 0) {
            ctx.fillRect(
              (cursorX + column) * scale,
              (y + row) * scale,
              scale,
              scale);
          }
        }
      }
      cursorX += font.cellWidth;
    }
  }

  function indicatorText() {
    return `${String(activePage() + 1).padStart(2, '0')}/${String(pages().length).padStart(2, '0')}`;
  }

  function drawIndicator(ctx, scale) {
    if (pages().length <= 1) return;
    const selected = navigationMode === 'SELECT';
    const background = selected ? '#000000' : '#ffffff';
    const foreground = selected ? '#ffffff' : '#000000';
    const border = '#ffffff';

    ctx.save();
    ctx.fillStyle = background;
    ctx.fillRect(INDICATOR.x * scale, INDICATOR.y * scale, INDICATOR.w * scale, INDICATOR.h * scale);
    ctx.strokeStyle = border;
    ctx.lineWidth = Math.max(1, scale);
    ctx.strokeRect(
      INDICATOR.x * scale + 0.5,
      INDICATOR.y * scale + 0.5,
      INDICATOR.w * scale - 1,
      INDICATOR.h * scale - 1);
    drawClassicText(ctx, indicatorText(), INDICATOR.textX, INDICATOR.textY, scale, foreground);
    ctx.restore();
  }

  function patchCanvases() {
    const zoom = Math.max(1, Number(zoomSelect?.value) || 3);
    const cfg = getNavBarConfig();

    // La barra HMI se dibuja en logicalCanvas (app.js → rebuildLogicalImage) y por
    // tanto ya aparece en lienzo, preview y minimapa. Sin barra: indicador NN/TT.
    if (!cfg.enabled) {
      drawIndicator(previewCanvas.getContext('2d', { alpha: false }), 1);
      drawIndicator(displayCanvas.getContext('2d', { alpha: false }), zoom);
    }
  }

  // Intercepción de Clics en la Barra de Navegación del Canvas
  displayCanvas.addEventListener('click', (e) => {
    const cfg = getNavBarConfig();
    if (!cfg.enabled || pages().length < 1) return;

    const rect = displayCanvas.getBoundingClientRect();
    const zoom = Math.max(1, Number(zoomSelect?.value) || 3);
    const px = Math.floor((e.clientX - rect.left) / zoom);
    const py = Math.floor((e.clientY - rect.top) / zoom);

    const layout = computeNavBarLayout(pages(), activePage());
    if (!layout.visible) return;

    if (py < layout.y || py > layout.y + layout.h) return;

    // Selecciona el objeto NAV_BAR en el lienzo/inspector
    const navBarField = editor()?.getAllFields?.()?.find((f) => f.type === 'NAV_BAR');
    if (navBarField && editor()?.setSelectedFieldKeys) {
      editor().setSelectedFieldKeys([navBarField.key]);
    }

    // Clic en pestaña
    for (const tab of layout.tabs) {
      if (px >= tab.x && px <= tab.x + tab.w) {
        if (tab.pageId !== activePage()) {
          editor()?.setActivePage?.(tab.pageId);
          patchUI();
        }
        return;
      }
    }

    // Clic en flecha izquierda ◄
    if (layout.leftArrow && layout.leftArrow.enabled) {
      if (px >= layout.leftArrow.x && px <= layout.leftArrow.x + layout.leftArrow.w) {
        editor()?.setActivePage?.(layout.leftArrow.targetPage);
        patchUI();
        return;
      }
    }

    // Clic en flecha derecha ►
    if (layout.rightArrow && layout.rightArrow.enabled) {
      if (px >= layout.rightArrow.x && px <= layout.rightArrow.x + layout.rightArrow.w) {
        editor()?.setActivePage?.(layout.rightArrow.targetPage);
        patchUI();
        return;
      }
    }
  });

  // --- Sincronización del Inspector para Barra de Navegación ---
  function syncNavBarInspector() {
    const section = document.getElementById('navBarInspectorSection');
    if (!section || section.hidden) return;

    const cfg = getNavBarConfig();
    const presetSelect = document.getElementById('navBarInspectorPreset');
    const positionSelect = document.getElementById('navBarInspectorPosition');
    const heightSelect = document.getElementById('navBarInspectorHeight');
    const pagesList = document.getElementById('navBarInspectorPagesList');

    if (presetSelect && presetSelect.value !== (cfg.preset || 'industrial-color')) {
      presetSelect.value = cfg.preset || 'industrial-color';
    }
    if (positionSelect && positionSelect.value !== (cfg.position || 'bottom')) {
      positionSelect.value = cfg.position || 'bottom';
    }
    if (heightSelect && heightSelect.value !== String(cfg.height || 22)) {
      heightSelect.value = String(cfg.height || 22);
    }

    if (pagesList) {
      pagesList.innerHTML = '';
      pages().forEach((page, idx) => {
        const row = document.createElement('div');
        row.className = 'hmi-nav-page-row';

        const num = document.createElement('span');
        num.className = 'hmi-nav-page-num';
        num.textContent = `${String(idx + 1).padStart(2, '0')}`;

        const name = document.createElement('span');
        name.className = 'hmi-nav-page-name';
        name.textContent = page.name;
        name.title = page.name;

        const shortInput = document.createElement('input');
        shortInput.type = 'text';
        shortInput.className = 'hmi-nav-short-input';
        shortInput.maxLength = 4;
        shortInput.value = page.shortName || computeSmartShortName(page.name, idx);
        shortInput.title = 'Abreviatura en pantalla (máx 4 caracteres)';
        shortInput.addEventListener('change', (e) => {
          const val = e.target.value.trim().toUpperCase().slice(0, 4);
          page.shortName = val;
          editor()?.setPageMetadata?.(page.id, { shortName: val });
          patchUI();
        });

        const colorBtn = document.createElement('button');
        colorBtn.type = 'button';
        colorBtn.className = 'hmi-nav-color-btn';
        const curCol = page.color ?? defaultPageColor(idx);
        colorBtn.style.backgroundColor = pageColorCss(curCol);
        colorBtn.title = 'Clic para cambiar color ST7789';

        colorBtn.addEventListener('click', (e) => {
          e.stopPropagation();
          const curHex = page.color ?? defaultPageColor(idx);
          const foundIdx = PRESET_PAGE_COLORS.findIndex((c) => c.hex565 === curHex);
          const nextIdx = (foundIdx + 1) % PRESET_PAGE_COLORS.length;
          const nextCol = PRESET_PAGE_COLORS[nextIdx].hex565;
          page.color = nextCol;
          editor()?.setPageMetadata?.(page.id, { color: nextCol });
          colorBtn.style.backgroundColor = PRESET_PAGE_COLORS[nextIdx].css;
          patchUI();
        });

        row.appendChild(num);
        row.appendChild(name);
        row.appendChild(shortInput);
        row.appendChild(colorBtn);
        pagesList.appendChild(row);
      });
    }
  }

  // Event Listeners de Controles del Inspector de Barra HMI
  document.getElementById('navBarInspectorPreset')?.addEventListener('change', (e) => {
    updateNavBarConfig({ preset: e.target.value });
  });

  document.getElementById('navBarInspectorPosition')?.addEventListener('change', (e) => {
    updateNavBarConfig({ position: e.target.value });
  });

  document.getElementById('navBarInspectorHeight')?.addEventListener('change', (e) => {
    updateNavBarConfig({ height: Number(e.target.value) });
  });

  document.getElementById('navBarInspectorDeleteBtn')?.addEventListener('click', () => {
    const navBarField = editor()?.getAllFields?.()?.find((f) => f.type === 'NAV_BAR');
    if (navBarField && editor()?.setSelectedFieldKeys) {
      editor().setSelectedFieldKeys([navBarField.key]);
      editor()?.deleteSelectedField?.();
    }
    updateNavBarConfig({ enabled: false });
  });

  function patchGeneratedCode() {
    if (!codeOutput?.textContent.startsWith('// Código generado por JWPLC HMI Designer')) return;

    let text = codeOutput.textContent
      .replace('// API pública JWPLC_UI · Alpha11 A11-3B', '// API pública JWPLC_UI · Alpha11 A11-3E')
      .replace('// API pública JWPLC_UI · Alpha11 A11-3C', '// API pública JWPLC_UI · Alpha11 A11-3E')
      .replace('// API pública JWPLC_UI · Alpha11 A11-3D', '// API pública JWPLC_UI · Alpha11 A11-3E');

    if (!text.includes('JWPLC_Display.setUserPageCount(')) {
      const registration = `        sizeof(HMI_FIELDS) / sizeof(HMI_FIELDS[0]));`;
      const setup = `${registration}\n    JWPLC_Display.setUserPageCount(${pages().length});\n    JWPLC_Display.setUserPage(0);`;
      text = text.replace(registration, setup);
    }

    codeOutput.textContent = text;
  }

  function patchStatus() {
    const current = pages().find((page) => page.id === activePage());
    const cfg = getNavBarConfig();
    if (statusPage) statusPage.textContent = `Página: ${activePage()} · ${current?.name || 'Página'}`;
    if (gate) gate.textContent = cfg.enabled ? `Gate: A11-3E PAGES · Menú HMI (${cfg.preset})` : 'Gate: A11-3E PAGES · indicador NN/TT';
    if (bottomSummary) bottomSummary.textContent = cfg.enabled ? `A11-3E · Páginas + Barra HMI (${pages().length} págs)` : 'A11-3E · Páginas + navegación compacta';
  }

  function patchUI() {
    rebuildTopTabs();
    syncInspectorPage();
    syncNavBarInspector();
    patchStatus();
    patchCanvases();
    patchGeneratedCode();
  }

  window.addEventListener('jwplc:editor-refresh', patchUI);
  window.addEventListener('resize', () => setTimeout(patchUI, 0));
  contractTab?.addEventListener('click', () => setTimeout(patchGeneratedCode, 0));
  statusTab?.addEventListener('click', () => setTimeout(patchUI, 0));
  generateButton?.addEventListener('click', () => setTimeout(patchGeneratedCode, 0));

  patchUI();

  window.JWPLCHMIPages = {
    getNavigationMode: () => navigationMode,
    setNavigationMode: (mode) => {
      navigationMode = mode === 'CONTENT' ? 'CONTENT' : 'SELECT';
      patchUI();
    },
    indicatorRect: () => ({ ...INDICATOR }),
    getNavBarConfig,
    setNavBarConfig: updateNavBarConfig,
    computeNavBarLayout,
    computeSmartShortName,
    drawHMINavigationBar,
    syncNavBarInspector,
    PRESET_PAGE_COLORS
  };
})();

// A11-4: el refinamiento de codegen depende de que el modelo de páginas ya esté
// disponible. Se carga desde aquí sin acoplarlo al core validado de app.js.
(() => {
  if (document.querySelector('script[data-a11-codegen]')) return;
  const script = document.createElement('script');
  script.src = './designer-codegen.js';
  script.async = false;
  script.dataset.a11Codegen = '1';
  document.body.appendChild(script);
})();
