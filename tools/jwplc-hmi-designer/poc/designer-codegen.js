(() => {
  'use strict';

  if (window.JWPLCHMICodegen) return;

  const codeOutput = document.getElementById('codeOutput');
  const contractTab = document.getElementById('contractTab');
  const statusTab = document.getElementById('statusTab');
  const generateButton = document.getElementById('generateButton');
  const fieldIdInput = document.getElementById('fieldId');
  const fieldVariableInput = document.getElementById('fieldVariable');

  if (!codeOutput || !fieldIdInput || !fieldVariableInput) return;

  let warningFieldKey = null;

  function editor() {
    return window.JWPLCHMIEditor || null;
  }

  function allFields() {
    return (editor()?.getAllFields?.() || []).filter((field) => field.type !== 'NAV_BAR');
  }

  function pages() {
    return editor()?.getPages?.() || [{ id: 0, name: 'Principal' }];
  }

  function selectedField() {
    return editor()?.getSelectedField?.() || null;
  }

  function sanitizeSymbol(value, fallback) {
    const cleaned = String(value || '').trim().replace(/[^A-Za-z0-9_]/g, '_');
    if (!cleaned) return fallback;
    return /^[A-Za-z_]/.test(cleaned) ? cleaned : `_${cleaned}`;
  }

  function sanitizePageToken(value, fallback) {
    const normalized = String(value || '')
      .normalize('NFD')
      .replace(/[\u0300-\u036f]/g, '')
      .trim()
      .replace(/[^A-Za-z0-9_]+/g, '_')
      .replace(/^_+|_+$/g, '')
      .toUpperCase();
    const token = normalized || fallback;
    return /^[A-Z_]/.test(token) ? token : `_${token}`;
  }

  function pageSymbols() {
    const used = new Set();
    return pages().map((page, index) => {
      const fallback = `PAGINA_${Number(page?.id ?? index) + 1}`;
      const token = sanitizePageToken(page?.name, fallback);
      let symbol = `PAGE_${token}`;
      if (used.has(symbol)) {
        symbol = `${symbol}_${Number(page?.id ?? index) + 1}`;
      }
      used.add(symbol);
      return { page, symbol };
    });
  }

  function serialFor(field, fallbackIndex = 0) {
    const match = String(field?.key || '').match(/-(\d+)$/);
    return match ? Number(match[1]) : fallbackIndex + 1;
  }

  function fieldFallbackId(field, index) {
    const serial = serialFor(field, index);
    if (field?.type === 'VALUE') return `FIELD_VALUE_${serial}`;
    if (field?.type === 'BOOL') return `FIELD_BOOL_${serial}`;
    if (field?.type === 'BAR') return `FIELD_BAR_${serial}`;
    return `FIELD_TEXT_${serial}`;
  }

  function variableFallback(field, index) {
    const serial = serialFor(field, index);
    if (field?.type === 'VALUE') return `valor${serial}`;
    if (field?.type === 'BOOL') return `estado${serial}`;
    if (field?.type === 'BAR') return `nivel${serial}`;
    return `texto${serial}`;
  }

  function canonicalFor(field, property, value, index) {
    const fallback = property === 'id'
      ? fieldFallbackId(field, index)
      : variableFallback(field, index);
    return sanitizeSymbol(value, fallback);
  }

  function pageForField(field) {
    const pageId = Number(field?.page || 0);
    return pages().find((page) => Number(page.id) === pageId) || {
      id: pageId,
      name: `Página ${pageId + 1}`
    };
  }

  function pageComment(page) {
    const name = String(page?.name || `Página ${Number(page?.id || 0) + 1}`)
      .replace(/[\r\n]+/g, ' ')
      .trim();
    return `Página ${String(Number(page?.id || 0) + 1).padStart(2, '0')} · ${name}`;
  }

  function duplicateFor(field, property, candidate) {
    const fields = allFields();
    const currentIndex = Math.max(0, fields.indexOf(field));
    const canonical = canonicalFor(field, property, candidate, currentIndex);

    for (let index = 0; index < fields.length; index += 1) {
      const other = fields[index];
      if (other === field) continue;
      const otherCanonical = canonicalFor(other, property, other[property], index);
      if (otherCanonical === canonical) {
        return { field: other, canonical };
      }
    }

    return null;
  }

  function ensureWarningNode(input, kind) {
    const id = `a11-duplicate-${kind}`;
    let node = document.getElementById(id);
    if (node) return node;
    node = document.createElement('span');
    node.id = id;
    node.className = 'a11-duplicate-warning';
    node.hidden = true;
    input.insertAdjacentElement('afterend', node);
    return node;
  }

  function injectStyles() {
    if (document.getElementById('a11-codegen-guard-style')) return;
    const style = document.createElement('style');
    style.id = 'a11-codegen-guard-style';
    style.textContent = `
      .a11-duplicate-warning {
        display:block;
        margin-top:5px;
        color:#ff8d88;
        font-size:10px;
        line-height:1.35;
      }
      .a11-duplicate-warning[hidden] { display:none; }
      .field-input.a11-duplicate-input {
        border-color:#e45f5a !important;
        box-shadow:0 0 0 1px rgba(228,95,90,.25) inset;
      }
    `;
    document.head.appendChild(style);
  }

  function clearWarning(input, kind) {
    input.classList.remove('a11-duplicate-input');
    const node = ensureWarningNode(input, kind);
    node.hidden = true;
    node.textContent = '';
  }

  function showDuplicateWarning(input, kind, duplicate) {
    const page = pageForField(duplicate.field);
    const objectName = duplicate.field.name || duplicate.field.type || 'objeto';
    const human = kind === 'id' ? 'ID C++' : 'Variable C++';
    const node = ensureWarningNode(input, kind);
    input.classList.add('a11-duplicate-input');
    node.textContent = `No se aplicó: ${human} “${duplicate.canonical}” ya existe en ${pageComment(page)} (${objectName}).`;
    node.hidden = false;
    warningFieldKey = selectedField()?.key || null;
  }

  function guardUniqueInput(input, property, kind) {
    if (input.dataset.a11UniqueGuard === '1') return;
    input.dataset.a11UniqueGuard = '1';

    ['input', 'change'].forEach((eventName) => {
      input.addEventListener(eventName, (event) => {
        const field = selectedField();
        if (!field) return;
        const duplicate = duplicateFor(field, property, input.value);
        if (!duplicate) {
          clearWarning(input, kind);
          return;
        }

        event.preventDefault();
        event.stopImmediatePropagation();
        input.value = String(field[property] || '');
        showDuplicateWarning(input, kind, duplicate);
        editor()?.render?.();
      }, true);
    });
  }

  function validationIssues() {
    const fields = allFields();
    const issues = [];

    ['id', 'variable'].forEach((property) => {
      const seen = new Map();
      fields.forEach((field, index) => {
        const canonical = canonicalFor(field, property, field[property], index);
        const previous = seen.get(canonical);
        if (previous) {
          issues.push({ property, canonical, first: previous, second: field });
        } else {
          seen.set(canonical, field);
        }
      });
    });

    return issues;
  }

  function variableDeclaration(field, index) {
    if (['LINE', 'RECT', 'ELLIPSE', 'TRIANGLE', 'POLYGON', 'RAW_TEXT'].includes(field.type)) return '';
    const variable = canonicalFor(field, 'variable', field.variable, index);
    if (field.type === 'VALUE' || field.type === 'BAR') return `float ${variable} = 0.0f;`;
    if (field.type === 'BOOL') return `bool ${variable} = false;`;
    const capacity = Math.max(1, Number(field.capacity) || 1);
    return `char ${variable}[${capacity + 1}] = {};`;
  }

  function setterHint(field, index) {
    if (['LINE', 'RECT', 'ELLIPSE', 'TRIANGLE', 'POLYGON', 'RAW_TEXT'].includes(field.type)) {
      const c = `0x${(field.frameColor || 0xFFFF).toString(16).toUpperCase().padStart(4, '0')}`;
      const fc = `0x${(field.fillColor || field.frameColor || 0xFFFF).toString(16).toUpperCase().padStart(4, '0')}`;
      const r = Math.max(0, Math.round(Number(field.cornerRadius) || 0));
      const rot = ((Number(field.rotation) || 0) % 360 + 360) % 360;
      const borderOn = field.borderEnabled !== false;

      if (field.type === 'LINE') return `JWPLC_Display.getTFT()->drawLine(${field.x}, ${field.y}, ${field.x2}, ${field.y2}, ${c});`;
      if (field.type === 'RECT' && rot === 0) {
        const minX = Math.min(field.x, field.x2);
        const minY = Math.min(field.y, field.y2);
        const w = Math.abs(field.x2 - field.x);
        const h = Math.abs(field.y2 - field.y);
        const cmds = [];
        if (r > 0) {
          if (field.fill) cmds.push(`JWPLC_Display.getTFT()->fillRoundRect(${minX}, ${minY}, ${w}, ${h}, ${r}, ${fc});`);
          if (borderOn) cmds.push(`JWPLC_Display.getTFT()->drawRoundRect(${minX}, ${minY}, ${w}, ${h}, ${r}, ${c});`);
        } else {
          if (field.fill) cmds.push(`JWPLC_Display.getTFT()->fillRect(${minX}, ${minY}, ${w}, ${h}, ${fc});`);
          if (borderOn) cmds.push(`JWPLC_Display.getTFT()->drawRect(${minX}, ${minY}, ${w}, ${h}, ${c});`);
        }
        return cmds.join('\n        ');
      }
      if (field.type === 'ELLIPSE') {
        const cx = Math.round(Math.min(field.x, field.x2) + Math.abs(field.x2 - field.x) / 2);
        const cy = Math.round(Math.min(field.y, field.y2) + Math.abs(field.y2 - field.y) / 2);
        const rx = Math.round(Math.abs(field.x2 - field.x) / 2);
        const ry = Math.round(Math.abs(field.y2 - field.y) / 2);
        const cmds = [];
        if (rx === ry) {
          if (field.fill) cmds.push(`JWPLC_Display.getTFT()->fillCircle(${cx}, ${cy}, ${rx}, ${fc});`);
          if (borderOn) cmds.push(`JWPLC_Display.getTFT()->drawCircle(${cx}, ${cy}, ${rx}, ${c});`);
        } else {
          if (field.fill) cmds.push(`JWPLC_Display.getTFT()->fillEllipse(${cx}, ${cy}, ${rx}, ${ry}, ${fc});`);
          if (borderOn) cmds.push(`JWPLC_Display.getTFT()->drawEllipse(${cx}, ${cy}, ${rx}, ${ry}, ${c});`);
        }
        return cmds.join('\n        ');
      }
      if (field.type === 'TRIANGLE' && r === 0 && rot === 0) {
        const x1 = Math.round((field.x + field.x2) / 2), y1 = field.y;
        const x2 = field.x, y2 = field.y2;
        const x3 = field.x2, y3 = field.y2;
        const cmds = [];
        if (field.fill) cmds.push(`JWPLC_Display.getTFT()->fillTriangle(${x1}, ${y1}, ${x2}, ${y2}, ${x3}, ${y3}, ${fc});`);
        if (borderOn) cmds.push(`JWPLC_Display.getTFT()->drawTriangle(${x1}, ${y1}, ${x2}, ${y2}, ${x3}, ${y3}, ${c});`);
        return cmds.join('\n        ');
      }
      if (['RECT', 'TRIANGLE', 'POLYGON'].includes(field.type)) {
        const apiVerts = window.JWPLCHMIEditor?.getShapeVertices?.(field);
        let pts = Array.isArray(apiVerts) && apiVerts.length >= 3 ? apiVerts : null;
        if (!pts) {
          const cx = (field.x + field.x2) / 2;
          const cy = (field.y + field.y2) / 2;
          const rx = Math.abs(field.x2 - field.x) / 2;
          const ry = Math.abs(field.y2 - field.y) / 2;
          const numSides = field.type === 'TRIANGLE' ? 3 : field.type === 'RECT' ? 4 : Math.max(3, Math.min(20, Math.trunc(Number(field.sides) || 5)));
          pts = [];
          for (let i = 0; i < numSides; i++) {
            const angle = (i * 2 * Math.PI / numSides) - Math.PI / 2;
            pts.push({
              x: Math.round(cx + rx * Math.cos(angle)),
              y: Math.round(cy + ry * Math.sin(angle))
            });
          }
        }
        const cmds = [];
        if (field.fill && pts.length >= 3) {
          for (let i = 1; i < pts.length - 1; i++) {
            cmds.push(`JWPLC_Display.getTFT()->fillTriangle(${pts[0].x}, ${pts[0].y}, ${pts[i].x}, ${pts[i].y}, ${pts[i + 1].x}, ${pts[i + 1].y}, ${fc});`);
          }
        }
        if (borderOn) {
          for (let i = 0; i < pts.length; i++) {
            const p1 = pts[i];
            const p2 = pts[(i + 1) % pts.length];
            cmds.push(`JWPLC_Display.getTFT()->drawLine(${p1.x}, ${p1.y}, ${p2.x}, ${p2.y}, ${c});`);
          }
        }
        return cmds.join('\n        ');
      }
      if (field.type === 'RAW_TEXT') {
        const RAW_BG_PAD = 2;
        const fg = `0x${(field.textColor ?? 0xFFFF).toString(16).toUpperCase().padStart(4, '0')}`;
        const text = String(field.text || '').replace(/\\/g, '\\\\').replace(/"/g, '\\"');
        const fonts = window.JWPLCDesignerFonts;
        const font = fonts ? fonts.fieldFont(field) : null;
        if (font && font.kind !== 'glcd') {
          const tft = 'JWPLC_Display.getTFT()->';
          const pad = '\n        ';
          const lines = [];
          if (!field.transparentBackground) {
            const bg = `0x${(field.backgroundColor ?? 0x0000).toString(16).toUpperCase().padStart(4, '0')}`;
            const w = fonts.measureWidth(font, String(field.text || ''), field.size || 1);
            const h = fonts.lineHeight(font, field.size || 1);
            // Padding de RAW_BG_PAD px alrededor del texto (igual que el preview)
            lines.push(`${tft}fillRect(${field.x - RAW_BG_PAD}, ${field.y - RAW_BG_PAD}, ${w + RAW_BG_PAD * 2}, ${h + RAW_BG_PAD * 2}, ${bg});`);
          }
          lines.push(
            `${tft}setFont(${fonts.cppSymbol(font.id)});`,
            `${tft}setTextDatum(JWPLC_TFTDatum::TOP_LEFT);`,
            `${tft}setTextSize(${field.size || 1});`,
            `${tft}setTextColor(${fg});`,
            `${tft}drawString("${text}", ${field.x}, ${field.y});`,
            `${tft}setFont(JWPLC_TFTFont::GLCD);`
          );
          return lines.join(pad);
        }
        // GLCD
        const tft = 'JWPLC_Display.getTFT()->';
        const pad = '\n        ';
        if (field.transparentBackground) {
          return [
            `${tft}setCursor(${field.x}, ${field.y});`,
            `${tft}setTextSize(${field.size || 1});`,
            `${tft}setTextColor(${fg});`,
            `${tft}print("${text}");`
          ].join(pad);
        } else {
          const bg = `0x${(field.backgroundColor ?? 0x0000).toString(16).toUpperCase().padStart(4, '0')}`;
          const fonts2 = window.JWPLCDesignerFonts;
          const glcd = fonts2 ? fonts2.get('GLCD') : null;
          const scale = field.size || 1;
          // Bounds GLCD: 6*scale*chars - scale de ancho, 7*scale de alto
          const chars = String(field.text || '').length;
          const w = chars > 0 ? chars * 6 * scale - scale : 0;
          const h = 7 * scale;
          return [
            // fillRect con padding antes del print (igual que el preview)
            `${tft}fillRect(${field.x - RAW_BG_PAD}, ${field.y - RAW_BG_PAD}, ${w + RAW_BG_PAD * 2}, ${h + RAW_BG_PAD * 2}, ${bg});`,
            `${tft}setCursor(${field.x}, ${field.y});`,
            `${tft}setTextSize(${scale});`,
            `${tft}setTextColor(${fg});`,
            `${tft}print("${text}");`
          ].join(pad);
        }
      }

    }
    const id = canonicalFor(field, 'id', field.id, index);
    const variable = canonicalFor(field, 'variable', field.variable, index);
    if (field.type === 'VALUE') return `JWPLC_Display.setValue(${id}, ${variable});`;
    if (field.type === 'BOOL') return `JWPLC_Display.setBool(${id}, ${variable});`;
    if (field.type === 'BAR') return `JWPLC_Display.setBar(${id}, ${variable});`;
    return `JWPLC_Display.setText(${id}, ${variable});`;
  }

  function groupedChunks(renderer, commentIndent = '') {
    const fields = allFields();
    const chunks = [];

    pages().forEach((page) => {
      const entries = fields
        .map((field, index) => ({ field, index }))
        .filter(({ field }) => Number(field.page || 0) === Number(page.id));
      if (!entries.length) return;
      const lines = entries.map(({ field, index }) => renderer(field, index)).filter(Boolean);
      if (!lines.length) return;
      chunks.push(`${commentIndent}// ${pageComment(page)}\n${lines.join('\n')}`);
    });

    return chunks.join('\n\n');
  }

  function navBarFunctionBlock() {
    const hmiPages = window.JWPLCHMIPages;
    if (!hmiPages) return '';
    const cfg = hmiPages.getNavBarConfig ? hmiPages.getNavBarConfig() : null;
    const pageList = pages();
    if (!cfg || !cfg.enabled || pageList.length <= 1) return '';

    const lines = [];
    lines.push(`// =====================================================`);
    lines.push(`// Barra de Navegación HMI Autogenerada (ST7789)`);
    lines.push(`// =====================================================`);
    lines.push(`static void jwplcDrawPageNavBar(uint8_t activePage)`);
    lines.push(`{`);
    lines.push(`    auto &tft = JWPLC_Display.display();`);

    const layout = hmiPages.computeNavBarLayout ? hmiPages.computeNavBarLayout(pageList, 0) : null;
    const x = layout ? layout.x : 0;
    const y = layout ? layout.y : 147;
    const w = layout ? layout.w : 320;
    const h = layout ? layout.h : 22;
    const isVert = Boolean(layout?.isVertical);
    const preset = cfg.preset || 'industrial-color';
    const radius = cfg.radius !== undefined ? Number(cfg.radius) : 3;

    lines.push(`    tft.fillRect(${x}, ${y}, ${w}, ${h}, 0x0000);`);
    lines.push(`    tft.setTextSize(1);`);
    lines.push(``);
    lines.push(`    switch (activePage)`);
    lines.push(`    {`);

    pageList.forEach((page, idx) => {
      const curLayout = hmiPages.computeNavBarLayout ? hmiPages.computeNavBarLayout(pageList, page.id) : null;
      lines.push(`    case ${idx}:`);
      lines.push(`        {`);
      if (curLayout && curLayout.tabs) {
        if (preset === 'compact-dots') {
          curLayout.tabs.forEach((tab) => {
            if (tab.isActive) {
              const r = Math.floor(Math.min(tab.w, tab.h) / 2);
              lines.push(`            tft.fillRoundRect(${tab.x}, ${tab.y}, ${tab.w}, ${tab.h}, ${r}, 0x07FF);`);
            } else {
              const cx = tab.x + Math.floor(tab.w / 2);
              const cy = tab.y + Math.floor(tab.h / 2);
              lines.push(`            tft.fillCircle(${cx}, ${cy}, 2, 0x4A69);`);
            }
          });
        } else {
          curLayout.tabs.forEach((tab) => {
            const col565 = `0x${Number(tab.color || 0xFFFF).toString(16).toUpperCase().padStart(4, '0')}`;
            const rad = Math.min(radius, Math.floor(Math.min(tab.w, tab.h) / 2));
            if (tab.isActive) {
              lines.push(`            tft.fillRoundRect(${tab.x}, ${tab.y}, ${tab.w}, ${tab.h}, ${rad}, 0xFFFF);`);
              lines.push(`            tft.drawRoundRect(${tab.x}, ${tab.y}, ${tab.w}, ${tab.h}, ${rad}, ${col565});`);
              lines.push(`            tft.setTextColor(${col565 === '0xFFFF' ? '0x0000' : col565}, 0xFFFF);`);
            } else {
              lines.push(`            tft.fillRoundRect(${tab.x}, ${tab.y}, ${tab.w}, ${tab.h}, ${rad}, 0x0000);`);
              lines.push(`            tft.drawRoundRect(${tab.x}, ${tab.y}, ${tab.w}, ${tab.h}, ${rad}, ${col565});`);
              lines.push(`            tft.setTextColor(${col565}, 0x0000);`);
            }
            const textW = tab.shortName.length * 6 - 1;
            const tx = tab.x + Math.max(1, Math.floor((tab.w - textW) / 2));
            const ty = tab.y + Math.max(1, Math.floor((tab.h - 7) / 2));
            lines.push(`            tft.setCursor(${tx}, ${ty});`);
            lines.push(`            tft.print("${tab.shortName}");`);
          });
        }

        if (curLayout.hasOverflow) {
          const arrowPrevChar = isVert ? "^" : "<";
          const arrowNextChar = isVert ? "v" : ">";
          if (curLayout.leftArrow) {
            const la = curLayout.leftArrow;
            const laCol = la.enabled ? '0x07FF' : '0x39E7';
            lines.push(`            tft.drawRoundRect(${la.x}, ${la.y}, ${la.w}, ${la.h}, 2, ${laCol});`);
            lines.push(`            tft.setTextColor(${laCol}, 0x0000);`);
            lines.push(`            tft.setCursor(${la.x + Math.floor((la.w - 5) / 2)}, ${la.y + Math.floor((la.h - 7) / 2)});`);
            lines.push(`            tft.print("${arrowPrevChar}");`);
          }
          if (curLayout.rightArrow) {
            const ra = curLayout.rightArrow;
            const raCol = ra.enabled ? '0x07FF' : '0x39E7';
            lines.push(`            tft.drawRoundRect(${ra.x}, ${ra.y}, ${ra.w}, ${ra.h}, 2, ${raCol});`);
            lines.push(`            tft.setTextColor(${raCol}, 0x0000);`);
            lines.push(`            tft.setCursor(${ra.x + Math.floor((ra.w - 5) / 2)}, ${ra.y + Math.floor((ra.h - 7) / 2)});`);
            lines.push(`            tft.print("${arrowNextChar}");`);
          }
          if (curLayout.counter) {
            const cnt = curLayout.counter;
            lines.push(`            tft.drawRoundRect(${cnt.x}, ${cnt.y}, ${cnt.w}, ${cnt.h}, 2, 0x4A69);`);
            lines.push(`            tft.setTextColor(0xFFFF, 0x0000);`);
            lines.push(`            tft.setCursor(${cnt.x + 2}, ${cnt.y + Math.floor((cnt.h - 7) / 2)});`);
            lines.push(`            tft.print("${cnt.text}");`);
          }
        }
      }
      lines.push(`        }`);
      lines.push(`        break;`);
    });

    lines.push(`    default:`);
    lines.push(`        break;`);
    lines.push(`    }`);
    lines.push(`}`);
    return lines.join('\n');
  }

  function generatedUIUpdateBlock() {
    const fields = allFields();
    const symbols = new Map(
      pageSymbols().map(({ page, symbol }) => [Number(page.id), symbol]));
    const cases = [];

    pages().forEach((page) => {
      const entries = fields
        .map((field, index) => ({ field, index }))
        .filter(({ field }) => Number(field.page || 0) === Number(page.id));
      if (!entries.length) return;

      const symbol = symbols.get(Number(page.id)) || String(Number(page.id));
      const lines = entries.map(({ field, index }) => `        ${setterHint(field, index)}`);
      cases.push(
        `    case ${symbol}:\n` +
        `        // ${pageComment(page)}\n` +
        `${lines.join('\n')}\n` +
        `        break;`);
    });

    const hasNavBar = Boolean(window.JWPLCHMIPages?.getNavBarConfig?.()?.enabled && pages().length > 1);
    const navBarCall = hasNavBar
      ? `    static uint8_t s_lastNavBarPage = 255;\n    const uint8_t curPage = JWPLC_Display.userPage();\n    if (curPage != s_lastNavBarPage)\n    {\n        s_lastNavBarPage = curPage;\n        jwplcDrawPageNavBar(curPage);\n    }\n\n`
      : '';
    const switchTarget = hasNavBar ? 'curPage' : 'JWPLC_Display.userPage()';

    return `// Sincronización gráfica autogenerada.\n` +
      `// La lógica de aplicación debe permanecer en loop().\n` +
      `void jwplcUIUpdate()\n` +
      `{\n` +
      `${navBarCall}` +
      `    switch (${switchTarget})\n` +
      `    {\n` +
      `${cases.join('\n\n')}\n\n` +
      `    default:\n` +
      `        break;\n` +
      `    }\n` +
      `}`;
  }

  function pageEnumBlock() {
    const lines = pageSymbols().map(({ page, symbol }) =>
      `    ${symbol} = ${Number(page?.id || 0)}`);
    return `enum HMIPageId : uint8_t\n{\n${lines.join(',\n')}\n};`;
  }

  function decorateHeader(text) {
    text = text
      .replace('// API pública JWPLC_UI · Alpha11 A11-3E', '// API pública JWPLC_UI · Alpha11 A11-4')
      .replace('// API pública JWPLC_UI · Alpha11 A11-3D', '// API pública JWPLC_UI · Alpha11 A11-4');

    if (text.includes('#pragma once')) return text;

    const marker = '// API pública JWPLC_UI · Alpha11 A11-4';
    const replacement = `${marker}\n// Destino recomendado: JWPLC_HMI_Generated.h\n// Archivo autogenerado: regenerar desde el Designer en lugar de editar a mano.\n\n#pragma once\n\n#include <JWPLC_Display.h>`;
    return text.replace(marker, replacement);
  }

  function groupGeneratedSections(text) {
    const enumLines = groupedChunks((field, index) => {
      if (['LINE', 'RECT', 'ELLIPSE', 'TRIANGLE', 'POLYGON'].includes(field.type)) return '';
      const id = canonicalFor(field, 'id', field.id, index);
      return `    ${id} = ${index + 1},`;
    }, '    ');

    const variables = groupedChunks((field, index) => variableDeclaration(field, index));
    const navBarBlock = navBarFunctionBlock();
    const uiUpdate = generatedUIUpdateBlock();
    const fullUiBlock = navBarBlock ? `${navBarBlock}\n\n${uiUpdate}` : uiUpdate;
    const pageEnum = pageEnumBlock();

    if (/enum HMIPageId : uint8_t\n\{[\s\S]*?\n\};/.test(text)) {
      text = text.replace(
        /enum HMIPageId : uint8_t\n\{[\s\S]*?\n\};/,
        pageEnum);
    } else {
      text = text.replace(
        /enum HMIFieldId : uint8_t/,
        `${pageEnum}\n\nenum HMIFieldId : uint8_t`);
    }

    text = text.replace(
      /enum HMIFieldId : uint8_t\n\{\n[\s\S]*?\n\};/,
      `enum HMIFieldId : uint8_t\n{\n${enumLines}\n};`);

    text = text.replace(
      /\/\/ Variables HMI\n[\s\S]*?\n\n\/\/ Definición declarativa/,
      `// Variables HMI\n${variables}\n\n// Definición declarativa`);

    const generatedTailMarker = text.indexOf('// Sincronización gráfica autogenerada.');
    const navBarTailMarker = text.indexOf('// Barra de Navegación HMI Autogenerada');
    const legacyTailMarker = text.indexOf('// jwplcUIUpdate() NO se genera.');
    const settersTailMarker = text.indexOf('// Setters públicos que corresponden a este diseño:');
    const tailCandidates = [generatedTailMarker, navBarTailMarker, legacyTailMarker, settersTailMarker]
      .filter((value) => value >= 0);

    if (tailCandidates.length) {
      const tailStart = Math.min(...tailCandidates);
      text = `${text.slice(0, tailStart).trimEnd()}\n\n${fullUiBlock}`;
    } else {
      text = `${text.trimEnd()}\n\n${fullUiBlock}`;
    }

    if (!text.includes('JWPLC_Display.setUserRefreshMode(')) {
      text = text.replace(
        /(JWPLC_Display\.setFields\([\s\S]*?\);\n)/,
        `$1    JWPLC_Display.setUserRefreshMode(USER_REFRESH_PERIODIC);\n`);
    }

    const firstPage = pageSymbols()[0];
    if (firstPage) {
      text = text.replace(
        /JWPLC_Display\.setUserPage\([^)]*\);/,
        `JWPLC_Display.setUserPage(${firstPage.symbol});`);
    }

    return text;
  }

  function duplicateDiagnostic(text, issues) {
    if (!issues.length) return text;
    const lines = issues.map((issue) => {
      const firstPage = pageForField(issue.first);
      const secondPage = pageForField(issue.second);
      const type = issue.property === 'id' ? 'ID C++' : 'Variable C++';
      return `// - ${type} ${issue.canonical}: ${pageComment(firstPage)} (${issue.first.name || issue.first.type}) <-> ${pageComment(secondPage)} (${issue.second.name || issue.second.type})`;
    });
    return `// ERROR CODEGEN: existen identificadores C++ duplicados. Corrígelos en el Inspector.\n${lines.join('\n')}\n\n${text}`;
  }

  function patchGeneratedCode() {
    if (!codeOutput?.textContent.startsWith('// Código generado por JWPLC HMI Designer')) return;
    let text = decorateHeader(codeOutput.textContent);
    text = groupGeneratedSections(text);
    text = duplicateDiagnostic(text, validationIssues());
    codeOutput.textContent = text;
  }

  function patchUI() {
    injectStyles();
    const selectedKey = selectedField()?.key || null;
    if (warningFieldKey && warningFieldKey !== selectedKey) {
      clearWarning(fieldIdInput, 'id');
      clearWarning(fieldVariableInput, 'variable');
      warningFieldKey = null;
    }
    patchGeneratedCode();
  }

  guardUniqueInput(fieldIdInput, 'id', 'id');
  guardUniqueInput(fieldVariableInput, 'variable', 'variable');

  window.addEventListener('jwplc:editor-refresh', patchUI);
  contractTab?.addEventListener('click', () => setTimeout(patchGeneratedCode, 0));
  statusTab?.addEventListener('click', () => setTimeout(patchUI, 0));
  generateButton?.addEventListener('click', () => setTimeout(patchGeneratedCode, 0));

  patchUI();

  window.JWPLCHMICodegen = {
    validateIdentifiers: validationIssues,
    getPageSymbols: () => pageSymbols().map(({ page, symbol }) => ({ id: Number(page.id), name: page.name, symbol })),
    refresh: patchGeneratedCode
  };
})();
