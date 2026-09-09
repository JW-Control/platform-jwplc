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

  const gate = document.querySelector('.page-tabs .gate');
  const bottomSummary = document.querySelector('.bottom-summary');
  const pageTabs = document.querySelector('.page-tabs');
  const pageNavBlock = document.querySelector('.left-panel .nav-block');
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

  if (!pageTabs || !pageNavBlock || !displayCanvas || !previewCanvas) return;

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

  function injectStyles() {
    if (document.getElementById('a11-pages-style')) return;
    const style = document.createElement('style');
    style.id = 'a11-pages-style';
    style.textContent = `
      .a11-page-mode { display:grid; grid-template-columns:1fr 1fr; gap:6px; margin-top:8px; }
      .a11-page-mode button { min-height:30px; font-size:11px; }
      .a11-page-mode button.active { border-color:#ff8a2a; color:#ffb36f; }
      .a11-page-hint { margin:7px 2px 0; color:#7697b0; font-size:10px; line-height:1.35; }
    `;
    document.head.appendChild(style);
  }

  function makePageButton(page) {
    const button = document.createElement('button');
    button.className = `page-item${page.id === activePage() ? ' active' : ''}`;
    button.type = 'button';
    button.dataset.pageId = String(page.id);
    button.title = 'Click: abrir página · Doble click: renombrar';
    button.textContent = pageLabel(page);

    button.addEventListener('click', () => {
      navigationMode = 'SELECT';
      editor()?.setActivePage?.(page.id);
      patchUI();
    });

    button.addEventListener('dblclick', () => {
      const next = window.prompt('Nombre de la página', page.name);
      if (next == null) return;
      editor()?.renamePage?.(page.id, next);
      patchUI();
    });

    return button;
  }

  function addPage() {
    const api = editor();
    if (!api) return;
    const nextNumber = pages().length + 1;
    const created = api.addPage?.(`Página ${nextNumber}`);
    if (created) navigationMode = 'SELECT';
    patchUI();
  }

  function rebuildLeftPages() {
    const heading = pageNavBlock.querySelector('.section-heading');
    if (!heading) return;

    pageNavBlock.querySelectorAll('.page-item, .a11-page-mode, .a11-page-hint').forEach((node) => node.remove());

    const addIcon = heading.querySelector('.icon-button');
    if (addIcon) {
      addIcon.disabled = pages().length >= (editor()?.getMaxPages?.() || 16);
      addIcon.title = 'Nueva página';
      if (!addIcon.dataset.a11PagesBound) {
        addIcon.dataset.a11PagesBound = '1';
        addIcon.addEventListener('click', addPage);
      }
    }

    pages().forEach((page) => pageNavBlock.appendChild(makePageButton(page)));

    const addWide = document.createElement('button');
    addWide.className = 'page-item secondary';
    addWide.type = 'button';
    addWide.disabled = pages().length >= (editor()?.getMaxPages?.() || 16);
    addWide.textContent = '＋ Nueva página';
    addWide.addEventListener('click', addPage);
    pageNavBlock.appendChild(addWide);

    const mode = document.createElement('div');
    mode.className = 'a11-page-mode';
    const selectButton = document.createElement('button');
    const contentButton = document.createElement('button');
    selectButton.type = 'button';
    contentButton.type = 'button';
    selectButton.textContent = 'Selector';
    contentButton.textContent = 'Dentro';
    selectButton.classList.toggle('active', navigationMode === 'SELECT');
    contentButton.classList.toggle('active', navigationMode === 'CONTENT');
    selectButton.title = 'Preview: flechas cambian página; OK entra';
    contentButton.title = 'Preview: flechas/OK quedan para la página; ESC sale';
    selectButton.addEventListener('click', () => { navigationMode = 'SELECT'; patchUI(); });
    contentButton.addEventListener('click', () => { navigationMode = 'CONTENT'; patchUI(); });
    mode.append(selectButton, contentButton);
    pageNavBlock.appendChild(mode);

    const hint = document.createElement('p');
    hint.className = 'a11-page-hint';
    hint.textContent = 'Indicador TFT: negro/blanco = selección · blanco/negro = dentro de página.';
    pageNavBlock.appendChild(hint);
  }

  function rebuildTopTabs() {
    const existing = [...pageTabs.querySelectorAll('.tab')];
    existing.forEach((node) => node.remove());
    const gateNode = pageTabs.querySelector('.gate');

    pages().forEach((page) => {
      const tab = document.createElement('button');
      tab.className = `tab${page.id === activePage() ? ' active' : ''}`;
      tab.type = 'button';
      tab.textContent = page.name;
      tab.title = pageLabel(page);
      tab.addEventListener('click', () => {
        navigationMode = 'SELECT';
        editor()?.setActivePage?.(page.id);
      });
      pageTabs.insertBefore(tab, gateNode);
    });

    const add = document.createElement('button');
    add.className = 'tab add-tab';
    add.type = 'button';
    add.textContent = '＋';
    add.disabled = pages().length >= (editor()?.getMaxPages?.() || 16);
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
    drawIndicator(previewCanvas.getContext('2d', { alpha: false }), 1);
    drawIndicator(displayCanvas.getContext('2d', { alpha: false }), zoom);
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
    if (statusPage) statusPage.textContent = `Página: ${activePage()} · ${current?.name || 'Página'}`;
    if (gate) gate.textContent = 'Gate: A11-3E PAGES · indicador NN/TT';
    if (bottomSummary) bottomSummary.textContent = 'A11-3E · Páginas + navegación compacta';
  }

  function patchUI() {
    injectStyles();
    rebuildLeftPages();
    rebuildTopTabs();
    syncInspectorPage();
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
    indicatorRect: () => ({ ...INDICATOR })
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
