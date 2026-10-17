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

  // --- Micro Font (4x6 cell: 3x5 bitmap + 1px spacing) para HMI ultra compacto ---
  const MICRO_FONT_3X5 = {
    '0': [0x1F, 0x11, 0x1F], '1': [0x00, 0x1F, 0x00], '2': [0x1D, 0x15, 0x17],
    '3': [0x15, 0x15, 0x1F], '4': [0x07, 0x04, 0x1F], '5': [0x17, 0x15, 0x1D],
    '6': [0x1F, 0x15, 0x1D], '7': [0x01, 0x1D, 0x03], '8': [0x1F, 0x15, 0x1F],
    '9': [0x17, 0x15, 0x1F], 'A': [0x1E, 0x05, 0x1E], 'B': [0x1F, 0x15, 0x0A],
    'C': [0x0E, 0x11, 0x11], 'D': [0x1F, 0x11, 0x0E], 'E': [0x1F, 0x15, 0x11],
    'F': [0x1F, 0x05, 0x01], 'G': [0x0E, 0x15, 0x1D], 'H': [0x1F, 0x04, 0x1F],
    'I': [0x11, 0x1F, 0x11], 'J': [0x08, 0x10, 0x0F], 'K': [0x1F, 0x04, 0x1B],
    'L': [0x1F, 0x10, 0x10], 'M': [0x1F, 0x02, 0x1F], 'N': [0x1F, 0x06, 0x1F],
    'O': [0x0E, 0x11, 0x0E], 'P': [0x1F, 0x05, 0x02], 'Q': [0x0E, 0x11, 0x1E],
    'R': [0x1F, 0x05, 0x1A], 'S': [0x12, 0x15, 0x09], 'T': [0x01, 0x1F, 0x01],
    'U': [0x0F, 0x10, 0x0F], 'V': [0x07, 0x18, 0x07], 'W': [0x1F, 0x08, 0x1F],
    'X': [0x1B, 0x04, 0x1B], 'Y': [0x03, 0x1C, 0x03], 'Z': [0x19, 0x15, 0x13],
    ':': [0x00, 0x0A, 0x00], '.': [0x00, 0x10, 0x00], '-': [0x04, 0x04, 0x04],
    '/': [0x18, 0x06, 0x01], '_': [0x10, 0x10, 0x10], '<': [0x04, 0x0A, 0x11],
    '>': [0x11, 0x0A, 0x04], ' ': [0x00, 0x00, 0x00]
  };

  function drawMicroText(ctx, text, x, y, scale, color) {
    if (!text) return;
    ctx.fillStyle = color;
    let cursorX = x;
    const str = String(text).toUpperCase();
    for (const char of str) {
      const cols = MICRO_FONT_3X5[char] || MICRO_FONT_3X5[' '];
      for (let c = 0; c < 3; c++) {
        const bits = cols[c];
        for (let r = 0; r < 5; r++) {
          if ((bits & (1 << r)) !== 0) {
            ctx.fillRect((cursorX + c) * scale, (y + r) * scale, scale, scale);
          }
        }
      }
      cursorX += 4;
    }
  }

  function getNavBarConfig() {
    const navBarField = editor()?.getAllFields?.()?.find((f) => f.type === 'NAV_BAR');
    const fromEditor = editor()?.getNavBarConfig?.() || {};

    const pos = navBarField?.position || fromEditor.position || 'bottom';
    const isVert = pos === 'left' || pos === 'right' || navBarField?.orientation === 'vertical' || fromEditor.orientation === 'vertical';

    const defaultW = isVert ? 22 : 320;
    const defaultH = isVert ? 170 : 22;

    let defaultX = 0;
    if (pos === 'right') defaultX = 320 - (Number(navBarField?.width || fromEditor.width) || defaultW) - 2;
    else if (pos === 'left') defaultX = 2;

    let defaultY = 0;
    if (pos === 'top') defaultY = 2;
    else if (pos === 'bottom') defaultY = 170 - (Number(navBarField?.height || fromEditor.height) || defaultH) - 1;

    const xVal = navBarField?.x !== undefined ? Number(navBarField.x) : (fromEditor.x !== undefined ? Number(fromEditor.x) : defaultX);
    const yVal = navBarField?.y !== undefined ? Number(navBarField.y) : (fromEditor.y !== undefined ? Number(fromEditor.y) : defaultY);

    return {
      showIndicator: true,
      showArrows: true,
      ...fromEditor,
      enabled: Boolean(navBarField && !navBarField.hidden),
      preset: navBarField?.preset || fromEditor.preset || 'industrial-color',
      position: pos,
      orientation: isVert ? 'vertical' : 'horizontal',
      x: xVal,
      y: yVal,
      width: Number(navBarField?.width || fromEditor.width || defaultW),
      height: Number(navBarField?.height || fromEditor.height || defaultH),
      tabDistribution: navBarField?.tabDistribution || fromEditor.tabDistribution || 'center',
      textAlign: navBarField?.textAlign || fromEditor.textAlign || 'center',
      fontSize: navBarField?.fontSize || fromEditor.fontSize || 'normal',
      labelFormat: navBarField?.labelFormat || fromEditor.labelFormat || 'short',
      bgMode: navBarField?.bgMode || fromEditor.bgMode || 'solid',
      borderMode: navBarField?.borderMode || fromEditor.borderMode || 'none',
      radius: navBarField?.radius !== undefined ? Number(navBarField.radius) : (fromEditor.radius !== undefined ? Number(fromEditor.radius) : 3),
      gap: navBarField?.gap !== undefined ? Number(navBarField.gap) : (fromEditor.gap !== undefined ? Number(fromEditor.gap) : 3)
    };
  }

  function updateNavBarConfig(next) {
    const current = getNavBarConfig();
    const updated = { ...current, ...next };
    const navBarField = editor()?.getAllFields?.()?.find((f) => f.type === 'NAV_BAR');

    if (next.position !== undefined) {
      const isVert = next.position === 'left' || next.position === 'right';
      updated.orientation = isVert ? 'vertical' : 'horizontal';
      if (isVert) {
        const curW = Number(next.width || current.width || 24);
        updated.width = curW >= 100 ? 24 : curW;
        updated.height = 170;
        updated.y = 0;
        if (next.position === 'left') {
          updated.x = 2;
        } else {
          updated.x = 320 - updated.width - 2;
        }
      } else {
        const curH = Number(next.height || current.height || 22);
        updated.height = curH >= 100 ? 22 : curH;
        updated.width = 320;
        updated.x = 0;
        if (next.position === 'top') {
          updated.y = 2;
        } else {
          updated.y = 170 - updated.height - 1;
        }
      }
    }

    if (next.x !== undefined) updated.x = Number(next.x);
    if (next.y !== undefined) updated.y = Number(next.y);
    if (next.width !== undefined) updated.width = Number(next.width);
    if (next.height !== undefined) updated.height = Number(next.height);

    if (navBarField) {
      if (next.preset !== undefined) navBarField.preset = next.preset;
      if (next.position !== undefined) {
        navBarField.position = next.position;
        navBarField.orientation = updated.orientation;
      }
      navBarField.x = updated.x;
      navBarField.y = updated.y;
      navBarField.width = updated.width;
      navBarField.height = updated.height;
      if (next.tabDistribution !== undefined) navBarField.tabDistribution = next.tabDistribution;
      if (next.textAlign !== undefined) navBarField.textAlign = next.textAlign;
      if (next.fontSize !== undefined) navBarField.fontSize = next.fontSize;
      if (next.labelFormat !== undefined) navBarField.labelFormat = next.labelFormat;
      if (next.bgMode !== undefined) navBarField.bgMode = next.bgMode;
      if (next.borderMode !== undefined) navBarField.borderMode = next.borderMode;
      if (next.radius !== undefined) navBarField.radius = Number(next.radius);
      if (next.gap !== undefined) navBarField.gap = Number(next.gap);
      if (next.enabled !== undefined) navBarField.hidden = !next.enabled;
    }
    editor()?.setNavBarConfig?.(updated);
    patchUI();
  }

  function syncNavBarConfigFromField(field) {
    if (!field || field.type !== 'NAV_BAR') return;
    const current = getNavBarConfig();
    const updated = {
      ...current,
      x: field.x,
      y: field.y,
      width: field.width || current.width,
      height: field.height || current.height,
      position: field.position || current.position
    };
    editor()?.setNavBarConfig?.(updated);
    syncNavBarInspector();
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

  // --- Algoritmo de Distribución Adaptativo para la Barra HMI (Horizontal y Vertical) ---
  function computeNavBarLayout(pageList, curActivePage) {
    const total = pageList.length;
    const cfg = getNavBarConfig();
    const isVert = cfg.position === 'left' || cfg.position === 'right' || cfg.orientation === 'vertical';

    const barW = Number(cfg.width) || (isVert ? 22 : 320);
    const barH = Number(cfg.height) || (isVert ? 170 : 22);
    const barX = Number(cfg.x) !== undefined && !Number.isNaN(Number(cfg.x)) ? Number(cfg.x) : 0;
    const barY = Number(cfg.y) !== undefined && !Number.isNaN(Number(cfg.y)) ? Number(cfg.y) : 0;

    if (!cfg.enabled || total < 1) {
      return {
        visible: false,
        isVertical: isVert,
        hasOverflow: false,
        tabs: [],
        leftArrow: null,
        rightArrow: null,
        counter: null,
        x: barX,
        y: barY,
        w: barW,
        h: barH,
        bounds: { x: barX, y: barY, w: barW, h: barH },
        cluster: null
      };
    }

    const preset = cfg.preset || 'industrial-color';

    // CASO ESPECIAL: PRESET COMPACT-DOTS (Puntos Agrupados y Minimalistas)
    if (preset === 'compact-dots') {
      const dotDiameter = 5;
      const dotGap = 5;

      if (!isVert) {
        // Horizontal: cápsula activa de 12x5, inactivas de 5x5
        const activeDotW = 12;
        const totalSpan = (total - 1) * (dotDiameter + dotGap) + activeDotW;

        let clusterX;
        if (cfg.tabDistribution === 'start') clusterX = barX + 8;
        else if (cfg.tabDistribution === 'end') clusterX = barX + barW - 8 - totalSpan;
        else clusterX = barX + Math.floor((barW - totalSpan) / 2);

        const clusterY = barY + Math.floor((barH - dotDiameter) / 2);

        let curX = clusterX;
        const tabs = pageList.map((page, idx) => {
          const isActive = page.id === curActivePage;
          const w = isActive ? activeDotW : dotDiameter;
          const h = dotDiameter;
          const x = curX;
          const y = clusterY;
          curX += w + dotGap;
          return {
            pageId: page.id,
            pageIndex: idx,
            name: page.name,
            shortName: page.shortName || computeSmartShortName(page.name, idx),
            color: page.color ?? defaultPageColor(idx),
            x,
            y,
            w,
            h,
            hitX: x - 4,
            hitY: barY,
            hitW: w + 8,
            hitH: barH,
            isActive
          };
        });

        return {
          visible: true,
          isVertical: false,
          hasOverflow: false,
          tabs,
          leftArrow: null,
          rightArrow: null,
          counter: null,
          x: barX,
          y: barY,
          w: barW,
          h: barH,
          bounds: { x: barX, y: barY, w: barW, h: barH },
          cluster: { x: clusterX, y: clusterY, w: totalSpan, h: dotDiameter }
        };
      } else {
        // Vertical: cápsula activa de 5x12, inactivas de 5x5
        const activeDotH = 12;
        const totalSpan = (total - 1) * (dotDiameter + dotGap) + activeDotH;

        const clusterX = barX + Math.floor((barW - dotDiameter) / 2);
        let clusterY;
        if (cfg.tabDistribution === 'start') clusterY = barY + 8;
        else if (cfg.tabDistribution === 'end') clusterY = barY + barH - 8 - totalSpan;
        else clusterY = barY + Math.floor((barH - totalSpan) / 2);

        let curY = clusterY;
        const tabs = pageList.map((page, idx) => {
          const isActive = page.id === curActivePage;
          const w = dotDiameter;
          const h = isActive ? activeDotH : dotDiameter;
          const x = clusterX;
          const y = curY;
          curY += h + dotGap;
          return {
            pageId: page.id,
            pageIndex: idx,
            name: page.name,
            shortName: page.shortName || computeSmartShortName(page.name, idx),
            color: page.color ?? defaultPageColor(idx),
            x,
            y,
            w,
            h,
            hitX: barX,
            hitY: y - 4,
            hitW: barW,
            hitH: h + 8,
            isActive
          };
        });

        return {
          visible: true,
          isVertical: true,
          hasOverflow: false,
          tabs,
          leftArrow: null,
          rightArrow: null,
          counter: null,
          x: barX,
          y: barY,
          w: barW,
          h: barH,
          bounds: { x: barX, y: barY, w: barW, h: barH },
          cluster: { x: clusterX, y: clusterY, w: dotDiameter, h: totalSpan }
        };
      }
    }

    // DISPOSICIÓN VERTICAL (Pestañas Apiladas en Lateral Izquierdo o Derecho)
    if (isVert) {
      const margin = 3;
      const gap = Number(cfg.gap) || 3;
      const availH = barH - (2 * margin);
      const minTabH = 16;
      const neededH = (total * minTabH) + ((total - 1) * gap);

      // Si caben verticalmente
      if (neededH <= availH) {
        const tabH = Math.min(32, Math.floor((availH - (total - 1) * gap) / total));
        const totalSpan = (total * tabH) + ((total - 1) * gap);
        let startY;
        if (cfg.tabDistribution === 'start') startY = barY + margin;
        else if (cfg.tabDistribution === 'end') startY = barY + barH - margin - totalSpan;
        else startY = barY + Math.floor((barH - totalSpan) / 2);

        const tabW = barW - 4;
        const tabX = barX + 2;

        const tabs = pageList.map((page, idx) => {
          const y = startY + idx * (tabH + gap);
          return {
            pageId: page.id,
            pageIndex: idx,
            name: page.name,
            shortName: page.shortName || computeSmartShortName(page.name, idx),
            color: page.color ?? defaultPageColor(idx),
            x: tabX,
            y,
            w: tabW,
            h: tabH,
            hitX: tabX,
            hitY: y,
            hitW: tabW,
            hitH: tabH,
            isActive: page.id === curActivePage
          };
        });

        return {
          visible: true,
          isVertical: true,
          hasOverflow: false,
          tabs,
          leftArrow: null,
          rightArrow: null,
          counter: null,
          x: barX,
          y: barY,
          w: barW,
          h: barH,
          bounds: { x: barX, y: barY, w: barW, h: barH },
          cluster: null
        };
      } else {
        // Desbordamiento vertical
        const arrowH = 14;
        const counterH = 18;
        const reservedH = (2 * margin) + (2 * arrowH) + counterH + (4 * gap);
        const tabsSpaceH = barH - reservedH;
        const tabH = 18;
        const visibleCount = Math.max(2, Math.min(total, Math.floor((tabsSpaceH + gap) / (tabH + gap))));
        const activeIdx = Math.max(0, pageList.findIndex((p) => p.id === curActivePage));

        const half = Math.floor(visibleCount / 2);
        let startIdx = activeIdx - half;
        if (startIdx < 0) startIdx = 0;
        if (startIdx + visibleCount > total) startIdx = Math.max(0, total - visibleCount);
        const endIdx = Math.min(total - 1, startIdx + visibleCount - 1);

        const tabW = barW - 4;
        const tabX = barX + 2;
        let cursorY = barY + margin;

        const leftArrow = {
          x: tabX,
          y: cursorY,
          w: tabW,
          h: arrowH,
          enabled: startIdx > 0,
          targetPage: Math.max(0, activeIdx - 1),
          text: '^'
        };
        cursorY += arrowH + gap;

        const tabs = [];
        for (let i = startIdx; i <= endIdx; i++) {
          const page = pageList[i];
          tabs.push({
            pageId: page.id,
            pageIndex: i,
            name: page.name,
            shortName: page.shortName || computeSmartShortName(page.name, i),
            color: page.color ?? defaultPageColor(i),
            x: tabX,
            y: cursorY,
            w: tabW,
            h: tabH,
            hitX: tabX,
            hitY: cursorY,
            hitW: tabW,
            hitH: tabH,
            isActive: page.id === curActivePage
          });
          cursorY += tabH + gap;
        }

        const rightArrow = {
          x: tabX,
          y: cursorY,
          w: tabW,
          h: arrowH,
          enabled: endIdx < total - 1,
          targetPage: Math.min(total - 1, activeIdx + 1),
          text: 'v'
        };
        cursorY += arrowH + gap;

        const counter = {
          x: tabX,
          y: cursorY,
          w: tabW,
          h: counterH,
          text: `${activeIdx + 1}/${total}`
        };

        return {
          visible: true,
          isVertical: true,
          hasOverflow: true,
          tabs,
          leftArrow,
          rightArrow,
          counter,
          x: barX,
          y: barY,
          w: barW,
          h: barH,
          bounds: { x: barX, y: barY, w: barW, h: barH },
          cluster: null
        };
      }
    }

    // DISPOSICIÓN HORIZONTAL (Inferior, Superior o Libre)
    const margin = 4;
    const gap = Number(cfg.gap) || 3;
    const availWidth = barW - (2 * margin);
    const minTabW = 30;
    const neededWidth = (total * minTabW) + ((total - 1) * gap);

    // Caso 1: Todas las páginas caben en el ancho horizontal
    if (neededWidth <= availWidth) {
      let tabW;
      if (cfg.tabDistribution === 'stretch') {
        tabW = Math.floor((availWidth - (total - 1) * gap) / total);
      } else {
        tabW = Math.min(total <= 3 ? 90 : 80, Math.floor((availWidth - (total - 1) * gap) / total));
      }

      const totalSpan = (total * tabW) + ((total - 1) * gap);
      let startX;
      if (cfg.tabDistribution === 'start') startX = barX + margin;
      else if (cfg.tabDistribution === 'end') startX = barX + barW - margin - totalSpan;
      else startX = barX + Math.floor((barW - totalSpan) / 2);

      const tabH = barH - 2;
      const tabY = barY + 1;

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
          y: tabY,
          w: tabW,
          h: tabH,
          hitX: x,
          hitY: tabY,
          hitW: tabW,
          hitH: tabH,
          isActive: page.id === curActivePage
        };
      });

      return {
        visible: true,
        isVertical: false,
        hasOverflow: false,
        tabs,
        leftArrow: null,
        rightArrow: null,
        counter: null,
        x: barX,
        y: barY,
        w: barW,
        h: barH,
        bounds: { x: barX, y: barY, w: barW, h: barH },
        cluster: null
      };
    }

    // Caso 2: Desbordamiento horizontal
    const arrowW = 16;
    const counterW = 34;
    const reserved = (2 * margin) + (2 * arrowW) + counterW + (4 * gap);
    const tabsSpace = barW - reserved;

    const tabW = 40;
    const visibleCount = Math.max(2, Math.min(total, Math.floor((tabsSpace + gap) / (tabW + gap))));
    const activeIdx = Math.max(0, pageList.findIndex((p) => p.id === curActivePage));

    const half = Math.floor(visibleCount / 2);
    let startIdx = activeIdx - half;
    if (startIdx < 0) startIdx = 0;
    if (startIdx + visibleCount > total) startIdx = Math.max(0, total - visibleCount);
    const endIdx = Math.min(total - 1, startIdx + visibleCount - 1);

    const tabH = barH - 2;
    const tabY = barY + 1;
    let cursorX = barX + margin;

    const leftArrow = {
      x: cursorX,
      y: tabY,
      w: arrowW,
      h: tabH,
      enabled: startIdx > 0,
      targetPage: Math.max(0, activeIdx - 1),
      text: '<'
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
        y: tabY,
        w: tabW,
        h: tabH,
        hitX: cursorX,
        hitY: tabY,
        hitW: tabW,
        hitH: tabH,
        isActive: page.id === curActivePage
      });
      cursorX += tabW + gap;
    }

    const rightArrow = {
      x: cursorX,
      y: tabY,
      w: arrowW,
      h: tabH,
      enabled: endIdx < total - 1,
      targetPage: Math.min(total - 1, activeIdx + 1),
      text: '>'
    };
    cursorX += arrowW + gap;

    const counter = {
      x: cursorX,
      y: tabY,
      w: counterW,
      h: tabH,
      text: `${String(activeIdx + 1).padStart(2, '0')}/${String(total).padStart(2, '0')}`
    };

    return {
      visible: true,
      isVertical: false,
      hasOverflow: true,
      tabs,
      leftArrow,
      rightArrow,
      counter,
      x: barX,
      y: barY,
      w: barW,
      h: barH,
      bounds: { x: barX, y: barY, w: barW, h: barH },
      cluster: null
    };
  }

  // --- Renderizado Gráfico de la Barra de Navegación HMI ---
  function drawHMINavigationBar(ctx, scale) {
    const cfg = getNavBarConfig();
    if (!cfg.enabled || pages().length < 1) return;

    const layout = computeNavBarLayout(pages(), activePage());
    if (!layout.visible) return;

    ctx.save();

    function fillRounded(x, y, w, h, r, col) {
      ctx.fillStyle = col;
      ctx.beginPath();
      const rx = Math.min(r, Math.floor(w / 2), Math.floor(h / 2)) * scale;
      if (typeof ctx.roundRect === 'function' && rx > 0) {
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
      const rx = Math.min(r, Math.floor(w / 2), Math.floor(h / 2)) * scale;
      if (typeof ctx.roundRect === 'function' && rx > 0) {
        ctx.roundRect(x * scale + 0.5, y * scale + 0.5, w * scale - 1, h * scale - 1, rx);
      } else {
        ctx.strokeRect(x * scale + 0.5, y * scale + 0.5, w * scale - 1, h * scale - 1);
      }
      ctx.stroke();
    }

    // 1. Fondo de la barra
    const preset = cfg.preset || 'industrial-color';
    const bgMode = cfg.bgMode || 'solid';

    if (preset === 'compact-dots') {
      // Para compact-dots: fondo discreto sólo en torno al cluster (si no es transparente)
      if (bgMode === 'solid' || bgMode === 'dark') {
        const cl = layout.cluster;
        if (cl) {
          const pillPadX = layout.isVertical ? 4 : 7;
          const pillPadY = layout.isVertical ? 7 : 4;
          const pillX = cl.x - pillPadX;
          const pillY = cl.y - pillPadY;
          const pillW = cl.w + pillPadX * 2;
          const pillH = cl.h + pillPadY * 2;
          const bgCol = bgMode === 'dark' ? '#0b141d' : '#000000';
          fillRounded(pillX, pillY, pillW, pillH, 7, bgCol);
          strokeRounded(pillX, pillY, pillW, pillH, 7, '#1b2d3d', 1);
        }
      }
    } else {
      if (bgMode === 'solid') {
        fillRounded(layout.x, layout.y, layout.w, layout.h, cfg.radius, '#000000');
      } else if (bgMode === 'dark') {
        fillRounded(layout.x, layout.y, layout.w, layout.h, cfg.radius, '#0b141d');
      }
    }

    // 2. Borde o línea divisoria
    if (cfg.borderMode === 'line') {
      ctx.strokeStyle = '#273c4f';
      ctx.lineWidth = Math.max(1, scale);
      ctx.beginPath();
      if (layout.isVertical) {
        const lx = (cfg.position === 'right') ? (layout.x * scale) : ((layout.x + layout.w) * scale);
        ctx.moveTo(lx, layout.y * scale);
        ctx.lineTo(lx, (layout.y + layout.h) * scale);
      } else {
        const ly = (cfg.position === 'top') ? ((layout.y + layout.h) * scale) : (layout.y * scale);
        ctx.moveTo(layout.x * scale, ly);
        ctx.lineTo((layout.x + layout.w) * scale, ly);
      }
      ctx.stroke();
    } else if (cfg.borderMode === 'frame') {
      strokeRounded(layout.x, layout.y, layout.w, layout.h, cfg.radius, '#273c4f', 1);
    }

    // Función de texto con tamaño y alineación
    function drawTabText(text, x, y, w, h, color) {
      if (!text) return;
      const str = String(text);
      const isMicro = cfg.fontSize === 'micro';
      const isMedium = cfg.fontSize === 'medium';
      const charW = isMicro ? 4 : (isMedium ? 7 : 6);
      const charH = isMicro ? 5 : (isMedium ? 9 : 7);
      const textW = str.length * charW - 1;

      let tx;
      if (cfg.textAlign === 'left') tx = x + 3;
      else if (cfg.textAlign === 'right') tx = x + w - textW - 3;
      else tx = x + Math.floor((w - textW) / 2);

      const ty = y + Math.floor((h - charH) / 2);

      if (isMicro) {
        drawMicroText(ctx, str, tx, ty, scale, color);
      } else {
        drawClassicText(ctx, str, tx, ty, scale, color);
      }
    }

    function formatLabel(tab) {
      if (cfg.labelFormat === 'number') return String(tab.pageIndex + 1);
      if (cfg.labelFormat === 'num_short') return `${tab.pageIndex + 1}:${tab.shortName}`;
      return tab.shortName;
    }

    const rad = Number(cfg.radius !== undefined ? cfg.radius : 3);

    // 3. Dibujo de cada pestaña según preset
    layout.tabs.forEach((tab) => {
      const colCss = pageColorCss(tab.color);
      const label = formatLabel(tab);

      if (preset === 'industrial-color') {
        if (tab.isActive) {
          fillRounded(tab.x, tab.y, tab.w, tab.h, rad, '#ffffff');
          strokeRounded(tab.x, tab.y, tab.w, tab.h, rad, colCss, 1.5);
          drawTabText(label, tab.x, tab.y, tab.w, tab.h, colCss === '#ffffff' ? '#9c27b0' : colCss);
        } else {
          fillRounded(tab.x, tab.y, tab.w, tab.h, rad, '#000000');
          strokeRounded(tab.x, tab.y, tab.w, tab.h, rad, colCss, 1);
          drawTabText(label, tab.x, tab.y, tab.w, tab.h, colCss);
        }
      } else if (preset === 'dark-pill') {
        if (tab.isActive) {
          fillRounded(tab.x, tab.y, tab.w, tab.h, Math.max(rad, 4), '#00c8ff');
          drawTabText(label, tab.x, tab.y, tab.w, tab.h, '#000000');
        } else {
          fillRounded(tab.x, tab.y, tab.w, tab.h, Math.max(rad, 4), '#151e2a');
          strokeRounded(tab.x, tab.y, tab.w, tab.h, Math.max(rad, 4), '#27384c', 1);
          drawTabText(label, tab.x, tab.y, tab.w, tab.h, '#8ca2b6');
        }
      } else if (preset === 'segmented-bar') {
        if (tab.isActive) {
          fillRounded(tab.x, tab.y, tab.w, tab.h, 2, '#1f2e42');
          ctx.fillStyle = '#52c9ff';
          if (layout.isVertical) {
            const ax = (cfg.position === 'right') ? (tab.x + tab.w - 2) : tab.x;
            ctx.fillRect(ax * scale, tab.y * scale, 2 * scale, tab.h * scale);
          } else {
            const ay = (cfg.position === 'top') ? (tab.y + tab.h - 2) : tab.y;
            ctx.fillRect(tab.x * scale, ay * scale, tab.w * scale, 2 * scale);
          }
          drawTabText(label, tab.x, tab.y, tab.w, tab.h, '#ffffff');
        } else {
          fillRounded(tab.x, tab.y, tab.w, tab.h, 2, '#0e1520');
          strokeRounded(tab.x, tab.y, tab.w, tab.h, 2, '#1e2b3c', 1);
          drawTabText(label, tab.x, tab.y, tab.w, tab.h, '#768d9f');
        }
      } else if (preset === 'compact-dots') {
        // Puntos limpios, elegantes y proporcionados
        if (tab.isActive) {
          fillRounded(tab.x, tab.y, tab.w, tab.h, 2.5, '#00c8ff');
        } else {
          fillRounded(tab.x, tab.y, tab.w, tab.h, 2.5, '#2e4357');
        }
      } else if (preset === 'edge-tabs') {
        // Lengüetas minimalistas en el borde
        if (tab.isActive) {
          fillRounded(tab.x, tab.y, tab.w, tab.h, 2, colCss);
          drawTabText(label, tab.x, tab.y, tab.w, tab.h, '#000000');
        } else {
          fillRounded(tab.x, tab.y, tab.w, tab.h, 2, '#111b25');
          strokeRounded(tab.x, tab.y, tab.w, tab.h, 2, colCss, 1);
          drawTabText(label, tab.x, tab.y, tab.w, tab.h, '#7c92a6');
        }
      } else if (preset === 'numeric-pills') {
        // Insignias circulares / redondeadas con números
        const numText = String(tab.pageIndex + 1);
        if (tab.isActive) {
          fillRounded(tab.x, tab.y, tab.w, tab.h, Math.max(rad, 4), '#00c8ff');
          drawTabText(numText, tab.x, tab.y, tab.w, tab.h, '#000000');
        } else {
          fillRounded(tab.x, tab.y, tab.w, tab.h, Math.max(rad, 4), '#14202d');
          strokeRounded(tab.x, tab.y, tab.w, tab.h, Math.max(rad, 4), '#263a4d', 1);
          drawTabText(numText, tab.x, tab.y, tab.w, tab.h, '#8ca3b8');
        }
      }
    });

    // 4. Controles de desbordamiento (flechas y contador)
    if (layout.hasOverflow) {
      if (layout.leftArrow) {
        const la = layout.leftArrow;
        const color = la.enabled ? '#52c9ff' : '#283848';
        fillRounded(la.x, la.y, la.w, la.h, 3, '#0b1118');
        strokeRounded(la.x, la.y, la.w, la.h, 3, color, 1);
        drawTabText(la.text || '<', la.x, la.y, la.w, la.h, color);
      }
      if (layout.rightArrow) {
        const ra = layout.rightArrow;
        const color = ra.enabled ? '#52c9ff' : '#283848';
        fillRounded(ra.x, ra.y, ra.w, ra.h, 3, '#0b1118');
        strokeRounded(ra.x, ra.y, ra.w, ra.h, 3, color, 1);
        drawTabText(ra.text || '>', ra.x, ra.y, ra.w, ra.h, color);
      }
      if (layout.counter) {
        const cnt = layout.counter;
        fillRounded(cnt.x, cnt.y, cnt.w, cnt.h, 3, '#0b1118');
        strokeRounded(cnt.x, cnt.y, cnt.w, cnt.h, 3, '#3b4e62', 1);
        drawTabText(cnt.text, cnt.x, cnt.y, cnt.w, cnt.h, '#ffffff');
      }
    }

    ctx.restore();
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

    // Selecciona el objeto NAV_BAR en el lienzo/inspector si se hace clic dentro del área de la barra
    if (px >= layout.x && px <= layout.x + layout.w && py >= layout.y && py <= layout.y + layout.h) {
      const navBarField = editor()?.getAllFields?.()?.find((f) => f.type === 'NAV_BAR');
      if (navBarField && editor()?.setSelectedFieldKeys) {
        editor().setSelectedFieldKeys([navBarField.key]);
      }
    }

    // Clic en pestaña / punto
    for (const tab of layout.tabs) {
      const hx = tab.hitX !== undefined ? tab.hitX : tab.x;
      const hy = tab.hitY !== undefined ? tab.hitY : tab.y;
      const hw = tab.hitW !== undefined ? tab.hitW : tab.w;
      const hh = tab.hitH !== undefined ? tab.hitH : tab.h;
      if (px >= hx && px <= hx + hw && py >= hy && py <= hy + hh) {
        if (tab.pageId !== activePage()) {
          editor()?.setActivePage?.(tab.pageId);
          patchUI();
        }
        return;
      }
    }

    // Clic en flecha previa (◄ / ▲)
    if (layout.leftArrow && layout.leftArrow.enabled) {
      const la = layout.leftArrow;
      if (px >= la.x && px <= la.x + la.w && py >= la.y && py <= la.y + la.h) {
        editor()?.setActivePage?.(layout.leftArrow.targetPage);
        patchUI();
        return;
      }
    }

    // Clic en flecha siguiente (► / ▼)
    if (layout.rightArrow && layout.rightArrow.enabled) {
      const ra = layout.rightArrow;
      if (px >= ra.x && px <= ra.x + ra.w && py >= ra.y && py <= ra.y + ra.h) {
        editor()?.setActivePage?.(layout.rightArrow.targetPage);
        patchUI();
        return;
      }
    }
  });

  // --- Inspector Barra de Navegación HMI (Homogéneo con BAR) ---
  let activeNavBarTab = 'design';

  function switchNavBarMainTab(tabName) {
    activeNavBarTab = tabName;
    const section = document.getElementById('navBarInspectorSection');
    if (!section) return;

    section.querySelectorAll('.bar-main-tab[data-navbar-tab]').forEach((btn) => {
      const isAct = btn.dataset.navbarTab === tabName;
      btn.classList.toggle('active', isAct);
      btn.setAttribute('aria-selected', isAct ? 'true' : 'false');
    });

    const paneDesign = document.getElementById('navBarPaneDesign');
    const panePages = document.getElementById('navBarPanePages');
    const paneTech = document.getElementById('navBarPaneTech');

    if (paneDesign) {
      paneDesign.hidden = (tabName !== 'design');
      paneDesign.style.display = (tabName === 'design') ? 'flex' : 'none';
    }
    if (panePages) {
      panePages.hidden = (tabName !== 'pages');
      panePages.style.display = (tabName === 'pages') ? 'flex' : 'none';
    }
    if (paneTech) {
      paneTech.hidden = (tabName !== 'tech');
      paneTech.style.display = (tabName === 'tech') ? 'flex' : 'none';
    }
  }

  // Event Listeners de Pestañas Principales del Inspector
  document.querySelectorAll('#navBarInspectorSection .bar-main-tab[data-navbar-tab]').forEach((btn) => {
    btn.addEventListener('click', (e) => {
      e.preventDefault();
      switchNavBarMainTab(btn.dataset.navbarTab);
    });
  });

  // Event Listeners de Botones de Prediseño Visual
  document.querySelectorAll('#navBarInspectorSection .bar-preset-btn[data-navbar-preset]').forEach((btn) => {
    btn.addEventListener('click', (e) => {
      e.preventDefault();
      const preset = btn.dataset.navbarPreset;
      updateNavBarConfig({ preset });
    });
  });

  // Event Listeners de Posición en Pantalla (Inferior, Superior, Izquierda, Derecha)
  document.querySelectorAll('#navBarInspectorSection .bar-visual-btn[data-navbar-pos]').forEach((btn) => {
    btn.addEventListener('click', (e) => {
      e.preventDefault();
      const pos = btn.dataset.navbarPos;
      updateNavBarConfig({ position: pos });
    });
  });

  // Event Listeners de Centrado y Alineación Rápida
  document.querySelectorAll('#navBarInspectorSection .bar-visual-btn[data-navbar-align]').forEach((btn) => {
    btn.addEventListener('click', (e) => {
      e.preventDefault();
      const action = btn.dataset.navbarAlign;
      const cfg = getNavBarConfig();
      const w = Number(cfg.width) || 320;
      const h = Number(cfg.height) || 22;

      if (action === 'hcenter') {
        const nextX = Math.max(0, Math.round((320 - w) / 2));
        updateNavBarConfig({ x: nextX });
      } else if (action === 'vcenter') {
        const nextY = Math.max(0, Math.round((170 - h) / 2));
        updateNavBarConfig({ y: nextY });
      } else if (action === 'full') {
        if (cfg.position === 'left' || cfg.position === 'right' || cfg.orientation === 'vertical') {
          updateNavBarConfig({ y: 0, height: 170 });
        } else {
          updateNavBarConfig({ x: 0, width: 320 });
        }
      }
    });
  });

  // Inputs Numéricos de Coordenadas
  const bindNumCoord = (id, prop, min, max) => {
    const el = document.getElementById(id);
    if (!el) return;
    el.addEventListener('change', () => {
      const val = Math.max(min, Math.min(max, Number(el.value) || 0));
      updateNavBarConfig({ [prop]: val });
    });
  };
  bindNumCoord('navBarCoordX', 'x', 0, 319);
  bindNumCoord('navBarCoordY', 'y', 0, 169);
  bindNumCoord('navBarCoordW', 'width', 10, 320);
  bindNumCoord('navBarCoordH', 'height', 10, 170);

  // Distribución de Pestañas
  document.querySelectorAll('#navBarInspectorSection .bar-visual-btn[data-navbar-dist]').forEach((btn) => {
    btn.addEventListener('click', (e) => {
      e.preventDefault();
      updateNavBarConfig({ tabDistribution: btn.dataset.navbarDist });
    });
  });

  // Alineación de Texto
  document.querySelectorAll('#navBarInspectorSection .bar-visual-btn[data-navbar-textalign]').forEach((btn) => {
    btn.addEventListener('click', (e) => {
      e.preventDefault();
      updateNavBarConfig({ textAlign: btn.dataset.navbarTextalign });
    });
  });

  // Tamaño de Fuente
  document.querySelectorAll('#navBarInspectorSection .bar-visual-btn[data-navbar-fontsize]').forEach((btn) => {
    btn.addEventListener('click', (e) => {
      e.preventDefault();
      updateNavBarConfig({ fontSize: btn.dataset.navbarFontsize });
    });
  });

  // Formato de Etiqueta
  document.querySelectorAll('#navBarInspectorSection .bar-visual-btn[data-navbar-labelformat]').forEach((btn) => {
    btn.addEventListener('click', (e) => {
      e.preventDefault();
      updateNavBarConfig({ labelFormat: btn.dataset.navbarLabelformat });
    });
  });

  // Modo de Fondo
  document.querySelectorAll('#navBarInspectorSection .bar-visual-btn[data-navbar-bgmode]').forEach((btn) => {
    btn.addEventListener('click', (e) => {
      e.preventDefault();
      updateNavBarConfig({ bgMode: btn.dataset.navbarBgmode });
    });
  });

  // Modo de Borde
  document.querySelectorAll('#navBarInspectorSection .bar-visual-btn[data-navbar-bordermode]').forEach((btn) => {
    btn.addEventListener('click', (e) => {
      e.preventDefault();
      updateNavBarConfig({ borderMode: btn.dataset.navbarBordermode });
    });
  });

  // Radio de Esquinas
  document.querySelectorAll('#navBarInspectorSection .bar-visual-btn[data-navbar-radius]').forEach((btn) => {
    btn.addEventListener('click', (e) => {
      e.preventDefault();
      updateNavBarConfig({ radius: Number(btn.dataset.navbarRadius) });
    });
  });

  // Separación Gap
  document.querySelectorAll('#navBarInspectorSection .bar-visual-btn[data-navbar-gap]').forEach((btn) => {
    btn.addEventListener('click', (e) => {
      e.preventDefault();
      updateNavBarConfig({ gap: Number(btn.dataset.navbarGap) });
    });
  });

  // Botón Copiar Primitiva C++
  document.getElementById('navBarCopyCodeBtn')?.addEventListener('click', async () => {
    const code = document.getElementById('navBarCppCallCode')?.textContent?.trim() || 'jwplcDrawPageNavBar(JWPLC_Display.userPage());';
    const copyBtn = document.getElementById('navBarCopyCodeBtn');
    try {
      await navigator.clipboard.writeText(code);
      if (copyBtn) {
        copyBtn.textContent = '✓';
        copyBtn.style.color = '#34c759';
        setTimeout(() => {
          copyBtn.textContent = '⧉';
          copyBtn.style.color = '';
        }, 1200);
      }
    } catch (err) {
      console.warn('Clipboard write error:', err);
    }
  });

  // Botón Eliminar Barra de Navegación
  document.getElementById('navBarInspectorDeleteBtn')?.addEventListener('click', () => {
    const navBarField = editor()?.getAllFields?.()?.find((f) => f.type === 'NAV_BAR');
    if (navBarField && editor()?.setSelectedFieldKeys) {
      editor().setSelectedFieldKeys([navBarField.key]);
      editor()?.deleteSelectedField?.();
    }
    updateNavBarConfig({ enabled: false });
    const section = document.getElementById('navBarInspectorSection');
    if (section) {
      section.hidden = true;
      section.style.display = 'none';
    }
    patchUI();
  });

  // --- Sincronización del Inspector para Barra de Navegación ---
  function syncNavBarInspector() {
    const section = document.getElementById('navBarInspectorSection');
    if (!section || section.hidden) return;

    const cfg = getNavBarConfig();

    // 1. Pestaña activa
    switchNavBarMainTab(activeNavBarTab);

    // 2. Marcar preset activo
    const curPreset = cfg.preset || 'industrial-color';
    section.querySelectorAll('.bar-preset-btn[data-navbar-preset]').forEach((btn) => {
      btn.classList.toggle('active', btn.dataset.navbarPreset === curPreset);
    });

    // 3. Marcar posición activa
    const curPos = cfg.position || 'bottom';
    section.querySelectorAll('.bar-visual-btn[data-navbar-pos]').forEach((btn) => {
      btn.classList.toggle('active', btn.dataset.navbarPos === curPos);
    });

    // 4. Marcar distribución activa
    section.querySelectorAll('.bar-visual-btn[data-navbar-dist]').forEach((btn) => {
      btn.classList.toggle('active', btn.dataset.navbarDist === (cfg.tabDistribution || 'center'));
    });

    // 5. Marcar alineación texto
    section.querySelectorAll('.bar-visual-btn[data-navbar-textalign]').forEach((btn) => {
      btn.classList.toggle('active', btn.dataset.navbarTextalign === (cfg.textAlign || 'center'));
    });

    // 6. Marcar tamaño fuente
    section.querySelectorAll('.bar-visual-btn[data-navbar-fontsize]').forEach((btn) => {
      btn.classList.toggle('active', btn.dataset.navbarFontsize === (cfg.fontSize || 'normal'));
    });

    // 7. Marcar formato etiqueta
    section.querySelectorAll('.bar-visual-btn[data-navbar-labelformat]').forEach((btn) => {
      btn.classList.toggle('active', btn.dataset.navbarLabelformat === (cfg.labelFormat || 'short'));
    });

    // 8. Marcar fondo
    section.querySelectorAll('.bar-visual-btn[data-navbar-bgmode]').forEach((btn) => {
      btn.classList.toggle('active', btn.dataset.navbarBgmode === (cfg.bgMode || 'solid'));
    });

    // 9. Marcar borde
    section.querySelectorAll('.bar-visual-btn[data-navbar-bordermode]').forEach((btn) => {
      btn.classList.toggle('active', btn.dataset.navbarBordermode === (cfg.borderMode || 'none'));
    });

    // 10. Marcar radio
    section.querySelectorAll('.bar-visual-btn[data-navbar-radius]').forEach((btn) => {
      btn.classList.toggle('active', Number(btn.dataset.navbarRadius) === Number(cfg.radius !== undefined ? cfg.radius : 3));
    });

    // 11. Marcar gap
    section.querySelectorAll('.bar-visual-btn[data-navbar-gap]').forEach((btn) => {
      btn.classList.toggle('active', Number(btn.dataset.navbarGap) === Number(cfg.gap !== undefined ? cfg.gap : 3));
    });

    // 12. Inputs numéricos (no sobreescribir si están en foco)
    const activeEl = document.activeElement;
    const inX = document.getElementById('navBarCoordX');
    const inY = document.getElementById('navBarCoordY');
    const inW = document.getElementById('navBarCoordW');
    const inH = document.getElementById('navBarCoordH');
    if (inX && inX !== activeEl) inX.value = String(cfg.x);
    if (inY && inY !== activeEl) inY.value = String(cfg.y);
    if (inW && inW !== activeEl) inW.value = String(cfg.width);
    if (inH && inH !== activeEl) inH.value = String(cfg.height);

    // 13. Lectura de Geometría
    const geoXY = document.getElementById('navBarGeoXY');
    const geoWH = document.getElementById('navBarGeoWH');
    if (geoXY) geoXY.textContent = `${cfg.x} / ${cfg.y}`;
    if (geoWH) geoWH.textContent = `${cfg.width} × ${cfg.height}`;

    // 6. Lista de páginas con abreviatura y color
    const pagesList = document.getElementById('navBarInspectorPagesList');
    if (pagesList) {
      const activeEl = document.activeElement;
      const isTyping = activeEl && pagesList.contains(activeEl) && activeEl.classList.contains('hmi-nav-short-input');
      const curPages = pages();
      const existingRows = pagesList.querySelectorAll('.hmi-nav-page-row');

      if (existingRows.length !== curPages.length || !isTyping) {
        pagesList.innerHTML = '';
        curPages.forEach((page, idx) => {
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
  }

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
    syncNavBarConfigFromField,
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
