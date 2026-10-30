// Selector de color nativo: Chromium emite 'input' en cada movimiento del
// ratón y cada uno re-renderiza el editor completo (codegen, Live, PixelMaps).
// En una PC lenta eso satura el hilo principal y el selector se traba. Se deja
// pasar el primer evento y luego como máximo uno cada 40 ms; el último valor
// siempre llega y 'change' (cerrar la paleta) no se toca. Aplica a todos los
// inspectores (campos, BAR, MENU, figuras, RAW_TEXT).
(() => {
  const INTERVAL_MS = 40;
  const pending = new Map();
  const last = new WeakMap();
  document.addEventListener('input', (event) => {
    const target = event.target;
    if (!(target instanceof HTMLInputElement) || target.type !== 'color' || event.jwplcThrottled) return;
    const now = performance.now();
    const elapsed = now - (last.get(target) || 0);
    if (!pending.has(target) && elapsed >= INTERVAL_MS) {
      last.set(target, now);
      return;
    }
    event.stopImmediatePropagation();
    if (pending.has(target)) return;
    pending.set(target, setTimeout(() => {
      pending.delete(target);
      last.set(target, performance.now());
      const replay = new Event('input', { bubbles: true });
      replay.jwplcThrottled = true;
      target.dispatchEvent(replay);
    }, Math.max(0, INTERVAL_MS - elapsed)));
  }, true);
  document.addEventListener('change', (event) => {
    const timer = pending.get(event.target);
    if (timer) {
      clearTimeout(timer);
      pending.delete(event.target);
    }
  }, true);
})();

(() => {
  'use strict';

  const WIDTH = 320;
  const HEIGHT = 170;
  const FIELD_PADDING = 3;
  const FIELD_GAP = 4;
  const MAX_FIELDS = 32;
  const MAX_PAGES = 16;
  const MAX_HISTORY = 50;

  const COLORS = [
    { name: 'BLACK', value: 0x0000 },
    { name: 'WHITE', value: 0xFFFF },
    { name: 'RED', value: 0xF800 },
    { name: 'GREEN', value: 0x07E0 },
    { name: 'BLUE', value: 0x001F },
    { name: 'CYAN', value: 0x07FF },
    { name: 'YELLOW', value: 0xFFE0 },
    { name: 'ORANGE', value: 0xFD20 }
  ];

  const pixelLayer = new Uint16Array(WIDTH * HEIGHT);
  const framebuffer = new Uint16Array(WIDTH * HEIGHT);

  const displayCanvas = document.getElementById('displayCanvas');
  const canvasViewport = document.getElementById('canvasViewport');
  const canvasStage = document.getElementById('canvasStage');
  const displayCtx = displayCanvas.getContext('2d', { alpha: false });
  const previewCanvas = document.getElementById('previewCanvas');
  const previewCtx = previewCanvas ? previewCanvas.getContext('2d', { alpha: false }) : null;
  const logicalCanvas = document.createElement('canvas');
  logicalCanvas.width = WIDTH;
  logicalCanvas.height = HEIGHT;
  const logicalCtx = logicalCanvas.getContext('2d', { alpha: false });

  const zoomSelect = document.getElementById('zoomSelect');
  const gridToggle = document.getElementById('gridToggle');
  const gridSizeSelect = document.getElementById('gridSizeSelect');
  const gridStyleSelect = document.getElementById('gridStyleSelect');
  const snapToggle = document.getElementById('snapToggle');
  const snapInspectorToggle = document.getElementById('snapInspectorToggle');
  const snapSizeSelect = document.getElementById('snapSizeSelect');
  const geometryToggle = document.getElementById('geometryToggle');

  function getSnapStep() {
    if (!snapToggle || !snapToggle.checked) return 0;
    const snapVal = snapSizeSelect ? snapSizeSelect.value : 'grid';
    if (snapVal === 'grid') {
      return Number(gridSizeSelect?.value) || 8;
    }
    return Number(snapVal) || 8;
  }

  function setSnapEnabled(enabled) {
    if (snapToggle) snapToggle.checked = enabled;
    if (snapInspectorToggle) snapInspectorToggle.checked = enabled;
    const snapPopoverToggle = document.getElementById('snapPopoverToggle');
    if (snapPopoverToggle) snapPopoverToggle.checked = enabled;
    const vertSnap = document.getElementById('vertSnapToggle');
    if (vertSnap) vertSnap.classList.toggle('active', enabled);
    const vertSnapGroup = document.getElementById('vertSnapGroup');
    if (vertSnapGroup) vertSnapGroup.classList.toggle('active', enabled);
    render();
  }

  function setGridEnabled(enabled) {
    if (gridToggle) {
      gridToggle.checked = enabled;
      gridToggle.dispatchEvent(new Event('change', { bubbles: true }));
    }
    const gridPopoverToggle = document.getElementById('gridPopoverToggle');
    if (gridPopoverToggle) gridPopoverToggle.checked = enabled;
    const vertGrid = document.getElementById('vertGridToggle');
    if (vertGrid) vertGrid.classList.toggle('active', enabled);
    const vertGridGroup = document.getElementById('vertGridGroup');
    if (vertGridGroup) vertGridGroup.classList.toggle('active', enabled);
    render();
  }

  function closeAllVertPopovers() {
    document.querySelectorAll('.vert-popover').forEach((p) => {
      p.style.display = 'none';
      p.closest('.vert-btn-group')?.classList.remove('popover-open');
    });
  }

  function toggleVertPopover(popover, force) {
    if (!popover) return;
    const shouldOpen = force !== undefined ? force : (popover.style.display === 'none');
    document.querySelectorAll('.vert-popover').forEach((p) => {
      if (p !== popover) {
        p.style.display = 'none';
        p.closest('.vert-btn-group')?.classList.remove('popover-open');
      }
    });
    popover.style.display = shouldOpen ? 'block' : 'none';
    popover.closest('.vert-btn-group')?.classList.toggle('popover-open', shouldOpen);
  }

  // Clic en los botones de la barra: toggle rápido On/Off
  document.getElementById('vertSnapToggle')?.addEventListener('click', function() {
    setSnapEnabled(!snapToggle.checked);
  });
  document.getElementById('vertGridToggle')?.addEventListener('click', function() {
    setGridEnabled(!gridToggle.checked);
  });

  // Clic en los chevrons: abre el popover de configuración
  document.getElementById('vertGridMenuBtn')?.addEventListener('click', function(e) {
    e.stopPropagation();
    toggleVertPopover(document.getElementById('gridConfigPopover'));
  });
  document.getElementById('vertSnapMenuBtn')?.addEventListener('click', function(e) {
    e.stopPropagation();
    toggleVertPopover(document.getElementById('snapConfigPopover'));
  });

  // Clic derecho en el botón: también abre la configuración
  document.getElementById('vertGridToggle')?.addEventListener('contextmenu', function(e) {
    e.preventDefault();
    toggleVertPopover(document.getElementById('gridConfigPopover'), true);
  });
  document.getElementById('vertSnapToggle')?.addEventListener('contextmenu', function(e) {
    e.preventDefault();
    toggleVertPopover(document.getElementById('snapConfigPopover'), true);
  });

  // Botones de cerrar popovers
  document.getElementById('gridPopoverClose')?.addEventListener('click', function() {
    closeAllVertPopovers();
  });
  document.getElementById('snapPopoverClose')?.addEventListener('click', function() {
    closeAllVertPopovers();
  });

  // Cerrar popovers si se hace clic fuera
  document.addEventListener('click', function(e) {
    if (!e.target.closest('.vert-btn-group')) {
      closeAllVertPopovers();
    }
  });

  // Switches dentro de los popovers
  document.getElementById('gridPopoverToggle')?.addEventListener('change', function() {
    setGridEnabled(this.checked);
  });
  document.getElementById('snapPopoverToggle')?.addEventListener('change', function() {
    setSnapEnabled(this.checked);
  });

  snapSizeSelect?.addEventListener('change', render);
  const clearButton = document.getElementById('clearButton');
  const newProjectButton = document.getElementById('newProjectButton');
  const demoButton = document.getElementById('demoButton');
  const demoValueButton = document.getElementById('demoValueButton');
  const cursorStatus = document.getElementById('cursorStatus');
  const pixelStatus = document.getElementById('pixelStatus');
  const palette = document.getElementById('palette');
  const activeColorSwatch = document.getElementById('activeColorSwatch');
  const activeColorName = document.getElementById('activeColorName');
  const activeColorValue = document.getElementById('activeColorValue');

  const eyedropperSection = document.getElementById('eyedropperSection');
  const eyedropperLiveSwatch = document.getElementById('eyedropperLiveSwatch');
  const eyedropperLiveName = document.getElementById('eyedropperLiveName');
  const eyedropperLive565 = document.getElementById('eyedropperLive565');
  const eyedropperLiveHex = document.getElementById('eyedropperLiveHex');
  const eyedropperLiveRgb = document.getElementById('eyedropperLiveRgb');
  const eyedropperLiveCoords = document.getElementById('eyedropperLiveCoords');
  const eyedropperActiveSwatch = document.getElementById('eyedropperActiveSwatch');
  const eyedropperActiveName = document.getElementById('eyedropperActiveName');
  const eyedropperActive565 = document.getElementById('eyedropperActive565');
  const eyedropperActiveHex = document.getElementById('eyedropperActiveHex');
  const eyedropperUseFillBtn = document.getElementById('eyedropperUseFillBtn');
  const recentColorsGrid = document.getElementById('recentColorsGrid');
  const clearRecentColorsBtn = document.getElementById('clearRecentColorsBtn');

  let recentColors = [0x0000, 0xFFFF, 0xF800, 0x07E0, 0x001F, 0x07FF, 0xFFE0, 0xFD20];
  let clipboard = [];

  const rawSection = document.getElementById('rawTextControlsSection');
  const fieldSection = document.getElementById('textFieldControlsSection');
  const shapeInspectorSection = document.getElementById('shapeInspectorSection');
  const shapeInspectorIcon = document.getElementById('shapeInspectorIcon');
  const shapeInspectorName = document.getElementById('shapeInspectorName');
  let shapeActiveColorKey = 'border';
  let shapeColorEditor = null;

  function initShapeColorEditor() {
    if (shapeColorEditor) return;
    const host = document.getElementById('shapeColorEditorHost');
    if (!host || !window.JWPLCColorEditor) return;
    shapeColorEditor = window.JWPLCColorEditor.create({
      paletteAttr: 'data-shape-palette',
      onInput: (v) => applyShapeColor(shapeActiveColorKey, v, false),
      onCommit: (v) => applyShapeColor(shapeActiveColorKey, v, true)
    });
    host.appendChild(shapeColorEditor.el);
  }

  function applyShapeColor(key, value565, commit) {
    const f = selectedField();
    if (!f) return;
    const col = value565 & 0xFFFF;
    if (key === 'border') {
      f.frameColor = col;
    } else if (key === 'fill') {
      f.fillColor = col;
    }
    syncShapeInspector();
    render();
    if (commit) commitHistory();
  }

  let activeRawMainTab = 'design';
  let activeRawSubTab = 'form';
  let rawActiveColorKey = 'textColor';
  let rawColorEditor = null;

  function initRawColorEditor() {
    if (rawColorEditor) return;
    const host = document.getElementById('rawColorEditorHost');
    if (!host || !window.JWPLCColorEditor) return;
    rawColorEditor = window.JWPLCColorEditor.create({
      paletteAttr: 'data-raw-palette',
      onInput: (v) => applyRawColor(rawActiveColorKey, v, false),
      onCommit: (v) => applyRawColor(rawActiveColorKey, v, true)
    });
    host.appendChild(rawColorEditor.el);
  }

  function applyRawColor(key, value565, commit) {
    const f = selectedField();
    const col = value565 & 0xFFFF;
    if (key === 'textColor') {
      if (f && f.type === 'RAW_TEXT') f.textColor = col;
      rawState.foreground = col;
      if (rawTextColorSwatch) rawTextColorSwatch.style.background = rgb565ToCss(col);
      if (rawTextColorInput) rawTextColorInput.value = rgb565ToHex888(col);
      if (rawTextColorCode) rawTextColorCode.textContent = rgb565ToHex888(col).toUpperCase();
      if (rawTextColor) ensureSelectOption(rawTextColor, col);
    } else if (key === 'bgColor') {
      if (f && f.type === 'RAW_TEXT') {
        f.backgroundColor = col;
        f.transparentBackground = false;
      }
      rawState.background = col;
      rawState.transparent = false;
      if (rawTextTransparentToggle) rawTextTransparentToggle.checked = true;
      if (rawTextTransparent) rawTextTransparent.value = '0';
      if (rawTextBgColorSwatch) rawTextBgColorSwatch.style.background = rgb565ToCss(col);
      if (rawTextBgColorInput) rawTextBgColorInput.value = rgb565ToHex888(col);
      if (rawTextBgColorCode) rawTextBgColorCode.textContent = rgb565ToHex888(col).toUpperCase();
      if (rawTextBackground) ensureSelectOption(rawTextBackground, col);
    }
    syncRawAppearance();
    syncRawTextInspector();
    render();
    if (commit) commitHistory();
  }
  const shapeX = document.getElementById('shapeX');
  const shapeY = document.getElementById('shapeY');
  const shapeW = document.getElementById('shapeW');
  const shapeH = document.getElementById('shapeH');
  const shapeSides2 = document.getElementById('shapeSides2');
  const shapeSidesWrap2 = document.getElementById('shapeSidesWrap2');
  const shapeRotation = document.getElementById('shapeRotation');
  const shapeBorderRow = document.getElementById('shapeBorderRow');
  const shapeBorderLabel = document.getElementById('shapeBorderLabel');
  const shapeBorderToggleWrap = document.getElementById('shapeBorderToggleWrap');
  const shapeBorderEnabled = document.getElementById('shapeBorderEnabled');
  const shapeBorderColorInput = document.getElementById('shapeBorderColorInput');
  const shapeBorderColorSwatch = document.getElementById('shapeBorderColorSwatch');
  const shapeBorderColorCode = document.getElementById('shapeBorderColorCode');
  const shapeBorderSize = document.getElementById('shapeBorderSize');
  const shapeFillRow = document.getElementById('shapeFillRow');
  const shapeFillEnabled = document.getElementById('shapeFillEnabled');
  const shapeFillColorInput = document.getElementById('shapeFillColorInput');
  const shapeFillColorSwatch = document.getElementById('shapeFillColorSwatch');
  const shapeFillColorCode = document.getElementById('shapeFillColorCode');
  const rawMetricsSection = document.getElementById('rawMetricsSection');
  const fieldMetricsSection = document.getElementById('fieldMetricsSection');
  const numericFormatDetails = document.getElementById('numericFormatDetails');
  const fieldInspectorTitle = document.getElementById('fieldInspectorTitle');
  const fieldCapacityWrap = document.getElementById('fieldCapacityWrap');
  const fieldCppType = document.getElementById('fieldCppType');

  const rawTextInput = document.getElementById('rawTextInput');
  const rawTextX = document.getElementById('rawTextX');
  const rawTextY = document.getElementById('rawTextY');
  const rawTextSize = document.getElementById('rawTextSize');
  const rawTextFont = document.getElementById('rawTextFont');
  const rawTextTransparent = document.getElementById('rawTextTransparent');
  const rawTextColor = document.getElementById('rawTextColor');
  const rawTextBackground = document.getElementById('rawTextBackground');
  const rawTextBackgroundWrap = document.getElementById('rawTextBackgroundWrap');
  const rawBoundsStatus = document.getElementById('rawBoundsStatus');
  const rawScalePxHint = document.getElementById('rawScalePxHint');
  const rawTextName = document.getElementById('rawTextName');
  const rawDuplicateBtn = document.getElementById('rawDuplicateBtn');
  const rawDeleteBtn = document.getElementById('rawDeleteBtn');
  const rawTextColorSwatch = document.getElementById('rawTextColorSwatch');
  const rawTextColorInput = document.getElementById('rawTextColorInput');
  const rawTextColorCode = document.getElementById('rawTextColorCode');
  const rawTextTransparentToggle = document.getElementById('rawTextTransparentToggle');
  const rawTextBgColorWrap = document.getElementById('rawTextBgColorWrap');
  const rawTextBgColorSwatch = document.getElementById('rawTextBgColorSwatch');
  const rawTextBgColorInput = document.getElementById('rawTextBgColorInput');
  const rawTextBgColorCode = document.getElementById('rawTextBgColorCode');
  const rawInspectorContract = document.getElementById('rawInspectorContract');
  const copyRawContractBtn = document.getElementById('copyRawContractBtn');

  const fieldName = document.getElementById('fieldName');
  const fieldId = document.getElementById('fieldId');
  const fieldVariable = document.getElementById('fieldVariable');
  const fieldCapacity = document.getElementById('fieldCapacity');
  const fieldX = document.getElementById('fieldX');
  const fieldY = document.getElementById('fieldY');
  const fieldX2Wrap = document.getElementById('fieldX2Wrap');
  const fieldX2 = document.getElementById('fieldX2');
  const fieldY2Wrap = document.getElementById('fieldY2Wrap');
  const fieldY2 = document.getElementById('fieldY2');
  const fieldSidesWrap = document.getElementById('fieldSidesWrap');
  const fieldSides = document.getElementById('fieldSides');
  const fieldFillWrap = document.getElementById('fieldFillWrap');
  const fieldFill = document.getElementById('fieldFill');
  const fieldFillColorWrap = document.getElementById('fieldFillColorWrap');
  const fieldFillColor = document.getElementById('fieldFillColor');
  const fieldPreview = document.getElementById('fieldPreview');
  const fieldLabel = document.getElementById('fieldLabel');
  const fieldUnit = document.getElementById('fieldUnit');
  const fieldValueSize = document.getElementById('fieldValueSize');
  const fieldLabelSize = document.getElementById('fieldLabelSize');
  const fieldFrame = document.getElementById('fieldFrame');
  const fieldLayout = document.getElementById('fieldLayout');
  const fieldAlign = document.getElementById('fieldAlign');
  const fieldLabelColor = document.getElementById('fieldLabelColor');
  const fieldValueColor = document.getElementById('fieldValueColor');
  const fieldBackgroundColor = document.getElementById('fieldBackgroundColor');
  const fieldFrameColor = document.getElementById('fieldFrameColor');

  const fieldIntegerDigits = document.getElementById('fieldIntegerDigits');
  const fieldDecimalDigits = document.getElementById('fieldDecimalDigits');
  const fieldSigned = document.getElementById('fieldSigned');
  const fieldLeadingZeros = document.getElementById('fieldLeadingZeros');
  const valueFormatSampleStatus = document.getElementById('valueFormatSampleStatus');
  const valueFormattedStatus = document.getElementById('valueFormattedStatus');

  const fieldPadStatus = document.getElementById('fieldPadStatus');
  const fieldBoundsStatus = document.getElementById('fieldBoundsStatus');
  const fieldValueBoundsStatus = document.getElementById('fieldValueBoundsStatus');
  const fieldValueXYStatus = document.getElementById('fieldValueXYStatus');
  const fieldLayoutStatus = document.getElementById('fieldLayoutStatus');
  const inspectorContract = document.getElementById('inspectorContract');

  const statusTab = document.getElementById('statusTab');
  const contractTab = document.getElementById('contractTab');
  const codeOutput = document.getElementById('codeOutput');

  const objectTemplate = document.getElementById('textObjectItem');
  const objectList = objectTemplate.parentElement;
  const countBadge = document.querySelector('.count-badge');
  const fieldsStatus = [...document.querySelectorAll('.statusbar span')]
    .find((span) => span.textContent.trim().startsWith('Campos:'));

  const toolbarButtons = [...document.querySelectorAll('.toolbar button')];
  const undoButton = toolbarButtons.find((button) => button.textContent.trim() === 'Deshacer');
  const redoButton = toolbarButtons.find((button) => button.textContent.trim() === 'Rehacer');
  if (undoButton) {
    undoButton.id = 'undoButton';
    undoButton.title = 'Deshacer (Ctrl+Z)';
  }
  if (redoButton) {
    redoButton.id = 'redoButton';
    redoButton.title = 'Rehacer (Ctrl+Y / Ctrl+Shift+Z)';
  }

  const inspectorTitle = fieldSection.querySelector('.inspector-title');
  const legacyDelete = inspectorTitle.querySelector('.inspector-delete');
  const duplicateButton = document.createElement('button');
  duplicateButton.id = 'duplicateObjectButton';
  duplicateButton.className = 'icon-button';
  duplicateButton.type = 'button';
  duplicateButton.textContent = '⧉';
  duplicateButton.title = 'Duplicar objeto (Ctrl+D)';
  const deleteButton = document.createElement('button');
  deleteButton.id = 'deleteObjectButton';
  deleteButton.className = 'icon-button inspector-delete';
  deleteButton.type = 'button';
  deleteButton.textContent = '🗑';
  deleteButton.title = 'Eliminar objeto (Delete)';
  const inspectorActions = document.createElement('span');
  inspectorActions.className = 'inspector-actions';
  inspectorActions.style.display = 'inline-flex';
  inspectorActions.style.gap = '4px';
  inspectorActions.append(duplicateButton, deleteButton);
  if (legacyDelete) legacyDelete.replaceWith(inspectorActions);
  else inspectorTitle.append(inspectorActions);

  let zoom = Number(zoomSelect.value);
  let selectedColor = COLORS.find((color) => color.name === 'ORANGE');
  let selectedTool = 'textField';
  let selectedFieldKeys = ['text-1'];
  let selectedFieldKey = 'text-1';
  let fieldSerial = 1;
  let drawing = false;
  let draggingObject = false;
  let resizingHandle = null;
  let resizeInitialBounds = null;
  let resizeInitialPointer = null;
  let resizeInitialFields = null;
  let resizeInitialPixelMap = null;
  let dragInitialPointer = null;
  let dragInitialFields = null;
  let dragInitialPixelMaps = null;
  let isMarquee = false;
  let marqueeStart = null;
  let marqueeEnd = null;
  let marqueeStartClient = null;
  let marqueeEndClient = null;
  let dragOffset = { x: 0, y: 0 };

  function updateMarqueeOverlay(startClient, endClient) {
    const boxEl = document.getElementById('cadMarqueeBox');
    if (!boxEl || !canvasViewport) return;
    if (!isMarquee || !startClient || !endClient) {
      boxEl.style.display = 'none';
      return;
    }
    const vpRect = canvasViewport.getBoundingClientRect();
    const left = Math.min(startClient.x, endClient.x) - vpRect.left;
    const top = Math.min(startClient.y, endClient.y) - vpRect.top;
    const width = Math.abs(endClient.x - startClient.x);
    const height = Math.abs(endClient.y - startClient.y);

    const isLeftToRight = endClient.x >= startClient.x;
    boxEl.style.left = `${left}px`;
    boxEl.style.top = `${top}px`;
    boxEl.style.width = `${width}px`;
    boxEl.style.height = `${height}px`;
    boxEl.className = 'cad-marquee-box ' + (isLeftToRight ? 'window-mode' : 'crossing-mode');
    boxEl.style.display = 'block';
  }

  let panX = 0;
  let panY = 0;
  let isPanning = false;
  let lastPanPointer = null;

  function updateMiniPreviewViewport() {
    const vpBox = document.getElementById('sidebarPreviewViewport');
    const miniCanvas = document.getElementById('sidebarMiniPreviewCanvas');
    const frame = document.getElementById('sidebarPreviewFrame');
    const vp = canvasViewport || document.getElementById('canvasViewport');
    if (!vpBox || !vp) return;
    const vpRect = vp.getBoundingClientRect();
    if (vpRect.width <= 0 || vpRect.height <= 0) return;

    const canvasLeft = (vpRect.width - WIDTH * zoom) / 2 + panX;
    const canvasTop = (vpRect.height - HEIGHT * zoom) / 2 + panY;

    const visX1 = Math.max(0, -canvasLeft / zoom);
    const visY1 = Math.max(0, -canvasTop / zoom);
    const visX2 = Math.min(WIDTH, (vpRect.width - canvasLeft) / zoom);
    const visY2 = Math.min(HEIGHT, (vpRect.height - canvasTop) / zoom);

    const isZoomed = (visX2 - visX1 < WIDTH - 2) || (visY2 - visY1 < HEIGHT - 2);
    if (isZoomed && zoom > 1.05) {
      if (miniCanvas && frame) {
        const cRect = miniCanvas.getBoundingClientRect();
        const fRect = frame.getBoundingClientRect();
        if (cRect.width > 0 && cRect.height > 0) {
          const scaleX = cRect.width / WIDTH;
          const scaleY = cRect.height / HEIGHT;
          const leftOffset = cRect.left - fRect.left;
          const topOffset = cRect.top - fRect.top;
          vpBox.style.display = 'block';
          vpBox.style.left = `${leftOffset + visX1 * scaleX}px`;
          vpBox.style.top = `${topOffset + visY1 * scaleY}px`;
          vpBox.style.width = `${Math.max(4, (visX2 - visX1) * scaleX)}px`;
          vpBox.style.height = `${Math.max(4, (visY2 - visY1) * scaleY)}px`;
          return;
        }
      }
      vpBox.style.display = 'block';
      vpBox.style.left = `${(visX1 / WIDTH) * 100}%`;
      vpBox.style.top = `${(visY1 / HEIGHT) * 100}%`;
      vpBox.style.width = `${((visX2 - visX1) / WIDTH) * 100}%`;
      vpBox.style.height = `${((visY2 - visY1) / HEIGHT) * 100}%`;
    } else {
      vpBox.style.display = 'none';
    }
  }

  function updateStageTransform() {
    if (canvasStage) {
      canvasStage.style.transform = `translate(${panX}px, ${panY}px)`;
    }
    if (canvasViewport) {
      canvasViewport.style.backgroundPosition = `${panX}px ${panY}px, ${panX + 12}px ${panY + 12}px`;
    }
    updateMiniPreviewViewport();
  }

  function applyZoom(nextZoom) {
    const val = Math.max(0.25, Math.min(16, Math.round(nextZoom * 100) / 100));
    zoom = val;
    let opt = zoomSelect.querySelector(`option[value="${val}"]:not([data-fit-option="1"])`);
    if (!opt) {
      opt = zoomSelect.querySelector('option[data-custom-zoom="1"]');
      if (!opt) {
        opt = document.createElement('option');
        opt.dataset.customZoom = '1';
        zoomSelect.appendChild(opt);
      }
      opt.value = String(val);
      opt.textContent = `${val}×`;
    }
    zoomSelect.value = String(val);
    render();
    updateMiniPreviewViewport();
    window.dispatchEvent(new CustomEvent('jwplc:zoom-change', { detail: { zoom: val } }));
  }

  function fitCanvas() {
    if (!canvasViewport) return;
    const vpRect = canvasViewport.getBoundingClientRect();
    const vpW = vpRect.width;
    const vpH = vpRect.height;
    if (vpW <= 50 || vpH <= 50) return;

    const topMargin = 72;
    const bottomMargin = 28;
    const leftMargin = 36;
    const rightMargin = 72;

    const rulerH = 24;
    const rulerW = 36;

    const availW = Math.max(80, vpW - leftMargin - rightMargin - rulerW * 2);
    const availH = Math.max(60, vpH - topMargin - bottomMargin - rulerH * 2);

    const rawZoom = Math.min(availW / WIDTH, availH / HEIGHT);
    const targetZoom = Math.max(0.5, Math.min(10, Math.floor(rawZoom * 20) / 20));

    applyZoom(targetZoom);

    const desiredCenterX = leftMargin + (vpW - leftMargin - rightMargin) / 2;
    const desiredCenterY = topMargin + (vpH - topMargin - bottomMargin) / 2;

    const targetPanX = Math.round(desiredCenterX - ((vpW - WIDTH * targetZoom) / 2 + (WIDTH / 2) * targetZoom));
    const targetPanY = Math.round(desiredCenterY - ((vpH - HEIGHT * targetZoom) / 2 + (HEIGHT / 2) * targetZoom));

    panX = targetPanX;
    panY = targetPanY;
    updateStageTransform();
  }

  function fitSelection() {
    if (!canvasViewport) return;
    const fields = selectedFields();
    let b = null;
    if (fields && fields.length > 0) {
      b = getSelectionBounds(fields);
    } else {
      const selPm = window.JWPLCHMIPixelMaps?.getSelected?.();
      if (selPm && selPm.pixels?.length) {
        let minX = Infinity, minY = Infinity, maxX = -Infinity, maxY = -Infinity;
        selPm.pixels.forEach((p) => {
          const px = selPm.x + p.x;
          const py = selPm.y + p.y;
          if (px < minX) minX = px;
          if (px > maxX) maxX = px;
          if (py < minY) minY = py;
          if (py > maxY) maxY = py;
        });
        if (minX <= maxX && minY <= maxY) {
          b = { minX, minY, maxX, maxY };
        }
      }
    }

    if (!b) {
      fitCanvas();
      return;
    }

    const vpRect = canvasViewport.getBoundingClientRect();
    const vpW = vpRect.width;
    const vpH = vpRect.height;
    if (vpW <= 50 || vpH <= 50) return;

    const topMargin = 72;
    const bottomMargin = 28;
    const leftMargin = 36;
    const rightMargin = 72;

    const availW = Math.max(80, vpW - leftMargin - rightMargin - 36);
    const availH = Math.max(60, vpH - topMargin - bottomMargin - 22);

    const selW = Math.max(4, (b.maxX - b.minX + 1));
    const selH = Math.max(4, (b.maxY - b.minY + 1));
    const selCenterX = (b.minX + b.maxX + 1) / 2;
    const selCenterY = (b.minY + b.maxY + 1) / 2;

    const zoomX = (availW * 0.75) / selW;
    const zoomY = (availH * 0.75) / selH;
    const rawZoom = Math.min(zoomX, zoomY);

    const targetZoom = Math.max(0.5, Math.min(12, Math.floor(rawZoom * 20) / 20));

    applyZoom(targetZoom);

    const desiredCenterX = leftMargin + (vpW - leftMargin - rightMargin) / 2;
    const desiredCenterY = topMargin + (vpH - topMargin - bottomMargin) / 2;

    const targetPanX = Math.round(desiredCenterX - ((vpW - WIDTH * targetZoom) / 2 + selCenterX * targetZoom));
    const targetPanY = Math.round(desiredCenterY - ((vpH - HEIGHT * targetZoom) / 2 + selCenterY * targetZoom));

    panX = targetPanX;
    panY = targetPanY;
    updateStageTransform();
  }
  let lastPoint = null;
  let codeMode = 'status';
  let gestureChanged = false;
  let keyboardNudgeChanged = false;
  let activePage = 0;
  let hmiPages = [{ id: 0, name: 'Principal' }];
  let navBarConfig = {
    enabled: true,
    preset: 'industrial-color',
    position: 'bottom',
    height: 22,
    showIndicator: true,
    showArrows: true
  };

  // Padding (px) que el fondo opaco de RAW_TEXT agrega alrededor del texto.
  const RAW_BG_PAD = 2;

  const rawState = {
    x: 20,
    y: 20,
    size: 1,
    font: 'GLCD',
    value: 'Texto RAW',
    foreground: 0xF800,
    background: 0xFFFF
  };


  function defaultTextField(key = 'text-1') {
    return {
      type: 'TEXT',
      key,
      name: 'Estado',
      id: 'FIELD_STATUS',
      variable: 'estadoTexto',
      capacity: 12,
      x: 20,
      y: 20,
      preview: 'READY',
      label: 'Estado',
      unit: '',
      valueSize: 2,
      labelSize: 1,
      frame: false,
      layout: 'INLINE',
      align: 'LEFT',
      page: 0,
      labelColor: 0xFFFF,
      valueColor: 0x07FF,
      backgroundColor: 0x0000,
      frameColor: 0xFFFF
    };
  }

  function defaultValueField(key = 'value-2') {
    return {
      type: 'VALUE',
      key,
      name: 'Temperatura',
      id: 'FIELD_TEMP',
      variable: 'temperatura',
      x: 36,
      y: 58,
      preview: '25.6',
      label: 'Temp',
      unit: 'C',
      integerDigits: 3,
      decimalDigits: 1,
      signedValue: false,
      leadingZeros: false,
      valueSize: 2,
      labelSize: 1,
      frame: false,
      layout: 'INLINE',
      align: 'RIGHT',
      page: 0,
      labelColor: 0xFFFF,
      valueColor: 0x07FF,
      backgroundColor: 0x0000,
      frameColor: 0xFFFF
    };
  }

  let hmiFields = [defaultTextField()];

  const history = [];
  let historyIndex = -1;

  function clamp(value, min, max) {
    return Math.min(max, Math.max(min, value));
  }

  function hex565(value) {
    // 'TRANSPARENT' u otro valor no numérico -> 0x0000 (los campos no tienen fondo transparente).
    const num = Number(value);
    return `0x${(Number.isFinite(num) ? num & 0xFFFF : 0).toString(16).toUpperCase().padStart(4, '0')}`;
  }

  function colorByName(name) {
    if (name === 'TRANSPARENT' || name === -1) {
      return { name: 'TRANSPARENT', value: -1 };
    }
    const str = String(name ?? '').trim();
    const match = COLORS.find((color) => color.name.toUpperCase() === str.toUpperCase());
    if (match) return match;

    const num = (str.startsWith('0x') || str.startsWith('0X')) ? parseInt(str, 16) : Number(str);
    if (Number.isFinite(num)) {
      const val565 = num & 0xFFFF;
      const byVal = COLORS.find((c) => c.value === val565);
      if (byVal) return byVal;
      return { name: hex565(val565), value: val565 };
    }
    return COLORS[0];
  }

  function colorName(value) {
    if (value === 'TRANSPARENT' || value === -1) return 'TRANSPARENT';
    const num = Number(value) & 0xFFFF;
    const match = COLORS.find((color) => color.value === num);
    return match ? match.name : hex565(num);
  }

  function rgb565ToRgb888(value) {
    const r5 = (value >> 11) & 0x1F;
    const g6 = (value >> 5) & 0x3F;
    const b5 = value & 0x1F;
    return {
      r: Math.round((r5 * 255) / 31),
      g: Math.round((g6 * 255) / 63),
      b: Math.round((b5 * 255) / 31)
    };
  }

  function rgb565ToCss(value) {
    if (value === 'TRANSPARENT' || value === -1) return 'transparent';
    const { r, g, b } = rgb565ToRgb888(value);
    return `rgb(${r}, ${g}, ${b})`;
  }

  function rgb565ToHex888(value) {
    if (value === 'TRANSPARENT' || value === -1) return '#000000';
    const { r, g, b } = rgb565ToRgb888(value);
    return `#${r.toString(16).padStart(2, '0')}${g.toString(16).padStart(2, '0')}${b.toString(16).padStart(2, '0')}`;
  }

  function hex888ToRgb565(hex) {
    const clean = String(hex || '').replace('#', '').trim();
    if (clean.length < 6) return 0x0000;
    const r = parseInt(clean.slice(0, 2), 16) || 0;
    const g = parseInt(clean.slice(2, 4), 16) || 0;
    const b = parseInt(clean.slice(4, 6), 16) || 0;
    return (((r >> 3) & 0x1F) << 11) | (((g >> 2) & 0x3F) << 5) | ((b >> 3) & 0x1F);
  }

  function ensureSelectOption(select, value) {
    if (!select) return;
    const allowed = new Set(['TRANSPARENT', 'CUSTOM', ...COLORS.map((c) => c.name)]);
    for (let i = select.options.length - 1; i >= 0; i--) {
      if (!allowed.has(select.options[i].value)) {
        select.options[i].remove();
      }
    }

    if (value === 'TRANSPARENT' || value === -1) {
      let opt = [...select.options].find((o) => o.value === 'TRANSPARENT');
      if (!opt) {
        opt = document.createElement('option');
        opt.value = 'TRANSPARENT';
        opt.textContent = 'TRANSPARENTE (Fondo libre)';
        select.insertBefore(opt, select.firstChild);
      }
      select.value = 'TRANSPARENT';
      return;
    }
    const val565 = Number(value) & 0xFFFF;
    const matched = COLORS.find((c) => c.value === val565);
    if (matched) {
      select.value = matched.name;
    } else {
      let opt = [...select.options].find((o) => o.value === 'CUSTOM');
      if (!opt) {
        opt = document.createElement('option');
        opt.value = 'CUSTOM';
        opt.textContent = 'Personalizado';
        select.appendChild(opt);
      }
      select.value = 'CUSTOM';
    }
  }

  function indexFor(x, y) { return y * WIDTH + x; }
  function inside(x, y) { return x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT; }

  function rotatePoint(px, py, cx, cy, angleDeg) {
    if (!angleDeg) return { x: Math.round(px), y: Math.round(py) };
    const rad = angleDeg * Math.PI / 180;
    const cos = Math.cos(rad);
    const sin = Math.sin(rad);
    const dx = px - cx;
    const dy = py - cy;
    return {
      x: Math.round(cx + dx * cos - dy * sin),
      y: Math.round(cy + dx * sin + dy * cos)
    };
  }

  function transformShapePoint(px, py, cx, cy, angleDeg, flipH, flipV) {
    let p = rotatePoint(px, py, cx, cy, angleDeg);
    if (flipH) p.x = Math.round(2 * cx - p.x);
    if (flipV) p.y = Math.round(2 * cy - p.y);
    return p;
  }

  function inverseTransformShapePoint(px, py, cx, cy, angleDeg, flipH, flipV) {
    let x = flipH ? (2 * cx - px) : px;
    let y = flipV ? (2 * cy - py) : py;
    return rotatePoint(x, y, cx, cy, -angleDeg);
  }

  function getMaxCornerRadius(field) {
    if (!field) return 0;
    const w = Math.abs((field.x2 ?? field.x ?? 0) - (field.x ?? 0));
    const h = Math.abs((field.y2 ?? field.y ?? 0) - (field.y ?? 0));
    const minDim = Math.min(w, h);
    if (field.type === 'RECT') return Math.max(0, Math.floor(minDim / 2));
    if (field.type === 'TRIANGLE' || field.type === 'POLYGON') return Math.max(0, Math.floor(minDim * 0.35));
    return 0;
  }

  function buildRoundedPolygonPoints(basePts, radius) {
    const r = Math.max(0, Math.round(Number(radius) || 0));
    if (r <= 0 || !Array.isArray(basePts) || basePts.length < 3) return basePts;
    const n = basePts.length;
    const out = [];
    const steps = Math.max(4, Math.min(10, r + 2));

    for (let i = 0; i < n; i++) {
      const prev = basePts[(i - 1 + n) % n];
      const curr = basePts[i];
      const next = basePts[(i + 1) % n];

      const dx1 = prev.x - curr.x;
      const dy1 = prev.y - curr.y;
      const len1 = Math.hypot(dx1, dy1);

      const dx2 = next.x - curr.x;
      const dy2 = next.y - curr.y;
      const len2 = Math.hypot(dx2, dy2);

      if (len1 < 2 || len2 < 2) {
        out.push({ x: curr.x, y: curr.y });
        continue;
      }

      const ux1 = dx1 / len1;
      const uy1 = dy1 / len1;
      const ux2 = dx2 / len2;
      const uy2 = dy2 / len2;

      const dot = Math.max(-0.995, Math.min(0.995, ux1 * ux2 + uy1 * uy2));
      const halfAngle = Math.acos(dot) / 2;
      const tanHalf = Math.tan(halfAngle);
      const rawDist = tanHalf > 0.05 ? (r / tanHalf) : r;
      const dist = Math.min(rawDist, len1 * 0.46, len2 * 0.46);

      const pStart = { x: curr.x + ux1 * dist, y: curr.y + uy1 * dist };
      const pEnd = { x: curr.x + ux2 * dist, y: curr.y + uy2 * dist };

      for (let s = 0; s <= steps; s++) {
        const t = s / steps;
        const omt = 1 - t;
        // Curva cuadrática suave tangente a ambos lados del vértice. Sin
        // redondear: se redondea tras rotar/espejar (roundShapePoints).
        out.push({
          x: omt * omt * pStart.x + 2 * omt * t * curr.x + t * t * pEnd.x,
          y: omt * omt * pStart.y + 2 * omt * t * curr.y + t * t * pEnd.y
        });
      }
    }
    return out;
  }

  // Vértices enteros (como los recibe la TFT) sin repetidos consecutivos.
  function roundShapePoints(pts) {
    const out = [];
    for (const p of pts) {
      const q = { x: Math.round(p.x), y: Math.round(p.y) };
      const last = out[out.length - 1];
      if (!last || last.x !== q.x || last.y !== q.y) out.push(q);
    }
    if (out.length > 1 && out[0].x === out[out.length - 1].x && out[0].y === out[out.length - 1].y) out.pop();
    return out;
  }

  function getShapeTransformedVertices(field) {
    const x1 = field.x ?? 0; const y1 = field.y ?? 0;
    const x2 = field.x2 ?? x1; const y2 = field.y2 ?? y1;
    const cx = (x1 + x2) / 2;
    const cy = (y1 + y2) / 2;
    const angleDeg = Number(field.rotation) || 0;
    const flipH = Boolean(field.flipH);
    const flipV = Boolean(field.flipV);
    const tf = (p) => transformShapePoint(p.x, p.y, cx, cy, angleDeg, flipH, flipV);
    const maxR = getMaxCornerRadius(field);
    const r = clamp(Math.round(Number(field.cornerRadius) || 0), 0, maxR);

    if (field.type === 'LINE') {
      return [tf({ x: x1, y: y1 }), tf({ x: x2, y: y2 })];
    }
    if (field.type === 'RECT') {
      const left = Math.min(x1, x2);
      const right = Math.max(x1, x2);
      const top = Math.min(y1, y2);
      const bottom = Math.max(y1, y2);
      if (r <= 0) {
        return [
          tf({ x: left, y: top }),
          tf({ x: right, y: top }),
          tf({ x: right, y: bottom }),
          tf({ x: left, y: bottom })
        ];
      }
      const corners = [
        { cx: left + r, cy: top + r, start: Math.PI, end: 1.5 * Math.PI },
        { cx: right - r, cy: top + r, start: 1.5 * Math.PI, end: 2 * Math.PI },
        { cx: right - r, cy: bottom - r, start: 0, end: 0.5 * Math.PI },
        { cx: left + r, cy: bottom - r, start: 0.5 * Math.PI, end: Math.PI }
      ];
      const steps = Math.max(4, Math.min(10, r + 2));
      const pts = [];
      corners.forEach((c) => {
        for (let s = 0; s <= steps; s++) {
          const ang = c.start + (c.end - c.start) * (s / steps);
          const pt = tf({ x: c.cx + r * Math.cos(ang), y: c.cy + r * Math.sin(ang) });
          if (pts.length === 0 || pts[pts.length - 1].x !== pt.x || pts[pts.length - 1].y !== pt.y) {
            pts.push(pt);
          }
        }
      });
      return pts;
    }
    if (field.type === 'TRIANGLE') {
      const topPt = { x: Math.round((x1 + x2) / 2), y: y1 };
      const bl = { x: x1, y: y2 };
      const br = { x: x2, y: y2 };
      const basePts = [topPt, br, bl];
      return roundShapePoints(buildRoundedPolygonPoints(basePts, r).map(tf));
    }
    if (field.type === 'POLYGON') {
      const basePts = getPolygonVertices(field);
      return roundShapePoints(buildRoundedPolygonPoints(basePts, r).map(tf));
    }
    if (field.type === 'ELLIPSE') {
      const minX = Math.min(x1, x2); const maxX = Math.max(x1, x2);
      const minY = Math.min(y1, y2); const maxY = Math.max(y1, y2);
      const ecx = (minX + maxX) / 2; const ecy = (minY + maxY) / 2;
      const erx = Math.max(1, (maxX - minX) / 2); const ery = Math.max(1, (maxY - minY) / 2);
      const N = 32;
      const pts = [];
      for (let i = 0; i < N; i++) {
        const ang = i * 2 * Math.PI / N;
        pts.push(tf({ x: ecx + erx * Math.cos(ang), y: ecy + ery * Math.sin(ang) }));
      }
      return pts;
    }
    return [];
  }

  function getShapeLocalBounds(field) {
    const x1 = field.x ?? 0; const y1 = field.y ?? 0;
    const x2 = field.x2 ?? x1; const y2 = field.y2 ?? y1;
    const minX = Math.min(x1, x2); const maxX = Math.max(x1, x2);
    const minY = Math.min(y1, y2); const maxY = Math.max(y1, y2);
    if (field.type === 'POLYGON') {
      const pts = getPolygonVertices(field);
      if (pts && pts.length > 0) {
        let pminX = Infinity, pmaxX = -Infinity, pminY = Infinity, pmaxY = -Infinity;
        for (const p of pts) {
          if (p.x < pminX) pminX = p.x;
          if (p.x > pmaxX) pmaxX = p.x;
          if (p.y < pminY) pminY = p.y;
          if (p.y > pmaxY) pmaxY = p.y;
        }
        return {
          minX: pminX,
          maxX: pmaxX,
          minY: pminY,
          maxY: pmaxY,
          cx: (pminX + pmaxX) / 2,
          cy: (pminY + pmaxY) / 2,
          width: Math.max(1, pmaxX - pminX),
          height: Math.max(1, pmaxY - pminY)
        };
      }
    }
    if (field.type === 'TRIANGLE') {
      const topPt = { x: Math.round((x1 + x2) / 2), y: y1 };
      const bl = { x: x1, y: y2 };
      const br = { x: x2, y: y2 };
      const pminX = Math.min(topPt.x, bl.x, br.x);
      const pmaxX = Math.max(topPt.x, bl.x, br.x);
      const pminY = Math.min(topPt.y, bl.y, br.y);
      const pmaxY = Math.max(topPt.y, bl.y, br.y);
      return {
        minX: pminX,
        maxX: pmaxX,
        minY: pminY,
        maxY: pmaxY,
        cx: (pminX + pmaxX) / 2,
        cy: (pminY + pmaxY) / 2,
        width: Math.max(1, pmaxX - pminX),
        height: Math.max(1, pmaxY - pminY)
      };
    }
    return {
      minX,
      maxX,
      minY,
      maxY,
      cx: (minX + maxX) / 2,
      cy: (minY + maxY) / 2,
      width: Math.max(1, maxX - minX),
      height: Math.max(1, maxY - minY)
    };
  }

  let toastTimer = null;
  function showToastMessage(msg, kind = 'info') {
    let toast = document.getElementById('hmiToastNotification');
    if (!toast) {
      toast = document.createElement('div');
      toast.id = 'hmiToastNotification';
      toast.style.position = 'fixed';
      toast.style.top = '65px';
      toast.style.left = '50%';
      toast.style.transform = 'translateX(-50%)';
      toast.style.padding = '8px 18px';
      toast.style.borderRadius = '6px';
      toast.style.fontSize = '13px';
      toast.style.fontWeight = '500';
      toast.style.fontFamily = 'system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif';
      toast.style.zIndex = '99999';
      toast.style.boxShadow = '0 6px 18px rgba(0,0,0,0.45)';
      toast.style.pointerEvents = 'none';
      toast.style.transition = 'opacity 0.25s ease, transform 0.25s ease';
      document.body.appendChild(toast);
    }
    if (kind === 'warning') {
      toast.style.background = '#1e293b';
      toast.style.color = '#fef08a';
      toast.style.border = '1px solid #eab308';
      toast.innerHTML = `<span style="margin-right:6px; color:#eab308; font-weight:bold;">⚠️</span> ${msg}`;
    } else {
      toast.style.background = '#1e293b';
      toast.style.color = '#e2e8f0';
      toast.style.border = '1px solid #38bdf8';
      toast.innerHTML = `<span style="margin-right:6px; color:#38bdf8; font-weight:bold;">ℹ️</span> ${msg}`;
    }
    toast.style.opacity = '1';
    toast.style.transform = 'translateX(-50%) translateY(0)';
    if (toastTimer) clearTimeout(toastTimer);
    toastTimer = setTimeout(() => {
      toast.style.opacity = '0';
      toast.style.transform = 'translateX(-50%) translateY(-8px)';
    }, 2800);
  }

  function addRecentColor(color565) {
    color565 = Number(color565) & 0xFFFF;
    recentColors = [color565, ...recentColors.filter((c) => (c & 0xFFFF) !== color565)].slice(0, 16);
    renderRecentColors();
  }

  // Estado del inspector Cuentagotas / Relleno.
  const colorToolPrefs = { fillMode: 'CONTIGUOUS', fillShapes: true, pickReturn: false };
  let toolBeforePick = 'pointer';
  let colorToolEditor = null;

  function setActiveColor(value) {
    const v = Number(value) & 0xFFFF;
    selectedColor = COLORS.find((c) => c.value === v) || { name: hex565(v), value: v };
    updateActiveColorUI();
    buildPalette();
  }

  function ensureColorToolEditor() {
    if (colorToolEditor || !window.JWPLCColorEditor) return;
    const host = document.getElementById('colorToolEditorHost');
    if (!host) return;
    colorToolEditor = window.JWPLCColorEditor.create({
      paletteAttr: 'data-colortool-palette',
      onInput: (v565) => setActiveColor(v565),
      onCommit: (v565) => { setActiveColor(v565); addRecentColor(v565); }
    });
    host.appendChild(colorToolEditor.el);
  }

  function syncColorToolInspector() {
    if (!eyedropperSection) return;
    const isPick = selectedTool === 'pick';
    const title = document.getElementById('colorToolTitle');
    if (title) title.textContent = isPick ? 'CUENTAGOTAS' : 'RELLENO';
    const icon = document.getElementById('colorToolIcon');
    const toolSvg = document.querySelector(`.tool[data-tool="${isPick ? 'pick' : 'fill'}"] svg`);
    if (icon && toolSvg) icon.innerHTML = toolSvg.outerHTML;
    const note = document.getElementById('colorToolNote');
    if (note) {
      note.textContent = isPick
        ? 'Clic en el lienzo para capturar el color de ese píxel. Atajo: I, o Alt+clic con cualquier herramienta.'
        : 'Clic en el lienzo para rellenar con el color activo. Atajo: G.';
    }
    eyedropperSection.querySelectorAll('[data-color-tool]').forEach((btn) => {
      btn.classList.toggle('active', btn.dataset.colorTool === selectedTool);
    });
    eyedropperSection.querySelectorAll('[data-color-tool-pane]').forEach((pane) => {
      pane.hidden = pane.dataset.colorToolPane !== selectedTool;
    });
    eyedropperSection.querySelectorAll('[data-fill-mode]').forEach((btn) => {
      btn.classList.toggle('active', btn.dataset.fillMode === colorToolPrefs.fillMode);
    });
    const fillShapes = document.getElementById('fillShapesToggle');
    if (fillShapes) fillShapes.checked = colorToolPrefs.fillShapes;
    const pickReturn = document.getElementById('pickReturnToggle');
    if (pickReturn) pickReturn.checked = colorToolPrefs.pickReturn;
    ensureColorToolEditor();
    colorToolEditor?.set(selectedColor.value & 0xFFFF, 'Color activo');
  }

  function updateEyedropperActiveUI() {
    if (!eyedropperActiveSwatch || !selectedColor) return;
    const val = selectedColor.value & 0xFFFF;
    eyedropperActiveSwatch.style.backgroundColor = rgb565ToCss(val);
    eyedropperActiveName.textContent = selectedColor.name;
    eyedropperActive565.textContent = hex565(val);
    if (eyedropperActiveHex) eyedropperActiveHex.textContent = rgb565ToHex888(val).toUpperCase();
  }

  // Lo que se ve en el lienzo: framebuffer de objetos + capas PIXEL visibles
  // de la página (las capas no se componen en el framebuffer). Mismo orden
  // que designer-pixelmap.js:render (un objeto más al frente tapa la capa).
  function compositeBuffer() {
    const out = framebuffer.slice();
    const maps = (window.JWPLCHMIPixelMaps?.getAll?.() || [])
      .filter((m) => Number(m.page || 0) === activePage && m.editorVisible !== false && m.pixels?.length)
      .sort((a, b) => (Number(b.layerOrder) || 0) - (Number(a.layerOrder) || 0));
    maps.forEach((m) => {
      const order = Number(m.layerOrder) || Infinity;
      m.pixels.forEach((p) => {
        const x = Number(m.x) + Number(p.x);
        const y = Number(m.y) + Number(p.y);
        if (!inside(x, y)) return;
        const idx = indexFor(x, y);
        if (topFieldOrder[idx] < order) return;
        out[idx] = Number(p.color) & 0xFFFF;
      });
    });
    return out;
  }

  function compositeColorAt(x, y) {
    if (!inside(x, y)) return null;
    return compositeBuffer()[indexFor(x, y)];
  }

  function updateEyedropperLiveUI(point) {
    if (!eyedropperLiveSwatch) return;
    if (!point || !inside(point.x, point.y)) {
      if (eyedropperLiveCoords) eyedropperLiveCoords.textContent = 'Fuera del lienzo';
      if (eyedropperLiveName) eyedropperLiveName.textContent = '—';
      if (eyedropperLive565) eyedropperLive565.textContent = '—';
      if (eyedropperLiveHex) eyedropperLiveHex.textContent = '—';
      if (eyedropperLiveRgb) eyedropperLiveRgb.textContent = '—';
      eyedropperLiveSwatch.style.backgroundColor = 'transparent';
      return;
    }
    const color565 = compositeColorAt(point.x, point.y);
    const cName = colorName(color565);
    const hexVal = hex565(color565);
    const hex888 = rgb565ToHex888(color565).toUpperCase();
    const rgb = rgb565ToRgb888(color565);

    eyedropperLiveSwatch.style.backgroundColor = rgb565ToCss(color565);
    if (eyedropperLiveName) eyedropperLiveName.textContent = cName;
    if (eyedropperLive565) eyedropperLive565.textContent = hexVal;
    if (eyedropperLiveHex) eyedropperLiveHex.textContent = hex888;
    if (eyedropperLiveRgb) eyedropperLiveRgb.textContent = `R:${rgb.r} G:${rgb.g} B:${rgb.b}`;
    if (eyedropperLiveCoords) eyedropperLiveCoords.textContent = `X: ${point.x} · Y: ${point.y}`;
  }

  function renderRecentColors() {
    if (!recentColorsGrid) return;
    recentColorsGrid.innerHTML = '';
    recentColors.forEach((colorVal) => {
      const btn = document.createElement('button');
      btn.className = 'recent-color-chip';
      btn.type = 'button';
      btn.style.backgroundColor = rgb565ToCss(colorVal);
      btn.title = `${colorName(colorVal)} (${hex565(colorVal)})`;
      if (selectedColor && (selectedColor.value & 0xFFFF) === (colorVal & 0xFFFF)) {
        btn.classList.add('active');
      }
      btn.addEventListener('click', () => {
        setActiveColor(colorVal);
        colorToolEditor?.set(colorVal & 0xFFFF, 'Color activo');
      });
      recentColorsGrid.appendChild(btn);
    });
  }

  function pageExists(page) {
    return hmiPages.some((item) => item.id === Number(page));
  }

  function fieldsForPage(page = activePage) {
    return hmiFields.filter((field) => (field.type === 'NAV_BAR' || field.global) || Number(field.page || 0) === Number(page));
  }

  function setSelectedKeys(keys) {
    selectedFieldKeys = Array.isArray(keys) ? Array.from(new Set(keys.filter(Boolean))) : (keys ? [keys] : []);
    selectedFieldKey = selectedFieldKeys[0] || null;
  }

  function selectedFields() {
    return hmiFields.filter((field) => selectedFieldKeys.includes(field.key) && (field.type === 'NAV_BAR' || field.global || Number(field.page || 0) === activePage));
  }

  function selectedField() {
    const list = selectedFields();
    return list.length > 0 ? list[list.length - 1] : null;
  }

  function toolForField(field) {
    if (!field) return 'none';
    if (field.type === 'VALUE') return 'valueField';
    if (field.type === 'BOOL') return 'boolField';
    if (field.type === 'BAR') return 'barField';
    if (field.type === 'TEXT') return 'textField';
    if (field.type === 'NAV_BAR') return 'navBarField';
    if (field.type === 'MENU') return 'menuField';
    if (field.type === 'EVENT_LOG') return 'eventLogField';
    if (field.type === 'ICON') return 'iconField';
    return 'pointer';
  }

  function setLayerPixel(x, y, value) {
    if (!inside(x, y)) return false;
    const index = indexFor(x, y);
    if (pixelLayer[index] === value) return false;
    pixelLayer[index] = value;
    return true;
  }

  // Región a rellenar sobre lo que se ve en el lienzo (compositeBuffer).
  // CONTIGUOUS: 4 vecinos del mismo color; GLOBAL: ese color en toda la página.
  // Devuelve la lista de puntos, vacía si ya tienen el color de reemplazo.
  function fillRegion(startX, startY, replacement, mode = 'CONTIGUOUS') {
    if (!inside(startX, startY)) return [];
    const buffer = compositeBuffer();
    const target = buffer[indexFor(startX, startY)];
    if (target === (replacement & 0xFFFF)) return [];
    const points = [];

    if (mode === 'GLOBAL') {
      for (let idx = 0; idx < buffer.length; idx++) {
        if (buffer[idx] === target) points.push({ x: idx % WIDTH, y: Math.floor(idx / WIDTH) });
      }
      return points;
    }

    const queue = new Int32Array(WIDTH * HEIGHT);
    const visited = new Uint8Array(WIDTH * HEIGHT);
    let head = 0;
    let tail = 0;
    const startIdx = indexFor(startX, startY);
    queue[tail++] = startIdx;
    visited[startIdx] = 1;
    while (head < tail) {
      const idx = queue[head++];
      const x = idx % WIDTH;
      const y = (idx - x) / WIDTH;
      points.push({ x, y });
      const push = (n) => {
        if (!visited[n] && buffer[n] === target) {
          visited[n] = 1;
          queue[tail++] = n;
        }
      };
      if (x > 0) push(idx - 1);
      if (x < WIDTH - 1) push(idx + 1);
      if (y > 0) push(idx - WIDTH);
      if (y < HEIGHT - 1) push(idx + WIDTH);
    }
    return points;
  }

  // Herramienta Relleno: figura cerrada bajo el puntero (si está activo) o
  // región de píxeles pintada en una capa PIXEL (lo que exporta el codegen).
  function fillAt(point) {
    const value = selectedColor.value & 0xFFFF;
    if (colorToolPrefs.fillShapes) {
      for (let index = hmiFields.length - 1; index >= 0; index--) {
        const f = hmiFields[index];
        if (Number(f.page || 0) !== activePage || f.hidden) continue;
        if (['RECT', 'ELLIPSE', 'TRIANGLE', 'POLYGON'].includes(f.type) && isPointInsideShapeArea(point.x, point.y, f)) {
          if (f.fill && (Number(f.fillColor) & 0xFFFF) === value) return false;
          f.fill = true;
          f.fillColor = value;
          render();
          commitHistory();
          return true;
        }
      }
    }
    const points = fillRegion(point.x, point.y, value, colorToolPrefs.fillMode);
    if (!points.length) return false;
    const map = window.JWPLCHMIPixelMaps?.paintPixels?.(points, value);
    if (!map) {
      showToastMessage('No se pudo crear otra capa PIXEL (máximo alcanzado).', 'warning');
      return false;
    }
    addRecentColor(value);
    return true;
  }

  let activeShapeColorDropperTarget = null;

  // Cuentagotas: el color capturado pasa a ser el color activo. Sólo se
  // aplica a un objeto cuando se pidió desde su botón de cuentagotas
  // (borde / relleno de figura); el cuentagotas general no recolorea nada.
  function pickColorAt(point) {
    const color565 = compositeColorAt(point.x, point.y);
    if (color565 === null) return false;
    setActiveColor(color565);
    addRecentColor(color565);
    colorToolEditor?.set(color565, 'Color activo');

    const field = activeShapeColorDropperTarget ? selectedField() : null;
    if (field) {
      if (['LINE', 'RECT', 'ELLIPSE', 'TRIANGLE', 'POLYGON'].includes(field.type)) {
        if (activeShapeColorDropperTarget === 'fill') {
          field.fillColor = selectedColor.value;
        } else if (activeShapeColorDropperTarget === 'border') {
          field.frameColor = selectedColor.value;
        } else if (field.fill) {
          field.fillColor = selectedColor.value;
        } else {
          field.frameColor = selectedColor.value;
        }
        if (activeShapeColorDropperTarget) {
          activeShapeColorDropperTarget = null;
          document.getElementById('shapeBorderEyedropperBtn')?.classList.remove('active');
          document.getElementById('shapeFillEyedropperBtn')?.classList.remove('active');
          selectedTool = 'pointer';
          syncToolUI();
        }
      } else if (field.type === 'RAW_TEXT') {
        field.textColor = selectedColor.value;
      } else if (['TEXT', 'VALUE', 'BOOL', 'BAR'].includes(field.type)) {
        field.valueColor = selectedColor.value;
      }
      syncInputsFromState();
    }
    return true;
  }

  const topFieldOrder = new Float64Array(WIDTH * HEIGHT);
  const topFieldIsBar = new Uint8Array(WIDTH * HEIGHT);
  let activeDrawingOrder = Infinity;

  function setBufferPixel(buffer, x, y, value) {
    if (inside(x, y)) {
      const idx = indexFor(x, y);
      buffer[idx] = value;
      if (buffer === framebuffer) {
        topFieldOrder[idx] = activeDrawingOrder;
        topFieldIsBar[idx] = 0;
      }
    }
  }

  function fillBufferRect(buffer, x, y, width, height, value) {
    const x0 = Math.max(0, x);
    const y0 = Math.max(0, y);
    const x1 = Math.min(WIDTH, x + width);
    const y1 = Math.min(HEIGHT, y + height);
    for (let py = y0; py < y1; py += 1) {
      for (let px = x0; px < x1; px += 1) setBufferPixel(buffer, px, py, value);
    }
  }

  // TFT_eSPI::fillCircle línea por línea (mismas filas que la TFT).
  function fillBufferCircle(buffer, x0, y0, r, value) {
    let x = 0;
    let dx = 1;
    let dy = r + r;
    let p = -(r >> 1);
    fillBufferRect(buffer, x0 - r, y0, dy + 1, 1, value);
    while (x < r) {
      if (p >= 0) {
        fillBufferRect(buffer, x0 - x, y0 + r, dx, 1, value);
        fillBufferRect(buffer, x0 - x, y0 - r, dx, 1, value);
        dy -= 2;
        p -= dy;
        r -= 1;
      }
      dx += 2;
      p += dx;
      x += 1;
      fillBufferRect(buffer, x0 - r, y0 + x, dy + 1, 1, value);
      fillBufferRect(buffer, x0 - r, y0 - x, dy + 1, 1, value);
    }
  }

  function drawBufferRect(buffer, x, y, width, height, value) {
    if (width <= 0 || height <= 0) return;
    fillBufferRect(buffer, x, y, width, 1, value);
    fillBufferRect(buffer, x, y + height - 1, width, 1, value);
    fillBufferRect(buffer, x, y, 1, height, value);
    fillBufferRect(buffer, x + width - 1, y, 1, height, value);
  }

  function rasterLineThick(buffer, x0, y0, x1, y1, value, size = 1) {
    if (size <= 1) {
      return rasterLineBuffer(buffer, x0, y0, x1, y1, value, 1);
    }
    const dx = x1 - x0;
    const dy = y1 - y0;
    const len = Math.hypot(dx, dy);
    if (len < 0.001) {
      const half = Math.floor(size / 2);
      for (let oy = 0; oy < size; oy++) {
        for (let ox = 0; ox < size; ox++) {
          setBufferPixel(buffer, Math.round(x0) + ox - half, Math.round(y0) + oy - half, value);
        }
      }
      return true;
    }
    const nx = -dy / len;
    const ny = dx / len;
    const h = (size - 1) / 2;

    const v = [
      { x: x0 + h * nx, y: y0 + h * ny },
      { x: x0 - h * nx, y: y0 - h * ny },
      { x: x1 - h * nx, y: y1 - h * ny },
      { x: x1 + h * nx, y: y1 + h * ny }
    ];

    const yMin = Math.max(0, Math.floor(Math.min(v[0].y, v[1].y, v[2].y, v[3].y)));
    const yMax = Math.min(HEIGHT - 1, Math.ceil(Math.max(v[0].y, v[1].y, v[2].y, v[3].y)));

    for (let y = yMin; y <= yMax; y++) {
      const xIntersects = [];
      for (let i = 0; i < 4; i++) {
        const p1 = v[i];
        const p2 = v[(i + 1) % 4];
        if (Math.abs(p1.y - p2.y) < 1e-6) continue;
        const eYMin = Math.min(p1.y, p2.y);
        const eYMax = Math.max(p1.y, p2.y);
        if (y >= eYMin - 1e-4 && y <= eYMax + 1e-4) {
          const t = (y - p1.y) / (p2.y - p1.y);
          xIntersects.push(p1.x + t * (p2.x - p1.x));
        }
      }
      if (xIntersects.length >= 2) {
        const xStart = Math.max(0, Math.floor(Math.min(...xIntersects)));
        const xEnd = Math.min(WIDTH - 1, Math.ceil(Math.max(...xIntersects)));
        for (let x = xStart; x <= xEnd; x++) {
          setBufferPixel(buffer, x, y, value);
        }
      }
    }
    return true;
  }

  function rasterLineBuffer(buffer, x0, y0, x1, y1, value, size = 1) {
    if (size > 1) {
      return rasterLineThick(buffer, x0, y0, x1, y1, value, size);
    }
    let dx = Math.abs(x1 - x0);
    const sx = x0 < x1 ? 1 : -1;
    let dy = -Math.abs(y1 - y0);
    const sy = y0 < y1 ? 1 : -1;
    let error = dx + dy;
    let changed = false;
    while (true) {
      setBufferPixel(buffer, x0, y0, value);
      changed = true;
      if (x0 === x1 && y0 === y1) break;
      const e2 = 2 * error;
      if (e2 >= dy) { error += dy; x0 += sx; }
      if (e2 <= dx) { error += dx; y0 += sy; }
    }
    return changed;
  }

  function rasterLine(x0, y0, x1, y1, value) {
    let dx = Math.abs(x1 - x0);
    const sx = x0 < x1 ? 1 : -1;
    let dy = -Math.abs(y1 - y0);
    const sy = y0 < y1 ? 1 : -1;
    let error = dx + dy;
    let changed = false;
    while (true) {
      changed = setLayerPixel(x0, y0, value) || changed;
      if (x0 === x1 && y0 === y1) break;
      const e2 = 2 * error;
      if (e2 >= dy) { error += dy; x0 += sx; }
      if (e2 <= dx) { error += dx; y0 += sy; }
    }
    return changed;
  }

  function drawShapeEllipse(buffer, a, b, color, size) {
    let x0 = a.x, y0 = a.y, x1 = b.x, y1 = b.y;
    let aDist = Math.abs(x1 - x0);
    let bDist = Math.abs(y1 - y0);
    let b1 = bDist & 1;
    let dx = 4 * (1 - aDist) * bDist * bDist;
    let dy = 4 * (b1 + 1) * aDist * aDist;
    let err = dx + dy + b1 * aDist * aDist;
    let e2;

    if (aDist === 0 || bDist === 0) {
      return rasterLineBuffer(buffer, a.x, a.y, b.x, b.y, color, size);
    }

    let left = Math.min(x0, x1), right = Math.max(x0, x1);
    let top = Math.min(y0, y1), bottom = Math.max(y0, y1);
    let curX0 = left, curX1 = right;
    let curY0 = top + Math.floor((bDist + 1) / 2), curY1 = curY0 - b1;
    aDist *= 8 * aDist;
    b1 = 8 * bDist * bDist;

    const plot = (px, py) => {
      if (!size || size <= 1) {
        setBufferPixel(buffer, px, py, color);
      } else {
        const half = Math.floor(size / 2);
        for (let oy = 0; oy < size; oy++) {
          for (let ox = 0; ox < size; ox++) {
            setBufferPixel(buffer, px + ox - half, py + oy - half, color);
          }
        }
      }
    };

    do {
      plot(curX1, curY0);
      plot(curX0, curY0);
      plot(curX0, curY1);
      plot(curX1, curY1);
      e2 = 2 * err;
      if (e2 <= dy) { curY0++; curY1--; err += dy += aDist; }
      if (e2 >= dx || 2 * err > dy) { curX0++; curX1--; err += dx += b1; }
    } while (curX0 <= curX1);

    while (curY0 - curY1 <= bDist) {
      plot(curX0 - 1, curY0);
      plot(curX1 + 1, curY0++);
      plot(curX0 - 1, curY1);
      plot(curX1 + 1, curY1--);
    }
  }

  function getPolygonVertices(field) {
    const x1 = field.x;
    const y1 = field.y;
    const x2 = field.x2 ?? field.x;
    const y2 = field.y2 ?? field.y;
    const cx = (x1 + x2) / 2;
    const cy = (y1 + y2) / 2;
    const rx = Math.abs(x2 - x1) / 2;
    const ry = Math.abs(y2 - y1) / 2;
    const numSides = Math.max(3, Math.min(12, Math.trunc(Number(field.sides) || 5)));
    const pts = [];
    for (let i = 0; i < numSides; i++) {
      const angle = (i * 2 * Math.PI / numSides) - Math.PI / 2;
      pts.push({
        x: Math.round(cx + rx * Math.cos(angle)),
        y: Math.round(cy + ry * Math.sin(angle))
      });
    }
    return pts;
  }

  function getShapeBaseVertices(field) {
    if (!field) return [];
    const x1 = field.x ?? 0; const y1 = field.y ?? 0;
    const x2 = field.x2 ?? x1; const y2 = field.y2 ?? y1;
    if (field.type === 'TRIANGLE') {
      const topPt = { x: Math.round((x1 + x2) / 2), y: y1 };
      const bl = { x: x1, y: y2 };
      const br = { x: x2, y: y2 };
      return [topPt, br, bl];
    }
    if (field.type === 'POLYGON') {
      return getPolygonVertices(field);
    }
    if (field.type === 'RECT') {
      const left = Math.min(x1, x2);
      const right = Math.max(x1, x2);
      const top = Math.min(y1, y2);
      const bottom = Math.max(y1, y2);
      return [
        { x: left, y: top },
        { x: right, y: top },
        { x: right, y: bottom },
        { x: left, y: bottom }
      ];
    }
    return [];
  }

  function insetPolygon(pts, s) {
    if (!Array.isArray(pts) || pts.length < 3 || s <= 0) return pts ? [...pts] : null;
    const n = pts.length;
    let cx = 0, cy = 0;
    for (const p of pts) { cx += p.x; cy += p.y; }
    cx /= n; cy /= n;

    const lines = [];
    for (let i = 0; i < n; i++) {
      const p1 = pts[i];
      const p2 = pts[(i + 1) % n];
      const dx = p2.x - p1.x;
      const dy = p2.y - p1.y;
      const len = Math.hypot(dx, dy);
      if (len < 1e-4) return null;
      const ux = dx / len;
      const uy = dy / len;
      let nx = -uy, ny = ux;
      const midX = (p1.x + p2.x) / 2, midY = (p1.y + p2.y) / 2;
      if (nx * (cx - midX) + ny * (cy - midY) < 0) {
        nx = -nx; ny = -ny;
      }
      lines.push({ p: { x: p1.x + s * nx, y: p1.y + s * ny }, u: { x: ux, y: uy } });
    }

    const out = [];
    for (let i = 0; i < n; i++) {
      const prev = lines[(i - 1 + n) % n];
      const curr = lines[i];
      const det = prev.u.x * (-curr.u.y) - prev.u.y * (-curr.u.x);
      if (Math.abs(det) < 1e-5) {
        out.push({ x: curr.p.x, y: curr.p.y });
        continue;
      }
      const dx = curr.p.x - prev.p.x;
      const dy = curr.p.y - prev.p.y;
      const t = (dx * (-curr.u.y) - dy * (-curr.u.x)) / det;
      const pt = {
        x: prev.p.x + t * prev.u.x,
        y: prev.p.y + t * prev.u.y
      };
      if ((pt.x - cx) * (pts[i].x - cx) + (pt.y - cy) * (pts[i].y - cy) < -1e-3) {
        return null;
      }
      out.push(pt);
    }
    return out;
  }

  function getPolygonScanlineSpan(pts, y) {
    if (!pts || pts.length < 3) return null;
    const n = pts.length;
    let minX = Infinity, maxX = -Infinity;
    let count = 0;
    for (let i = 0; i < n; i++) {
      const p1 = pts[i];
      const p2 = pts[(i + 1) % n];
      if ((p1.y <= y && p2.y > y) || (p2.y <= y && p1.y > y)) {
        const x = p1.x + ((y - p1.y) * (p2.x - p1.x)) / (p2.y - p1.y);
        if (x < minX) minX = x;
        if (x > maxX) maxX = x;
        count++;
      } else if (p1.y === y && p2.y === y) {
        const xL = Math.min(p1.x, p2.x);
        const xR = Math.max(p1.x, p2.x);
        if (xL < minX) minX = xL;
        if (xR > maxX) maxX = xR;
        count += 2;
      }
    }
    if (count >= 2 && Number.isFinite(minX) && Number.isFinite(maxX)) {
      return { left: Math.round(minX), right: Math.round(maxX) };
    }
    return null;
  }

  function fillShapeEllipse(buffer, a, b, color) {
    const minX = Math.min(a.x, b.x);
    const maxX = Math.max(a.x, b.x);
    const minY = Math.min(a.y, b.y);
    const maxY = Math.max(a.y, b.y);
    const rx = (maxX - minX) / 2;
    const ry = (maxY - minY) / 2;
    if (rx <= 0 || ry <= 0) return;
    const cx = (minX + maxX) / 2;
    const cy = (minY + maxY) / 2;

    const startY = Math.max(0, Math.ceil(minY));
    const endY = Math.min(HEIGHT - 1, Math.floor(maxY));
    for (let y = startY; y <= endY; y++) {
      const dy = (y - cy) / ry;
      const term = 1 - dy * dy;
      if (term < 0) continue;
      const dx = Math.round(rx * Math.sqrt(term));
      const left = Math.max(0, Math.round(cx - dx));
      const right = Math.min(WIDTH - 1, Math.round(cx + dx));
      if (right >= left) {
        fillBufferRect(buffer, left, y, right - left + 1, 1, color);
      }
    }
  }

  function fillShapeTriangle(buffer, p1, p2, p3, color) {
    const pts = [p1, p2, p3].sort((m, n) => m.y - n.y);
    const [v1, v2, v3] = pts;
    if (v1.y === v3.y) return;

    function interpolateX(y, ya, yb, xa, xb) {
      if (ya === yb) return xa;
      return xa + ((y - ya) * (xb - xa)) / (yb - ya);
    }

    const startY = Math.max(0, Math.ceil(v1.y));
    const endY = Math.min(HEIGHT - 1, Math.floor(v3.y));

    for (let y = startY; y <= endY; y++) {
      const xA = interpolateX(y, v1.y, v3.y, v1.x, v3.x);
      const xB = (y < v2.y)
        ? interpolateX(y, v1.y, v2.y, v1.x, v2.x)
        : interpolateX(y, v2.y, v3.y, v2.x, v3.x);

      const left = Math.max(0, Math.round(Math.min(xA, xB)));
      const right = Math.min(WIDTH - 1, Math.round(Math.max(xA, xB)));
      if (right >= left) {
        fillBufferRect(buffer, left, y, right - left + 1, 1, color);
      }
    }
  }

  function fillShapePolygon(buffer, pts, color) {
    if (!pts || pts.length < 3) return;
    let minY = Infinity, maxY = -Infinity;
    for (const p of pts) {
      if (p.y < minY) minY = p.y;
      if (p.y > maxY) maxY = p.y;
    }
    const startY = Math.max(0, Math.ceil(minY));
    const endY = Math.min(HEIGHT - 1, Math.floor(maxY));
    const n = pts.length;

    for (let y = startY; y <= endY; y++) {
      const nodes = [];
      for (let i = 0; i < n; i++) {
        const pA = pts[i];
        const pB = pts[(i + 1) % n];
        if ((pA.y <= y && pB.y > y) || (pB.y <= y && pA.y > y)) {
          const x = pA.x + ((y - pA.y) * (pB.x - pA.x)) / (pB.y - pA.y);
          nodes.push(x);
        }
      }
      nodes.sort((a, b) => a - b);
      for (let i = 0; i < nodes.length; i += 2) {
        if (i + 1 >= nodes.length) break;
        const left = Math.max(0, Math.round(nodes[i]));
        const right = Math.min(WIDTH - 1, Math.round(nodes[i + 1]));
        if (right >= left) {
          fillBufferRect(buffer, left, y, right - left + 1, 1, color);
        }
      }
    }
  }

  function isPointInPolygon(px, py, pts) {
    if (!pts || pts.length < 3) return false;
    let inside = false;
    const n = pts.length;
    for (let i = 0, j = n - 1; i < n; j = i++) {
      const xi = pts[i].x, yi = pts[i].y;
      const xj = pts[j].x, yj = pts[j].y;
      const intersect = ((yi > py) !== (yj > py)) &&
        (px < (xj - xi) * (py - yi) / (yj - yi) + xi);
      if (intersect) inside = !inside;
    }
    return inside;
  }

  function distToPolygonBoundary(px, py, pts) {
    if (!pts || pts.length < 2) return Infinity;
    let minDist = Infinity;
    const n = pts.length;
    for (let i = 0; i < n; i++) {
      const p1 = pts[i];
      const p2 = pts[(i + 1) % n];
      const d = distToSegment(px, py, p1.x, p1.y, p2.x, p2.y);
      if (d < minDist) minDist = d;
    }
    return minDist;
  }

  function isPointInsideShapeArea(px, py, field) {
    if (!field) return false;
    if (field.type === 'LINE') {
      const strokeSize = field.borderEnabled !== false ? Math.max(1, Math.round(field.size || 1)) : 1;
      const x1 = field.x ?? 0; const y1 = field.y ?? 0;
      const x2 = field.x2 ?? x1; const y2 = field.y2 ?? y1;
      return distToSegment(px, py, x1, y1, x2, y2) <= Math.max(3.5, strokeSize / 2 + 2.0);
    }
    const pts = getShapeTransformedVertices(field);
    if (pts && pts.length >= 3) {
      return isPointInPolygon(px, py, pts);
    }
    return false;
  }

  function drawShapeRectScanline(buffer, field, frameColor, fillColor, strokeSize, hasBorder, hasFill) {
    const x1 = field.x ?? 0; const y1 = field.y ?? 0;
    const x2 = field.x2 ?? x1; const y2 = field.y2 ?? y1;
    const left = Math.min(x1, x2);
    const right = Math.max(x1, x2);
    const top = Math.min(y1, y2);
    const bottom = Math.max(y1, y2);
    const W = right - left + 1;
    const H = bottom - top + 1;
    if (W <= 0 || H <= 0) return;

    const maxR = Math.floor(Math.min(W, H) / 2);
    const R = clamp(Math.round(Number(field.cornerRadius) || 0), 0, maxR);
    const effStroke = hasBorder ? Math.min(strokeSize, maxR > 0 ? maxR : Math.floor(Math.min(W, H) / 2) || 1) : 0;
    const borderOn = hasBorder && effStroke > 0;

    const inLeft = left + effStroke;
    const inRight = right - effStroke;
    const inTop = top + effStroke;
    const inBottom = bottom - effStroke;
    const inR = Math.max(0, R - effStroke);
    const hasInner = borderOn ? (inLeft <= inRight && inTop <= inBottom) : true;

    const yStart = Math.max(0, top);
    const yEnd = Math.min(HEIGHT - 1, bottom);

    for (let y = yStart; y <= yEnd; y++) {
      let dxOut = 0;
      if (R > 0) {
        if (y < top + R) {
          const dy = (top + R) - y;
          dxOut = R - Math.round(Math.sqrt(Math.max(0, R * R - dy * dy)));
        } else if (y > bottom - R) {
          const dy = y - (bottom - R);
          dxOut = R - Math.round(Math.sqrt(Math.max(0, R * R - dy * dy)));
        }
      }
      const xOutL = Math.max(0, left + dxOut);
      const xOutR = Math.min(WIDTH - 1, right - dxOut);
      if (xOutL > xOutR) continue;

      if (!borderOn) {
        if (hasFill) {
          for (let x = xOutL; x <= xOutR; x++) setBufferPixel(buffer, x, y, fillColor);
        }
        continue;
      }

      let innerValid = false;
      let xInL = 0, xInR = -1;
      if (hasInner && y >= inTop && y <= inBottom) {
        let dxIn = 0;
        if (inR > 0) {
          if (y < inTop + inR) {
            const dy = (inTop + inR) - y;
            dxIn = inR - Math.round(Math.sqrt(Math.max(0, inR * inR - dy * dy)));
          } else if (y > inBottom - inR) {
            const dy = y - (inBottom - inR);
            dxIn = inR - Math.round(Math.sqrt(Math.max(0, inR * inR - dy * dy)));
          }
        }
        xInL = Math.max(0, inLeft + dxIn);
        xInR = Math.min(WIDTH - 1, inRight - dxIn);
        if (xInL <= xInR && xInL >= xOutL && xInR <= xOutR) {
          innerValid = true;
        }
      }

      if (!innerValid) {
        for (let x = xOutL; x <= xOutR; x++) setBufferPixel(buffer, x, y, frameColor);
      } else {
        for (let x = xOutL; x < xInL; x++) setBufferPixel(buffer, x, y, frameColor);
        if (hasFill) {
          for (let x = xInL; x <= xInR; x++) setBufferPixel(buffer, x, y, fillColor);
        }
        for (let x = xInR + 1; x <= xOutR; x++) setBufferPixel(buffer, x, y, frameColor);
      }
    }
  }

  function drawShapeEllipseScanline(buffer, field, frameColor, fillColor, strokeSize, hasBorder, hasFill) {
    const x1 = field.x ?? 0; const y1 = field.y ?? 0;
    const x2 = field.x2 ?? x1; const y2 = field.y2 ?? y1;
    const left = Math.min(x1, x2);
    const right = Math.max(x1, x2);
    const top = Math.min(y1, y2);
    const bottom = Math.max(y1, y2);
    const W = right - left + 1;
    const H = bottom - top + 1;
    if (W <= 0 || H <= 0) return;

    const cx = (left + right) / 2;
    const cy = (top + bottom) / 2;
    const rx = (right - left) / 2;
    const ry = (bottom - top) / 2;
    if (rx < 0.5 || ry < 0.5) return;

    const effStroke = hasBorder ? Math.min(strokeSize, Math.floor(Math.min(rx, ry)) || 1) : 0;
    const borderOn = hasBorder && effStroke > 0;
    const inRx = Math.max(0, rx - effStroke);
    const inRy = Math.max(0, ry - effStroke);

    const yStart = Math.max(0, Math.floor(top));
    const yEnd = Math.min(HEIGHT - 1, Math.ceil(bottom));

    for (let y = yStart; y <= yEnd; y++) {
      const dy = y - cy;
      if (Math.abs(dy) > ry) continue;
      const factorOut = Math.max(0, 1 - (dy * dy) / (ry * ry));
      const dxOut = rx * Math.sqrt(factorOut);
      const xOutL = Math.max(0, Math.round(cx - dxOut));
      const xOutR = Math.min(WIDTH - 1, Math.round(cx + dxOut));
      if (xOutL > xOutR) continue;

      if (!borderOn) {
        if (hasFill) {
          for (let x = xOutL; x <= xOutR; x++) setBufferPixel(buffer, x, y, fillColor);
        }
        continue;
      }

      let innerValid = false;
      let xInL = 0, xInR = -1;
      if (inRx > 0 && inRy > 0 && Math.abs(dy) < inRy) {
        const factorIn = Math.max(0, 1 - (dy * dy) / (inRy * inRy));
        const dxIn = inRx * Math.sqrt(factorIn);
        xInL = Math.max(0, Math.round(cx - dxIn));
        xInR = Math.min(WIDTH - 1, Math.round(cx + dxIn));
        if (xInL <= xInR && xInL >= xOutL && xInR <= xOutR) {
          innerValid = true;
        }
      }

      if (!innerValid) {
        for (let x = xOutL; x <= xOutR; x++) setBufferPixel(buffer, x, y, frameColor);
      } else {
        for (let x = xOutL; x < xInL; x++) setBufferPixel(buffer, x, y, frameColor);
        if (hasFill) {
          for (let x = xInL; x <= xInR; x++) setBufferPixel(buffer, x, y, fillColor);
        }
        for (let x = xInR + 1; x <= xOutR; x++) setBufferPixel(buffer, x, y, frameColor);
      }
    }
  }

  // Port de TFT_eSPI::fillTriangle / jwplcHMIFillTriangle: mismas filas y
  // extremos que pinta el firmware (división entera truncada como en C).
  function fillBufferTriangleTft(buffer, x0, y0, x1, y1, x2, y2, value) {
    if (y0 > y1) { [y0, y1] = [y1, y0]; [x0, x1] = [x1, x0]; }
    if (y1 > y2) { [y2, y1] = [y1, y2]; [x2, x1] = [x1, x2]; }
    if (y0 > y1) { [y0, y1] = [y1, y0]; [x0, x1] = [x1, x0]; }

    if (y0 === y2) {
      const a = Math.min(x0, x1, x2);
      const b = Math.max(x0, x1, x2);
      fillBufferRect(buffer, a, y0, b - a + 1, 1, value);
      return;
    }

    const dx01 = x1 - x0, dy01 = y1 - y0;
    const dx02 = x2 - x0, dy02 = y2 - y0;
    const dx12 = x2 - x1, dy12 = y2 - y1;
    let sa = 0;
    let sb = 0;
    const last = (y1 === y2) ? y1 : y1 - 1;
    let y = y0;

    for (; y <= last; y++) {
      let a = x0 + Math.trunc(sa / dy01);
      let b = x0 + Math.trunc(sb / dy02);
      sa += dx01;
      sb += dx02;
      if (a > b) [a, b] = [b, a];
      fillBufferRect(buffer, a, y, b - a + 1, 1, value);
    }

    sa = dx12 * (y - y1);
    sb = dx02 * (y - y0);
    for (; y <= y2; y++) {
      let a = x1 + Math.trunc(sa / dy12);
      let b = x0 + Math.trunc(sb / dy02);
      sa += dx12;
      sb += dx02;
      if (a > b) [a, b] = [b, a];
      fillBufferRect(buffer, a, y, b - a + 1, 1, value);
    }
  }

  // Polígono cerrado (TRIANGLE, POLYGON, RECT/ELLIPSE rotados) dibujado igual
  // que el código generado (designer-codegen.js:staticDrawCode): relleno en
  // abanico de fillTriangle desde el primer vértice y borde con drawLine entre
  // vértices consecutivos, todo sobre vértices enteros.
  function drawShapePolygonScanline(buffer, pts, frameColor, fillColor, strokeSize, hasBorder, hasFill, field = null) {
    if (!pts || pts.length < 3) return;
    const vOuter = pts.map((p) => ({ x: Math.round(p.x), y: Math.round(p.y) }));
    const n = vOuter.length;

    const effStroke = hasBorder ? Math.max(1, Math.round(strokeSize || 1)) : 0;
    const borderOn = hasBorder && effStroke > 0;

    let vInner = null;
    if (borderOn) {
      if (field && (field.type === 'TRIANGLE' || field.type === 'POLYGON' || field.type === 'RECT')) {
        const basePts = getShapeBaseVertices(field);
        const r = clamp(Math.round(Number(field.cornerRadius) || 0), 0, getMaxCornerRadius(field));
        const x1 = field.x ?? 0; const y1 = field.y ?? 0;
        const x2 = field.x2 ?? x1; const y2 = field.y2 ?? y1;
        const cx = (x1 + x2) / 2; const cy = (y1 + y2) / 2;
        const angleDeg = Number(field.rotation) || 0;
        const flipH = Boolean(field.flipH); const flipV = Boolean(field.flipV);
        const tf = (p) => transformShapePoint(p.x, p.y, cx, cy, angleDeg, flipH, flipV);
        const baseInner = insetPolygon(basePts, effStroke);
        vInner = baseInner ? roundShapePoints(buildRoundedPolygonPoints(baseInner, Math.max(0, r - effStroke)).map(tf)) : null;
      } else {
        vInner = insetPolygon(vOuter, effStroke);
      }
    }

    if (!borderOn) {
      if (hasFill) {
        for (let i = 1; i < n - 1; i++) {
          fillBufferTriangleTft(buffer, vOuter[0].x, vOuter[0].y, vOuter[i].x, vOuter[i].y, vOuter[i + 1].x, vOuter[i + 1].y, fillColor);
        }
      }
      return;
    }

    if (effStroke === 1) {
      if (hasFill) {
        for (let i = 1; i < n - 1; i++) {
          fillBufferTriangleTft(buffer, vOuter[0].x, vOuter[0].y, vOuter[i].x, vOuter[i].y, vOuter[i + 1].x, vOuter[i + 1].y, fillColor);
        }
      }
      for (let i = 0; i < n; i++) {
        const a = vOuter[i];
        const b = vOuter[(i + 1) % n];
        rasterLineBuffer(buffer, a.x, a.y, b.x, b.y, frameColor, 1);
      }
      return;
    }

    let minY = Infinity, maxY = -Infinity;
    for (const p of vOuter) {
      if (p.y < minY) minY = p.y;
      if (p.y > maxY) maxY = p.y;
    }
    const yStart = Math.max(0, Math.floor(minY));
    const yEnd = Math.min(HEIGHT - 1, Math.ceil(maxY));

    for (let y = yStart; y <= yEnd; y++) {
      const spanOut = getPolygonScanlineSpan(vOuter, y);
      if (!spanOut) continue;
      const xOutL = Math.max(0, spanOut.left);
      const xOutR = Math.min(WIDTH - 1, spanOut.right);
      if (xOutL > xOutR) continue;

      const spanIn = vInner ? getPolygonScanlineSpan(vInner, y) : null;
      let innerValid = false;
      let xInL = 0, xInR = -1;
      if (spanIn && spanIn.left <= spanIn.right) {
        xInL = Math.max(0, spanIn.left);
        xInR = Math.min(WIDTH - 1, spanIn.right);
        if (xInL <= xInR && xInL >= xOutL && xInR <= xOutR) {
          innerValid = true;
        }
      }

      if (!innerValid) {
        fillBufferRect(buffer, xOutL, y, xOutR - xOutL + 1, 1, frameColor);
      } else {
        if (xInL > xOutL) {
          fillBufferRect(buffer, xOutL, y, xInL - xOutL, 1, frameColor);
        }
        if (hasFill && xInR >= xInL) {
          fillBufferRect(buffer, xInL, y, xInR - xInL + 1, 1, fillColor);
        }
        if (xOutR > xInR) {
          fillBufferRect(buffer, xInR + 1, y, xOutR - xInR, 1, frameColor);
        }
      }
    }

    for (let i = 0; i < n; i++) {
      const a = vOuter[i];
      const b = vOuter[(i + 1) % n];
      rasterLineBuffer(buffer, a.x, a.y, b.x, b.y, frameColor, 1);
    }
  }

  function drawShape(buffer, field) {
    const hasBorder = field.borderEnabled !== false;
    const hasFill = Boolean(field.fill);
    if (!hasBorder && !hasFill) return;

    const color = field.frameColor || 0xFFFF;
    const fillColor = field.fillColor ?? color;
    const size = Math.max(1, Math.round(field.size || 1));
    const angleDeg = Number(field.rotation) || 0;

    if (field.type === 'LINE') {
      if (hasBorder) {
        const x1 = field.x ?? 0; const y1 = field.y ?? 0;
        const x2 = field.x2 ?? x1; const y2 = field.y2 ?? y1;
        rasterLineThick(buffer, x1, y1, x2, y2, color, size);
      }
      return;
    }

    if (field.type === 'RECT' && angleDeg === 0) {
      drawShapeRectScanline(buffer, field, color, fillColor, size, hasBorder, hasFill);
      return;
    }

    if (field.type === 'ELLIPSE' && angleDeg === 0) {
      drawShapeEllipseScanline(buffer, field, color, fillColor, size, hasBorder, hasFill);
      return;
    }

    const pts = getShapeTransformedVertices(field);
    if (!pts || pts.length === 0) return;

    if (field.type === 'RECT' || field.type === 'TRIANGLE' || field.type === 'POLYGON' || field.type === 'ELLIPSE') {
      drawShapePolygonScanline(buffer, pts, color, fillColor, size, hasBorder, hasFill, field);
      return;
    }
  }

  function drawClassicChar(buffer, x, y, charCode, foreground, background, size) {
    const font = window.JWPLCGfxClassicFont;
    const glyph = font.glyphFor(charCode);
    const scale = Math.max(1, Math.trunc(size));
    const isTransparent = (background === null || background === undefined || background === -1 || background === 'TRANSPARENT');
    for (let column = 0; column < font.cellWidth; column += 1) {
      const bits = column < font.bytesPerGlyph ? glyph[column] : 0;
      for (let row = 0; row < font.cellHeight; row += 1) {
        const on = column < font.bytesPerGlyph && ((bits >> row) & 0x01) !== 0;
        if (on) {
          fillBufferRect(buffer, x + column * scale, y + row * scale, scale, scale, foreground);
        } else if (!isTransparent) {
          fillBufferRect(buffer, x + column * scale, y + row * scale, scale, scale, background);
        }
      }
    }
  }

  function drawClassicTextAt(buffer, text, x, y, foreground, background, size) {
    if (!text) return;
    const font = window.JWPLCGfxClassicFont;
    const scale = Math.max(1, Math.trunc(size));
    let cursorX = x;
    let cursorY = y;
    for (const character of text) {
      if (character === '\n') { cursorX = x; cursorY += font.cellHeight * scale; continue; }
      if (character === '\r') continue;
      drawClassicChar(buffer, cursorX, cursorY, character.codePointAt(0), foreground, background, scale);
      cursorX += font.cellWidth * scale;
    }
  }

  function drawRawTextAt(buffer, text, x, y, foreground, background, size, fontId) {
    if (!text) return;
    const isTransparent = (background === null || background === undefined || background === -1 || background === 'TRANSPARENT');

    // Primero calculamos los bounds del texto (sin padding).
    const tb = rawTextBounds(text, size, fontId);

    // Fondo opaco: un unico fillRect con RAW_BG_PAD px de margen en todos los
    // lados, antes de pintar los glifos. Esto es uniforme para GLCD y FreeFonts.
    if (!isTransparent && tb.width > 0 && tb.height > 0) {
      fillBufferRect(
        buffer,
        x - RAW_BG_PAD, y - RAW_BG_PAD,
        tb.width  + RAW_BG_PAD * 2,
        tb.height + RAW_BG_PAD * 2,
        background
      );
    }

    // Ahora pintamos los glifos con fondo transparente (ya lo manejamos arriba).
    const fonts = window.JWPLCDesignerFonts;
    const font = fonts ? fonts.get(fonts.effectiveId(fontId)) : null;
    if (!font || font.kind === 'glcd') {
      drawClassicTextAt(buffer, text, x, y, foreground, null, size);
      return;
    }
    const mask = fonts.rasterize(font, text, size);
    for (let row = 0; row < mask.height; row += 1) {
      for (let column = 0; column < mask.width; column += 1) {
        if (mask.data[row * mask.width + column]) {
          fillBufferRect(buffer, x + column, y + row, 1, 1, foreground);
        }
      }
    }
  }

  function rawTextBounds(text, size, fontId) {
    const fonts = window.JWPLCDesignerFonts;
    const font = fonts ? fonts.get(fonts.effectiveId(fontId)) : null;
    if (!font || font.kind === 'glcd') return nominalTextBounds(text, size);
    if (!text) return { width: 0, height: 0 };
    return { width: fonts.measureWidth(font, text, size), height: fonts.lineHeight(font, size) };
  }

  function nominalTextBounds(text, size) {
    if (!text) return { width: 0, height: 0 };
    const scale = Math.max(1, Math.trunc(size || 1));
    return { width: text.length * 6 * scale - scale, height: 7 * scale };
  }

  // BOOL con colores por estado (texto del valor y fondo de todo el campo),
  // igual que JWPLC_UI.cpp con JWPLC_UIBoolText de 6 argumentos.
  // BOOL «Piloto» (JWPLC_UIBoolPilot): círculo en lugar del texto.
  function isBoolPilot(field) {
    return field?.type === 'BOOL' && field.boolStyle === 'PILOT';
  }

  function fieldValueColor565(field) {
    if (field.type === 'BOOL' && field.stateColors) {
      return field.boolValue ? Number(field.trueColor) : Number(field.falseColor);
    }
    return field.valueColor;
  }

  function fieldValueBackground565(field) {
    if (field.type === 'BOOL' && field.stateColors) {
      const bg = field.boolValue ? field.trueBackground : field.falseBackground;
      return Number.isFinite(Number(bg)) ? Number(bg) : Number(field.backgroundColor) || 0;
    }
    return null;
  }

  // Margen derecho/inferior (JWPLC_UI.cpp:effectiveFieldPadding): el fondo del
  // texto ocupa la celda 6×8 completa (una escala más que el cuerpo 5×7) y la
  // línea del marco 1 px más, así que pad >= escala + 1.
  function effectiveFieldPadding(field) {
    return Math.max(FIELD_PADDING, (field.labelSize || 1) + 1, (field.valueSize || 1) + 1);
  }

  // Formato de VALUE: designer-value-format.js, gemelo de JWPLC_UI_Format.cpp
  // (número, separador de miles y duración; tests/value-format-parity.cjs).
  const valueFormat = () => window.JWPLCValueFormat;

  function makeNumericSample(field) {
    return valueFormat().sample(field);
  }

  function formatNumericPreview(field) {
    return valueFormat().preview(field);
  }

  function valueSampleForField(field) {
    if (field.type === 'VALUE') return makeNumericSample(field);
    return 'W'.repeat(Math.max(1, field.capacity || 1));
  }

  function previewTextForField(field) {
    if (field.type === 'VALUE') return formatNumericPreview(field);
    return String(field.preview || '').slice(0, Math.max(1, field.capacity || 1));
  }

  function getFieldBounds(field) {
    if (!field) return { x: 0, y: 0, minX: 0, maxX: 0, minY: 0, maxY: 0, width: 0, height: 0 };
    if (field.type === 'RAW_TEXT') {
      const tb = rawTextBounds(field.text || '', field.size || 1, field.font);
      const w = Math.max(6, tb.width);
      const h = Math.max(7, tb.height);
      const pad = field.transparentBackground ? 0 : RAW_BG_PAD;
      const minX = field.x - pad;
      const minY = field.y - pad;
      return {
        x: minX,
        y: minY,
        minX,
        maxX: field.x + w - 1 + pad,
        minY,
        maxY: field.y + h - 1 + pad,
        width:  w + pad * 2,
        height: h + pad * 2
      };
    }

    if (field.type === 'MENU' || field.type === 'EVENT_LOG') {
      const x = Number(field.x) || 0;
      const y = Number(field.y) || 0;
      const w = Math.max(1, Number(field.width) || (field.type === 'EVENT_LOG' ? 320 : 1));
      const h = Math.max(1, Number(field.height) || (field.type === 'EVENT_LOG' ? 170 : 1));
      return { x, y, minX: x, maxX: x + w - 1, minY: y, maxY: y + h - 1, width: w, height: h };
    }

    if (field.type === 'NAV_BAR') {
      const isVert = field.position === 'left' || field.position === 'right' || field.orientation === 'vertical';
      const defaultW = isVert ? 22 : 320;
      const defaultH = isVert ? 170 : 22;

      let defaultX = 0;
      if (field.position === 'right') defaultX = 320 - (Number(field.width) || defaultW) - 2;
      else if (field.position === 'left') defaultX = 2;

      let defaultY = 0;
      if (field.position === 'top') defaultY = 2;
      else if (field.position === 'bottom') defaultY = 170 - (Number(field.height) || defaultH) - 1;

      const w = Number(field.width) || defaultW;
      const h = Number(field.height) || defaultH;
      const x = field.x !== undefined ? Number(field.x) : defaultX;
      const y = field.y !== undefined ? Number(field.y) : defaultY;
      return {
        x,
        y,
        minX: x,
        maxX: x + w - 1,
        minY: y,
        maxY: y + h - 1,
        width: w,
        height: h
      };
    }

    if (['LINE', 'RECT', 'ELLIPSE', 'TRIANGLE', 'POLYGON'].includes(field.type)) {
      if (field.type === 'LINE') {
        const x1 = field.x ?? 0; const y1 = field.y ?? 0;
        const x2 = field.x2 ?? x1; const y2 = field.y2 ?? y1;
        const strokeSize = field.borderEnabled !== false ? Math.max(1, Math.round(field.size || 1)) : 1;
        const dx = x2 - x1;
        const dy = y2 - y1;
        const len = Math.hypot(dx, dy);
        let minX, maxX, minY, maxY;
        if (len < 0.001) {
          const h = Math.floor(strokeSize / 2);
          minX = x1 - h; maxX = x1 + h;
          minY = y1 - h; maxY = y1 + h;
        } else {
          const nx = -dy / len;
          const ny = dx / len;
          const h = (strokeSize - 1) / 2;
          const v = [
            { x: x1 + h * nx, y: y1 + h * ny },
            { x: x1 - h * nx, y: y1 - h * ny },
            { x: x2 - h * nx, y: y2 - h * ny },
            { x: x2 + h * nx, y: y2 + h * ny }
          ];
          minX = Math.floor(Math.min(v[0].x, v[1].x, v[2].x, v[3].x));
          maxX = Math.ceil(Math.max(v[0].x, v[1].x, v[2].x, v[3].x));
          minY = Math.floor(Math.min(v[0].y, v[1].y, v[2].y, v[3].y));
          maxY = Math.ceil(Math.max(v[0].y, v[1].y, v[2].y, v[3].y));
        }
        return {
          x: minX,
          y: minY,
          minX,
          maxX,
          minY,
          maxY,
          width: Math.max(1, maxX - minX + 1),
          height: Math.max(1, maxY - minY + 1)
        };
      }
      const angleDeg = Number(field.rotation) || 0;
      const lb = getShapeLocalBounds(field);
      if (angleDeg === 0) {
        return {
          x: lb.minX,
          y: lb.minY,
          minX: lb.minX,
          maxX: lb.maxX,
          minY: lb.minY,
          maxY: lb.maxY,
          width: Math.max(1, lb.maxX - lb.minX + 1),
          height: Math.max(1, lb.maxY - lb.minY + 1)
        };
      }
      const x1 = field.x ?? 0; const y1 = field.y ?? 0;
      const x2 = field.x2 ?? x1; const y2 = field.y2 ?? y1;
      const cx = (x1 + x2) / 2; const cy = (y1 + y2) / 2;
      const flipH = Boolean(field.flipH); const flipV = Boolean(field.flipV);
      const corners = [
        [lb.minX, lb.minY], [lb.maxX, lb.minY], [lb.maxX, lb.maxY], [lb.minX, lb.maxY]
      ].map(([px, py]) => transformShapePoint(px, py, cx, cy, angleDeg, flipH, flipV));
      let minX = Infinity, maxX = -Infinity, minY = Infinity, maxY = -Infinity;
      for (const p of corners) {
        if (p.x < minX) minX = p.x;
        if (p.x > maxX) maxX = p.x;
        if (p.y < minY) minY = p.y;
        if (p.y > maxY) maxY = p.y;
      }
      if (!Number.isFinite(minX)) {
        minX = field.x ?? 0; maxX = minX; minY = field.y ?? 0; maxY = minY;
      }
      return {
        x: minX,
        y: minY,
        minX,
        maxX,
        minY,
        maxY,
        width: Math.max(1, maxX - minX + 1),
        height: Math.max(1, maxY - minY + 1)
      };
    }
    const g = computeFieldGeometry(field);
    return {
      x: g.fieldX,
      y: g.fieldY,
      minX: g.fieldX,
      maxX: g.fieldX + g.fieldW - 1,
      minY: g.fieldY,
      maxY: g.fieldY + g.fieldH - 1,
      width: g.fieldW,
      height: g.fieldH
    };
  }

  function computeFieldGeometry(field) {
    if (!field) return null;
    if (field.type === 'NAV_BAR' || field.type === 'MENU') {
      const b = getFieldBounds(field);
      return {
        pad: 0,
        fieldX: b.minX,
        fieldY: b.minY,
        fieldW: b.width,
        fieldH: b.height,
        valueX: 0,
        valueY: 0,
        valueW: 0,
        valueH: 0,
        labelBounds: { width: 0, height: 0 },
        unitBounds: { width: 0, height: 0 }
      };
    }
    if (field.type === 'BAR') {
      if (window.JWPLCHMIBar?.computeGeometry) {
        return window.JWPLCHMIBar.computeGeometry(field);
      }
      const pad = effectiveFieldPadding(field);
      const w = Math.max(24, Number(field.barWidth) || 110);
      const h = Math.max(12, Number(field.barHeight) || 12);
      return {
        pad,
        fieldX: field.x,
        fieldY: field.y,
        fieldW: w + 2 * pad,
        fieldH: h + 2 * pad,
        valueX: field.x + pad,
        valueY: field.y + pad,
        valueW: w,
        valueH: h,
        labelBounds: { width: 0, height: 0 },
        unitBounds: { width: 0, height: 0 }
      };
    }
    if (['LINE', 'RECT', 'ELLIPSE', 'TRIANGLE', 'POLYGON', 'RAW_TEXT'].includes(field.type)) {
      const b = getFieldBounds(field);
      return {
        pad: 0,
        fieldX: b.minX,
        fieldY: b.minY,
        fieldW: b.width,
        fieldH: b.height,
        valueX: 0,
        valueY: 0,
        valueW: 0,
        valueH: 0,
        labelBounds: { width: 0, height: 0 },
        unitBounds: { width: 0, height: 0 }
      };
    }
    // Igual que JWPLC_UI.cpp: margen superior/izquierdo fijo (lead) y el extra
    // de escalas grandes sólo a la derecha/abajo (pad), para que etiqueta y
    // valor no se muevan al pasar a 4×.
    const lead = FIELD_PADDING;
    const pad = effectiveFieldPadding(field);
    const labelBounds = nominalTextBounds(field.label, field.labelSize);
    const unitBounds = nominalTextBounds(field.unit, field.labelSize);
    const pilotSide = 7 * Math.max(1, Math.trunc(Number(field.valueSize) || 1));
    const icon = field.type === 'ICON' ? iconEntry(field) : null;
    const valueBounds = field.type === 'ICON'
      ? (icon ? { width: icon.width, height: icon.height } : { width: 0, height: 7 })
      : isBoolPilot(field)
        ? { width: pilotSide, height: pilotSide }
        : nominalTextBounds(valueSampleForField(field), field.valueSize);
    let fieldW;
    let fieldH;
    let valueX;
    let valueY;

    if (field.layout === 'STACKED') {
      const valueAndUnitW = valueBounds.width + (unitBounds.width > 0 ? FIELD_GAP + unitBounds.width : 0);
      fieldW = lead + pad + Math.max(labelBounds.width, valueAndUnitW);
      fieldH = lead + pad + labelBounds.height + (labelBounds.height > 0 ? FIELD_GAP : 0) + Math.max(valueBounds.height, unitBounds.height);
      valueX = field.x + lead;
      valueY = field.y + lead + labelBounds.height + (labelBounds.height > 0 ? FIELD_GAP : 0);
    } else {
      fieldW = lead + pad + labelBounds.width + (labelBounds.width > 0 ? FIELD_GAP : 0) + valueBounds.width + (unitBounds.width > 0 ? FIELD_GAP + unitBounds.width : 0);
      fieldH = lead + pad + Math.max(labelBounds.height, valueBounds.height, unitBounds.height);
      valueX = field.x + lead + labelBounds.width + (labelBounds.width > 0 ? FIELD_GAP : 0);
      valueY = field.y + lead;
    }

    return {
      pad,
      fieldX: field.x,
      fieldY: field.y,
      fieldW,
      fieldH,
      valueX,
      valueY,
      valueW: valueBounds.width,
      valueH: valueBounds.height,
      labelBounds,
      unitBounds
    };
  }

  function alignedValueX(field, geometry) {
    const current = nominalTextBounds(previewTextForField(field), field.valueSize).width;
    if (current >= geometry.valueW) return geometry.valueX;
    const free = geometry.valueW - current;
    if (field.align === 'CENTER') return geometry.valueX + Math.floor(free / 2);
    if (field.align === 'RIGHT') return geometry.valueX + free;
    return geometry.valueX;
  }

  function drawField(buffer, field) {
    if (field.type === 'BAR') return null;
    const g = computeFieldGeometry(field);
    const isTrans = field.transparentBackground || field.backgroundColor === 'TRANSPARENT' || field.backgroundColor === -1 || field.backgroundColor === null || field.backgroundColor === undefined;
    // BOOL con fondo por estado: el fondo de todo el campo es el del estado
    // (JWPLC_UI.cpp:fieldBackground); el marco no cambia.
    const stateBg = fieldValueBackground565(field);
    const background = stateBg ?? field.backgroundColor;
    if (!isTrans || stateBg !== null) {
      fillBufferRect(buffer, g.fieldX, g.fieldY, g.fieldW, g.fieldH, background);
    }
    if (field.frame && g.fieldW > 1 && g.fieldH > 1) {
      drawBufferRect(buffer, g.fieldX, g.fieldY, g.fieldW, g.fieldH, field.frameColor);
    }
    const textBg = (isTrans && stateBg === null) ? null : background;
    if (field.label) {
      drawClassicTextAt(buffer, field.label, field.x + FIELD_PADDING, field.y + FIELD_PADDING, field.labelColor, textBg, field.labelSize);
    }
    if (field.unit) {
      drawClassicTextAt(buffer, field.unit, g.valueX + g.valueW + FIELD_GAP, g.valueY, field.labelColor, textBg, field.labelSize);
    }
    if (field.type === 'ICON') {
      // JWPLC_UI.cpp:drawIcon: el índice transparente toma el fondo del campo.
      const entry = iconEntry(field);
      if (!entry) return g;
      // Sin fondo: lo transparente en todos los estados no se pinta; lo que es
      // transparente sólo en algunos toma el fondo del campo.
      const img = window.JWPLCIconsCore.decode(entry, field.iconState, field._animFrame || 0);
      const mask = field.transparentBackground ? window.JWPLCIconsCore.alwaysTransparentMask(entry) : null;
      const iconBg = mask ? (Number(field.backgroundColor) & 0xFFFF) : textBg;
      for (let y = 0; y < img.height; y += 1) {
        for (let x = 0; x < img.width; x += 1) {
          const i = y * img.width + x;
          const c = img.pixels[i];
          if (c >= 0) setBufferPixel(buffer, g.valueX + x, g.valueY + y, c);
          else if (mask && mask[i]) continue;
          else if (iconBg !== null) setBufferPixel(buffer, g.valueX + x, g.valueY + y, iconBg);
        }
      }
      return g;
    }
    if (isBoolPilot(field)) {
      // JWPLC_UI.cpp: aro del color de la etiqueta y relleno del estado.
      const r = Math.floor((g.valueH - 1) / 2);
      const cx = g.valueX + r;
      const cy = g.valueY + r;
      fillBufferCircle(buffer, cx, cy, r, Number(field.labelColor) & 0xFFFF);
      if (r > 1) fillBufferCircle(buffer, cx, cy, r - 1, (field.boolValue ? Number(field.trueColor) : Number(field.falseColor)) & 0xFFFF);
      return g;
    }
    const preview = previewTextForField(field);
    if (preview) {
      drawClassicTextAt(buffer, preview, alignedValueX(field, g), g.valueY, fieldValueColor565(field), textBg, field.valueSize);
    }
    return g;
  }

  // Vista aislada de un campo (mini-modelos de Apariencia): framebuffer propio,
  // sin tocar el lienzo ni el orden de capas.
  function renderFieldPreview(field) {
    const buffer = new Uint16Array(WIDTH * HEIGHT);
    let g = null;
    if (['TEXT', 'VALUE', 'BOOL', 'ICON'].includes(field.type)) {
      g = drawField(buffer, field);
    } else if (field.type === 'RAW_TEXT') {
      const bg = field.transparentBackground ? null : (field.backgroundColor ?? 0x0000);
      drawRawTextAt(buffer, field.text || '', field.x, field.y, field.textColor ?? 0xFFFF, bg, field.size || 1, field.font);
      const b = getFieldBounds(field);
      g = { fieldX: b.x, fieldY: b.y, fieldW: b.width, fieldH: b.height };
    }
    return { buffer, g, width: WIDTH, height: HEIGHT };
  }

  let barLayerCanvas = null;
  let barLayerCtx = null;

  function drawBarToFramebuffer(buffer, field, onlyOrder = false) {
    if (!window.JWPLCHMIBar?.drawBarField) return;
    if (!barLayerCanvas) {
      barLayerCanvas = document.createElement('canvas');
      barLayerCanvas.width = WIDTH;
      barLayerCanvas.height = HEIGHT;
      barLayerCtx = barLayerCanvas.getContext('2d', { willReadFrequently: true });
    }
    barLayerCtx.clearRect(0, 0, WIDTH, HEIGHT);
    window.JWPLCHMIBar.drawBarField(barLayerCtx, field, 1);

    const g = window.JWPLCHMIBar.computeGeometry ? window.JWPLCHMIBar.computeGeometry(field) : null;
    let minX = 0, minY = 0, maxW = WIDTH, maxH = HEIGHT;
    if (g && Number.isFinite(g.fieldX) && Number.isFinite(g.fieldY) && Number.isFinite(g.fieldW) && Number.isFinite(g.fieldH)) {
      minX = Math.max(0, Math.min(WIDTH - 1, Math.floor(g.fieldX)));
      minY = Math.max(0, Math.min(HEIGHT - 1, Math.floor(g.fieldY)));
      maxW = Math.max(1, Math.min(WIDTH - minX, Math.ceil(g.fieldW)));
      maxH = Math.max(1, Math.min(HEIGHT - minY, Math.ceil(g.fieldH)));
    }

    const imgData = barLayerCtx.getImageData(minX, minY, maxW, maxH);
    const data = imgData.data;
    for (let py = 0; py < maxH; py++) {
      const fbRowOffset = (minY + py) * WIDTH + minX;
      for (let px = 0; px < maxW; px++) {
        const idx = (py * maxW + px) * 4;
        const a = data[idx + 3];
        if (a === 0) continue;
        const fbIdx = fbRowOffset + px;
        if (buffer === framebuffer) {
          topFieldOrder[fbIdx] = activeDrawingOrder;
          topFieldIsBar[fbIdx] = 1;
        }
        if (onlyOrder) continue;
        const r = data[idx];
        const g = data[idx + 1];
        const b = data[idx + 2];
        if (a >= 250) {
          buffer[fbIdx] = (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)) & 0xFFFF;
        } else {
          const dst565 = buffer[fbIdx];
          const dr = Math.round(((dst565 >> 11) & 0x1F) * 255 / 31);
          const dg = Math.round(((dst565 >> 5) & 0x3F) * 255 / 63);
          const db = Math.round((dst565 & 0x1F) * 255 / 31);
          const alphaNorm = a / 255;
          const outR = Math.round(r * alphaNorm + dr * (1 - alphaNorm));
          const outG = Math.round(g * alphaNorm + dg * (1 - alphaNorm));
          const outB = Math.round(b * alphaNorm + db * (1 - alphaNorm));
          buffer[fbIdx] = (((outR & 0xF8) << 8) | ((outG & 0xFC) << 3) | (outB >> 3)) & 0xFFFF;
        }
      }
    }
  }

  function composeFramebuffer() {
    framebuffer.set(pixelLayer);
    topFieldOrder.fill(Infinity);
    topFieldIsBar.fill(0);
    // Del fondo al frente por layerOrder (menor = más al frente). El arreglo
    // no sirve de orden: los objetos nuevos se agregan al final con el
    // layerOrder más bajo (assignNewLayerOrder) y quedaban dibujados detrás.
    pageObjectsInOrder(activePage);
    const pageFields = fieldsForPage(activePage)
      .map((field, index) => ({ field, index }))
      .sort((a, b) => (Number(b.field.layerOrder) - Number(a.field.layerOrder)) || (b.index - a.index))
      .map(({ field }) => field);
    for (let i = 0; i < pageFields.length; i++) {
      const field = pageFields[i];
      if (field.hidden) continue;
      activeDrawingOrder = ensureLayerOrder(field);
      if (['TEXT', 'VALUE', 'BOOL', 'ICON'].includes(field.type)) {
        drawField(framebuffer, field);
      } else if (field.type === 'BAR') {
        drawBarToFramebuffer(framebuffer, field, true);
      } else if (field.type === 'MENU') {
        window.JWPLCHMIMenu?.paint?.(field, {
          fillRect: (x, y, w, h, c) => fillBufferRect(framebuffer, x, y, w, h, c),
          drawRect: (x, y, w, h, c) => drawBufferRect(framebuffer, x, y, w, h, c),
          pixel: (x, y, c) => setBufferPixel(framebuffer, x, y, c)
        });
      } else if (field.type === 'EVENT_LOG') {
        window.JWPLCHMIEventLog?.paint?.(field, {
          fillRect: (x, y, w, h, c) => fillBufferRect(framebuffer, x, y, w, h, c),
          drawRect: (x, y, w, h, c) => drawBufferRect(framebuffer, x, y, w, h, c),
          drawText: (text, x, y, fg, bg, sz) => drawRawTextAt(framebuffer, text, x, y, fg, bg, sz || 1),
          pixel: (x, y, c) => setBufferPixel(framebuffer, x, y, c)
        });
      } else if (['LINE', 'RECT', 'ELLIPSE', 'TRIANGLE', 'POLYGON'].includes(field.type)) {
        drawShape(framebuffer, field);
      } else if (field.type === 'RAW_TEXT') {
        const bg = field.transparentBackground ? null : (field.backgroundColor ?? 0x0000);
        drawRawTextAt(framebuffer, field.text || '', field.x, field.y, field.textColor ?? 0xFFFF, bg, field.size || 1, field.font);
      }
    }
    activeDrawingOrder = Infinity;
    if (selectedTool === 'rawText' && !hmiFields.some((f) => f.type === 'RAW_TEXT' && Number(f.page || 0) === activePage)) {
      const bg = rawState.transparent ? null : rawState.background;
      drawRawTextAt(framebuffer, rawState.value, rawState.x, rawState.y, rawState.foreground, bg, rawState.size, rawState.font);
    }
  }

  function rebuildLogicalImage() {
    composeFramebuffer();
    const image = logicalCtx.createImageData(WIDTH, HEIGHT);
    const bytes = image.data;
    for (let i = 0; i < framebuffer.length; i += 1) {
      const { r, g, b } = rgb565ToRgb888(framebuffer[i]);
      const offset = i * 4;
      bytes[offset] = r;
      bytes[offset + 1] = g;
      bytes[offset + 2] = b;
      bytes[offset + 3] = 255;
    }
    logicalCtx.putImageData(image, 0, 0);

    // Dibuja la barra de navegación HMI directamente en el lienzo lógico
    if (window.JWPLCHMIPages?.drawHMINavigationBar) {
      window.JWPLCHMIPages.drawHMINavigationBar(logicalCtx, 1);
    }
  }

  function drawGrid() {
    if (!gridToggle.checked) return;
    const gridSize = Number(gridSizeSelect.value) || 8;
    // Grilla de 1 px desde zoom 3 (una línea cada 3 px de pantalla).
    if (gridSize * zoom < 3) return;
    const gridStyle = gridStyleSelect ? gridStyleSelect.value : 'lines';

    displayCtx.save();
    if (gridStyle === 'lines') {
      displayCtx.strokeStyle = 'rgba(255, 255, 255, 0.08)';
      displayCtx.lineWidth = 1;
      displayCtx.beginPath();
      for (let x = 0; x <= WIDTH; x += gridSize) {
        const px = Math.floor(x * zoom) + 0.5;
        displayCtx.moveTo(px, 0);
        displayCtx.lineTo(px, displayCanvas.height);
      }
      for (let y = 0; y <= HEIGHT; y += gridSize) {
        const py = Math.floor(y * zoom) + 0.5;
        displayCtx.moveTo(0, py);
        displayCtx.lineTo(displayCanvas.width, py);
      }
      displayCtx.stroke();
    } else {
      displayCtx.fillStyle = 'rgba(118, 151, 176, 0.45)';
      const dotSize = Math.max(1, Math.floor(zoom / 3));
      const offset = Math.floor(dotSize / 2);
      for (let x = 0; x <= WIDTH; x += gridSize) {
        for (let y = 0; y <= HEIGHT; y += gridSize) {
          displayCtx.fillRect(Math.floor(x * zoom) - offset, Math.floor(y * zoom) - offset, dotSize, dotSize);
        }
      }
    }
    displayCtx.restore();
  }

  function sanitizeSymbol(value, fallback) {
    const cleaned = String(value || '').trim().replace(/[^A-Za-z0-9_]/g, '_');
    if (!cleaned) return fallback;
    return /^[A-Za-z_]/.test(cleaned) ? cleaned : `_${cleaned}`;
  }

  function fieldFallbackId(field, index) {
    if (field.type === 'ICON') return `FIELD_ICON_${index + 1}`;
    if (field.type === 'VALUE') return `FIELD_VALUE_${index + 1}`;
    if (field.type === 'BOOL') return `FIELD_BOOL_${index + 1}`;
    if (field.type === 'BAR') return `FIELD_BAR_${index + 1}`;
    return `FIELD_TEXT_${index + 1}`;
  }

  function variableFallback(field, index) {
    if (field.type === 'ICON') return `icono${index + 1}`;
    if (field.type === 'VALUE') return `valor${index + 1}`;
    if (field.type === 'BOOL') return `estado${index + 1}`;
    if (field.type === 'BAR') return `nivel${index + 1}`;
    return `texto${index + 1}`;
  }

  function variableDeclaration(field, index) {
    const variable = sanitizeSymbol(field.variable, variableFallback(field, index));
    if (field.type === 'VALUE') return window.JWPLCHMICodegenCore.valueDeclaration(field, variable);
    if (field.type === 'BAR') return `float ${variable} = 0.0f;`;
    if (field.type === 'BOOL') return `bool ${variable} = false;`;
    if (field.type === 'ICON') return window.JWPLCIconsCore.iconDeclaration(field, variable, window.JWPLCHMIIcons?.iconInfo?.(field.iconId)?.stateNames || window.JWPLCIconsCore.stateNamesFor(window.JWPLCIconCatalog, field.iconId));
    return window.JWPLCHMICodegenCore.textDeclaration(field, variable);
  }

  // A15-5: la definición C++ de cada campo vive en designer-codegen-core.js.
  // BOOL y BAR salen aquí como JWPLC_UITextField provisional: sus capas
  // (designer-bool/-bar) normalizan el campo y reemplazan esa llamada.
  function fieldContract(field, index) {
    const id = sanitizeSymbol(field.id, fieldFallbackId(field, index));
    const core = window.JWPLCHMICodegenCore;
    if (field.type === 'ICON') {
      const entry = iconEntry(field);
      const symbol = window.JWPLCIconsCore.symbolFor(field.iconId, entry ? entry.width : field.iconSize);
      return window.JWPLCIconsCore.iconFieldBlock(field, id, symbol, core.cppString);
    }
    return field.type === 'VALUE' ? core.valueFieldBlock(field, id) : core.textFieldBlock(field, id);
  }

  // Alpha16: un bloque por (icono, tamaño) usado, en orden de aparición.
  function iconAssetsText(fields) {
    const core = window.JWPLCIconsCore;
    if (!core) return '';
    const seen = new Set();
    const assets = [];
    fields.filter((field) => field.type === 'ICON').forEach((field) => {
      const entry = iconEntry(field);
      if (!entry) return;
      const key = `${field.iconId}@${entry.width}`;
      if (seen.has(key)) return;
      seen.add(key);
      const icon = window.JWPLCHMIIcons?.iconInfo?.(field.iconId) || core.findIcon(window.JWPLCIconCatalog, field.iconId);
      assets.push({ id: field.iconId, name: icon?.name, stateNames: icon?.stateNames, entry });
    });
    return core.assetsSection(assets);
  }

  function setterHint(field, index) {
    const id = sanitizeSymbol(field.id, fieldFallbackId(field, index));
    const variable = sanitizeSymbol(field.variable, variableFallback(field, index));
    if (window.JWPLCHMICodegenCore?.isClockField?.(field)) return `// ${id}: fecha/hora del RTC (la actualiza el firmware)`;
    if (field.type === 'VALUE') return `// JWPLC_Display.setValue(${id}, ${variable});`;
    if (field.type === 'BOOL') return `// JWPLC_Display.setBool(${id}, ${variable});`;
    if (field.type === 'ICON') return `// ${window.JWPLCIconsCore.iconSetter(id, variable)}`;
    if (field.type === 'BAR') return `// JWPLC_Display.setBar(${id}, ${variable});`;
    return `// JWPLC_Display.setText(${id}, ${variable});`;
  }

  function buildContractText() {
    const validFields = hmiFields.filter(f => !['LINE', 'RECT', 'ELLIPSE', 'TRIANGLE', 'POLYGON', 'RAW_TEXT', 'NAV_BAR', 'MENU'].includes(f.type));
    const hasStaticObjects = hmiFields.some(f => ['LINE', 'RECT', 'ELLIPSE', 'TRIANGLE', 'POLYGON', 'RAW_TEXT', 'NAV_BAR', 'MENU'].includes(f.type));
    if (validFields.length === 0 && !hasStaticObjects) return '// Sin campos HMI. Agrega TEXT, VALUE, BOOL o BAR para comenzar.';
    if (validFields.length === 0) {
      // Sólo gráfica estática: sin HMI_FIELDS (un arreglo vacío no es C++ válido).
      return `// Código generado por JWPLC HMI Designer\n// API pública JWPLC_UI · Alpha11 A11-3E\n\nenum HMIFieldId : uint8_t\n{\n    // Sin campos dinámicos.\n};\n\n// Variables HMI\n\n\n// Definición declarativa\n// Sin campos dinámicos: sólo gráfica estática por página.\n\nvoid jwplcHMISetup()\n{\n    JWPLC_Display.setUserPageCount(${hmiPages.length});\n    JWPLC_Display.setUserPage(0);\n    JWPLC_Display.setUserRefreshMode(USER_REFRESH_ON_DEMAND);\n}\n\n// jwplcUIUpdate() NO se genera.`;
    }
    const enumLines = validFields
      .map((field, index) => `    ${sanitizeSymbol(field.id, fieldFallbackId(field, index))} = ${index + 1}`)
      .join(',\n');
    const variables = validFields.map(variableDeclaration).join('\n');
    const fields = validFields.map(fieldContract).join(',\n');
    const setters = validFields.map(setterHint).join('\n');
    const icons = iconAssetsText(validFields);

    return `// Código generado por JWPLC HMI Designer\n// API pública JWPLC_UI · Alpha11 A11-3E\n\nenum HMIFieldId : uint8_t\n{\n${enumLines}\n};\n\n${icons ? `${icons}\n` : ''}// Variables HMI\n${variables}\n\n// Definición declarativa\nstatic const JWPLC_UIField HMI_FIELDS[] =\n{\n${fields}\n};\n\nvoid jwplcHMISetup()\n{\n    JWPLC_Display.setFields(\n        HMI_FIELDS,\n        sizeof(HMI_FIELDS) / sizeof(HMI_FIELDS[0]));\n}\n\n// jwplcUIUpdate() NO se genera.\n// El usuario alimenta las variables anteriores dentro de su jwplcUIUpdate().\n// Setters públicos que corresponden a este diseño:\n${setters}`;
  }

  function buildStatusText() {
    const field = selectedField();
    const g = field ? computeFieldGeometry(field) : null;
    const textCount = hmiFields.filter((item) => item.type === 'TEXT').length;
    const valueCount = hmiFields.filter((item) => item.type === 'VALUE').length;
    const current = hmiPages.find((item) => item.id === activePage);
    return `JWPLC HMI Designer 2.1.0-alpha.15\n\n- Página activa: ${activePage} · ${current?.name || 'Página'}\n- Páginas: ${hmiPages.length}/${MAX_PAGES}\n- Campos globales: ${hmiFields.length}/${MAX_FIELDS}\n- Campos página: ${fieldsForPage().length}\n- TEXT: ${textCount}\n- VALUE: ${valueCount}\n- Selección: ${field ? `${field.type} ${field.name} · ${field.id}` : 'ninguna'}\n- Movimiento: flechas 1 px / Shift+flechas 10 px\n- Duplicar: Ctrl+D\n- Eliminar: Delete\n${g ? `- AUTO field: ${g.fieldW} × ${g.fieldH} px\n- X/Y: ${field.x}, ${field.y}\n- effectivePadding: ${g.pad} px` : ''}${field?.type === 'VALUE' ? `\n- sample reservado: ${makeNumericSample(field)}\n- preview formateado: ${formatNumericPreview(field) || '(vacío)'}` : ''}`;
  }

  function updateMetrics() {
    const selF = selectedField();
    if (selF && selF.type === 'RAW_TEXT') {
      const isGlcd = !selF.font || selF.font === 'GLCD';
      const b = rawTextBounds(selF.text || '', selF.size || 1, selF.font);
      const width = isGlcd ? (selF.text ? selF.text.length * 6 * (selF.size || 1) : 0) : b.width;
      const height = isGlcd ? (selF.text ? 8 * (selF.size || 1) : 0) : b.height;
      rawBoundsStatus.textContent = `${width} × ${height} px`;
      return;
    }
    if (selectedTool === 'rawText') {
      const isGlcd = !rawState.font || rawState.font === 'GLCD';
      const b = rawTextBounds(rawState.value || '', rawState.size, rawState.font);
      const width = isGlcd ? (rawState.value ? rawState.value.length * 6 * rawState.size : 0) : b.width;
      const height = isGlcd ? (rawState.value ? 8 * rawState.size : 0) : b.height;
      rawBoundsStatus.textContent = `${width} × ${height} px`;
      return;
    }

    const field = selectedField();
    if (field) {
      const g = computeFieldGeometry(field);
      fieldPadStatus.textContent = `${g.pad} px`;
      fieldBoundsStatus.textContent = `${g.fieldW} × ${g.fieldH} px`;
      fieldValueBoundsStatus.textContent = `${g.valueW} × ${g.valueH} px`;
      fieldValueXYStatus.textContent = `${g.valueX}, ${g.valueY}`;
      fieldLayoutStatus.textContent = field.layout;
      if (field.type === 'VALUE') {
        if (valueFormatSampleStatus) valueFormatSampleStatus.textContent = makeNumericSample(field);
        if (valueFormattedStatus) valueFormattedStatus.textContent = formatNumericPreview(field) || '—';
      }
    }
  }

  function updateCodePanel() {
    codeOutput.textContent = codeMode === 'contract' ? buildContractText() : buildStatusText();
  }

  let layerSeq = 0;

  function ensureLayerOrder(item) {
    if (!item) return 0;
    if (!Number.isFinite(Number(item.layerOrder)) || Number(item.layerOrder) <= 0) {
      layerSeq += 1;
      item.layerOrder = layerSeq;
    } else if (Number(item.layerOrder) > layerSeq) {
      layerSeq = Number(item.layerOrder);
    }
    return Number(item.layerOrder);
  }

  function assignNewLayerOrder(item) {
    if (!item) return 0;
    const pFields = fieldsForPage(activePage);
    const pMaps = (window.JWPLCHMIPixelMaps?.getAll?.() || []).filter(
      (pm) => Number(pm.page || 0) === Number(activePage)
    );
    pFields.forEach((f) => { if (f !== item) ensureLayerOrder(f); });
    pMaps.forEach((m) => { if (m !== item) ensureLayerOrder(m); });
    // layerOrder menor = más al frente: lo nuevo queda por encima de todo lo
    // que ya había en la página (NAV_BAR es global y queda fuera de la cuenta).
    const others = [...pFields, ...pMaps].filter((o) => o !== item && o.type !== 'NAV_BAR');
    const front = others.reduce((min, o) => Math.min(min, Number(o.layerOrder)), Infinity);
    if (!Number.isFinite(front)) {
      layerSeq += 1;
      item.layerOrder = layerSeq;
    } else if (front > 1) {
      item.layerOrder = front - 1;
    } else {
      others.forEach((o) => { o.layerOrder = Number(o.layerOrder) + 1; });
      layerSeq += 1;
      item.layerOrder = 1;
    }
    return item.layerOrder;
  }

  function pageObjectsInOrder(page = activePage) {
    const pFields = fieldsForPage(page);
    const pMaps = (window.JWPLCHMIPixelMaps?.getAll?.() || []).filter(
      (pm) => Number(pm.page || 0) === Number(page)
    );
    pFields.forEach(ensureLayerOrder);
    pMaps.forEach(ensureLayerOrder);
    return [...pFields, ...pMaps].sort((a, b) => a.layerOrder - b.layerOrder);
  }

  function sortObjectListDOM() {
    if (!objectList) return;
    const rows = [...objectList.querySelectorAll('.object-item')];
    rows.sort((a, b) => (Number(a.dataset.layerOrder) || 0) - (Number(b.dataset.layerOrder) || 0));
    rows.forEach((node) => objectList.appendChild(node));
  }

  function reorderPageFields(sourceKey, targetKey, insertAbove) {
    if (!sourceKey || !targetKey || sourceKey === targetKey) return;
    const items = pageObjectsInOrder(activePage);
    const sourceIdx = items.findIndex((item) => item.key === sourceKey);
    const targetIdx = items.findIndex((item) => item.key === targetKey);
    if (sourceIdx === -1 || targetIdx === -1 || sourceIdx === targetIdx) return;

    const [moved] = items.splice(sourceIdx, 1);
    let newTargetIdx = items.findIndex((item) => item.key === targetKey);
    if (!insertAbove) newTargetIdx += 1;
    items.splice(newTargetIdx, 0, moved);

    items.forEach((item, idx) => {
      item.layerOrder = idx + 1;
    });
    if (items.length > layerSeq) layerSeq = items.length;

    const pageFieldsOrdered = items.filter((item) => item.type !== 'PIXELMAP');
    const nonPageFields = hmiFields.filter((f) => !(f.type === 'NAV_BAR' || f.global) && Number(f.page || 0) !== activePage);
    hmiFields = [...pageFieldsOrdered, ...nonPageFields];

    const pmAll = window.JWPLCHMIPixelMaps?.getAll?.();
    if (Array.isArray(pmAll)) {
      const pageMapsOrdered = items.filter((item) => item.type === 'PIXELMAP');
      const nonPageMaps = pmAll.filter((m) => Number(m.page || 0) !== activePage);
      pmAll.splice(0, pmAll.length, ...pageMapsOrdered, ...nonPageMaps);
    }

    render();
    window.JWPLCHMIPixelMaps?.refresh?.();
    commitHistory();
  }

  function updateMiniPreview() {
    const miniCanvas = document.getElementById('sidebarMiniPreviewCanvas');
    if (miniCanvas) {
      const mctx = miniCanvas.getContext('2d');
      if (mctx) {
        mctx.imageSmoothingEnabled = true;
        mctx.imageSmoothingQuality = 'high';
        mctx.clearRect(0, 0, WIDTH, HEIGHT);
        mctx.drawImage(previewCanvas || logicalCanvas, 0, 0);
      }
    }

    const badge = document.getElementById('sidebarPreviewBadge');
    if (badge) {
      const cur = hmiPages.find((p) => p.id === activePage);
      badge.textContent = `${WIDTH} × ${HEIGHT} · ${cur?.name || 'Pág. ' + activePage}`;
    }

    updateMiniPreviewViewport();
  }

  const SVG_EYE_OPEN = `<svg width="13" height="13" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"/><circle cx="12" cy="12" r="3"/></svg>`;
  const SVG_EYE_OFF = `<svg width="13" height="13" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M17.94 17.94A10.07 10.07 0 0 1 12 20c-7 0-11-8-11-8a18.45 18.45 0 0 1 5.06-5.94M9.9 4.24A9.12 9.12 0 0 1 12 4c7 0 11 8 11 8a18.5 18.5 0 0 1-2.16 3.19m-6.72-1.07a3 3 0 1 1-4.24-4.24"/><line x1="1" y1="1" x2="23" y2="23"/></svg>`;
  const SVG_LOCK_CLOSED = `<svg width="13" height="13" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><rect x="3" y="11" width="18" height="11" rx="2" ry="2"/><path d="M7 11V7a5 5 0 0 1 10 0v4"/></svg>`;
  const SVG_LOCK_OPEN = `<svg width="13" height="13" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><rect x="3" y="11" width="18" height="11" rx="2" ry="2"/><path d="M7 11V7a5 5 0 0 1 9.9-1"/></svg>`;

  function formatLopakaFieldName(field, index) {
    if (!field) return `Objeto ${index + 1}`;
    if (field.name && field.name.trim()) {
      let n = field.name.trim().replace(/^FIELD[_\s-]*/i, '').trim();
      if (n) return n.replace(/_+/g, ' ');
    }
    if (field.label && field.label.trim()) return field.label.trim();
    if (field.text && field.text.trim()) return field.text.trim();
    const typeNames = {
      TEXT: 'Texto',
      VALUE: 'Valor',
      BOOL: 'Bool',
      BAR: 'Indicador',
      RAW_TEXT: 'Texto RAW',
      LINE: 'Línea',
      RECT: 'Rectángulo',
      ELLIPSE: 'Elipse',
      TRIANGLE: 'Triángulo',
      POLYGON: 'Polígono',
      NAV_BAR: 'Barra de Navegación'
    };
    const tName = typeNames[field.type] || field.type || 'Objeto';
    return `${tName} ${index + 1}`;
  }

  function renderObjectList() {
    objectList.querySelectorAll('.object-item:not(.a11-pixel-row)').forEach((item) => item.remove());
    const visibleFields = fieldsForPage(activePage);
    visibleFields.forEach((field, index) => {
      ensureLayerOrder(field);
      const button = document.createElement('div');
      const active = selectedFieldKeys.includes(field.key);
      button.className = `object-item${active ? ' active' : ''}${field.hidden ? ' is-hidden' : ''}${field.locked ? ' is-locked' : ''}`;
      button.dataset.fieldKey = field.key;
      button.dataset.layerOrder = String(field.layerOrder);
      const displayName = formatLopakaFieldName(field, index);
      button.title = `${displayName} (Doble clic: zoom in · Clic: editar)`;

      let icon = 'Aa';
      if (['TEXT', 'VALUE', 'RAW_TEXT'].includes(field.type)) icon = 'Aa';
      else if (field.type === 'BOOL') icon = '○';
      else if (field.type === 'BAR') icon = '▰';
      else if (field.type === 'LINE') icon = '╱';
      else if (field.type === 'RECT') icon = '□';
      else if (field.type === 'ELLIPSE') icon = '○';
      else if (field.type === 'TRIANGLE') icon = '△';
      else if (field.type === 'POLYGON') icon = '⬡';
      else if (field.type === 'NAV_BAR') icon = '☰';
      else if (field.type === 'MENU') icon = '▤';
      else if (field.type === 'EVENT_LOG') icon = '📋';
      else if (field.type === 'ICON') icon = '◈';

      button.innerHTML = `
        <span class="object-icon">${icon}</span>
        <span class="object-type" style="display:none">${field.type}</span>
        <span class="object-name" title="${displayName}">${displayName}</span>
        <div class="object-actions">
          <button type="button" class="object-btn btn-eye" title="${field.hidden ? 'Mostrar' : 'Ocultar'}">${field.hidden ? SVG_EYE_OFF : SVG_EYE_OPEN}</button>
          <button type="button" class="object-btn btn-lock" title="${field.locked ? 'Desbloquear' : 'Bloquear'}">${field.locked ? SVG_LOCK_CLOSED : SVG_LOCK_OPEN}</button>
        </div>
      `;

      if (field.type === 'VALUE') {
        const iconNode = button.querySelector('.object-icon');
        if (iconNode) {
          iconNode.style.fontWeight = '800';
          iconNode.style.color = '#52c9ff';
        }
      }

      // Drag and drop for layer reordering
      button.draggable = true;
      button.addEventListener('dragstart', (e) => {
        e.dataTransfer.setData('text/plain', field.key);
        e.dataTransfer.effectAllowed = 'move';
        button.classList.add('is-dragging');
      });

      button.addEventListener('dragend', () => {
        button.classList.remove('is-dragging');
        objectList.querySelectorAll('.drop-target-above, .drop-target-below').forEach((el) => {
          el.classList.remove('drop-target-above', 'drop-target-below');
        });
      });

      button.addEventListener('dragover', (e) => {
        e.preventDefault();
        e.dataTransfer.dropEffect = 'move';
        const rect = button.getBoundingClientRect();
        const isAbove = (e.clientY - rect.top) < rect.height / 2;
        button.classList.toggle('drop-target-above', isAbove);
        button.classList.toggle('drop-target-below', !isAbove);
      });

      button.addEventListener('dragleave', () => {
        button.classList.remove('drop-target-above', 'drop-target-below');
      });

      button.addEventListener('drop', (e) => {
        e.preventDefault();
        button.classList.remove('drop-target-above', 'drop-target-below');
        const sourceKey = e.dataTransfer.getData('text/plain');
        if (!sourceKey || sourceKey === field.key) return;
        const rect = button.getBoundingClientRect();
        const isAbove = (e.clientY - rect.top) < rect.height / 2;
        reorderPageFields(sourceKey, field.key, isAbove);
      });

      // Actions: Eye & Lock buttons
      const eyeBtn = button.querySelector('.btn-eye');
      eyeBtn.addEventListener('click', (e) => {
        e.stopPropagation();
        e.preventDefault();
        field.hidden = !field.hidden;
        render();
        commitHistory();
      });

      const lockBtn = button.querySelector('.btn-lock');
      lockBtn.addEventListener('click', (e) => {
        e.stopPropagation();
        e.preventDefault();
        field.locked = !field.locked;
        render();
        commitHistory();
      });

      // Single click selects and allows editing
      button.addEventListener('click', (e) => {
        if (e.target.closest('.object-btn')) return;
        if (e.shiftKey || e.ctrlKey) {
          if (selectedFieldKeys.includes(field.key)) {
            setSelectedKeys(selectedFieldKeys.filter((k) => k !== field.key));
          } else {
            setSelectedKeys([...selectedFieldKeys, field.key]);
          }
        } else {
          setSelectedKeys([field.key]);
        }
        selectedTool = toolForField(field);
        syncInputsFromState();
        syncToolUI();
        render();
      });

      // Double click does zoom in to the object!
      button.addEventListener('dblclick', (e) => {
        if (e.target.closest('.object-btn')) return;
        e.preventDefault();
        setSelectedKeys([field.key]);
        selectedTool = toolForField(field);
        syncInputsFromState();
        syncToolUI();
        fitSelection();
        render();
      });

      objectList.appendChild(button);
    });
    sortObjectListDOM();
    const pmCount = (window.JWPLCHMIPixelMaps?.getAll?.() || []).filter((pm) => Number(pm.page || 0) === activePage).length;
    countBadge.textContent = String(visibleFields.length + pmCount);
    if (fieldsStatus) fieldsStatus.textContent = `Campos: ${hmiFields.length}/${MAX_FIELDS}`;
  }

  function updateHistoryButtons() {
    if (undoButton) undoButton.disabled = historyIndex <= 0;
    if (redoButton) redoButton.disabled = historyIndex < 0 || historyIndex >= history.length - 1;
    duplicateButton.disabled = !selectedField() || hmiFields.length >= MAX_FIELDS;
    deleteButton.disabled = !selectedField();
  }

  function render() {
    rebuildLogicalImage();
    displayCanvas.width = WIDTH * zoom;
    displayCanvas.height = HEIGHT * zoom;
    displayCanvas.style.width = `${WIDTH * zoom}px`;
    displayCanvas.style.height = `${HEIGHT * zoom}px`;
    displayCtx.imageSmoothingEnabled = false;
    displayCtx.clearRect(0, 0, displayCanvas.width, displayCanvas.height);
    displayCtx.drawImage(logicalCanvas, 0, 0, WIDTH, HEIGHT, 0, 0, WIDTH * zoom, HEIGHT * zoom);

    // Del fondo al frente (layerOrder mayor primero), como composeFramebuffer.
    const visiblePageBars = fieldsForPage(activePage)
      .filter((field) => field.type === 'BAR' && !field.hidden)
      .sort((a, b) => ensureLayerOrder(b) - ensureLayerOrder(a));
    const stampHigherLayersOverBar = (ctx, barField, scale) => {
      const barOrder = ensureLayerOrder(barField);
      const b = getFieldBounds(barField);
      const x0 = Math.max(0, Math.floor(b.minX ?? b.x ?? 0));
      const y0 = Math.max(0, Math.floor(b.minY ?? b.y ?? 0));
      const x1 = Math.min(WIDTH, Math.ceil((b.maxX ?? (x0 + (b.width || 0))) + 1));
      const y1 = Math.min(HEIGHT, Math.ceil((b.maxY ?? (y0 + (b.height || 0))) + 1));
      for (let py = y0; py < y1; py++) {
        let spanStart = -1;
        for (let px = x0; px <= x1; px++) {
          const idx = py * WIDTH + px;
          const covered = px < x1 && topFieldOrder[idx] < barOrder && !topFieldIsBar[idx];
          if (covered && spanStart < 0) {
            spanStart = px;
          } else if (!covered && spanStart >= 0) {
            const spanW = px - spanStart;
            ctx.drawImage(logicalCanvas, spanStart, py, spanW, 1, spanStart * scale, py * scale, spanW * scale, scale);
            spanStart = -1;
          }
        }
      }
    };
    if (window.JWPLCHMIBar?.drawBarField) {
      for (let i = 0; i < visiblePageBars.length; i++) {
        window.JWPLCHMIBar.drawBarField(displayCtx, visiblePageBars[i], zoom);
        stampHigherLayersOverBar(displayCtx, visiblePageBars[i], zoom);
      }
    }

    drawGrid();

    if (previewCtx) {
      previewCtx.imageSmoothingEnabled = false;
      previewCtx.clearRect(0, 0, WIDTH, HEIGHT);
      previewCtx.drawImage(logicalCanvas, 0, 0);
      if (window.JWPLCHMIBar?.drawBarField) {
        for (let i = 0; i < visiblePageBars.length; i++) {
          window.JWPLCHMIBar.drawBarField(previewCtx, visiblePageBars[i], 1);
          stampHigherLayersOverBar(previewCtx, visiblePageBars[i], 1);
        }
      }
    }

    renderObjectList();

    if (!window.JWPLCHMIEditor?.integratedBarLayers && window.JWPLCHMIBar?.drawBars) {
      window.JWPLCHMIBar.drawBars(displayCtx, zoom, previewCtx);
    }

    drawSelectionAndGuides();
    drawRulers();
    updateMiniPreview();
    updateMiniPreviewViewport();
  
    

    updateMetrics();
    updateCodePanel();
    updateHistoryButtons();
    window.dispatchEvent(new CustomEvent('jwplc:editor-refresh', {
      detail: {
        selectedTool,
        hasFieldSelection: Boolean(selectedField()),
        fieldType: selectedField()?.type || null,
        activePage,
        pageCount: hmiPages.length
      }
    }));
  }

  function redrawCanvasOnly() {
    rebuildLogicalImage();
    displayCtx.imageSmoothingEnabled = false;
    displayCtx.clearRect(0, 0, displayCanvas.width, displayCanvas.height);
    displayCtx.drawImage(logicalCanvas, 0, 0, WIDTH, HEIGHT, 0, 0, WIDTH * zoom, HEIGHT * zoom);

    const visiblePageBars = fieldsForPage(activePage)
      .filter((field) => field.type === 'BAR' && !field.hidden)
      .sort((a, b) => ensureLayerOrder(b) - ensureLayerOrder(a));
    const stampHigherLayersOverBar = (ctx, barField, scale) => {
      const barOrder = ensureLayerOrder(barField);
      const b = getFieldBounds(barField);
      const x0 = Math.max(0, Math.floor(b.minX ?? b.x ?? 0));
      const y0 = Math.max(0, Math.floor(b.minY ?? b.y ?? 0));
      const x1 = Math.min(WIDTH, Math.ceil((b.maxX ?? (x0 + (b.width || 0))) + 1));
      const y1 = Math.min(HEIGHT, Math.ceil((b.maxY ?? (y0 + (b.height || 0))) + 1));
      for (let py = y0; py < y1; py++) {
        let spanStart = -1;
        for (let px = x0; px <= x1; px++) {
          const idx = py * WIDTH + px;
          const covered = px < x1 && topFieldOrder[idx] < barOrder && !topFieldIsBar[idx];
          if (covered && spanStart < 0) {
            spanStart = px;
          } else if (!covered && spanStart >= 0) {
            const spanW = px - spanStart;
            ctx.drawImage(logicalCanvas, spanStart, py, spanW, 1, spanStart * scale, py * scale, spanW * scale, scale);
            spanStart = -1;
          }
        }
      }
    };
    if (window.JWPLCHMIBar?.drawBarField) {
      for (let i = 0; i < visiblePageBars.length; i++) {
        window.JWPLCHMIBar.drawBarField(displayCtx, visiblePageBars[i], zoom);
        stampHigherLayersOverBar(displayCtx, visiblePageBars[i], zoom);
      }
    }

    drawGrid();

    if (previewCtx) {
      previewCtx.imageSmoothingEnabled = false;
      previewCtx.clearRect(0, 0, WIDTH, HEIGHT);
      previewCtx.drawImage(logicalCanvas, 0, 0);
      if (window.JWPLCHMIBar?.drawBarField) {
        for (let i = 0; i < visiblePageBars.length; i++) {
          window.JWPLCHMIBar.drawBarField(previewCtx, visiblePageBars[i], 1);
          stampHigherLayersOverBar(previewCtx, visiblePageBars[i], 1);
        }
      }
    }

    if (!window.JWPLCHMIEditor?.integratedBarLayers && window.JWPLCHMIBar?.drawBars) {
      window.JWPLCHMIBar.drawBars(displayCtx, zoom, previewCtx);
    }

    drawSelectionAndGuides();
  }

  function pointFromPointer(event) {
    const rect = displayCanvas.getBoundingClientRect();
    const scaleX = displayCanvas.width / rect.width;
    const scaleY = displayCanvas.height / rect.height;
    return {
      x: Math.floor(((event.clientX - rect.left) * scaleX) / zoom),
      y: Math.floor(((event.clientY - rect.top) * scaleY) / zoom)
    };
  }

  function drawCADMarquee() {
    // Rendered via viewport DOM overlay #cadMarqueeBox for unclipped visibility across canvas and workspace
  }

  function getSelectionBounds(fields, pms = []) {
    let minX = Infinity, minY = Infinity, maxX = -Infinity, maxY = -Infinity;
    if (fields && fields.length > 0) {
      for (const f of fields) {
        const b = getFieldBounds(f);
        if (b.minX < minX) minX = b.minX;
        if (b.maxX > maxX) maxX = b.maxX;
        if (b.minY < minY) minY = b.minY;
        if (b.maxY > maxY) maxY = b.maxY;
      }
    }
    if (pms && pms.length > 0) {
      for (const pm of pms) {
        const b = getPixelMapBounds(pm);
        if (b) {
          if (b.minX < minX) minX = b.minX;
          if (b.maxX > maxX) maxX = b.maxX;
          if (b.minY < minY) minY = b.minY;
          if (b.maxY > maxY) maxY = b.maxY;
        }
      }
    }
    if (minX > maxX || minY > maxY || minX === Infinity) return null;

    const screenX1 = minX * zoom;
    const screenY1 = minY * zoom;
    const screenX2 = (maxX + 1) * zoom - 1;
    const screenY2 = (maxY + 1) * zoom - 1;
    const screenW = Math.max(1, screenX2 - screenX1);
    const screenH = Math.max(1, screenY2 - screenY1);

    return {
      minX,
      minY,
      maxX,
      maxY,
      screenX1,
      screenY1,
      screenX2,
      screenY2,
      screenW,
      screenH
    };
  }

  function getSelectionHandles(fields, pms = null) {
    const selPms = pms || (window.JWPLCHMIPixelMaps?.getSelectedMaps?.() || []).filter(
      (pm) => Number(pm.page || 0) === activePage && pm.editorVisible !== false && pm.pixels?.length
    );

    if ((!fields || fields.length === 0) && selPms.length === 0) {
      return [];
    }

    if (fields && fields.length === 1 && selPms.length === 0) {
      const f = fields[0];
      // RAW_TEXT: sin handles de resize (el texto no se deforma geometricamente)
      if (f.type === 'RAW_TEXT') return [];
      if (f.type === 'LINE') {
        const b = getFieldBounds(f);
        const sx1 = b.minX * zoom;
        const sy1 = b.minY * zoom;
        const sx2 = (b.maxX + 1) * zoom - 1;
        const sy2 = (b.maxY + 1) * zoom - 1;
        const x1 = f.x ?? 0; const y1 = f.y ?? 0;
        const x2 = f.x2 ?? x1; const y2 = f.y2 ?? y1;

        const midX = (sx1 + sx2) / 2;
        const midY = (sy1 + sy2) / 2;

        let p1Pos, p2Pos;
        if (Math.abs(x1 - x2) < 0.001) {
          if (y1 <= y2) {
            p1Pos = { x: midX, y: sy1 };
            p2Pos = { x: midX, y: sy2 };
          } else {
            p1Pos = { x: midX, y: sy2 };
            p2Pos = { x: midX, y: sy1 };
          }
        } else if (Math.abs(y1 - y2) < 0.001) {
          if (x1 <= x2) {
            p1Pos = { x: sx1, y: midY };
            p2Pos = { x: sx2, y: midY };
          } else {
            p1Pos = { x: sx2, y: midY };
            p2Pos = { x: sx1, y: midY };
          }
        } else {
          if (x1 <= x2 && y1 <= y2) {
            p1Pos = { x: sx1, y: sy1 };
            p2Pos = { x: sx2, y: sy2 };
          } else if (x1 <= x2 && y1 > y2) {
            p1Pos = { x: sx1, y: sy2 };
            p2Pos = { x: sx2, y: sy1 };
          } else if (x1 > x2 && y1 <= y2) {
            p1Pos = { x: sx2, y: sy1 };
            p2Pos = { x: sx1, y: sy2 };
          } else {
            p1Pos = { x: sx2, y: sy2 };
            p2Pos = { x: sx1, y: sy1 };
          }
        }
        return [
          { id: 'p1', x: p1Pos.x, y: p1Pos.y, cursor: 'crosshair' },
          { id: 'p2', x: p2Pos.x, y: p2Pos.y, cursor: 'crosshair' }
        ];
      }
      if (['RECT', 'ELLIPSE', 'TRIANGLE', 'POLYGON'].includes(f.type)) {
        const angleDeg = Number(f.rotation) || 0;
        if (angleDeg === 0) {
          const b = getFieldBounds(f);
          const sx1 = b.minX * zoom;
          const sy1 = b.minY * zoom;
          const sx2 = (b.maxX + 1) * zoom - 1;
          const sy2 = (b.maxY + 1) * zoom - 1;
          const midX = (sx1 + sx2) / 2;
          const midY = (sy1 + sy2) / 2;
          return [
            { id: 'tl', x: sx1, y: sy1, cursor: 'nwse-resize' },
            { id: 'tc', x: midX, y: sy1, cursor: 'ns-resize' },
            { id: 'tr', x: sx2, y: sy1, cursor: 'nesw-resize' },
            { id: 'rc', x: sx2, y: midY, cursor: 'ew-resize' },
            { id: 'br', x: sx2, y: sy2, cursor: 'nwse-resize' },
            { id: 'bc', x: midX, y: sy2, cursor: 'ns-resize' },
            { id: 'bl', x: sx1, y: sy2, cursor: 'nesw-resize' },
            { id: 'lc', x: sx1, y: midY, cursor: 'ew-resize' }
          ];
        }
        const lb = getShapeLocalBounds(f);
        const x1 = f.x ?? 0; const y1 = f.y ?? 0;
        const x2 = f.x2 ?? x1; const y2 = f.y2 ?? y1;
        const cx = (x1 + x2) / 2; const cy = (y1 + y2) / 2;
        const flipH = Boolean(f.flipH); const flipV = Boolean(f.flipV);
        const midX = (lb.minX + lb.maxX) / 2;
        const midY = (lb.minY + lb.maxY) / 2;

        const tp = (px, py) => {
          const r = transformShapePoint(px, py, cx, cy, angleDeg, flipH, flipV);
          return { x: r.x * zoom, y: r.y * zoom };
        };

        return [
          { id: 'tl', ...tp(lb.minX, lb.minY), cursor: 'crosshair' },
          { id: 'tc', ...tp(midX, lb.minY), cursor: 'crosshair' },
          { id: 'tr', ...tp(lb.maxX, lb.minY), cursor: 'crosshair' },
          { id: 'rc', ...tp(lb.maxX, midY), cursor: 'crosshair' },
          { id: 'br', ...tp(lb.maxX, lb.maxY), cursor: 'crosshair' },
          { id: 'bc', ...tp(midX, lb.maxY), cursor: 'crosshair' },
          { id: 'bl', ...tp(lb.minX, lb.maxY), cursor: 'crosshair' },
          { id: 'lc', ...tp(lb.minX, midY), cursor: 'crosshair' }
        ];
      }
    }

    // Default: AABB handles for multi-selection or pixelmaps
    const b = getSelectionBounds(fields, selPms);
    if (!b) return [];

    return [
      { id: 'tl', x: b.screenX1, y: b.screenY1, cursor: 'nwse-resize' },
      { id: 'tc', x: b.screenX1 + b.screenW / 2, y: b.screenY1, cursor: 'ns-resize' },
      { id: 'tr', x: b.screenX2, y: b.screenY1, cursor: 'nesw-resize' },
      { id: 'rc', x: b.screenX2, y: b.screenY1 + b.screenH / 2, cursor: 'ew-resize' },
      { id: 'br', x: b.screenX2, y: b.screenY2, cursor: 'nwse-resize' },
      { id: 'bc', x: b.screenX1 + b.screenW / 2, y: b.screenY2, cursor: 'ns-resize' },
      { id: 'bl', x: b.screenX1, y: b.screenY2, cursor: 'nesw-resize' },
      { id: 'lc', x: b.screenX1, y: b.screenY1 + b.screenH / 2, cursor: 'ew-resize' }
    ];
  }

  function hitTestHandle(event) {
    const fields = selectedFields();
    const selPms = (window.JWPLCHMIPixelMaps?.getSelectedMaps?.() || []).filter(
      (pm) => Number(pm.page || 0) === activePage && pm.editorVisible !== false && pm.pixels?.length
    );
    if (fields.length === 0 && selPms.length === 0) return null;
    const rect = displayCanvas.getBoundingClientRect();
    const scaleX = displayCanvas.width / rect.width;
    const scaleY = displayCanvas.height / rect.height;
    const mouseScreenX = (event.clientX - rect.left) * scaleX;
    const mouseScreenY = (event.clientY - rect.top) * scaleY;

    const handles = getSelectionHandles(fields, selPms);
    const hitRadius = 12;
    for (const h of handles) {
      const dx = mouseScreenX - h.x;
      const dy = mouseScreenY - h.y;
      if (dx * dx + dy * dy <= hitRadius * hitRadius) {
        return h;
      }
    }
    return null;
  }

  function isPointInsideSelection(point) {
    const fields = selectedFields();
    const selPms = (window.JWPLCHMIPixelMaps?.getSelectedMaps?.() || []).filter(
      (pm) => Number(pm.page || 0) === activePage && pm.editorVisible !== false && pm.pixels?.length
    );
    const totalCount = fields.length + selPms.length;
    if (totalCount === 0) return false;

    if (fields.length === 0 && selPms.length > 0) {
      if (selectedTool !== 'pixel' && selectedTool !== 'erase') {
        return selPms.some((selPm) => selPm.pixels.some((p) => {
          const px = selPm.x + p.x;
          const py = selPm.y + p.y;
          return Math.abs(px - point.x) <= 2 && Math.abs(py - point.y) <= 2;
        }));
      }
      return false;
    }

    if (fields.length === 1 && selPms.length === 0) {
      if (fields[0].type === 'LINE') {
        const strokeSize = fields[0].borderEnabled !== false ? Math.max(1, Math.round(fields[0].size || 1)) : 1;
        const x1 = fields[0].x ?? 0; const y1 = fields[0].y ?? 0;
        const x2 = fields[0].x2 ?? x1; const y2 = fields[0].y2 ?? y1;
        const dist = distToSegment(point.x, point.y, x1, y1, x2, y2);
        return dist <= Math.max(3.5, strokeSize / 2 + 2.0);
      }
      if (['RECT', 'ELLIPSE', 'TRIANGLE', 'POLYGON'].includes(fields[0].type)) {
        const f = fields[0];
        const strokeSize = f.borderEnabled !== false ? Math.max(1, Math.round(f.size || 1)) : 0;
        const pts = getShapeTransformedVertices(f);
        if (pts && pts.length >= 3) {
          const isInside = isPointInPolygon(point.x, point.y, pts);
          if (f.fill && isInside) return true;
          const distToBorder = distToPolygonBoundary(point.x, point.y, pts);
          return distToBorder <= Math.max(3.5, strokeSize + 2.0);
        }
      }
    }

    // Check pixelmaps in selection
    if (selPms.some((selPm) => selPm.pixels.some((p) => {
      const px = selPm.x + p.x;
      const py = selPm.y + p.y;
      return Math.abs(px - point.x) <= 2 && Math.abs(py - point.y) <= 2;
    }))) {
      return true;
    }

    // Check field geometries in selection
    for (const f of fields) {
      if (f.type === 'LINE') {
        const strokeSize = f.borderEnabled !== false ? Math.max(1, Math.round(f.size || 1)) : 1;
        const x1 = f.x ?? 0; const y1 = f.y ?? 0;
        const x2 = f.x2 ?? x1; const y2 = f.y2 ?? y1;
        if (distToSegment(point.x, point.y, x1, y1, x2, y2) <= Math.max(3.5, strokeSize / 2 + 2.0)) return true;
      } else if (['RECT', 'ELLIPSE', 'TRIANGLE', 'POLYGON'].includes(f.type)) {
        const pts = getShapeTransformedVertices(f);
        if (pts && pts.length >= 3) {
          if (f.fill && isPointInPolygon(point.x, point.y, pts)) return true;
          const strokeSize = f.borderEnabled !== false ? Math.max(1, Math.round(f.size || 1)) : 0;
          if (distToPolygonBoundary(point.x, point.y, pts) <= Math.max(3.5, strokeSize + 2.0)) return true;
        }
      }
    }

    const b = getSelectionBounds(fields, selPms);
    if (!b) return false;
    const margin = 4;
    return (
      point.x >= b.minX - margin &&
      point.x <= b.maxX + margin &&
      point.y >= b.minY - margin &&
      point.y <= b.maxY + margin
    );
  }

  function getPixelMapBounds(pm) {
    if (!pm || !pm.pixels?.length) return null;
    let minX = Infinity, minY = Infinity, maxX = -Infinity, maxY = -Infinity;
    pm.pixels.forEach((p) => {
      const px = pm.x + p.x;
      const py = pm.y + p.y;
      if (px < minX) minX = px;
      if (px > maxX) maxX = px;
      if (py < minY) minY = py;
      if (py > maxY) maxY = py;
    });
    if (minX === Infinity) return null;
    const screenX1 = minX * zoom;
    const screenY1 = minY * zoom;
    const screenX2 = (maxX + 1) * zoom - 1;
    const screenY2 = (maxY + 1) * zoom - 1;
    return {
      minX, minY, maxX, maxY,
      screenX1, screenY1, screenX2, screenY2,
      screenW: Math.max(8, screenX2 - screenX1),
      screenH: Math.max(8, screenY2 - screenY1)
    };
  }

  function getPixelMapsUnionBounds(pms) {
    if (!pms || pms.length === 0) return null;
    let minX = Infinity, minY = Infinity, maxX = -Infinity, maxY = -Infinity;
    pms.forEach((pm) => {
      const b = getPixelMapBounds(pm);
      if (b) {
        if (b.minX < minX) minX = b.minX;
        if (b.maxX > maxX) maxX = b.maxX;
        if (b.minY < minY) minY = b.minY;
        if (b.maxY > maxY) maxY = b.maxY;
      }
    });
    if (minX === Infinity) return null;
    const screenX1 = minX * zoom;
    const screenY1 = minY * zoom;
    const screenX2 = (maxX + 1) * zoom - 1;
    const screenY2 = (maxY + 1) * zoom - 1;
    return {
      minX, minY, maxX, maxY,
      screenX1, screenY1, screenX2, screenY2,
      screenW: Math.max(8, screenX2 - screenX1),
      screenH: Math.max(8, screenY2 - screenY1)
    };
  }

  function drawSelectionAndGuides() {
    const fields = selectedFields();
    const selPms = (window.JWPLCHMIPixelMaps?.getSelectedMaps?.() || []).filter(
      (pm) => Number(pm.page || 0) === activePage && pm.editorVisible !== false && pm.pixels?.length
    );

    const totalCount = fields.length + selPms.length;
    if (totalCount === 0 || selectedTool === 'pixel' || selectedTool === 'erase') {
      return;
    }

    displayCtx.save();

    // 1. Draw individual blue outline for each selected pixel map
    if (selPms.length > 0) {
      displayCtx.strokeStyle = '#0084ff';
      displayCtx.lineWidth = 1;
      selPms.forEach((pm) => {
        const b = getPixelMapBounds(pm);
        if (b) {
          displayCtx.strokeRect(b.screenX1 + 0.5, b.screenY1 + 0.5, b.screenW, b.screenH);
        }
      });
    }

    // 2. Draw individual outline for each selected shape/field
    if (fields.length > 0) {
      displayCtx.strokeStyle = '#0084ff';
      displayCtx.lineWidth = 1;

      if (fields.length === 1 && totalCount === 1) {
        const f = fields[0];
        const angleDeg = Number(f.rotation) || 0;
        if (f.type === 'LINE') {
          const b = getFieldBounds(f);
          const sw = Math.max(1, (b.maxX - b.minX + 1) * zoom - 1);
          const sh = Math.max(1, (b.maxY - b.minY + 1) * zoom - 1);
          displayCtx.strokeRect(b.minX * zoom + 0.5, b.minY * zoom + 0.5, sw, sh);
        } else if (['RECT', 'ELLIPSE', 'TRIANGLE', 'POLYGON'].includes(f.type) && angleDeg !== 0) {
          const lb = getShapeLocalBounds(f);
          const x1 = f.x ?? 0; const y1 = f.y ?? 0;
          const x2 = f.x2 ?? x1; const y2 = f.y2 ?? y1;
          const cx = (x1 + x2) / 2; const cy = (y1 + y2) / 2;
          const flipH = Boolean(f.flipH); const flipV = Boolean(f.flipV);
          const corners = [
            [lb.minX, lb.minY], [lb.maxX, lb.minY], [lb.maxX, lb.maxY], [lb.minX, lb.maxY]
          ].map(([px, py]) => {
            const r = transformShapePoint(px, py, cx, cy, angleDeg, flipH, flipV);
            return [r.x * zoom + 0.5, r.y * zoom + 0.5];
          });
          displayCtx.beginPath();
          displayCtx.moveTo(corners[0][0], corners[0][1]);
          for (let i = 1; i < corners.length; i++) displayCtx.lineTo(corners[i][0], corners[i][1]);
          displayCtx.closePath();
          displayCtx.stroke();
        } else {
          const fb = getFieldBounds(f);
          if (fb) {
            const sw = Math.max(1, (fb.maxX - fb.minX + 1) * zoom - 1);
            const sh = Math.max(1, (fb.maxY - fb.minY + 1) * zoom - 1);
            displayCtx.strokeRect(fb.minX * zoom + 0.5, fb.minY * zoom + 0.5, sw, sh);
          }
        }
      } else {
        // Multi-selection (either multiple fields, or fields + pixelmaps)
        fields.forEach((f) => {
          const fb = getFieldBounds(f);
          if (fb) {
            const sw = Math.max(1, (fb.maxX - fb.minX + 1) * zoom - 1);
            const sh = Math.max(1, (fb.maxY - fb.minY + 1) * zoom - 1);
            displayCtx.strokeRect(fb.minX * zoom + 0.5, fb.minY * zoom + 0.5, sw, sh);
          }
        });
      }
    }

    // 3. Combined union bounds for guides, outer dashed box and handles
    const b = getSelectionBounds(fields, selPms);
    if (b) {
      // Dotted projection lines to rulers
      displayCtx.strokeStyle = 'rgba(255, 255, 255, 0.40)';
      displayCtx.setLineDash([2, 3]);
      displayCtx.lineWidth = 1;
      displayCtx.beginPath();
      displayCtx.moveTo(b.screenX1 + 0.5, 0);
      displayCtx.lineTo(b.screenX1 + 0.5, displayCanvas.height);
      displayCtx.moveTo(b.screenX2 + 0.5, 0);
      displayCtx.lineTo(b.screenX2 + 0.5, displayCanvas.height);
      displayCtx.moveTo(0, b.screenY1 + 0.5);
      displayCtx.lineTo(displayCanvas.width, b.screenY1 + 0.5);
      displayCtx.moveTo(0, b.screenY2 + 0.5);
      displayCtx.lineTo(displayCanvas.width, b.screenY2 + 0.5);
      displayCtx.stroke();
      displayCtx.setLineDash([]);

      // Outer dashed bounding box if multiple items selected
      if (totalCount > 1) {
        displayCtx.strokeStyle = 'rgba(0, 132, 255, 0.6)';
        displayCtx.setLineDash([4, 4]);
        displayCtx.strokeRect(b.screenX1 + 0.5, b.screenY1 + 0.5, b.screenW, b.screenH);
        displayCtx.setLineDash([]);
      }

      // Selection Handles around union bounding box (or shape's own handles)
      const hs = 6;
      const handles = getSelectionHandles(fields, selPms);
      handles.forEach((h) => {
        const rx = Math.round(h.x - hs / 2);
        const ry = Math.round(h.y - hs / 2);
        displayCtx.fillStyle = '#060d13';
        displayCtx.fillRect(rx, ry, hs, hs);
        displayCtx.strokeStyle = '#0084ff';
        displayCtx.lineWidth = 1.5;
        displayCtx.strokeRect(rx + 0.5, ry + 0.5, hs - 1, hs - 1);
      });
    }

    displayCtx.restore();
  }

  let activeCursorPoint = null;

  function updateCrosshairs(pt) {
    const crosshairX = document.getElementById('crosshairX');
    const crosshairY = document.getElementById('crosshairY');
    if (!crosshairX || !crosshairY) return;
    if (!pt || !inside(pt.x, pt.y)) {
      crosshairX.style.display = 'none';
      crosshairY.style.display = 'none';
      return;
    }
    const rulerW = 36;
    const rulerH = 24;
    const cell = (v) => (Math.floor(v * zoom) + Math.floor((v + 1) * zoom)) / 2;
    const px = rulerW + Math.round(cell(pt.x));
    const py = rulerH + Math.round(cell(pt.y));
    crosshairX.style.display = 'block';
    crosshairX.style.left = `${px}px`;
    crosshairY.style.display = 'block';
    crosshairY.style.top = `${py}px`;
  }

  function drawRulers() {
    const topCanvas = document.getElementById('topRulerCanvas');
    const bottomCanvas = document.getElementById('bottomRulerCanvas');
    const leftCanvas = document.getElementById('leftRulerCanvas');
    const rightCanvas = document.getElementById('rightRulerCanvas');
    if (!topCanvas || !bottomCanvas || !leftCanvas || !rightCanvas) return;

    const dpr = Math.max(1, window.devicePixelRatio || 1);
    const targetTopW = Math.round(WIDTH * zoom);
    const targetMidH = Math.round(HEIGHT * zoom);
    const rulerH = 24;
    const rulerW = 36;

    const topPhysW = Math.round(targetTopW * dpr);
    const topPhysH = Math.round(rulerH * dpr);
    const sidePhysW = Math.round(rulerW * dpr);
    const sidePhysH = Math.round(targetMidH * dpr);

    if (topCanvas.width !== topPhysW || topCanvas.height !== topPhysH) {
      topCanvas.width = topPhysW;
      topCanvas.height = topPhysH;
      topCanvas.style.width = `${targetTopW}px`;
      topCanvas.style.height = `${rulerH}px`;
    }
    if (bottomCanvas.width !== topPhysW || bottomCanvas.height !== topPhysH) {
      bottomCanvas.width = topPhysW;
      bottomCanvas.height = topPhysH;
      bottomCanvas.style.width = `${targetTopW}px`;
      bottomCanvas.style.height = `${rulerH}px`;
    }
    if (leftCanvas.width !== sidePhysW || leftCanvas.height !== sidePhysH) {
      leftCanvas.width = sidePhysW;
      leftCanvas.height = sidePhysH;
      leftCanvas.style.width = `${rulerW}px`;
      leftCanvas.style.height = `${targetMidH}px`;
    }
    if (rightCanvas.width !== sidePhysW || rightCanvas.height !== sidePhysH) {
      rightCanvas.width = sidePhysW;
      rightCanvas.height = sidePhysH;
      rightCanvas.style.width = `${rulerW}px`;
      rightCanvas.style.height = `${targetMidH}px`;
    }

    const topCtx = topCanvas.getContext('2d');
    const bottomCtx = bottomCanvas.getContext('2d');
    const leftCtx = leftCanvas.getContext('2d');
    const rightCtx = rightCanvas.getContext('2d');

    topCtx.setTransform(dpr, 0, 0, dpr, 0, 0);
    bottomCtx.setTransform(dpr, 0, 0, dpr, 0, 0);
    leftCtx.setTransform(dpr, 0, 0, dpr, 0, 0);
    rightCtx.setTransform(dpr, 0, 0, dpr, 0, 0);

    topCtx.clearRect(0, 0, targetTopW, rulerH);
    bottomCtx.clearRect(0, 0, targetTopW, rulerH);
    leftCtx.clearRect(0, 0, rulerW, targetMidH);
    rightCtx.clearRect(0, 0, rulerW, targetMidH);

    // Standard ticks: Top & Bottom
    topCtx.strokeStyle = '#4e6475';
    topCtx.lineWidth = 1;
    topCtx.beginPath();

    bottomCtx.strokeStyle = '#4e6475';
    bottomCtx.lineWidth = 1;
    bottomCtx.beginPath();

    // Misma fórmula que drawGrid() (floor): con zoom fraccionario round() corría
    // las marcas 1 px respecto de las líneas de la grilla.
    const edge = (v) => Math.floor(v * zoom);

    for (let x = 0; x <= WIDTH; x += 2) {
      const px = edge(x) + 0.5;
      let tickLen = 0;
      if (x % 50 === 0) tickLen = 8;
      else if (x % 10 === 0) tickLen = 5;
      else if (x % 2 === 0 && zoom >= 3) tickLen = 3;
      if (tickLen > 0) {
        topCtx.moveTo(px, rulerH - tickLen);
        topCtx.lineTo(px, rulerH);

        bottomCtx.moveTo(px, 0);
        bottomCtx.lineTo(px, tickLen);
      }
    }
    topCtx.stroke();
    bottomCtx.stroke();

    // Standard ticks: Left & Right
    leftCtx.strokeStyle = '#4e6475';
    leftCtx.lineWidth = 1;
    leftCtx.beginPath();

    rightCtx.strokeStyle = '#4e6475';
    rightCtx.lineWidth = 1;
    rightCtx.beginPath();

    for (let y = 0; y <= HEIGHT; y += 2) {
      const py = edge(y) + 0.5;
      let tickLen = 0;
      if (y % 50 === 0) tickLen = 8;
      else if (y % 10 === 0) tickLen = 5;
      else if (y % 2 === 0 && zoom >= 3) tickLen = 3;
      if (tickLen > 0) {
        leftCtx.moveTo(rulerW - tickLen, py);
        leftCtx.lineTo(rulerW, py);

        rightCtx.moveTo(0, py);
        rightCtx.lineTo(tickLen, py);
      }
    }
    leftCtx.stroke();
    rightCtx.stroke();

    const rulerFont = '500 10px "Segoe UI", system-ui, -apple-system, sans-serif';
    const cursorFont = '700 10.5px "Segoe UI", system-ui, -apple-system, sans-serif';

    // Static interval coordinates (skip if close to cursor)
    topCtx.font = rulerFont;
    topCtx.fillStyle = '#647c8f';
    topCtx.textAlign = 'center';
    topCtx.textBaseline = 'top';

    bottomCtx.font = rulerFont;
    bottomCtx.fillStyle = '#647c8f';
    bottomCtx.textAlign = 'center';
    bottomCtx.textBaseline = 'bottom';

    for (let x = 50; x < WIDTH; x += 50) {
      if (activeCursorPoint && Math.abs(x - activeCursorPoint.x) < 22) continue;
      const px = edge(x);
      topCtx.fillText(String(x), px, 2);
      bottomCtx.fillText(String(x), px, rulerH - 2);
    }

    leftCtx.font = rulerFont;
    leftCtx.fillStyle = '#647c8f';
    leftCtx.textAlign = 'right';
    leftCtx.textBaseline = 'middle';

    rightCtx.font = rulerFont;
    rightCtx.fillStyle = '#647c8f';
    rightCtx.textAlign = 'left';
    rightCtx.textBaseline = 'middle';

    for (let y = 50; y < HEIGHT; y += 50) {
      if (activeCursorPoint && Math.abs(y - activeCursorPoint.y) < 14) continue;
      const py = edge(y);
      leftCtx.fillText(String(y), rulerW - 11, py);
      rightCtx.fillText(String(y), 11, py);
    }

    // Selected field coordinates / bounds
    const sel = selectedField();
    let selBounds = null;
    if (sel && sel.type !== 'PIXELMAP') {
      if (['LINE', 'RECT', 'ELLIPSE', 'TRIANGLE', 'POLYGON'].includes(sel.type)) {
        const left = Math.min(sel.x, sel.x2 ?? sel.x);
        const top = Math.min(sel.y, sel.y2 ?? sel.y);
        const w = Math.max(1, Math.abs((sel.x2 ?? sel.x) - sel.x));
        const h = Math.max(1, Math.abs((sel.y2 ?? sel.y) - sel.y));
        selBounds = { x1: left, x2: left + w, y1: top, y2: top + h };
      } else {
        const g = computeFieldGeometry(sel);
        selBounds = { x1: g.fieldX, x2: g.fieldX + g.fieldW, y1: g.fieldY, y2: g.fieldY + g.fieldH };
      }
    }

    if (selBounds) {
      const px1 = edge(selBounds.x1) + 0.5;
      const px2 = edge(selBounds.x2) + 0.5;
      const py1 = edge(selBounds.y1) + 0.5;
      const py2 = edge(selBounds.y2) + 0.5;

      // Selection bounds ticks on top/bottom
      topCtx.strokeStyle = '#0084ff';
      topCtx.lineWidth = 1.5;
      topCtx.beginPath();
      topCtx.moveTo(px1, rulerH - 12);
      topCtx.lineTo(px1, rulerH);
      topCtx.moveTo(px2, rulerH - 12);
      topCtx.lineTo(px2, rulerH);
      topCtx.stroke();

      bottomCtx.strokeStyle = '#0084ff';
      bottomCtx.lineWidth = 1.5;
      bottomCtx.beginPath();
      bottomCtx.moveTo(px1, 0);
      bottomCtx.lineTo(px1, 12);
      bottomCtx.moveTo(px2, 0);
      bottomCtx.lineTo(px2, 12);
      bottomCtx.stroke();

      // Selection bounds ticks on left/right
      leftCtx.strokeStyle = '#0084ff';
      leftCtx.lineWidth = 1.5;
      leftCtx.beginPath();
      leftCtx.moveTo(rulerW - 12, py1);
      leftCtx.lineTo(rulerW, py1);
      leftCtx.moveTo(rulerW - 12, py2);
      leftCtx.lineTo(rulerW, py2);
      leftCtx.stroke();

      rightCtx.strokeStyle = '#0084ff';
      rightCtx.lineWidth = 1.5;
      rightCtx.beginPath();
      rightCtx.moveTo(0, py1);
      rightCtx.lineTo(12, py1);
      rightCtx.moveTo(0, py2);
      rightCtx.lineTo(12, py2);
      rightCtx.stroke();
    }

    // Cursor: resalta la celda del píxel (de su borde izquierdo/superior al
    // siguiente), alineada con las marcas y la grilla; el número va centrado.
    if (activeCursorPoint && inside(activeCursorPoint.x, activeCursorPoint.y)) {
      const cx = activeCursorPoint.x;
      const cy = activeCursorPoint.y;
      const x0 = edge(cx);
      const x1 = Math.max(x0 + 1, edge(cx + 1));
      const y0 = edge(cy);
      const y1 = Math.max(y0 + 1, edge(cy + 1));
      const cpx = (x0 + x1) / 2;
      const cpy = (y0 + y1) / 2;
      const band = 'rgba(230, 242, 250, 0.85)';

      topCtx.fillStyle = band;
      topCtx.fillRect(x0, rulerH - 11, x1 - x0, 11);
      topCtx.font = cursorFont;
      topCtx.fillStyle = '#f0f7fd';
      topCtx.fillText(String(cx), cpx, 2);

      bottomCtx.fillStyle = band;
      bottomCtx.fillRect(x0, 0, x1 - x0, 11);
      bottomCtx.font = cursorFont;
      bottomCtx.fillStyle = '#f0f7fd';
      bottomCtx.fillText(String(cx), cpx, rulerH - 2);

      leftCtx.fillStyle = band;
      leftCtx.fillRect(rulerW - 11, y0, 11, y1 - y0);
      leftCtx.font = cursorFont;
      leftCtx.fillStyle = '#f0f7fd';
      leftCtx.fillText(String(cy), rulerW - 13, cpy);

      rightCtx.fillStyle = band;
      rightCtx.fillRect(0, y0, 11, y1 - y0);
      rightCtx.font = cursorFont;
      rightCtx.fillStyle = '#f0f7fd';
      rightCtx.fillText(String(cy), 13, cpy);
    }
  }

  function updateCursor(point) {
    if (!inside(point.x, point.y)) {
      cursorStatus.textContent = 'X: — · Y: —';
      pixelStatus.textContent = 'Pixel: —';
      updateEyedropperLiveUI(point);
      if (activeCursorPoint !== null) {
        activeCursorPoint = null;
        updateCrosshairs(null);
        drawRulers();
      }
      return;
    }
    cursorStatus.textContent = `X: ${point.x} · Y: ${point.y}`;
    pixelStatus.textContent = `Pixel: ${hex565(framebuffer[indexFor(point.x, point.y)])}`;
    updateEyedropperLiveUI(point);

    if (!activeCursorPoint || activeCursorPoint.x !== point.x || activeCursorPoint.y !== point.y) {
      activeCursorPoint = { x: point.x, y: point.y };
      updateCrosshairs(activeCursorPoint);
      drawRulers();
    }
  }

  // Cambia de herramienta como si se pulsara su botón de la barra (así
  // reaccionan también los módulos PIXEL); sin botón, sólo el estado.
  function activateTool(tool) {
    const button = document.querySelector(`.tool[data-tool="${tool}"]`);
    if (button) {
      button.click();
      return;
    }
    selectedTool = tool;
    syncToolUI();
    render();
  }

  let lastSyncedTool = null;

  function syncToolUI() {
    if (selectedTool === 'pick' && lastSyncedTool && lastSyncedTool !== 'pick') toolBeforePick = lastSyncedTool;
    lastSyncedTool = selectedTool;
    document.querySelectorAll('.tool[data-tool]').forEach((button) => {
      button.classList.toggle('active', button.dataset.tool === selectedTool);
    });

    const isVarTool = ['valueField', 'boolField', 'barField', 'navBarField', 'menuField'].includes(selectedTool);
    const varToolBtn = document.getElementById('variableToolBtn');
    if (varToolBtn) {
      varToolBtn.classList.toggle('active', isVarTool);
      if (isVarTool && typeof updateVariableButtonDisplay === 'function') {
        updateVariableButtonDisplay(selectedTool);
      }
    }

    const isTextTool = ['textField', 'rawText'].includes(selectedTool);
    const textToolBtn = document.getElementById('textToolBtn');
    if (textToolBtn) {
      textToolBtn.classList.toggle('active', isTextTool);
      if (isTextTool && typeof updateTextButtonDisplay === 'function') {
        updateTextButtonDisplay(selectedTool);
      }
    }

    const field = selectedField();
    const isRaw = selectedTool === 'rawText' || field?.type === 'RAW_TEXT';
    const isColorTool = (selectedTool === 'pick' || selectedTool === 'fill');
    const fieldTools = ['textField', 'valueField', 'boolField', 'barField', 'pointer'];

    const SHAPE_TYPES = ['LINE', 'RECT', 'ELLIPSE', 'TRIANGLE', 'POLYGON'];
    const isShape = Boolean(field && SHAPE_TYPES.includes(field.type));
    const isNavBar = selectedTool === 'navBarField' || field?.type === 'NAV_BAR';
    const isMenu = field?.type === 'MENU';
    const isIcon = field?.type === 'ICON';
    const iconSection = document.getElementById('iconInspectorSection');
    if (iconSection) {
      iconSection.hidden = !isIcon;
      iconSection.style.display = isIcon ? '' : 'none';
    }
    const menuSection = document.getElementById('menuInspectorSection');
    if (menuSection) {
      menuSection.hidden = !isMenu;
      menuSection.style.display = isMenu ? '' : 'none';
    }
    const navBarSection = document.getElementById('navBarInspectorSection');
    if (navBarSection) {
      navBarSection.hidden = !isNavBar;
      navBarSection.style.display = isNavBar ? '' : 'none';
      if (isNavBar && window.JWPLCHMIPages?.syncNavBarInspector) {
        window.JWPLCHMIPages.syncNavBarInspector();
      }
    }

    if (eyedropperSection) {
      eyedropperSection.hidden = !isColorTool;
      eyedropperSection.style.display = isColorTool ? '' : 'none';
    }
    if (shapeInspectorSection) {
      shapeInspectorSection.hidden = !isShape;
      shapeInspectorSection.style.display = isShape ? '' : 'none';
    }
    if (rawSection) {
      rawSection.hidden = !isRaw;
      rawSection.style.display = isRaw ? '' : 'none';
      if (isRaw) {
        syncRawTextInspector();
      }
    }
    const showField = Boolean(field && fieldTools.includes(selectedTool) && field.type !== 'RAW_TEXT' && !isShape && field.type !== 'NAV_BAR' && !isMenu);
    if (fieldSection) {
      fieldSection.hidden = !showField;
      fieldSection.style.display = showField ? '' : 'none';
    }
    // rawMetricsSection ya vive dentro del details de Tipografia: no necesita hidden dinamico.
    if (fieldMetricsSection) fieldMetricsSection.hidden = !showField;

    if (isShape) {
      syncShapeInspector();
    }

    if (isColorTool) {
      updateEyedropperActiveUI();
      renderRecentColors();
      syncColorToolInspector();
    }

    if (field) {
      const isValue = field.type === 'VALUE';
      const isShape = ['LINE', 'RECT', 'ELLIPSE', 'TRIANGLE', 'POLYGON'].includes(field.type);
      const isClosedShape = ['RECT', 'ELLIPSE', 'TRIANGLE', 'POLYGON'].includes(field.type);
      if (fieldInspectorTitle) fieldInspectorTitle.textContent = `Inspector · ${field.type} field`;
      if (numericFormatDetails) numericFormatDetails.hidden = !isValue;
      if (fieldCapacityWrap) fieldCapacityWrap.hidden = isValue || isShape;
      if (fieldCppType) fieldCppType.value = isValue ? 'float' : (isShape ? 'none' : 'char[]');
      
      if (fieldX2Wrap) fieldX2Wrap.hidden = !isShape;
      if (fieldY2Wrap) fieldY2Wrap.hidden = !isShape;
      if (fieldSidesWrap) fieldSidesWrap.hidden = field.type !== 'POLYGON';
      if (fieldFillWrap) fieldFillWrap.hidden = !isClosedShape;
      if (fieldFillColorWrap) fieldFillColorWrap.hidden = !isClosedShape || !field.fill;
      const fieldValueBoundsWrap = document.getElementById('fieldValueBoundsWrap');
      if (fieldValueBoundsWrap) fieldValueBoundsWrap.hidden = isShape;
      if (fieldMetricsSection) fieldMetricsSection.hidden = isShape || !fieldTools.includes(selectedTool);
    }
    window.dispatchEvent(new CustomEvent('jwplc:tool-changed', { detail: { tool: selectedTool } }));
  }

  function updateActiveColorUI() {
    activeColorSwatch.style.background = rgb565ToCss(selectedColor.value);
    activeColorName.textContent = selectedColor.name;
    activeColorValue.textContent = hex565(selectedColor.value);
    updateEyedropperActiveUI();
    renderRecentColors();
  }

  function buildColorSelect(select, selectedName, allowTransparent = false) {
    if (!select) return;
    select.innerHTML = '';
    if (allowTransparent) {
      const transOpt = document.createElement('option');
      transOpt.value = 'TRANSPARENT';
      transOpt.textContent = 'TRANSPARENTE (Fondo libre)';
      transOpt.selected = selectedName === 'TRANSPARENT';
      select.appendChild(transOpt);
    }
    COLORS.forEach((color) => {
      const option = document.createElement('option');
      option.value = color.name;
      option.textContent = `${color.name} · ${hex565(color.value)}`;
      option.selected = color.name === selectedName;
      select.appendChild(option);
    });
  }

  const visualColorSyncs = [];

  function attachVisualColorPicker(select, getValue, setValue) {
    const label = select?.closest('label');
    if (!select || !label || label.dataset.hasPicker === '1') return () => {};
    label.dataset.hasPicker = '1';

    const host = document.createElement('div');
    host.className = 'a11-custom-color';
    const row = document.createElement('div');
    row.className = 'a11-color-control';
    const transSwatch = document.createElement('span');
    transSwatch.className = 'a11-trans-swatch';
    transSwatch.title = 'Fondo transparente';
    transSwatch.style.display = 'none';
    const visual = document.createElement('input');
    visual.type = 'color';
    visual.title = 'Selector visual interactivo RGB';
    const hexBadge = document.createElement('span');
    hexBadge.className = 'a11-hex-badge';
    row.append(transSwatch, visual, hexBadge);
    host.appendChild(row);
    select.insertAdjacentElement('afterend', host);

    function sync() {
      const raw = getValue();
      if (raw === 'TRANSPARENT' || raw === -1) {
        visual.style.display = 'none';
        transSwatch.style.display = 'inline-block';
        hexBadge.textContent = 'Transparente';
        hexBadge.style.color = '#8ea3b3';
        hexBadge.title = 'Sin fondo (transparente)';
        ensureSelectOption(select, 'TRANSPARENT');
        return;
      }
      visual.style.display = '';
      transSwatch.style.display = 'none';
      const val = Number(raw) & 0xFFFF;
      const hex888 = rgb565ToHex888(val);
      visual.value = hex888;
      hexBadge.textContent = hex565(val);
      hexBadge.style.color = '#52c9ff';
      hexBadge.title = `${hex565(val)} · ${hex888.toUpperCase()}`;
      ensureSelectOption(select, val);
    }

    visual.addEventListener('input', () => {
      const c565 = hex888ToRgb565(visual.value);
      setValue(c565);
      ensureSelectOption(select, c565);
      sync();
      render();
    });

    visual.addEventListener('change', () => {
      const c565 = hex888ToRgb565(visual.value);
      setValue(c565);
      ensureSelectOption(select, c565);
      addRecentColor(c565);
      sync();
      render();
      commitHistory();
    });

    select.addEventListener('change', () => {
      if (select.value === 'TRANSPARENT') {
        setValue('TRANSPARENT');
        sync();
        render();
        commitHistory();
        return;
      }
      if (select.value === 'CUSTOM') {
        return;
      }
      const matched = colorByName(select.value);
      const val = matched ? matched.value : (Number(select.value) || 0);
      setValue(val);
      addRecentColor(val);
      sync();
      render();
      commitHistory();
    });

    visualColorSyncs.push(sync);
    sync();
    return sync;
  }

  function syncAllColorControls() {
    visualColorSyncs.forEach((fn) => fn());
  }

  function buildPalette() {
    palette.innerHTML = '';
    COLORS.forEach((color) => {
      const button = document.createElement('button');
      button.className = 'palette-button';
      button.title = `${color.name} ${hex565(color.value)}`;
      button.style.background = rgb565ToCss(color.value);
      button.classList.toggle('active', selectedColor.name === color.name);
      button.addEventListener('click', () => {
        selectedColor = color;
        if (selectedTool === 'rawText') rawState.foreground = color.value;
        const field = selectedField();
        if (field) {
          if (['LINE', 'RECT', 'ELLIPSE', 'TRIANGLE', 'POLYGON'].includes(field.type)) {
            field.frameColor = color.value;
            syncShapeInspector();
          } else if (field.type === 'RAW_TEXT') {
            field.textColor = color.value;
          } else if (['TEXT', 'VALUE', 'BOOL', 'BAR'].includes(field.type)) {
            field.valueColor = color.value;
          }
          syncInputsFromState();
        } else if (window.JWPLCHMIPixelMaps?.getSelected?.()) {
          window.JWPLCHMIPixelMaps.setActiveColor(color.value);
        }
        updateActiveColorUI();
        buildPalette();
        render();
        commitHistory();
      });
      palette.appendChild(button);
    });
  }

  function buildShapeCppSnippet(field) {
    if (!field) return '';
    // Misma salida que el header generado (designer-codegen.js).
    if (window.JWPLCHMICodegen?.staticDrawSnippet) return window.JWPLCHMICodegen.staticDrawSnippet(field);
    const tft = 'JWPLC_Display.tft().';
    const c = hex565(field.frameColor ?? 0xFFFF);
    const fc = hex565(field.fillColor ?? field.frameColor ?? 0xFFFF);
    const hasBorder = field.borderEnabled !== false;
    const hasFill = Boolean(field.fill);
    const rot = Number(field.rotation) || 0;
    const maxR = getMaxCornerRadius(field);
    const r = clamp(Math.round(Number(field.cornerRadius) || 0), 0, maxR);

    if (field.type === 'LINE') {
      const pts = getShapeTransformedVertices(field);
      const p0 = pts[0] || { x: field.x, y: field.y };
      const p1 = pts[1] || { x: field.x2, y: field.y2 };
      return `${tft}drawLine(${p0.x}, ${p0.y}, ${p1.x}, ${p1.y}, ${c});`;
    }
    if (field.type === 'RECT' && rot === 0) {
      const minX = Math.min(field.x, field.x2);
      const minY = Math.min(field.y, field.y2);
      const w = Math.abs(field.x2 - field.x);
      const h = Math.abs(field.y2 - field.y);
      const cmds = [];
      if (hasFill) {
        cmds.push(r > 0
          ? `${tft}fillRoundRect(${minX}, ${minY}, ${w}, ${h}, ${r}, ${fc});`
          : `${tft}fillRect(${minX}, ${minY}, ${w}, ${h}, ${fc});`);
      }
      if (hasBorder) {
        cmds.push(r > 0
          ? `${tft}drawRoundRect(${minX}, ${minY}, ${w}, ${h}, ${r}, ${c});`
          : `${tft}drawRect(${minX}, ${minY}, ${w}, ${h}, ${c});`);
      }
      return cmds.join('\n') || '// Sin trazo ni relleno activo';
    }
    if (field.type === 'ELLIPSE' && rot === 0) {
      const cx = Math.round((field.x + field.x2) / 2);
      const cy = Math.round((field.y + field.y2) / 2);
      const rx = Math.round(Math.abs(field.x2 - field.x) / 2);
      const ry = Math.round(Math.abs(field.y2 - field.y) / 2);
      const rad = Math.max(rx, ry);
      const cmds = [];
      if (hasFill) cmds.push(`${tft}fillCircle(${cx}, ${cy}, ${rad}, ${fc});`);
      if (hasBorder) cmds.push(`${tft}drawCircle(${cx}, ${cy}, ${rad}, ${c});`);
      return cmds.join('\n') || '// Sin trazo ni relleno activo';
    }
    if (field.type === 'TRIANGLE' && rot === 0 && r <= 0 && !field.flipH && !field.flipV) {
      const x1 = Math.round((field.x + field.x2) / 2), y1 = field.y;
      const x2 = field.x, y2 = field.y2;
      const x3 = field.x2, y3 = field.y2;
      const cmds = [];
      if (hasFill) cmds.push(`${tft}fillTriangle(${x1}, ${y1}, ${x2}, ${y2}, ${x3}, ${y3}, ${fc});`);
      if (hasBorder) cmds.push(`${tft}drawTriangle(${x1}, ${y1}, ${x2}, ${y2}, ${x3}, ${y3}, ${c});`);
      return cmds.join('\n') || '// Sin trazo ni relleno activo';
    }
    const pts = getShapeTransformedVertices(field);
    const cmds = [];
    for (let i = 0; i < pts.length; i++) {
      const p1 = pts[i];
      const p2 = pts[(i + 1) % pts.length];
      cmds.push(`${tft}drawLine(${p1.x}, ${p1.y}, ${p2.x}, ${p2.y}, ${c});`);
    }
    if (cmds.length > 4) {
      return `${cmds.slice(0, 3).join('\n')}\n// ... (+${cmds.length - 3} segmentos vectoriales)`;
    }
    return cmds.join('\n');
  }

  function syncShapeInspector() {
    const field = selectedField();
    if (!field) return;
    const isLine = field.type === 'LINE';
    const SHAPE_ICONS = { LINE: '\u2571', RECT: '\u25a1', ELLIPSE: '\u2b2d', TRIANGLE: '\u25b3', POLYGON: '\u2394' };
    if (shapeInspectorIcon) shapeInspectorIcon.textContent = SHAPE_ICONS[field.type] || '\u25a1';
    const displayName = field.name || (field.type.toLowerCase());
    if (shapeInspectorName && document.activeElement !== shapeInspectorName) {
      shapeInspectorName.value = displayName;
    }
    const headerTitle = document.getElementById('shapeInspectorHeaderTitle');
    const curFields = selectedFields();
    const curPms = (window.JWPLCHMIPixelMaps?.getSelectedMaps?.() || []).filter(
      (pm) => Number(pm.page || 0) === activePage && pm.editorVisible !== false && pm.pixels?.length
    );
    const selCount = curFields.length + curPms.length;
    if (headerTitle) {
      headerTitle.textContent = selCount > 1
        ? `${displayName.toUpperCase()} (${selCount} seleccionados)`
        : displayName.toUpperCase();
    }

    // Active shape type button
    document.querySelectorAll('#shapeTypeSelectorGrid [data-shape-type]').forEach((btn) => {
      btn.classList.toggle('active', btn.dataset.shapeType === field.type);
    });

    const x1 = field.x ?? 0; const y1 = field.y ?? 0;
    const x2 = field.x2 ?? x1; const y2 = field.y2 ?? y1;
    const cx = (x1 + x2) / 2; const cy = (y1 + y2) / 2;
    if (shapeX && document.activeElement !== shapeX) shapeX.value = Math.round(cx);
    if (shapeY && document.activeElement !== shapeY) shapeY.value = Math.round(cy);
    if (shapeW && document.activeElement !== shapeW) shapeW.value = Math.abs(x2 - x1) || 1;
    if (shapeH && document.activeElement !== shapeH) shapeH.value = Math.abs(y2 - y1) || 1;

    // Corner Radius (RECT, TRIANGLE, POLYGON)
    const supportsCorners = ['RECT', 'TRIANGLE', 'POLYGON'].includes(field.type);
    const cornerCard = document.getElementById('shapeCornerCard');
    if (cornerCard) cornerCard.hidden = !supportsCorners;
    if (supportsCorners) {
      const maxR = getMaxCornerRadius(field);
      const r = clamp(Math.round(Number(field.cornerRadius) || 0), 0, maxR);
      field.cornerRadius = r;
      const cornerInput = document.getElementById('shapeCornerRadius');
      const cornerMaxHint = document.getElementById('shapeCornerMaxHint');
      if (cornerInput && document.activeElement !== cornerInput) {
        cornerInput.max = String(maxR);
        cornerInput.value = String(r);
      }
      if (cornerMaxHint) cornerMaxHint.textContent = `Máx: ${maxR} px`;
      document.querySelectorAll('#shapeCornerPresetGrid [data-corner-preset]').forEach((btn) => {
        const p = btn.dataset.cornerPreset;
        const val = p === 'max' ? maxR : Number(p);
        btn.classList.toggle('active', val === r);
      });
    }

    // Polygon sides
    if (shapeSidesWrap2) shapeSidesWrap2.hidden = field.type !== 'POLYGON';
    const sides = Number(field.sides) || 5;
    if (shapeSides2 && document.activeElement !== shapeSides2) shapeSides2.value = sides;
    document.querySelectorAll('#shapeSidesPresetGrid [data-sides-preset]').forEach((btn) => {
      btn.classList.toggle('active', Number(btn.dataset.sidesPreset) === sides);
    });

    // Rotation
    const rot = Number(field.rotation) || 0;
    if (shapeRotation && document.activeElement !== shapeRotation) shapeRotation.value = rot;
    document.querySelectorAll('#shapeRotPresetGrid [data-rot-preset]').forEach((btn) => {
      btn.classList.toggle('active', Number(btn.dataset.rotPreset) === rot);
    });

    // Stroke / Border
    if (shapeBorderLabel) shapeBorderLabel.textContent = isLine ? 'Línea' : 'Borde';
    if (shapeBorderToggleWrap) shapeBorderToggleWrap.hidden = isLine;
    if (shapeBorderEnabled) {
      if (isLine) field.borderEnabled = true;
      shapeBorderEnabled.checked = field.borderEnabled !== false;
    }
    const strokeSize = Number(field.size) || 1;
    if (shapeBorderSize && document.activeElement !== shapeBorderSize) shapeBorderSize.value = strokeSize;
    document.querySelectorAll('#shapeStrokePresetGrid [data-stroke-preset]').forEach((btn) => {
      btn.classList.toggle('active', Number(btn.dataset.strokePreset) === strokeSize);
    });

    // Fill (hidden for lines)
    if (shapeFillRow) shapeFillRow.hidden = isLine;
    if (shapeFillEnabled) shapeFillEnabled.checked = !isLine && !!field.fill;

    // Apariencia: Editor de color fijo (designer-color-editor.js)
    if (isLine && shapeActiveColorKey === 'fill') shapeActiveColorKey = 'border';
    const isLight565 = (v) => ((v >> 11) + ((v >> 5) & 0x3F) / 2 + (v & 0x1F)) > 40;
    const fc = field.frameColor ?? 0xFFFF;
    const fillC = field.fillColor ?? 0xFFE0;

    const bSwatch = document.querySelector('[data-shape-swatch="border"]');
    const bRow = document.querySelector('[data-shape-legend="border"]');
    if (bSwatch) {
      bSwatch.style.background = rgb565ToCss(fc);
      bSwatch.style.color = isLight565(fc) ? '#000' : '#fff';
      bSwatch.classList.toggle('active', shapeActiveColorKey === 'border');
    }
    if (bRow) bRow.classList.toggle('is-active', shapeActiveColorKey === 'border');

    const fSwatch = document.querySelector('[data-shape-swatch="fill"]');
    const fRow = document.querySelector('[data-shape-legend="fill"]');
    if (fSwatch) {
      fSwatch.style.background = rgb565ToCss(fillC);
      fSwatch.style.color = isLight565(fillC) ? '#000' : '#fff';
      fSwatch.classList.toggle('active', shapeActiveColorKey === 'fill');
    }
    if (fRow) fRow.classList.toggle('is-active', shapeActiveColorKey === 'fill');

    initShapeColorEditor();
    if (shapeColorEditor) {
      const activeCol = (shapeActiveColorKey === 'border') ? fc : fillC;
      const activeName = (shapeActiveColorKey === 'border') ? (isLine ? 'Línea' : 'Borde') : 'Relleno';
      shapeColorEditor.set(activeCol, activeName);
    }

    // Tech tab C++ snippet preview
    const techPreview = document.getElementById('shapeTechCodePreview');
    if (techPreview) techPreview.textContent = buildShapeCppSnippet(field);
  }

  function syncRawTabPanes() {
    const mainTabs = document.querySelectorAll('#rawMainTabs [data-raw-tab]');
    mainTabs.forEach((btn) => btn.classList.toggle('active', btn.dataset.rawTab === activeRawMainTab));

    const subTabsBar = document.getElementById('rawSubTabs');
    const isDesign = activeRawMainTab === 'design';
    if (subTabsBar) {
      subTabsBar.hidden = !isDesign;
      subTabsBar.style.display = isDesign ? 'flex' : 'none';
    }

    const subTabs = document.querySelectorAll('#rawSubTabs [data-raw-subtab]');
    subTabs.forEach((btn) => btn.classList.toggle('active', isDesign && btn.dataset.rawSubtab === activeRawSubTab));

    const panels = {
      form: document.getElementById('rawPaneForm'),
      content: document.getElementById('rawPaneContent'),
      appearance: document.getElementById('rawPaneAppearance'),
      placement: document.getElementById('rawPanePlacement'),
      data: document.getElementById('rawPaneData'),
      tech: document.getElementById('rawPaneTech')
    };

    const activePanelKey = isDesign ? activeRawSubTab : activeRawMainTab;

    Object.entries(panels).forEach(([key, panel]) => {
      if (!panel) return;
      const isVisible = key === activePanelKey;
      panel.hidden = !isVisible;
      panel.style.display = isVisible ? 'flex' : 'none';
    });

    if (isDesign && activeRawSubTab === 'appearance') {
      requestAnimationFrame(() => syncRawAppearance());
    }
  }

  function syncRawPages(currentField) {
    const host = document.getElementById('rawPageChipRow');
    if (!host) return;
    const current = Number(currentField?.page ?? activePage);
    const signature = hmiPages.map((p) => `${p.id}:${p.name}`).join('|') + `#${current}`;
    if (host.dataset.signature === signature) return;
    host.dataset.signature = signature;
    host.innerHTML = hmiPages.map((p) => {
      const active = Number(p.id) === current;
      const title = p.name ? `Página ${p.id}: ${p.name}` : `Página ${p.id}`;
      return `<button type="button" class="insp-chip${active ? ' active' : ''}" data-raw-page-id="${p.id}" title="${title}">${String(Number(p.id) + 1).padStart(2, '0')} · ${p.name || ('Página ' + (Number(p.id) + 1))}</button>`;
    }).join('');
  }

  function syncRawAppearance() {
    const field = selectedField();
    const isRawField = field && field.type === 'RAW_TEXT';
    const text = isRawField ? (field.text || '') : (rawState.value || '');
    const font = isRawField ? field.font : rawState.font;
    const size = Number(isRawField ? field.size : rawState.size) || 1;
    const isTrans = isRawField ? Boolean(field.transparentBackground) : Boolean(rawState.transparent);
    const fg = isRawField ? (field.textColor ?? 0xFFFF) : rawState.foreground;
    const bg = isRawField ? (field.backgroundColor ?? 0x0000) : rawState.background;

    const isLight = (v) => ((v >> 11) + ((v >> 5) & 0x3F) / 2 + (v & 0x1F)) > 40;

    const tSwatch = document.querySelector('[data-raw-swatch="textColor"]');
    const tRow = document.querySelector('[data-raw-swatch-row="textColor"]');
    if (tSwatch) {
      tSwatch.style.background = rgb565ToCss(fg);
      tSwatch.style.color = isLight(fg) ? '#000' : '#fff';
      tSwatch.classList.toggle('active', rawActiveColorKey === 'textColor');
    }
    if (tRow) tRow.classList.toggle('is-active', rawActiveColorKey === 'textColor');

    const fSwatch = document.querySelector('[data-raw-swatch="bgColor"]');
    const fRow = document.querySelector('[data-raw-swatch-row="bgColor"]');
    if (fSwatch) {
      fSwatch.style.background = rgb565ToCss(bg);
      fSwatch.style.color = isLight(bg) ? '#000' : '#fff';
      fSwatch.classList.toggle('active', rawActiveColorKey === 'bgColor');
      if (isTrans) {
        fSwatch.style.border = '1px dashed rgba(255, 255, 255, 0.6)';
        fSwatch.title = 'Color del fondo (sólo modo opaco — clic para activar)';
      } else {
        fSwatch.style.border = '';
        fSwatch.title = 'Color del fondo';
      }
    }
    if (fRow) {
      fRow.classList.toggle('is-active', rawActiveColorKey === 'bgColor');
      fRow.style.opacity = isTrans ? '0.75' : '1';
    }

    const canvas = document.getElementById('rawCaseCanvas');
    const body = document.getElementById('rawCaseBody');
    if (canvas && body && body.offsetParent !== null) {
      const pad = isTrans ? 0 : RAW_BG_PAD;
      const previewField = {
        type: 'RAW_TEXT',
        text: text || 'Texto RAW',
        font,
        size,
        transparentBackground: isTrans,
        textColor: fg,
        backgroundColor: bg,
        x: pad,
        y: pad
      };
      const out = renderFieldPreview(previewField);
      if (out && out.g) {
        const w = Math.max(1, Math.min(out.width, Math.ceil(out.g.fieldW)));
        const h = Math.max(1, Math.min(out.height, Math.ceil(out.g.fieldH)));
        const legend = body.querySelector('.menu-case-legend');
        const gutter = (window.JWPLCZoneIndicator?.CHANNEL ?? 12) + 14;
        const bodyW = body.clientWidth || 280;
        const legendW = legend ? legend.offsetWidth : 60;
        const fit = (avail) => Math.max(0, Math.min(3, Math.floor(avail / w)));
        const rightScale = fit(bodyW - legendW - gutter);
        const belowScale = fit(bodyW - gutter);
        const below = rightScale < 2 && belowScale > rightScale;
        body.classList.toggle('is-below', below);
        const scale = Math.max(1, below ? belowScale : rightScale);
        canvas.width = w * scale;
        canvas.height = h * scale;
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
            img.data[o]     = ((v >> 11) & 0x1F) * 255 / 31;
            img.data[o + 1] = ((v >> 5) & 0x3F) * 255 / 63;
            img.data[o + 2] = (v & 0x1F) * 255 / 31;
            img.data[o + 3] = 255;
          }
        }
        ctx.putImageData(img, 0, 0);

        const tb = rawTextBounds(previewField.text, size, font);
        const zones = {
          textColor: [{ x: pad, y: pad, w: Math.max(1, tb.width), h: Math.max(1, tb.height) }],
          bgColor: [{ x: 0, y: 0, w, h, inset: true }]
        };
        const rects = zones[rawActiveColorKey] || [];
        window.JWPLCZoneIndicator?.draw(body, canvas, rects);
      }
    }

    initRawColorEditor();
    if (rawColorEditor) {
      const activeCol = (rawActiveColorKey === 'textColor') ? fg : bg;
      const activeName = (rawActiveColorKey === 'textColor') ? 'Texto' : 'Fondo';
      rawColorEditor.set(activeCol, activeName);
    }
  }

  function syncRawTextInspector() {
    const field = selectedField();
    const isRawField = field && field.type === 'RAW_TEXT';
    const textVal = isRawField ? (field.text || '') : (rawState.value || '');
    const fFont = isRawField ? field.font : rawState.font;
    const s = Number(isRawField ? field.size : rawState.size) || 1;
    const isTrans = isRawField ? Boolean(field.transparentBackground) : Boolean(rawState.transparent);
    const fg = isRawField ? (field.textColor ?? 0xFFFF) : rawState.foreground;
    const bg = isRawField ? (field.backgroundColor ?? 0x0000) : rawState.background;
    const curX = isRawField ? (field.x ?? 0) : (rawState.x ?? 0);
    const curY = isRawField ? (field.y ?? 0) : (rawState.y ?? 0);

    // Header title
    const headerTitle = document.getElementById('rawTextHeaderTitle');
    if (headerTitle) {
      headerTitle.textContent = (isRawField && field.name) ? field.name : (textVal ? `"${textVal.slice(0, 16)}"` : 'Texto RAW');
    }

    // Tabs
    syncRawTabPanes();

    // Forma
    if (rawTextFont && window.JWPLCDesignerFonts && document.activeElement !== rawTextFont) {
      rawTextFont.value = window.JWPLCDesignerFonts.normalizeId(fFont);
    }
    document.querySelectorAll('#rawScaleChipRow [data-raw-scale]').forEach((chip) => {
      chip.classList.toggle('active', Number(chip.dataset.rawScale) === s);
    });
    if (rawTextSize) rawTextSize.value = String(s);
    if (typeof updateScalePxHint === 'function') updateScalePxHint();

    document.querySelectorAll('#rawBgModeGrid [data-raw-bgmode]').forEach((btn) => {
      btn.classList.toggle('active', (btn.dataset.rawBgmode === 'transparent') === isTrans);
    });
    if (rawTextTransparentToggle) rawTextTransparentToggle.checked = !isTrans;
    if (rawTextTransparent) rawTextTransparent.value = isTrans ? '1' : '0';
    const bgNote = document.getElementById('rawBgModeNote');
    if (bgNote) {
      bgNote.textContent = isTrans
        ? 'Texto directo sobre el fondo de la pantalla o capas inferiores.'
        : 'Fondo rectangular con +2 px de relleno alrededor del texto.';
    }

    // Contenido
    if (rawTextInput && document.activeElement !== rawTextInput) rawTextInput.value = textVal;
    const charCounter = document.getElementById('rawCharCounter');
    if (charCounter) charCounter.textContent = `${textVal.length}/40`;
    const rulerText = document.getElementById('rawRulerText');
    if (rulerText) rulerText.textContent = textVal || 'Texto RAW';
    const tb = rawTextBounds(textVal || 'Texto RAW', s, fFont);
    const rulerPx = document.getElementById('rawRulerPx');
    if (rulerPx) rulerPx.textContent = `${tb.width} × ${tb.height} px`;

    // Apariencia
    syncRawAppearance();

    // Ubicacion
    syncRawPages(field);
    if (rawTextX && document.activeElement !== rawTextX) rawTextX.value = String(curX);
    if (rawTextY && document.activeElement !== rawTextY) rawTextY.value = String(curY);
    const b = isRawField ? getFieldBounds(field) : { width: tb.width, height: tb.height };
    const boundsStatus = document.getElementById('rawBoundsStatus');
    if (boundsStatus) {
      boundsStatus.textContent = `X: ${curX}..${curX + b.width}, Y: ${curY}..${curY + b.height} (${b.width} × ${b.height} px)`;
    }

    // Sincronizar estado visual de la matriz 3×3 de alineacion
    const curPad = isTrans ? 0 : RAW_BG_PAD;
    const curMinX = curX - curPad;
    const curMinY = curY - curPad;
    const curMaxX = curX + tb.width - 1 + curPad;
    const curMaxY = curY + tb.height - 1 + curPad;
    const isL = curMinX === 0;
    const isR = curMaxX === WIDTH - 1;
    const isCX = Math.abs(curMinX - Math.round((WIDTH - b.width) / 2)) <= 1;
    const isT = curMinY === 0;
    const isB = curMaxY === HEIGHT - 1;
    const isCY = Math.abs(curMinY - Math.round((HEIGHT - b.height) / 2)) <= 1;

    let matchedAlign = null;
    if (isT && isL) matchedAlign = 'top-left';
    else if (isT && isCX) matchedAlign = 'top';
    else if (isT && isR) matchedAlign = 'top-right';
    else if (isCY && isL) matchedAlign = 'left';
    else if (isCY && isCX) matchedAlign = 'center';
    else if (isCY && isR) matchedAlign = 'right';
    else if (isB && isL) matchedAlign = 'bottom-left';
    else if (isB && isCX) matchedAlign = 'bottom';
    else if (isB && isR) matchedAlign = 'bottom-right';

    document.querySelectorAll('#rawAlignMatrix3x3 [data-align]').forEach((btn) => {
      btn.classList.toggle('active', btn.dataset.align === matchedAlign);
    });

    // Tecnico
    if (rawTextName && document.activeElement !== rawTextName) {
      rawTextName.value = (isRawField && field.name) ? field.name : 'Texto RAW';
    }
    const rawTextId = document.getElementById('rawTextId');
    if (rawTextId) {
      rawTextId.value = (isRawField && field.id) ? field.id : 'FIELD_RAW_1';
    }
    if (rawInspectorContract) {
      const curF = isRawField ? field : {
        type: 'RAW_TEXT',
        text: textVal,
        x: curX,
        y: curY,
        font: fFont,
        size: s,
        transparentBackground: isTrans,
        textColor: fg,
        backgroundColor: bg
      };
      const snippet = window.JWPLCHMICodegen?.staticDrawSnippet
        ? window.JWPLCHMICodegen.staticDrawSnippet(curF)
        : `JWPLC_Display.getTFT()->setCursor(${curF.x}, ${curF.y});\nJWPLC_Display.getTFT()->print("${curF.text || ''}");`;
      rawInspectorContract.textContent = snippet;
    }
    const diagFont = document.getElementById('rawDiagFont');
    if (diagFont) {
      const fontObj = window.JWPLCDesignerFonts?.get(fFont);
      diagFont.textContent = fontObj ? fontObj.label : (fFont || 'GLCD 5×7');
    }
    const diagScale = document.getElementById('rawDiagScale');
    if (diagScale) diagScale.textContent = `${s}×`;
    const diagBg = document.getElementById('rawDiagBg');
    if (diagBg) diagBg.textContent = isTrans ? 'Transparente' : 'Opaco (+2 px)';
    const diagBounds = document.getElementById('rawDiagBounds');
    if (diagBounds) diagBounds.textContent = `${b.width} × ${b.height} px`;
  }

  function syncInputsFromState() {
    const field = selectedField();

    if (field && field.type === 'RAW_TEXT') {
      const isTrans = Boolean(field.transparentBackground);
      if (rawTextTransparent) rawTextTransparent.value = isTrans ? '1' : '0';
      if (rawTextTransparentToggle) rawTextTransparentToggle.checked = !isTrans;
      if (rawTextBgColorWrap) rawTextBgColorWrap.style.display = isTrans ? 'none' : 'flex';

      const fg = field.textColor ?? 0xFFFF;
      const bg = field.backgroundColor ?? 0x0000;
      if (rawTextColorSwatch) rawTextColorSwatch.style.background = rgb565ToCss(fg);
      if (rawTextColorInput) rawTextColorInput.value = rgb565ToHex888(fg);
      if (rawTextColorCode) rawTextColorCode.textContent = rgb565ToHex888(fg).toUpperCase();
      if (rawTextBgColorSwatch) rawTextBgColorSwatch.style.background = rgb565ToCss(bg);
      if (rawTextBgColorInput) rawTextBgColorInput.value = rgb565ToHex888(bg);
      if (rawTextBgColorCode) rawTextBgColorCode.textContent = rgb565ToHex888(bg).toUpperCase();

      if (rawTextColor) ensureSelectOption(rawTextColor, fg);
      if (rawTextBackground) ensureSelectOption(rawTextBackground, bg);
      if (rawTextBackgroundWrap) rawTextBackgroundWrap.hidden = isTrans;
      syncAllColorControls();

      syncRawTextInspector();
      inspectorContract.textContent = '// Texto RAW estático: se dibuja en jwplcUIPageEnter() con JWPLC_Display.tft().print(...)';
      return;
    }

    if (!field) {
      if (document.activeElement !== rawTextInput) rawTextInput.value = rawState.value;
      if (document.activeElement !== rawTextX) rawTextX.value = String(rawState.x);
      if (document.activeElement !== rawTextY) rawTextY.value = String(rawState.y);
      if (rawTextSize) rawTextSize.value = String(rawState.size);
      if (rawTextName && document.activeElement !== rawTextName) rawTextName.value = 'Texto RAW';
      if (rawTextTransparent) rawTextTransparent.value = rawState.transparent ? '1' : '0';
      if (rawTextTransparentToggle) rawTextTransparentToggle.checked = !Boolean(rawState.transparent);
      if (rawTextBgColorWrap) rawTextBgColorWrap.style.display = rawState.transparent ? 'none' : 'flex';
      if (rawTextColorSwatch) rawTextColorSwatch.style.background = rgb565ToCss(rawState.foreground);
      if (rawTextColorInput) rawTextColorInput.value = rgb565ToHex888(rawState.foreground);
      if (rawTextColorCode) rawTextColorCode.textContent = rgb565ToHex888(rawState.foreground).toUpperCase();
      if (rawTextBgColorSwatch) rawTextBgColorSwatch.style.background = rgb565ToCss(rawState.background);
      if (rawTextBgColorInput) rawTextBgColorInput.value = rgb565ToHex888(rawState.background);
      if (rawTextBgColorCode) rawTextBgColorCode.textContent = rgb565ToHex888(rawState.background).toUpperCase();
      if (rawTextColor) ensureSelectOption(rawTextColor, rawState.foreground);
      if (rawTextBackground) ensureSelectOption(rawTextBackground, rawState.background);
      if (rawTextBackgroundWrap) rawTextBackgroundWrap.hidden = Boolean(rawState.transparent);

      inspectorContract.textContent = 'Sin objeto seleccionado';
      if (rawInspectorContract) rawInspectorContract.textContent = '// Sin objeto seleccionado';
      syncAllColorControls();
      if (selectedTool === 'rawText') {
        syncRawTextInspector();
      }
      return;
    }


    // Shape fields: hide old inspector immediately and sync shape inspector only
    if (['LINE', 'RECT', 'ELLIPSE', 'TRIANGLE', 'POLYGON'].includes(field.type)) {
      if (fieldSection) { fieldSection.hidden = true; fieldSection.style.display = 'none'; }
      if (fieldMetricsSection) fieldMetricsSection.hidden = true;
      syncShapeInspector();
      inspectorContract.textContent = `// Objeto estático: no requiere variable de estado`;
      syncAllColorControls();
      return;
    }

    fieldName.value = field.name || '';
    fieldId.value = field.id || '';
    fieldVariable.value = field.variable || '';
    fieldCapacity.value = String(field.capacity || 12);
    fieldX.value = String(field.x);
    fieldY.value = String(field.y);
    if (field.x2 !== undefined) fieldX2.value = String(field.x2);
    if (field.y2 !== undefined) fieldY2.value = String(field.y2);
    if (field.sides !== undefined) fieldSides.value = String(field.sides);
    if (fieldFill) fieldFill.value = field.fill ? '1' : '0';
    if (fieldFillColor) ensureSelectOption(fieldFillColor, field.fillColor ?? field.frameColor ?? 0xFFFF);
    if (fieldFillColorWrap) fieldFillColorWrap.hidden = !field.fill;
    fieldPreview.value = String(field.preview ?? '');
    fieldLabel.value = field.label || '';
    fieldUnit.value = field.unit || '';
    fieldValueSize.value = String(field.valueSize || 1);
    fieldLabelSize.value = String(field.labelSize || 1);
    fieldFrame.value = field.frame ? '1' : '0';
    const frameColorWrap = fieldFrameColor?.closest('label');
    if (frameColorWrap) frameColorWrap.style.display = field.frame ? '' : 'none';
    fieldLayout.value = field.layout || 'INLINE';
    if (fieldAlign) fieldAlign.value = field.align || 'LEFT';
    ensureSelectOption(fieldLabelColor, field.labelColor ?? 0xFFFF);
    ensureSelectOption(fieldValueColor, field.valueColor ?? 0xFFFF);
    ensureSelectOption(fieldBackgroundColor, field.backgroundColor ?? 'TRANSPARENT');
    ensureSelectOption(fieldFrameColor, field.frameColor ?? 0xFFFF);
    syncAllColorControls();

    if (field.type === 'VALUE') {
      if (fieldIntegerDigits) fieldIntegerDigits.value = String(field.integerDigits);
      if (fieldDecimalDigits) fieldDecimalDigits.value = String(field.decimalDigits);
      if (fieldSigned) fieldSigned.value = field.signedValue ? '1' : '0';
      if (fieldLeadingZeros) fieldLeadingZeros.value = field.leadingZeros ? '1' : '0';
      inspectorContract.textContent = window.JWPLCHMICodegenCore.valueDeclaration(field, sanitizeSymbol(field.variable, 'valor'));
    } else if (field.type === 'BOOL') {
      inspectorContract.textContent = `bool ${sanitizeSymbol(field.variable, 'estado')} = false;`;
    } else if (field.type === 'BAR') {
      inspectorContract.textContent = `float ${sanitizeSymbol(field.variable, 'nivel')} = 0.0f;`;
    } else if (['LINE', 'RECT', 'ELLIPSE', 'TRIANGLE', 'POLYGON'].includes(field.type)) {
      inspectorContract.textContent = `// Objeto est\u00e1tico: no requiere variable de estado`;
      syncShapeInspector();
    } else {
      inspectorContract.textContent = window.JWPLCHMICodegenCore.textDeclaration(field, sanitizeSymbol(field.variable, 'texto'));
    }
  }

  function captureSnapshot() {
    return {
      fields: hmiFields.map((field) => ({ ...field })),
      pages: hmiPages.map((page) => ({ ...page })),
      navBar: { ...navBarConfig },
      activePage,
      selectedFieldKeys: [...selectedFieldKeys],
      selectedFieldKey,
      selectedTool,
      raw: { ...rawState },
      pixels: pixelLayer.slice(),
      serial: fieldSerial
    };
  }

  function equalPixels(a, b) {
    if (a.length !== b.length) return false;
    for (let i = 0; i < a.length; i += 1) if (a[i] !== b[i]) return false;
    return true;
  }

  function sameSnapshot(a, b) {
    if (!a || !b) return false;
    if (a.activePage !== b.activePage || a.selectedFieldKey !== b.selectedFieldKey || a.selectedTool !== b.selectedTool || a.serial !== b.serial) return false;
    if (JSON.stringify(a.selectedFieldKeys || []) !== JSON.stringify(b.selectedFieldKeys || [])) return false;
    if (JSON.stringify(a.fields) !== JSON.stringify(b.fields)) return false;
    if (JSON.stringify(a.pages) !== JSON.stringify(b.pages)) return false;
    if (JSON.stringify(a.navBar || {}) !== JSON.stringify(b.navBar || {})) return false;
    if (JSON.stringify(a.raw) !== JSON.stringify(b.raw)) return false;
    return equalPixels(a.pixels, b.pixels);
  }

  function commitHistory() {
    const snapshot = captureSnapshot();
    if (historyIndex >= 0 && sameSnapshot(snapshot, history[historyIndex])) return;
    history.splice(historyIndex + 1);
    history.push(snapshot);
    if (history.length > MAX_HISTORY) history.shift();
    historyIndex = history.length - 1;
    updateHistoryButtons();
  }

  function restoreSnapshot(snapshot) {
    hmiFields = snapshot.fields.map((field) => ({ ...field }));
    hmiPages = (snapshot.pages || [{ id: 0, name: 'Principal' }]).map((page) => ({ ...page }));
    if (snapshot.navBar) navBarConfig = { ...snapshot.navBar };
    activePage = pageExists(snapshot.activePage) ? snapshot.activePage : 0;
    setSelectedKeys(snapshot.selectedFieldKeys || (snapshot.selectedFieldKey ? [snapshot.selectedFieldKey] : []));
    selectedTool = snapshot.selectedTool;
    Object.assign(rawState, snapshot.raw);
    pixelLayer.set(snapshot.pixels);
    fieldSerial = snapshot.serial;
    const selected = selectedField();
    if (!selected || Number(selected.page || 0) !== activePage) {
      const replacement = fieldsForPage(activePage)[0] || null;
      setSelectedKeys(replacement ? [replacement.key] : []);
      selectedTool = replacement ? toolForField(replacement) : 'none';
    } else if (!['rawText', 'pixel', 'erase'].includes(selectedTool)) {
      selectedTool = toolForField(selected);
    }
    syncInputsFromState();
    syncToolUI();
    render();
  }

  function undo() {
    if (historyIndex <= 0) return;
    historyIndex -= 1;
    restoreSnapshot(history[historyIndex]);
  }

  function redo() {
    if (historyIndex >= history.length - 1) return;
    historyIndex += 1;
    restoreSnapshot(history[historyIndex]);
  }

  // Nombre / ID / variable consecutivo para duplicar y pegar: "Rect 2" ->
  // "Rect 3", FIELD_TEXT_1 -> FIELD_TEXT_2, texto1 -> texto2 (el primer
  // número libre). Quita sufijos " copia" / _COPY / Copy de versiones previas.
  const SEQUENCE_RULES = {
    name: { strip: /(\s+copia(\s+\d+)?)+$/i, sep: ' ' },
    id: { strip: /(_COPY(_\d+)?)+$/i, sep: '_' },
    variable: { strip: /(Copy\d*)+$/, sep: '' }
  };

  function usedValues(kind) {
    const used = new Set(hmiFields.map((field) => String(field[kind] ?? '')));
    if (kind === 'name') (window.JWPLCHMIPixelMaps?.getAll?.() || []).forEach((m) => used.add(String(m.name ?? '')));
    return used;
  }

  function nextSequential(value, kind) {
    const rule = SEQUENCE_RULES[kind];
    const used = usedValues(kind);
    const base = String(value ?? '').trim().replace(rule.strip, '') || 'Objeto';
    const match = base.match(/^(.*?)(\d+)$/);
    const stem = match ? match[1] : `${base}${rule.sep}`;
    let n = match ? Number(match[2]) + 1 : 2;
    while (used.has(`${stem}${n}`)) n += 1;
    return `${stem}${n}`;
  }

  // ID C++ y variable de la copia: consecutivos. Figuras y RAW_TEXT sólo
  // tienen ID (y variable si la traían).
  function assignSequentialSymbols(copy, source) {
    const isStatic = ['LINE', 'RECT', 'ELLIPSE', 'TRIANGLE', 'POLYGON', 'RAW_TEXT'].includes(source.type);
    if (source.id || !isStatic) copy.id = nextSequential(sanitizeSymbol(source.id, fieldFallbackId(source, 0)), 'id');
    if (source.variable || !isStatic) copy.variable = nextSequential(sanitizeSymbol(source.variable, variableFallback(source, 0)), 'variable');
  }

  function uniqueName(value) {
    return usedValues('name').has(value) ? nextSequential(value, 'name') : value;
  }

  function uniqueFieldSymbol(base) {
    const used = new Set(hmiFields.map((field) => field.id));
    let candidate = base;
    let suffix = 2;
    while (used.has(candidate)) {
      candidate = `${base}_${suffix}`;
      suffix += 1;
    }
    return candidate;
  }

  function uniqueVariable(base) {
    const used = new Set(hmiFields.map((field) => field.variable));
    let candidate = base;
    let suffix = 2;
    while (used.has(candidate)) {
      candidate = `${base}${suffix}`;
      suffix += 1;
    }
    return candidate;
  }

  function placeNewField(field) {
    const g = computeFieldGeometry(field);
    field.x = clamp(field.x, 0, Math.max(0, WIDTH - g.fieldW));
    field.y = clamp(field.y, 0, Math.max(0, HEIGHT - g.fieldH));
  }

  function selectFirstOnPage() {
    const first = fieldsForPage(activePage)[0] || null;
    setSelectedKeys(first ? [first.key] : []);
    selectedTool = first ? toolForField(first) : 'none';
  }

  function setActivePage(page) {
    const target = Number(page);
    if (!pageExists(target) || target === activePage) return false;
    activePage = target;
    const selected = selectedField();
    if (!selected || Number(selected.page || 0) !== activePage) selectFirstOnPage();
    syncInputsFromState();
    syncToolUI();
    render();
    return true;
  }

  function addPage(name) {
    if (hmiPages.length >= MAX_PAGES) return null;
    let id = 0;
    while (pageExists(id) && id < MAX_PAGES) id += 1;
    if (id >= MAX_PAGES) return null;
    const page = { id, name: String(name || `Página ${id + 1}`).slice(0, 24) };
    hmiPages.push(page);
    hmiPages.sort((a, b) => a.id - b.id);
    activePage = id;
    setSelectedKeys([]);
    selectedTool = 'none';
    syncInputsFromState();
    syncToolUI();
    render();
    commitHistory();
    return { ...page };
  }

  function renamePage(page, name) {
    const target = hmiPages.find((item) => item.id === Number(page));
    if (!target) return false;
    const next = String(name || '').trim().slice(0, 24);
    if (!next || next === target.name) return false;
    target.name = next;
    render();
    commitHistory();
    return true;
  }

  // options.commit: false = vista previa en vivo sin historial (editor de color
  // mientras se arrastra); true = registra el paso aunque el valor ya esté.
  function setPageMetadata(page, metadata = {}, options = {}) {
    const target = hmiPages.find((item) => item.id === Number(page));
    if (!target) return false;
    let changed = false;
    if (metadata.name !== undefined) {
      const next = String(metadata.name || '').trim().slice(0, 24);
      if (next && next !== target.name) {
        target.name = next;
        changed = true;
      }
    }
    if (metadata.shortName !== undefined) {
      const nextShort = String(metadata.shortName || '').trim().slice(0, 8);
      if (nextShort !== target.shortName) {
        target.shortName = nextShort;
        changed = true;
      }
    }
    if (metadata.color !== undefined) {
      const col = metadata.color === null ? null : Number(metadata.color);
      if (col !== target.color) {
        target.color = col;
        changed = true;
      }
    }
    if (changed) render();
    if (options.commit !== false && (changed || options.commit === true)) commitHistory();
    return changed;
  }

  function deletePage(page) {
    const target = Number(page);
    if (!pageExists(target) || hmiPages.length <= 1) return false;
    const idx = hmiPages.findIndex((item) => item.id === target);
    if (idx === -1) return false;

    // Remove page from array
    hmiPages.splice(idx, 1);

    // Remove fields on that page
    hmiFields = hmiFields.filter((field) => field.type === 'NAV_BAR' || field.global || Number(field.page || 0) !== target);

    // Remove pixel maps on that page (getAll() devuelve el arreglo vivo).
    const pixelMaps = window.JWPLCHMIPixelMaps?.getAll?.();
    if (Array.isArray(pixelMaps)) {
      for (let i = pixelMaps.length - 1; i >= 0; i -= 1) {
        if (Number(pixelMaps[i].page || 0) === target) pixelMaps.splice(i, 1);
      }
    }

    // Switch to adjacent page (todavía con ids antiguos)
    if (activePage === target) {
      activePage = hmiPages[Math.max(0, idx - 1)]?.id ?? hmiPages[0].id;
    }

    // A15-2: el runtime y el formato .jwhmi exigen ids contiguos 0..N-1.
    // Se renumeran páginas, campos y PixelMaps conservando el orden.
    const remap = new Map(hmiPages.map((item, index) => [item.id, index]));
    hmiPages.forEach((item) => { item.id = remap.get(item.id); });
    hmiFields.forEach((field) => {
      if (field.type === 'NAV_BAR' || field.global) return;
      const next = remap.get(Number(field.page || 0));
      if (next !== undefined) field.page = next;
    });
    if (Array.isArray(pixelMaps)) {
      pixelMaps.forEach((map) => {
        const next = remap.get(Number(map.page || 0));
        if (next !== undefined) map.page = next;
      });
    }
    activePage = remap.get(activePage) ?? 0;

    setSelectedKeys([]);
    selectedTool = 'none';
    syncInputsFromState();
    syncToolUI();
    render();
    commitHistory();
    window.dispatchEvent(new CustomEvent('jwplc:editor-refresh'));
    return true;
  }

  function moveSelectedFieldToPage(page) {
    const target = Number(page);
    const field = selectedField();
    if (!field || !pageExists(target)) return false;
    if (Number(field.page || 0) === target) return true;
    field.page = target;
    activePage = target;
    setSelectedKeys([field.key]);
    selectedTool = toolForField(field);
    syncInputsFromState();
    syncToolUI();
    render();
    commitHistory();
    return true;
  }

  function addShapeField(type, startPoint, endPoint) {
    if (hmiFields.length >= MAX_FIELDS) return;
    fieldSerial += 1;
    const typeLabel = type.charAt(0) + type.slice(1).toLowerCase();
    const field = {
      type: type,
      key: `shape-${fieldSerial}`,
      name: uniqueName(`${typeLabel} ${fieldSerial}`),
      id: uniqueFieldSymbol(`FIELD_${type}_${fieldSerial}`),
      variable: '',
      x: startPoint.x,
      y: startPoint.y,
      x2: endPoint.x,
      y2: endPoint.y,
      page: activePage,
      frameColor: selectedColor ? selectedColor.value : 0xFFFF,
      fillColor: selectedColor ? (selectedColor.value === 0xFFFF ? 0x001F : 0xFFFF) : 0x001F,
      backgroundColor: 0x0000,
      fill: false,
      size: 1,
      cornerRadius: 0,
      sides: Number(document.getElementById('mainPolySidesInput')?.value || 5)
    };
    assignNewLayerOrder(field);
    hmiFields.push(field);
    setSelectedKeys([field.key]);
    syncInputsFromState();
    syncToolUI();
    render();
    commitHistory();
  }

  function addRawTextField(point) {
    if (hmiFields.length >= MAX_FIELDS) return null;
    fieldSerial += 1;
    const px = point ? point.x : 20 + ((fieldSerial - 1) * 8) % 80;
    const py = point ? point.y : 20 + ((fieldSerial - 1) * 8) % 60;
    const field = {
      type: 'RAW_TEXT',
      key: `rawText-${fieldSerial}`,
      name: `RAW TEXT ${fieldSerial}`,
      id: uniqueFieldSymbol(`FIELD_RAW_${fieldSerial}`),
      text: rawState.value || 'Texto RAW',
      x: px,
      y: py,
      size: rawState.size || 1,
      font: rawState.font || 'GLCD',
      transparentBackground: true,
      textColor: selectedColor ? selectedColor.value : (rawState.foreground ?? 0xFFFF),
      backgroundColor: rawState.background ?? 0x0000,
      page: activePage
    };
    assignNewLayerOrder(field);
    hmiFields.push(field);
    setSelectedKeys([field.key]);
    selectedTool = 'pointer';
    syncInputsFromState();
    syncToolUI();
    window.dispatchEvent(new CustomEvent('jwplc:tool-changed', { detail: { tool: 'pointer' } }));
    render();
    commitHistory();
    return field;
  }

  function addTextField() {
    if (hmiFields.length >= MAX_FIELDS) return;
    fieldSerial += 1;
    const field = defaultTextField(`text-${fieldSerial}`);
    field.name = uniqueName(`TEXT ${fieldSerial}`);
    field.id = uniqueFieldSymbol(`FIELD_TEXT_${fieldSerial}`);
    field.variable = uniqueVariable(`texto${fieldSerial}`);
    field.label = `Texto ${fieldSerial}`;
    field.preview = 'READY';
    field.page = activePage;
    field.x = 20 + ((fieldSerial - 1) * 8) % 80;
    field.y = 20 + ((fieldSerial - 1) * 8) % 60;
    placeNewField(field);
    assignNewLayerOrder(field);
    hmiFields.push(field);
    setSelectedKeys([field.key]);
    selectedTool = 'textField';
    syncInputsFromState();
    syncToolUI();
    render();
    commitHistory();
  }

  // A15-8: MENU (árbol de configuración). Uno por página; el modelo y el
  // dibujo viven en designer-menu.js.
  
  function addEventLogField() {
    const api = window.JWPLCHMIEventLog;
    if (!api || hmiFields.length >= MAX_FIELDS) return;
    if (hmiFields.some((f) => f.type === 'EVENT_LOG' && Number(f.page || 0) === activePage)) {
      window.JWPLCDialog.alert('Esta página ya tiene un EVENT_LOG: sólo se permite uno por página.');
      return;
    }
    fieldSerial += 1;
    const field = api.createDefault(`eventlog-${fieldSerial}`, fieldSerial);
    field.id = uniqueFieldSymbol(field.id);
    field.page = activePage;
    placeNewField(field);
    assignNewLayerOrder(field);
    hmiFields.push(field);
    setSelectedKeys([field.key]);
    selectedTool = 'eventLogField';
    syncInputsFromState();
    syncToolUI();
    render();
    commitHistory();
  }

  function addMenuField() {
    const api = window.JWPLCHMIMenu;
    if (!api || hmiFields.length >= MAX_FIELDS) return;
    if (hmiFields.some((f) => f.type === 'MENU' && Number(f.page || 0) === activePage)) {
      window.JWPLCDialog.alert('Esta página ya tiene un MENU: el firmware atiende un solo MENU por página.');
      return;
    }
    fieldSerial += 1;
    const field = api.createDefault(`menu-${fieldSerial}`, fieldSerial, hmiFields);
    field.id = uniqueFieldSymbol(field.id);
    field.page = activePage;
    placeNewField(field);
    assignNewLayerOrder(field);
    hmiFields.push(field);
    setSelectedKeys([field.key]);
    selectedTool = 'menuField';
    syncInputsFromState();
    syncToolUI();
    render();
    commitHistory();
  }

  // Alpha16: ICON (imagen por estado ligada a una variable). La biblioteca y
  // el inspector viven en designer-icons.js; los datos en designer-icon-catalog.js.
  function iconEntry(field) {
    return window.JWPLCHMIIcons?.entryFor?.(field) ||
      window.JWPLCIconsCore?.entryFor?.(window.JWPLCIconCatalog, field?.iconId, field?.iconSize) || null;
  }

  // Primer hueco libre de la página (barrido de 8 px) para no tapar otro objeto.
  function placeIconInFreeSpot(field) {
    const g = computeFieldGeometry({ ...field, x: 0, y: 0 });
    const others = fieldsForPage(activePage).filter((f) => !f.hidden).map((f) => getFieldBounds(f));
    for (let y = 4; y + g.fieldH <= HEIGHT; y += 8) {
      for (let x = 4; x + g.fieldW <= WIDTH; x += 8) {
        const free = others.every((b) => x + g.fieldW <= b.minX || x > b.maxX || y + g.fieldH <= b.minY || y > b.maxY);
        if (free) { field.x = x; field.y = y; return; }
      }
    }
    field.x = 20;
    field.y = 20;
  }

  function addIconField(iconId, size, at = null) {
    const core = window.JWPLCIconsCore;
    const icon = window.JWPLCHMIIcons?.iconInfo?.(iconId) || core?.findIcon?.(window.JWPLCIconCatalog, iconId);
    if (!icon) return null;
    if (hmiFields.length >= MAX_FIELDS) {
      showToastMessage(`Límite de ${MAX_FIELDS} campos: el ICON cuenta como campo.`, 'warning');
      return null;
    }
    fieldSerial += 1;
    const field = defaultTextField(`icon-${fieldSerial}`);
    const base = String(iconId).split('/').pop().replace(/[^A-Za-z0-9]+/g, '_');
    field.type = 'ICON';
    field.name = uniqueName(`${icon.name} ${fieldSerial}`);
    field.id = uniqueFieldSymbol(`FIELD_ICON_${fieldSerial}`);
    field.variable = uniqueVariable(`${base}${fieldSerial}`);
    field.iconId = iconId;
    field.iconSize = core.resolveSize(icon, size || 32);
    field.iconVarType = icon.stateNames.length > 2 ? 'uint8' : 'bool';
    field.iconState = 0;
    field.transparentBackground = false;
    field.label = '';
    field.preview = '';
    field.unit = '';
    field.capacity = 1;
    field.valueSize = 1;
    field.layout = 'STACKED';
    field.align = 'LEFT';
    field.valueColor = 0xFFFF;
    field.page = activePage;
    if (at) {
      field.x = Math.round(at.x);
      field.y = Math.round(at.y);
    } else {
      placeIconInFreeSpot(field);
    }
    placeNewField(field);
    assignNewLayerOrder(field);
    hmiFields.push(field);
    setSelectedKeys([field.key]);
    selectedTool = 'iconField';
    syncInputsFromState();
    syncToolUI();
    render();
    commitHistory();
    return field;
  }

  function addValueField() {
    if (hmiFields.length >= MAX_FIELDS) return;
    fieldSerial += 1;
    const field = defaultValueField(`value-${fieldSerial}`);
    field.name = uniqueName(`VALUE ${fieldSerial}`);
    field.id = uniqueFieldSymbol(`FIELD_VALUE_${fieldSerial}`);
    field.variable = uniqueVariable(`valor${fieldSerial}`);
    field.label = `Valor ${fieldSerial}`;
    field.page = activePage;
    field.x = 28 + ((fieldSerial - 1) * 8) % 90;
    field.y = 44 + ((fieldSerial - 1) * 8) % 70;
    placeNewField(field);
    assignNewLayerOrder(field);
    hmiFields.push(field);
    setSelectedKeys([field.key]);
    selectedTool = 'valueField';
    syncInputsFromState();
    syncToolUI();
    render();
    commitHistory();
  }

  function duplicateSelectedField() {
    const sources = selectedFields();
    const selPms = window.JWPLCHMIPixelMaps?.getSelectedMaps?.() || [];
    if (sources.length === 0 && selPms.length === 0) return;
    if (sources.length > 0 && hmiFields.length + sources.length <= MAX_FIELDS) {
      const newKeys = [];
      sources.forEach((source) => {
        fieldSerial += 1;
        const copy = { ...source };
        delete copy.layerOrder;
        const prefix = source.type.toLowerCase();
        copy.key = `${prefix}-${fieldSerial}`;
        copy.name = nextSequential(source.name || source.type, 'name');
        assignSequentialSymbols(copy, source);
        copy.page = activePage;
        copy.x = source.x + 8;
        copy.y = source.y + 8;
        if (source.x2 !== undefined) copy.x2 = source.x2 + 8;
        if (source.y2 !== undefined) copy.y2 = source.y2 + 8;
        placeNewField(copy);
        assignNewLayerOrder(copy);
        hmiFields.push(copy);
        newKeys.push(copy.key);
      });
      setSelectedKeys(newKeys);
    }
    if (window.JWPLCHMIPixelMaps?.duplicate) {
      window.JWPLCHMIPixelMaps.duplicate();
    }
    const primary = selectedField();
    selectedTool = primary ? toolForField(primary) : 'pointer';
    syncInputsFromState();
    syncToolUI();
    render();
    commitHistory();
  }

  // Selección en la que todo está bloqueado (campos y capas PIXEL): no se
  // mueve, redimensiona ni borra desde el lienzo / teclado.
  function selectionIsLocked() {
    const items = [...selectedFields(), ...(window.JWPLCHMIPixelMaps?.getSelectedMaps?.() || [])];
    return items.length > 0 && items.every((item) => item.locked);
  }

  function deleteSelectedField() {
    const keysToDelete = new Set(selectedFieldKeys);
    if (keysToDelete.size > 0) {
      hmiFields = hmiFields.filter((field) => !keysToDelete.has(field.key));
    }
    if (window.JWPLCHMIPixelMaps?.removeSelected) {
      window.JWPLCHMIPixelMaps.removeSelected();
    }
    const pageFields = fieldsForPage(activePage);
    const replacement = pageFields[pageFields.length - 1] || null;
    setSelectedKeys(replacement ? [replacement.key] : []);
    selectedTool = replacement ? toolForField(replacement) : 'none';
    syncInputsFromState();
    syncToolUI();
    render();
    commitHistory();
  }

  function resetProject() {
    pixelLayer.fill(0x0000);
    fieldSerial = 1;
    hmiPages = [{ id: 0, name: 'Principal' }];
    activePage = 0;
    hmiFields = [defaultTextField('text-1')];
    setSelectedKeys(['text-1']);
    selectedTool = 'textField';
    syncInputsFromState();
    syncToolUI();
    render();
    commitHistory();
  }

  function demoTextField() {
    pixelLayer.fill(0x0000);
    const demo = defaultTextField('text-1');
    Object.assign(demo, {
      name: 'Estado de máquina',
      id: 'FIELD_STATUS',
      variable: 'estadoTexto',
      capacity: 12,
      x: 18,
      y: 28,
      preview: 'PRODUCCION',
      label: 'Estado',
      valueSize: 2,
      labelSize: 1,
      frame: true,
      layout: 'STACKED',
      align: 'CENTER',
      page: 0,
      labelColor: 0xFFFF,
      valueColor: 0x07E0,
      backgroundColor: 0x0000,
      frameColor: 0xFD20
    });
    fieldSerial = 1;
    hmiPages = [{ id: 0, name: 'Principal' }];
    activePage = 0;
    hmiFields = [demo];
    setSelectedKeys([demo.key]);
    selectedTool = 'textField';
    syncInputsFromState();
    syncToolUI();
    render();
    commitHistory();
  }

  function demoValueField() {
    pixelLayer.fill(0x0000);
    const demo = defaultValueField('value-1');
    Object.assign(demo, {
      name: 'Temperatura',
      id: 'FIELD_TEMP',
      variable: 'temperatura',
      x: 22,
      y: 28,
      preview: '25.6',
      label: 'Temp',
      unit: 'C',
      integerDigits: 3,
      decimalDigits: 1,
      signedValue: true,
      leadingZeros: false,
      valueSize: 2,
      labelSize: 1,
      frame: true,
      layout: 'INLINE',
      align: 'RIGHT',
      page: 0,
      labelColor: 0xFFFF,
      valueColor: 0x07FF,
      backgroundColor: 0x0000,
      frameColor: 0xFD20
    });
    fieldSerial = 1;
    hmiPages = [{ id: 0, name: 'Principal' }];
    activePage = 0;
    hmiFields = [demo];
    setSelectedKeys([demo.key]);
    selectedTool = 'valueField';
    syncInputsFromState();
    syncToolUI();
    render();
    commitHistory();
  }

  function bindRawInput(element, handler) {
    element.addEventListener('input', () => {
      handler();
      render();
    });
    element.addEventListener('change', () => {
      handler();
      render();
      commitHistory();
    });
  }

  function bindFieldInput(element, handler) {
    if (!element) return;
    element.addEventListener('input', () => {
      const field = selectedField();
      if (!field) return;
      handler(field);
      syncInputsFromState();
      syncToolUI();
      render();
    });
    element.addEventListener('change', () => {
      const field = selectedField();
      if (!field) return;
      handler(field);
      syncInputsFromState();
      syncToolUI();
      render();
      commitHistory();
    });
  }

  document.querySelectorAll('.tool[data-tool]').forEach((button) => {
    button.addEventListener('click', () => {
      const tool = button.dataset.tool;
      const field = selectedField();
      if (tool === 'textField') {
        if (!field || field.type !== 'TEXT') addTextField();
        else {
          selectedTool = 'textField';
          syncToolUI();
          render();
        }
        return;
      }
      if (tool === 'valueField') {
        if (!field || field.type !== 'VALUE') addValueField();
        else {
          selectedTool = 'valueField';
          syncToolUI();
          render();
        }
        return;
      }
      if (tool === 'boolField') {
        selectedTool = 'boolField';
        syncToolUI();
        render();
        return;
      }
      if (tool === 'barField') {
        selectedTool = 'barField';
        syncToolUI();
        render();
        return;
      }
      if (tool === 'navBarField') {
        let navBar = hmiFields.find((f) => f.type === 'NAV_BAR');
        if (!navBar) {
          const cfg = window.JWPLCHMIPages?.getNavBarConfig?.() || { preset: 'industrial-color', position: 'bottom', height: 22 };
          const h = Number(cfg.height) || 22;
          const y = cfg.position === 'top' ? 2 : (170 - h - 1);
          navBar = {
            key: 'NAV_BAR',
            type: 'NAV_BAR',
            name: 'Barra de Navegación',
            label: 'NAV_BAR',
            x: 0,
            y,
            preset: cfg.preset || 'industrial-color',
            position: cfg.position || 'bottom',
            height: h,
            global: true,
            hidden: false,
            locked: false,
            layerOrder: 999
          };
          hmiFields.push(navBar);
          if (window.JWPLCHMIPages?.setNavBarConfig) {
            window.JWPLCHMIPages.setNavBarConfig({ ...cfg, enabled: true });
          }
          commitHistory();
        }
        setSelectedKeys([navBar.key]);
        selectedTool = 'navBarField';
        syncInputsFromState();
        syncToolUI();
        render();
        return;
      }
      if (tool === 'rawText') {
        addRawTextField();
        return;
      }
      if (tool === 'menuField') {
        addMenuField();
        return;
      }
      if (tool === 'eventLogField') {
        addEventLogField();
        return;
      }
      selectedTool = tool;
      if (!['pointer', 'textField', 'valueField', 'boolField', 'barField', 'navBarField', 'menuField'].includes(tool)) {
        setSelectedKeys([]);
      }
      window.dispatchEvent(new CustomEvent('jwplc:tool-changed', { detail: { tool } }));
      syncInputsFromState();
      syncToolUI();
      render();
    });
  });

  const varDropdownContainer = document.getElementById('variableDropdownContainer');
  const varToolBtn = document.getElementById('variableToolBtn');
  const varDropdownMenu = document.getElementById('variableDropdownMenu');
  const varActiveIcon = document.getElementById('variableToolActiveIcon');

  const textDropdownContainer = document.getElementById('textDropdownContainer');
  const textToolBtn = document.getElementById('textToolBtn');
  const textDropdownMenu = document.getElementById('textDropdownMenu');
  const textActiveIcon = document.getElementById('textToolActiveIcon');

  const VAR_ICONS = {
    valueField: '<svg width="18" height="16" viewBox="0 0 24 24"><text x="12" y="16.5" text-anchor="middle" font-size="11.5" font-weight="800" font-family="\'Segoe UI\', -apple-system, sans-serif" fill="currentColor" letter-spacing="-0.5px">123</text></svg>',
    boolField: '<svg width="18" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><rect x="2" y="6" width="20" height="12" rx="6"/><circle cx="16" cy="12" r="3" fill="currentColor"/></svg>',
    barField: '<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><rect x="2" y="7" width="20" height="10" rx="2.5"/><rect x="5" y="10" width="7" height="4" rx="1" fill="currentColor"/></svg>',
    navBarField: '<svg width="18" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><rect x="2" y="7" width="20" height="10" rx="2"></rect><path d="M7 7v10M12 7v10M17 7v10"></path></svg>',
    menuField: '<svg width="18" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><rect x="3" y="3" width="18" height="18" rx="2"/><path d="M3 8h18M7 12h6M7 16h10"/></svg>'
  };

  const VAR_TITLES = {
    valueField: 'Variables del PLC · Valor Numérico (VALUE)',
    boolField: 'Variables del PLC · Booleano (BOOL)',
    barField: 'Variables del PLC · Indicadores de Nivel (BAR)',
    navBarField: 'Variables del PLC · Barra de Navegación (NAV)',
    menuField: 'Variables del PLC · Menú de configuración (MENU)'
  };

  const TEXT_ICONS = {
    textField: '<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M4 7V4h16v3"/><path d="M12 4v16"/><path d="M8 20h8"/></svg>',
    rawText: '<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round"><path d="M4 19.5L12 3.5l8 16"/><line x1="7" y1="14.5" x2="17" y2="14.5"/></svg>'
  };

  const TEXT_TITLES = {
    textField: 'Herramientas de Texto · Texto Dinámico (TEXT)',
    rawText: 'Herramientas de Texto · Texto RAW (A)'
  };

  function updateVariableButtonDisplay(tool) {
    if (!VAR_ICONS[tool]) return;
    if (varActiveIcon) varActiveIcon.innerHTML = VAR_ICONS[tool];
    if (varToolBtn) varToolBtn.title = VAR_TITLES[tool] || 'Variables del PLC';
    document.querySelectorAll('.var-dropdown-item').forEach((item) => {
      item.classList.toggle('active', item.dataset.tool === tool);
    });
  }

  function updateTextButtonDisplay(tool) {
    if (!TEXT_ICONS[tool]) return;
    if (textActiveIcon) textActiveIcon.innerHTML = TEXT_ICONS[tool];
    if (textToolBtn) textToolBtn.title = TEXT_TITLES[tool] || 'Herramientas de Texto';
    document.querySelectorAll('.text-dropdown-item').forEach((item) => {
      item.classList.toggle('active', item.dataset.tool === tool);
    });
  }

  function closeAllDropdowns() {
    if (varDropdownMenu) varDropdownMenu.style.display = 'none';
    varDropdownContainer?.classList.remove('open');
    if (textDropdownMenu) textDropdownMenu.style.display = 'none';
    textDropdownContainer?.classList.remove('open');
  }

  if (varToolBtn && varDropdownMenu) {
    varToolBtn.addEventListener('click', (e) => {
      e.stopPropagation();
      const isOpen = varDropdownMenu.style.display !== 'none';
      closeAllDropdowns();
      if (!isOpen) {
        varDropdownMenu.style.display = 'flex';
        varDropdownContainer?.classList.add('open');
      }
    });

    document.querySelectorAll('.var-dropdown-item').forEach((item) => {
      item.addEventListener('click', () => {
        const tool = item.dataset.tool;
        updateVariableButtonDisplay(tool);
        closeAllDropdowns();
      });
    });
  }

  if (textToolBtn && textDropdownMenu) {
    textToolBtn.addEventListener('click', (e) => {
      e.stopPropagation();
      const isOpen = textDropdownMenu.style.display !== 'none';
      closeAllDropdowns();
      if (!isOpen) {
        textDropdownMenu.style.display = 'flex';
        textDropdownContainer?.classList.add('open');
      }
    });

    document.querySelectorAll('.text-dropdown-item').forEach((item) => {
      item.addEventListener('click', () => {
        const tool = item.dataset.tool;
        updateTextButtonDisplay(tool);
        closeAllDropdowns();
      });
    });
  }

  document.addEventListener('click', (e) => {
    if (
      (varDropdownContainer && !varDropdownContainer.contains(e.target)) &&
      (textDropdownContainer && !textDropdownContainer.contains(e.target))
    ) {
      closeAllDropdowns();
    }
  });

  zoomSelect.addEventListener('change', () => {
    zoom = Number(zoomSelect.value);
    render();
  });
  gridToggle.addEventListener('change', render);
  gridSizeSelect?.addEventListener('change', render);
  gridStyleSelect?.addEventListener('change', render);
  newProjectButton.addEventListener('click', resetProject);
  demoButton.addEventListener('click', demoTextField);
  if (demoValueButton) demoValueButton.addEventListener('click', demoValueField);
  clearButton.addEventListener('click', () => {
    pixelLayer.fill(0x0000);
    if (selectedTool === 'rawText') rawState.value = '';
    const field = selectedField();
    if (field) field.preview = field.type === 'VALUE' ? '0' : '';
    syncInputsFromState();
    render();
    commitHistory();
  });

  if (undoButton) undoButton.addEventListener('click', undo);
  if (redoButton) redoButton.addEventListener('click', redo);
  duplicateButton.addEventListener('click', duplicateSelectedField);
  deleteButton.addEventListener('click', deleteSelectedField);

  bindRawInput(rawTextInput, () => {
    rawState.value = rawTextInput.value;
    const field = selectedField();
    if (field && field.type === 'RAW_TEXT') field.text = rawTextInput.value;
    syncRawTextInspector();
  });
  bindRawInput(rawTextX, () => {
    rawState.x = clamp(Number(rawTextX.value) || 0, 0, WIDTH - 1);
    const field = selectedField();
    if (field && field.type === 'RAW_TEXT') field.x = rawState.x;
    syncRawTextInspector();
  });
  bindRawInput(rawTextY, () => {
    rawState.y = clamp(Number(rawTextY.value) || 0, 0, HEIGHT - 1);
    const field = selectedField();
    if (field && field.type === 'RAW_TEXT') field.y = rawState.y;
    syncRawTextInspector();
  });
  if (rawTextFont && window.JWPLCDesignerFonts) {
    const groups = new Map();
    window.JWPLCDesignerFonts.list.forEach((font) => {
      if (!groups.has(font.group)) {
        const group = document.createElement('optgroup');
        group.label = font.group;
        groups.set(font.group, group);
        rawTextFont.appendChild(group);
      }
      const option = document.createElement('option');
      option.value = font.id;
      option.textContent = font.label;
      // Font2/Font4 no están en el firmware (agregarían ~9 KB a todo sketch).
      if (!window.JWPLCDesignerFonts.isSupported(font)) {
        option.disabled = true;
        option.textContent = `${font.label} (no incluida en el firmware)`;
      }
      groups.get(font.group).appendChild(option);
    });
    rawTextFont.value = rawState.font;
  }
  function updateScalePxHint() {
    if (!rawScalePxHint || !window.JWPLCDesignerFonts) return;
    const font = window.JWPLCDesignerFonts.get(rawState.font);
    if (!font) { rawScalePxHint.textContent = ''; return; }
    const h = window.JWPLCDesignerFonts.lineHeight(font, rawState.size);
    rawScalePxHint.textContent = `· ${h} px`;
  }

  bindRawInput(rawTextFont, () => {
    const fonts = window.JWPLCDesignerFonts;
    if (!fonts) return;
    rawState.font = fonts.normalizeId(rawTextFont.value);
    const field = selectedField();
    if (field && field.type === 'RAW_TEXT') {
      field.font = rawState.font;
      syncInputsFromState();
    }
    updateScalePxHint();
  });
  bindRawInput(rawTextSize, () => {
    rawState.size = Number(rawTextSize.value) || 1;
    const field = selectedField();
    if (field && field.type === 'RAW_TEXT') field.size = rawState.size;
    updateScalePxHint();
  });

  // Controles de Texto RAW (estilo Lopaka)
  rawTextName?.addEventListener('input', () => {
    const field = selectedField();
    if (field && field.type === 'RAW_TEXT') {
      field.name = rawTextName.value;
      render();
    }
  });
  rawTextName?.addEventListener('change', () => commitHistory());

  rawDuplicateBtn?.addEventListener('click', duplicateSelectedField);
  rawDeleteBtn?.addEventListener('click', deleteSelectedField);

  copyRawContractBtn?.addEventListener('click', () => {
    const code = rawInspectorContract?.textContent || '';
    if (navigator.clipboard) {
      navigator.clipboard.writeText(code);
      copyRawContractBtn.textContent = '¡Copiado!';
      setTimeout(() => { copyRawContractBtn.textContent = 'Copiar'; }, 1500);
    }
  });

  // Pestañas principales de Texto RAW
  document.querySelectorAll('#rawMainTabs [data-raw-tab]').forEach((btn) => {
    btn.addEventListener('click', (e) => {
      e.preventDefault();
      activeRawMainTab = btn.dataset.rawTab;
      syncRawTabPanes();
      syncRawTextInspector();
    });
  });

  // Subpestañas de Texto RAW
  document.querySelectorAll('#rawSubTabs [data-raw-subtab]').forEach((btn) => {
    btn.addEventListener('click', (e) => {
      e.preventDefault();
      activeRawSubTab = btn.dataset.rawSubtab;
      syncRawTabPanes();
      syncRawTextInspector();
    });
  });

  // Selector de escala por chips 1×..4×
  document.getElementById('rawScaleChipRow')?.addEventListener('click', (e) => {
    const chip = e.target.closest('[data-raw-scale]');
    if (!chip) return;
    const s = Number(chip.dataset.rawScale) || 1;
    const field = selectedField();
    if (field && field.type === 'RAW_TEXT') field.size = s;
    rawState.size = s;
    if (rawTextSize) rawTextSize.value = String(s);
    syncRawTextInspector();
    render();
    commitHistory();
  });

  // Selector de fondo (Transparente vs Opaco)
  document.getElementById('rawBgModeGrid')?.addEventListener('click', (e) => {
    const btn = e.target.closest('[data-raw-bgmode]');
    if (!btn) return;
    const isTrans = btn.dataset.rawBgmode === 'transparent';
    const field = selectedField();
    if (field && field.type === 'RAW_TEXT') field.transparentBackground = isTrans;
    rawState.transparent = isTrans;
    if (rawTextTransparentToggle) rawTextTransparentToggle.checked = !isTrans;
    if (rawTextTransparent) rawTextTransparent.value = isTrans ? '1' : '0';
    if (rawTextBgColorWrap) rawTextBgColorWrap.style.display = isTrans ? 'none' : 'flex';
    syncRawTextInspector();
    syncRawAppearance();
    render();
    commitHistory();
  });

  // Leyenda de swatches (Texto / Fondo)
  document.getElementById('rawCaseLegend')?.addEventListener('click', (e) => {
    const row = e.target.closest('[data-raw-swatch-row]');
    if (row) {
      rawActiveColorKey = row.dataset.rawSwatchRow;
      if (rawActiveColorKey === 'bgColor') {
        const field = selectedField();
        const wasTrans = (field && field.type === 'RAW_TEXT') ? Boolean(field.transparentBackground) : Boolean(rawState.transparent);
        if (wasTrans) {
          if (field && field.type === 'RAW_TEXT') field.transparentBackground = false;
          rawState.transparent = false;
          if (rawTextTransparentToggle) rawTextTransparentToggle.checked = true;
          if (rawTextTransparent) rawTextTransparent.value = '0';
          render();
          commitHistory();
        }
      }
      syncRawTextInspector();
      syncRawAppearance();
    }
  });

  // Matriz 3×3 de alineación en pantalla
  document.getElementById('rawAlignMatrix3x3')?.addEventListener('click', (e) => {
    const btn = e.target.closest('[data-align]');
    if (!btn) return;
    alignSelectedShapes(btn.dataset.align);
  });

  // Chips de páginas
  document.getElementById('rawPageChipRow')?.addEventListener('click', (e) => {
    const chip = e.target.closest('[data-raw-page-id]');
    if (!chip) return;
    const target = Number(chip.dataset.rawPageId);
    const field = selectedField();
    if (field && field.type === 'RAW_TEXT') {
      moveSelectedFieldToPage(target);
    } else {
      selectPage(target);
    }
    syncRawTextInspector();
  });

  window.addEventListener('jwplc:editor-refresh', () => {
    if (rawSection && !rawSection.hidden) {
      initRawColorEditor();
      syncRawAppearance();
    }
  });

  window.addEventListener('resize', () => {
    if (rawSection && !rawSection.hidden && activeRawMainTab === 'design' && activeRawSubTab === 'appearance') {
      syncRawAppearance();
    }
  });

  rawTextColorSwatch?.addEventListener('click', () => triggerColorPicker(rawTextColorInput));
  rawTextColorInput?.addEventListener('input', () => {
    const c = hex888ToRgb565(rawTextColorInput.value);
    const field = selectedField();
    if (field && field.type === 'RAW_TEXT') field.textColor = c;
    rawState.foreground = c;
    if (rawTextColorSwatch) rawTextColorSwatch.style.background = rgb565ToCss(c);
    if (rawTextColorCode) rawTextColorCode.textContent = rgb565ToHex888(c).toUpperCase();
    render();
  });
  rawTextColorInput?.addEventListener('change', () => commitHistory());

  rawTextTransparentToggle?.addEventListener('change', () => {
    const isOpaque = rawTextTransparentToggle.checked;
    const field = selectedField();
    if (field && field.type === 'RAW_TEXT') field.transparentBackground = !isOpaque;
    rawState.transparent = !isOpaque;
    if (rawTextBgColorWrap) rawTextBgColorWrap.style.display = isOpaque ? 'flex' : 'none';
    render();
    commitHistory();
  });

  rawTextBgColorSwatch?.addEventListener('click', () => triggerColorPicker(rawTextBgColorInput));
  rawTextBgColorInput?.addEventListener('input', () => {
    const c = hex888ToRgb565(rawTextBgColorInput.value);
    const field = selectedField();
    if (field && field.type === 'RAW_TEXT') field.backgroundColor = c;
    rawState.background = c;
    if (rawTextBgColorSwatch) rawTextBgColorSwatch.style.background = rgb565ToCss(c);
    if (rawTextBgColorCode) rawTextBgColorCode.textContent = rgb565ToHex888(c).toUpperCase();
    render();
  });
  rawTextBgColorInput?.addEventListener('change', () => commitHistory());

  bindRawInput(rawTextTransparent, () => {
    const isTrans = rawTextTransparent.value === '1';
    rawState.transparent = isTrans;
    const field = selectedField();
    if (field && field.type === 'RAW_TEXT') field.transparentBackground = isTrans;
    if (rawTextBackgroundWrap) rawTextBackgroundWrap.hidden = isTrans;
    if (rawTextTransparentToggle) rawTextTransparentToggle.checked = !isTrans;
    if (rawTextBgColorWrap) rawTextBgColorWrap.style.display = isTrans ? 'none' : 'flex';
  });
  bindRawInput(rawTextColor, () => {
    const color = colorByName(rawTextColor.value).value;
    rawState.foreground = color;
    const field = selectedField();
    if (field && field.type === 'RAW_TEXT') field.textColor = color;
    if (rawTextColorSwatch) rawTextColorSwatch.style.background = rgb565ToCss(color);
    if (rawTextColorInput) rawTextColorInput.value = rgb565ToHex888(color);
    if (rawTextColorCode) rawTextColorCode.textContent = rgb565ToHex888(color).toUpperCase();
  });
  bindRawInput(rawTextBackground, () => {
    rawState.background = colorByName(rawTextBackground.value).value;
    const field = selectedField();
    if (field && field.type === 'RAW_TEXT') field.backgroundColor = rawState.background;
    if (rawTextBgColorSwatch) rawTextBgColorSwatch.style.background = rgb565ToCss(rawState.background);
    if (rawTextBgColorInput) rawTextBgColorInput.value = rgb565ToHex888(rawState.background);
    if (rawTextBgColorCode) rawTextBgColorCode.textContent = rgb565ToHex888(rawState.background).toUpperCase();
  });


  bindFieldInput(fieldName, (field) => { field.name = fieldName.value; });
  bindFieldInput(fieldId, (field) => { field.id = fieldId.value; });
  bindFieldInput(fieldVariable, (field) => { field.variable = fieldVariable.value; });
  bindFieldInput(fieldCapacity, (field) => {
    if (field.type !== 'TEXT') return;
    field.capacity = clamp(Number(fieldCapacity.value) || 1, 1, 39);
    field.preview = String(field.preview || '').slice(0, field.capacity);
  });
  bindFieldInput(fieldX, (field) => { field.x = clamp(Number(fieldX.value) || 0, 0, WIDTH - 1); });
  bindFieldInput(fieldY, (field) => { field.y = clamp(Number(fieldY.value) || 0, 0, HEIGHT - 1); });
  bindFieldInput(fieldX2, (field) => { field.x2 = clamp(Number(fieldX2.value) || 0, 0, WIDTH - 1); });
  bindFieldInput(fieldY2, (field) => { field.y2 = clamp(Number(fieldY2.value) || 0, 0, HEIGHT - 1); });
  bindFieldInput(fieldSides, (field) => {
    let val = parseInt(fieldSides.value, 10);
    if (!isNaN(val)) {
      if (val > 12) {
        showToastMessage('El número máximo de lados para un polígono es 12.', 'warning');
        val = 12;
        fieldSides.value = '12';
      }
      field.sides = clamp(val, 3, 12);
      if (shapeSides2) shapeSides2.value = String(field.sides);
    }
  });
  bindFieldInput(fieldFill, (field) => {
    field.fill = fieldFill.value === '1';
    if (fieldFillColorWrap) fieldFillColorWrap.hidden = !field.fill;
  });
  bindFieldInput(fieldFillColor, (field) => {
    field.fillColor = colorByName(fieldFillColor.value).value;
  });
  bindFieldInput(fieldPreview, (field) => {
    field.preview = field.type === 'TEXT'
      ? fieldPreview.value.slice(0, Math.max(1, field.capacity))
      : fieldPreview.value;
  });
  bindFieldInput(fieldLabel, (field) => { field.label = fieldLabel.value; });
  bindFieldInput(fieldUnit, (field) => { field.unit = fieldUnit.value; });
  bindFieldInput(fieldValueSize, (field) => { field.valueSize = Number(fieldValueSize.value) || 1; });
  bindFieldInput(fieldLabelSize, (field) => { field.labelSize = Number(fieldLabelSize.value) || 1; });
  bindFieldInput(fieldFrame, (field) => {
    field.frame = fieldFrame.value === '1';
    const frameColorWrap = fieldFrameColor?.closest('label');
    if (frameColorWrap) frameColorWrap.style.display = field.frame ? '' : 'none';
  });
  bindFieldInput(fieldLayout, (field) => { field.layout = fieldLayout.value; });
  bindFieldInput(fieldAlign, (field) => { field.align = fieldAlign.value; });
  bindFieldInput(fieldLabelColor, (field) => { field.labelColor = colorByName(fieldLabelColor.value).value; });
  bindFieldInput(fieldValueColor, (field) => { field.valueColor = colorByName(fieldValueColor.value).value; });
  bindFieldInput(fieldBackgroundColor, (field) => {
    if (fieldBackgroundColor.value === 'TRANSPARENT') {
      field.backgroundColor = 'TRANSPARENT';
    } else {
      field.backgroundColor = colorByName(fieldBackgroundColor.value).value;
    }
  });
  bindFieldInput(fieldFrameColor, (field) => { field.frameColor = colorByName(fieldFrameColor.value).value; });

  bindFieldInput(fieldIntegerDigits, (field) => {
    if (field.type === 'VALUE') field.integerDigits = clamp(Number(fieldIntegerDigits.value) || 1, 1, 9);
  });
  bindFieldInput(fieldDecimalDigits, (field) => {
    if (field.type === 'VALUE') field.decimalDigits = clamp(Number(fieldDecimalDigits.value) || 0, 0, 6);
  });
  bindFieldInput(fieldSigned, (field) => {
    if (field.type === 'VALUE') field.signedValue = fieldSigned.value === '1';
  });
  bindFieldInput(fieldLeadingZeros, (field) => {
    if (field.type === 'VALUE') field.leadingZeros = fieldLeadingZeros.value === '1';
  });

  statusTab.addEventListener('click', () => {
    codeMode = 'status';
    statusTab.classList.add('active');
    contractTab.classList.remove('active');
    updateCodePanel();
  });
  contractTab.addEventListener('click', () => {
    codeMode = 'contract';
    contractTab.classList.add('active');
    statusTab.classList.remove('active');
    updateCodePanel();
  });

  const ELLIPSE_SAMPLE_STEPS = 48;
  const ELLIPSE_UNIT_SAMPLES = [];
  for (let i = 0; i <= ELLIPSE_SAMPLE_STEPS; i++) {
    const a = (i * 2 * Math.PI) / ELLIPSE_SAMPLE_STEPS;
    ELLIPSE_UNIT_SAMPLES.push({ c: Math.cos(a), s: Math.sin(a) });
  }

  function distToSegment(px, py, x1, y1, x2, y2) {
    const dx = x2 - x1;
    const dy = y2 - y1;
    const l2 = dx * dx + dy * dy;
    if (l2 === 0) return Math.hypot(px - x1, py - y1);
    let t = ((px - x1) * dx + (py - y1) * dy) / l2;
    t = Math.max(0, Math.min(1, t));
    return Math.hypot(px - (x1 + t * dx), py - (y1 + t * dy));
  }

  function distanceToFieldBorder(point, field) {
    if (!field) return Infinity;
    if (field.type === 'LINE') {
      const a = { x: field.x ?? 0, y: field.y ?? 0 };
      const b = { x: field.x2 ?? a.x, y: field.y2 ?? a.y };
      return distToSegment(point.x, point.y, a.x, a.y, b.x, b.y);
    }

    if (['RECT', 'ELLIPSE', 'TRIANGLE', 'POLYGON'].includes(field.type)) {
      const pts = getShapeTransformedVertices(field);
      if (pts && pts.length >= 3) {
        if (field.fill && isPointInPolygon(point.x, point.y, pts)) {
          return 0.0;
        }
        return distToPolygonBoundary(point.x, point.y, pts);
      }
    }

    // For TEXT, VALUE, BOOL, BAR, etc.
    const g = computeFieldGeometry(field);
    if (!g) return Infinity;
    if (
      point.x >= g.fieldX && point.x <= g.fieldX + g.fieldW &&
      point.y >= g.fieldY && point.y <= g.fieldY + g.fieldH
    ) {
      return 0;
    }
    return Math.min(
      distToSegment(point.x, point.y, g.fieldX, g.fieldY, g.fieldX + g.fieldW, g.fieldY),
      distToSegment(point.x, point.y, g.fieldX + g.fieldW, g.fieldY, g.fieldX + g.fieldW, g.fieldY + g.fieldH),
      distToSegment(point.x, point.y, g.fieldX + g.fieldW, g.fieldY + g.fieldH, g.fieldX, g.fieldY + g.fieldH),
      distToSegment(point.x, point.y, g.fieldX, g.fieldY + g.fieldH, g.fieldX, g.fieldY)
    );
  }

  function hitTestField(point) {
    const pageFields = fieldsForPage(activePage);

    // Iterar de adelante hacia atrás (top-to-bottom en orden de dibujo)
    for (let index = pageFields.length - 1; index >= 0; index--) {
      const field = pageFields[index];
      if (field.hidden || field.locked) continue;

      if (field.type === 'LINE') {
        const strokeSize = field.borderEnabled !== false ? Math.max(1, Math.round(field.size || 1)) : 1;
        const dist = distanceToFieldBorder(point, field);
        const hitTol = Math.max(3.5, strokeSize / 2 + 2.0);
        if (dist <= hitTol) return field;
        continue;
      }

      if (['RECT', 'ELLIPSE', 'TRIANGLE', 'POLYGON'].includes(field.type)) {
        const strokeSize = field.borderEnabled !== false ? Math.max(1, Math.round(field.size || 1)) : 0;
        const pts = getShapeTransformedVertices(field);
        if (pts && pts.length >= 3) {
          const isInside = isPointInPolygon(point.x, point.y, pts);
          if (field.fill && isInside) return field;
          const distToBorder = distToPolygonBoundary(point.x, point.y, pts);
          const hitTol = Math.max(3.5, strokeSize + 2.0);
          if (isInside) {
            if (distToBorder <= hitTol) return field;
          } else {
            if (distToBorder <= 3.5) return field;
          }
        }
        continue;
      }

      if (field.type === 'RAW_TEXT') {
        const b = getFieldBounds(field);
        if (point.x >= b.minX - 2 && point.x <= b.maxX + 2 && point.y >= b.minY - 2 && point.y <= b.maxY + 2) {
          return field;
        }
        continue;
      }

      const dist = distanceToFieldBorder(point, field);
      if (dist <= 3.0) return field;
    }
    return null;
  }

  function handlePointerDown(event) {
    if (event.button !== 0) return;
    if (
      event.target.closest('.canvas-toolbar') ||
      event.target.closest('.right-vertical-toolbar') ||
      event.target.closest('.panel') ||
      event.target.closest('header') ||
      event.target.closest('nav') ||
      event.target.closest('button') ||
      event.target.closest('input') ||
      event.target.closest('select') ||
      event.target.id === 'topRulerCanvas' ||
      event.target.id === 'bottomRulerCanvas' ||
      event.target.id === 'leftRulerCanvas' ||
      event.target.id === 'rightRulerCanvas' ||
      event.target.closest('.ruler-corner')
    ) {
      return;
    }

    const point = pointFromPointer(event);
    const isTargetDisplay = (event.target === displayCanvas);
    gestureChanged = false;

    if (isTargetDisplay && (selectedTool === 'pick' || selectedTool === 'colorPicker' || event.altKey)) {
      const wasPickTool = selectedTool === 'pick';
      if (pickColorAt(point)) {
        if (wasPickTool && colorToolPrefs.pickReturn && toolBeforePick && toolBeforePick !== 'pick') {
          activateTool(toolBeforePick);
        } else {
          syncToolUI();
          render();
        }
      }
      return;
    }

    if (isTargetDisplay && selectedTool === 'fill') {
      fillAt(point);
      return;
    }

    if (isTargetDisplay && (selectedTool === 'pixel' || selectedTool === 'erase')) {
      return;
    }

    if (['line', 'rect', 'ellipse', 'triangle', 'polygon'].includes(selectedTool) && isTargetDisplay) {
      const type = selectedTool.toUpperCase();
      const snapStep = getSnapStep();
      const startPt = snapStep > 0
        ? { x: Math.round(point.x / snapStep) * snapStep, y: Math.round(point.y / snapStep) * snapStep }
        : point;
      addShapeField(type, startPt, startPt);
      drawing = true;
      render();
      return;
    }

    if (selectedTool === 'rawText' && isTargetDisplay) {
      const hit = hitTestField(point);
      if (hit && hit.type === 'RAW_TEXT') {
        setSelectedKeys([hit.key]);
        draggingObject = true;
        dragInitialPointer = { ...point, rawX: hit.x, rawY: hit.y };
        dragInitialFields = [{ ...hit }];
        syncInputsFromState();
        syncToolUI();
        render();
        return;
      }
      const newField = addRawTextField(point);
      if (newField) {
        draggingObject = true;
        dragInitialPointer = { ...point, rawX: newField.x, rawY: newField.y };
        dragInitialFields = [{ ...newField }];
        render();
      }
      return;
    }

    // 1. Check if clicking on an active selection handle (to resize / deform)
    const handle = hitTestHandle(event);
    if (handle && selectionIsLocked()) {
      showToastMessage('Objeto bloqueado: desbloquéalo en el árbol de objetos para editarlo.', 'warning');
      return;
    }
    if (handle) {
      resizingHandle = handle;
      const flds = selectedFields();
      if (flds.length > 0) {
        resizeInitialBounds = getSelectionBounds(flds);
        resizeInitialPointer = point;
        resizeInitialFields = flds.map((f) => ({ ...f }));
      } else {
        const selPm = window.JWPLCHMIPixelMaps?.getSelected?.();
        if (selPm && selPm.pixels?.length) {
          let minX = Infinity, minY = Infinity, maxX = -Infinity, maxY = -Infinity;
          selPm.pixels.forEach((p) => {
            const px = selPm.x + p.x;
            const py = selPm.y + p.y;
            if (px < minX) minX = px;
            if (px > maxX) maxX = px;
            if (py < minY) minY = py;
            if (py > maxY) maxY = py;
          });
          resizeInitialBounds = { minX, minY, maxX, maxY };
          resizeInitialPointer = point;
          resizeInitialFields = [];
          resizeInitialPixelMap = {
            key: selPm.key,
            x: selPm.x,
            y: selPm.y,
            minX,
            minY,
            maxX,
            maxY,
            pixels: selPm.pixels.map((p) => ({ ...p }))
          };
        }
      }
      return;
    }

    // 2. Check if Shift or Ctrl is pressed: toggle selection on field contour click
    if (event.shiftKey || event.ctrlKey) {
      const hit = hitTestField(point);
      if (hit) {
        if (selectedFieldKeys.includes(hit.key)) {
          setSelectedKeys(selectedFieldKeys.filter((k) => k !== hit.key));
        } else {
          setSelectedKeys([...selectedFieldKeys, hit.key]);
        }
        const primary = selectedField();
        selectedTool = primary ? toolForField(primary) : 'pointer';
        syncInputsFromState();
        syncToolUI();
        render();
        return;
      }
    }

    // 3. Check if clicking inside the current selection bounding box
    // (Allows dragging without having to aim for the 1.5px contour)
    if (isPointInsideSelection(point)) {
      const curSelected = selectedFields();
      if (!selectionIsLocked()) {
        draggingObject = true;
        dragInitialPointer = { ...point, rawX: point.x, rawY: point.y };
        dragInitialFields = curSelected.filter((f) => !f.locked).map((f) => ({ ...f }));
        dragInitialPixelMaps = (window.JWPLCHMIPixelMaps?.getSelectedMaps?.() || []).filter((m) => !m.locked).map((m) => ({ key: m.key, x: m.x, y: m.y }));
        render();
        return;
      }
    }

    // 4. Check if clicking directly on any field's contour (1.5px tolerance)
    const hit = hitTestField(point);
    if (hit) {
      setSelectedKeys([hit.key]);
      window.JWPLCHMIPixelMaps?.setSelectedKeys?.([]);
      selectedTool = toolForField(hit);
      if (!hit.locked) {
        draggingObject = true;
        dragInitialPointer = { ...point, rawX: hit.x, rawY: hit.y };
        dragInitialFields = [{ ...hit }];
      }
      syncInputsFromState();
      syncToolUI();
      render();
      return;
    }

    // 5. Empty space clicked: Clear selection and start CAD Marquee box
    setSelectedKeys([]);
    window.JWPLCHMIPixelMaps?.setSelectedKeys?.([]);
    selectedTool = 'pointer';
    isMarquee = true;
    marqueeStart = point;
    marqueeEnd = point;
    marqueeStartClient = { x: event.clientX, y: event.clientY };
    marqueeEndClient = { x: event.clientX, y: event.clientY };
    updateMarqueeOverlay(marqueeStartClient, marqueeEndClient);
    syncToolUI();
    render();
  }

  function handlePointerMove(event) {
    const rawPoint = pointFromPointer(event);
    updateCursor(rawPoint);

    if (isMarquee && marqueeStart) {
      marqueeEnd = rawPoint;
      marqueeEndClient = { x: event.clientX, y: event.clientY };
      updateMarqueeOverlay(marqueeStartClient, marqueeEndClient);
      render();
    }

    if (resizingHandle) {
      const snapStep = getSnapStep();
      let curX = rawPoint.x;
      let curY = rawPoint.y;
      if (snapStep > 0) {
        curX = Math.round(curX / snapStep) * snapStep;
        curY = Math.round(curY / snapStep) * snapStep;
      }

      const fields = selectedFields();
      const initB = resizeInitialBounds;
      if (!initB) return;

      // ---- Single shape deformation (rotation-aware & anchor-pinned) ----
      if (fields.length === 1 && ['LINE', 'RECT', 'ELLIPSE', 'TRIANGLE', 'POLYGON'].includes(fields[0].type)) {
        const f = fields[0];
        const orig = resizeInitialFields?.[0];
        if (orig) {
          const hid = resizingHandle.id;

          // LINE deformation (endpoint dragging)
          if (f.type === 'LINE') {
            const ox1 = orig.x ?? 0; const oy1 = orig.y ?? 0;
            const ox2 = orig.x2 ?? ox1; const oy2 = orig.y2 ?? oy1;

            const dx = curX - (resizeInitialPointer?.x ?? curX);
            const dy = curY - (resizeInitialPointer?.y ?? curY);

            if (hid === 'p1') {
              f.x = clamp(Math.round(ox1 + dx), 0, WIDTH - 1);
              f.y = clamp(Math.round(oy1 + dy), 0, HEIGHT - 1);
            } else if (hid === 'p2') {
              f.x2 = clamp(Math.round(ox2 + dx), 0, WIDTH - 1);
              f.y2 = clamp(Math.round(oy2 + dy), 0, HEIGHT - 1);
            }
            gestureChanged = true;
            syncShapeInspector();
            render();
            return;
          }

          // 2D Shapes: RECT, ELLIPSE, TRIANGLE, POLYGON
          const origX1 = orig.x ?? 0; const origY1 = orig.y ?? 0;
          const origX2 = orig.x2 ?? origX1; const origY2 = orig.y2 ?? origY1;
          const origW = Math.max(4, Math.abs(origX2 - origX1));
          const origH = Math.max(4, Math.abs(origY2 - origY1));
          const origCx = (origX1 + origX2) / 2;
          const origCy = (origY1 + origY2) / 2;
          const halfW = origW / 2;
          const halfH = origH / 2;
          const angleDeg = Number(orig.rotation) || 0;
          const flipH = Boolean(orig.flipH);
          const flipV = Boolean(orig.flipV);

          // Local unit vectors in world space
          const rad = angleDeg * Math.PI / 180;
          const cos = Math.cos(rad);
          const sin = Math.sin(rad);
          let ux = cos, uy = sin;
          if (flipH) ux = -ux;
          if (flipV) uy = -uy;
          let vx = -sin, vy = cos;
          if (flipH) vx = -vx;
          if (flipV) vy = -vy;

          let anchorRelU = 0, anchorRelV = 0;
          let handleDirU = 0, handleDirV = 0;

          if (hid === 'br')      { anchorRelU = -halfW; anchorRelV = -halfH; handleDirU = 1;  handleDirV = 1;  }
          else if (hid === 'tl') { anchorRelU = halfW;  anchorRelV = halfH;  handleDirU = -1; handleDirV = -1; }
          else if (hid === 'tr') { anchorRelU = -halfW; anchorRelV = halfH;  handleDirU = 1;  handleDirV = -1; }
          else if (hid === 'bl') { anchorRelU = halfW;  anchorRelV = -halfH; handleDirU = -1; handleDirV = 1;  }
          else if (hid === 'rc') { anchorRelU = -halfW; anchorRelV = 0;      handleDirU = 1;  handleDirV = 0;  }
          else if (hid === 'lc') { anchorRelU = halfW;  anchorRelV = 0;      handleDirU = -1; handleDirV = 0;  }
          else if (hid === 'bc') { anchorRelU = 0;      anchorRelV = -halfH; handleDirU = 0;  handleDirV = 1;  }
          else if (hid === 'tc') { anchorRelU = 0;      anchorRelV = halfH;  handleDirU = 0;  handleDirV = -1; }

          const anchorX = origCx + anchorRelU * ux + anchorRelV * vx;
          const anchorY = origCy + anchorRelU * uy + anchorRelV * vy;

          const deltaX = curX - anchorX;
          const deltaY = curY - anchorY;
          const projU = deltaX * ux + deltaY * uy;
          const projV = deltaX * vx + deltaY * vy;

          let newW = origW;
          let newH = origH;
          if (handleDirU !== 0) newW = Math.max(4, projU * handleDirU);
          if (handleDirV !== 0) newH = Math.max(4, projV * handleDirV);

          let newCx = anchorX;
          let newCy = anchorY;
          if (handleDirU !== 0) {
            newCx += (newW / 2) * handleDirU * ux;
            newCy += (newW / 2) * handleDirU * uy;
          }
          if (handleDirV !== 0) {
            newCx += (newH / 2) * handleDirV * vx;
            newCy += (newH / 2) * handleDirV * vy;
          }

          f.x = Math.round(newCx - newW / 2);
          f.y = Math.round(newCy - newH / 2);
          f.x2 = Math.round(newCx + newW / 2);
          f.y2 = Math.round(newCy + newH / 2);

          gestureChanged = true;
          syncShapeInspector();
          render();
          return;
        }
      }

      // ---- Standard proportional resize (unrotated shapes or multi-selection) ----
      const initMinX = initB.minX;
      const initMaxX = initB.maxX;
      const initMinY = initB.minY;
      const initMaxY = initB.maxY;
      const initW = Math.max(1, initMaxX - initMinX);
      const initH = Math.max(1, initMaxY - initMinY);

      let newMinX = initMinX;
      let newMaxX = initMaxX;
      let newMinY = initMinY;
      let newMaxY = initMaxY;

      const hid = resizingHandle.id;
      if (hid === 'br') {
        newMaxX = Math.max(initMinX + 1, curX);
        newMaxY = Math.max(initMinY + 1, curY);
      } else if (hid === 'bl') {
        newMinX = Math.min(initMaxX - 1, curX);
        newMaxY = Math.max(initMinY + 1, curY);
      } else if (hid === 'tr') {
        newMaxX = Math.max(initMinX + 1, curX);
        newMinY = Math.min(initMaxY - 1, curY);
      } else if (hid === 'tl') {
        newMinX = Math.min(initMaxX - 1, curX);
        newMinY = Math.min(initMaxY - 1, curY);
      } else if (hid === 'tc') {
        newMinY = Math.min(initMaxY - 1, curY);
      } else if (hid === 'bc') {
        newMaxY = Math.max(initMinY + 1, curY);
      } else if (hid === 'lc') {
        newMinX = Math.min(initMaxX - 1, curX);
      } else if (hid === 'rc') {
        newMaxX = Math.max(initMinX + 1, curX);
      }

      const newW = Math.max(1, newMaxX - newMinX);
      const newH = Math.max(1, newMaxY - newMinY);
      const scaleX = newW / initW;
      const scaleY = newH / initH;

      fields.forEach((f) => {
        const orig = resizeInitialFields?.find((o) => o.key === f.key);
        if (!orig) return;

        if (['LINE', 'RECT', 'ELLIPSE', 'TRIANGLE', 'POLYGON'].includes(f.type)) {
          const origRelX1 = orig.x - initMinX;
          const origRelY1 = orig.y - initMinY;
          const origRelX2 = (orig.x2 ?? orig.x) - initMinX;
          const origRelY2 = (orig.y2 ?? orig.y) - initMinY;

          f.x = Math.round(newMinX + origRelX1 * scaleX);
          f.y = Math.round(newMinY + origRelY1 * scaleY);
          f.x2 = Math.round(newMinX + origRelX2 * scaleX);
          f.y2 = Math.round(newMinY + origRelY2 * scaleY);
        } else if (f.type === 'MENU') {
          f.x = Math.round(newMinX + (orig.x - initMinX) * scaleX);
          f.y = Math.round(newMinY + (orig.y - initMinY) * scaleY);
          f.width = clamp(Math.round((Number(orig.width) || 1) * scaleX), 20, WIDTH);
          f.height = clamp(Math.round((Number(orig.height) || 1) * scaleY), 8, HEIGHT);
        } else if (f.type === 'BAR') {
          f.barAutoWidth = false;
          const origW = Number(orig.barWidth) || 110;
          const origH = Number(orig.barHeight) || 12;
          const origRadius = Number(orig.circleRadius) || 26;
          const deltaW = newW - initW;
          const deltaH = newH - initH;

          if (f.barPreset === 'CIRCULAR') {
            const deltaSide = (Math.abs(deltaW) >= Math.abs(deltaH)) ? deltaW : deltaH;
            f.circleRadius = Math.max(10, Math.min(80, Math.round(origRadius + deltaSide / 2)));
          } else if (f.barOrientation === 'VERTICAL') {
            if (['rc', 'lc'].includes(hid)) {
              f.barWidth = Math.max(6, Math.min(100, Math.round(origW + deltaW)));
            }
            if (['tc', 'bc', 'tl', 'tr', 'bl', 'br'].includes(hid)) {
              f.barHeight = Math.max(10, Math.min(170, Math.round(origH + deltaH)));
            }
          } else {
            // HORIZONTAL
            if (['rc', 'lc', 'tl', 'tr', 'bl', 'br'].includes(hid)) {
              f.barWidth = Math.max(20, Math.min(320, Math.round(origW + deltaW)));
            }
            if (['tc', 'bc'].includes(hid)) {
              f.barHeight = Math.max(4, Math.min(170, Math.round(origH + deltaH)));
            }
          }

          if (['tl', 'bl', 'lc'].includes(hid)) {
            f.x = Math.round(newMinX);
          }
          if (['tl', 'tr', 'tc'].includes(hid)) {
            f.y = Math.round(newMinY);
          }
        } else {
          const origRelX = orig.x - initMinX;
          const origRelY = orig.y - initMinY;
          f.x = Math.round(newMinX + origRelX * scaleX);
          f.y = Math.round(newMinY + origRelY * scaleY);
          if (orig.width !== undefined) {
            f.width = Math.max(10, Math.round(orig.width * scaleX));
          }
          if (orig.height !== undefined) {
            f.height = Math.max(4, Math.round(orig.height * scaleY));
          }
        }
      });

      if (fields.length === 0 && resizeInitialPixelMap) {
        const pm = window.JWPLCHMIPixelMaps?.getSelected?.();
        if (pm && pm.key === resizeInitialPixelMap.key) {
          const origMinLocalX = resizeInitialPixelMap.minX - resizeInitialPixelMap.x;
          const origMinLocalY = resizeInitialPixelMap.minY - resizeInitialPixelMap.y;
          pm.x = Math.round(newMinX);
          pm.y = Math.round(newMinY);
          const scaledPixels = [];
          const seen = new Set();
          resizeInitialPixelMap.pixels.forEach((p) => {
            const relX = p.x - origMinLocalX;
            const relY = p.y - origMinLocalY;
            const nx = Math.round(relX * scaleX);
            const ny = Math.round(relY * scaleY);
            const key = `${nx},${ny}`;
            if (!seen.has(key)) {
              seen.add(key);
              scaledPixels.push({ x: nx, y: ny, color: p.color });
            }
          });
          pm.pixels = scaledPixels;
          window.dispatchEvent(new CustomEvent('jwplc:pixelmap-updated'));
        }
      }

      gestureChanged = true;
      syncInputsFromState();
      render();
      return;
    }

    if (!drawing && !draggingObject) {
      if (!isMarquee) {
        const hoveredHandle = hitTestHandle(event);
        if (hoveredHandle) {
          displayCanvas.style.cursor = hoveredHandle.cursor || 'pointer';
        } else if (isPointInsideSelection(rawPoint)) {
          displayCanvas.style.cursor = 'move';
        } else if (hitTestField(rawPoint)) {
          displayCanvas.style.cursor = 'pointer';
        } else if (selectedTool === 'pointer' && window.JWPLCHMIPixelMaps?.hitTest?.(rawPoint)) {
          displayCanvas.style.cursor = 'pointer';
        } else {
          displayCanvas.style.cursor = (['pixel', 'erase', 'fill', 'pick'].includes(selectedTool) ? (selectedTool === 'erase' ? 'none' : 'crosshair') : 'default');
        }
      }
      return;
    }

    if (drawing) {
      if (['line', 'rect', 'ellipse', 'triangle', 'polygon'].includes(selectedTool) || ['LINE', 'RECT', 'ELLIPSE', 'TRIANGLE', 'POLYGON'].includes(selectedField()?.type)) {
        const field = selectedField();
        if (field) {
          const snapStep = getSnapStep();
          let nextX = rawPoint.x;
          let nextY = rawPoint.y;
          if (snapStep > 0) {
            nextX = Math.round(nextX / snapStep) * snapStep;
            nextY = Math.round(nextY / snapStep) * snapStep;
          }
          field.x2 = nextX;
          field.y2 = nextY;
          gestureChanged = true;
          syncInputsFromState();
          render();
        }
      } else {
        if (!inside(rawPoint.x, rawPoint.y)) return;
        const value = selectedTool === 'erase' ? 0x0000 : selectedColor.value;
        if (lastPoint) gestureChanged = rasterLine(lastPoint.x, lastPoint.y, rawPoint.x, rawPoint.y, value) || gestureChanged;
        else gestureChanged = setLayerPixel(rawPoint.x, rawPoint.y, value) || gestureChanged;
        lastPoint = rawPoint;
        render();
      }
    } else if (draggingObject) {
      const snapStep = getSnapStep();
      let dx = rawPoint.x - (dragInitialPointer?.x ?? 0);
      let dy = rawPoint.y - (dragInitialPointer?.y ?? 0);

      if (selectedTool === 'rawText' && selectedFields().length === 0) {
        let nextX = (dragInitialPointer?.rawX ?? 0) + dx;
        let nextY = (dragInitialPointer?.rawY ?? 0) + dy;
        if (snapStep > 0) {
          nextX = Math.round(nextX / snapStep) * snapStep;
          nextY = Math.round(nextY / snapStep) * snapStep;
        }
        nextX = clamp(nextX, 0, WIDTH - 1);
        nextY = clamp(nextY, 0, HEIGHT - 1);
        gestureChanged = gestureChanged || nextX !== rawState.x || nextY !== rawState.y;
        rawState.x = nextX;
        rawState.y = nextY;
      } else {
        const fields = selectedFields();
        const primaryOrig = dragInitialFields?.[0];
        if (snapStep > 0 && primaryOrig) {
          const snappedX = Math.round((primaryOrig.x + dx) / snapStep) * snapStep;
          const snappedY = Math.round((primaryOrig.y + dy) / snapStep) * snapStep;
          dx = snappedX - primaryOrig.x;
          dy = snappedY - primaryOrig.y;
        } else if (snapStep > 0) {
          dx = Math.round(dx / snapStep) * snapStep;
          dy = Math.round(dy / snapStep) * snapStep;
        }
        fields.forEach((f) => {
          const orig = dragInitialFields?.find((o) => o.key === f.key);
          if (!orig) return;
          if (['LINE', 'RECT', 'ELLIPSE', 'TRIANGLE', 'POLYGON'].includes(f.type)) {
            f.x = orig.x + dx;
            f.y = orig.y + dy;
            if (orig.x2 !== undefined) f.x2 = orig.x2 + dx;
            if (orig.y2 !== undefined) f.y2 = orig.y2 + dy;
          } else {
            f.x = orig.x + dx;
            f.y = orig.y + dy;
          }
          if (f.type === 'RAW_TEXT') {
            rawState.x = f.x;
            rawState.y = f.y;
          }
          if (f.type === 'NAV_BAR') {
            window.JWPLCHMIPages?.syncNavBarConfigFromField?.(f);
          }
          gestureChanged = true;
        });
        if (dragInitialPixelMaps && dragInitialPixelMaps.length > 0) {
          dragInitialPixelMaps.forEach((orig) => {
            const pm = window.JWPLCHMIPixelMaps?.getAll?.()?.find((x) => x.key === orig.key);
            if (pm) {
              pm.x = orig.x + dx;
              pm.y = orig.y + dy;
              gestureChanged = true;
            }
          });
        }
      }
      syncInputsFromState();
      render();
    }
  }

  function segmentsIntersect(x1, y1, x2, y2, x3, y3, x4, y4) {
    function ccw(ax, ay, bx, by, cx, cy) {
      return (cy - ay) * (bx - ax) > (by - ay) * (cx - ax);
    }
    return (ccw(x1, y1, x3, y3, x4, y4) !== ccw(x2, y2, x3, y3, x4, y4)) &&
           (ccw(x1, y1, x2, y2, x3, y3) !== ccw(x1, y1, x2, y2, x4, y4));
  }

  function segmentIntersectsBox(x1, y1, x2, y2, L, R, T, B) {
    if ((x1 >= L && x1 <= R && y1 >= T && y1 <= B) || (x2 >= L && x2 <= R && y2 >= T && y2 <= B)) {
      return true;
    }
    return (
      segmentsIntersect(x1, y1, x2, y2, L, T, R, T) ||
      segmentsIntersect(x1, y1, x2, y2, L, B, R, B) ||
      segmentsIntersect(x1, y1, x2, y2, L, T, L, B) ||
      segmentsIntersect(x1, y1, x2, y2, R, T, R, B)
    );
  }

  function doesBoxTouchContour(boxL, boxR, boxT, boxB, field) {
    const a = { x: field.x, y: field.y };
    const b = { x: field.x2 ?? field.x, y: field.y2 ?? field.y };

    if (field.type === 'LINE') {
      return segmentIntersectsBox(a.x, a.y, b.x, b.y, boxL, boxR, boxT, boxB);
    }

    if (field.type === 'RECT') {
      const left = Math.min(a.x, b.x);
      const right = Math.max(a.x, b.x);
      const top = Math.min(a.y, b.y);
      const bottom = Math.max(a.y, b.y);
      return (
        segmentIntersectsBox(left, top, right, top, boxL, boxR, boxT, boxB) ||
        segmentIntersectsBox(right, top, right, bottom, boxL, boxR, boxT, boxB) ||
        segmentIntersectsBox(right, bottom, left, bottom, boxL, boxR, boxT, boxB) ||
        segmentIntersectsBox(left, bottom, left, top, boxL, boxR, boxT, boxB)
      );
    }

    if (field.type === 'ELLIPSE') {
      const cx = (a.x + b.x) / 2;
      const cy = (a.y + b.y) / 2;
      const rx = Math.max(0.5, Math.abs(b.x - a.x) / 2);
      const ry = Math.max(0.5, Math.abs(b.y - a.y) / 2);
      const steps = 24;
      let prevX = cx + rx;
      let prevY = cy;
      for (let i = 1; i <= steps; i++) {
        const angle = (i * 2 * Math.PI) / steps;
        const curX = cx + rx * Math.cos(angle);
        const curY = cy + ry * Math.sin(angle);
        if (segmentIntersectsBox(prevX, prevY, curX, curY, boxL, boxR, boxT, boxB)) {
          return true;
        }
        prevX = curX;
        prevY = curY;
      }
      return false;
    }

    if (field.type === 'TRIANGLE') {
      const topPt = { x: Math.round((a.x + b.x) / 2), y: a.y };
      const bl = { x: a.x, y: b.y };
      const br = { x: b.x, y: b.y };
      return (
        segmentIntersectsBox(topPt.x, topPt.y, bl.x, bl.y, boxL, boxR, boxT, boxB) ||
        segmentIntersectsBox(bl.x, bl.y, br.x, br.y, boxL, boxR, boxT, boxB) ||
        segmentIntersectsBox(br.x, br.y, topPt.x, topPt.y, boxL, boxR, boxT, boxB)
      );
    }

    if (field.type === 'POLYGON') {
      const pts = getPolygonVertices(field);
      const numSides = pts.length;
      for (let i = 0; i < numSides; i++) {
        const p1 = pts[i];
        const p2 = pts[(i + 1) % numSides];
        if (segmentIntersectsBox(p1.x, p1.y, p2.x, p2.y, boxL, boxR, boxT, boxB)) {
          return true;
        }
      }
      return false;
    }

    // For TEXT, VALUE, BOOL, BAR, etc.
    const g = computeFieldGeometry(field);
    const fL = g.fieldX;
    const fR = g.fieldX + g.fieldW;
    const fT = g.fieldY;
    const fB = g.fieldY + g.fieldH;
    return !(fR < boxL || fL > boxR || fB < boxT || fT > boxB);
  }

  function endPointer() {
    const changed = gestureChanged;
    const wasDrawingShape = drawing && ['line', 'rect', 'ellipse', 'triangle', 'polygon'].includes(selectedTool);
    drawing = false;
    draggingObject = false;
    resizingHandle = null;
    resizeInitialBounds = null;
    resizeInitialPointer = null;
    resizeInitialFields = null;
    resizeInitialPixelMap = null;
    dragInitialPointer = null;
    dragInitialFields = null;
    if (dragInitialPixelMaps && dragInitialPixelMaps.length > 0) {
      window.JWPLCHMIPixelMaps?.syncInspector?.();
      window.JWPLCHMIPixelMaps?.notifyChanged?.();
    }
    dragInitialPixelMaps = null;
    lastPoint = null;
    gestureChanged = false;

    if (isMarquee) {
      updateMarqueeOverlay(null, null);
      if (marqueeStart && marqueeEnd) {
        const boxL = Math.min(marqueeStart.x, marqueeEnd.x);
        const boxR = Math.max(marqueeStart.x, marqueeEnd.x);
        const boxT = Math.min(marqueeStart.y, marqueeEnd.y);
        const boxB = Math.max(marqueeStart.y, marqueeEnd.y);

        const isLeftToRight = marqueeEnd.x >= marqueeStart.x;

        // When marquee selection covers near or across canvas edges, ensure full canvas coverage
        const effectiveBoxL = boxL <= 4 ? -Infinity : boxL;
        const effectiveBoxR = boxR >= WIDTH - 5 ? Infinity : boxR;
        const effectiveBoxT = boxT <= 4 ? -Infinity : boxT;
        const effectiveBoxB = boxB >= HEIGHT - 5 ? Infinity : boxB;

        if (boxR - boxL >= 2 || boxB - boxT >= 2) {
          const matchedList = [];
          for (let i = 0; i < hmiFields.length; i++) {
            const f = hmiFields[i];
            if (Number(f.page || 0) !== activePage) continue;

            if (isLeftToRight) {
              const fb = getFieldBounds(f);
              const fullyInside = (fb.minX >= effectiveBoxL && fb.maxX <= effectiveBoxR && fb.minY >= effectiveBoxT && fb.maxY <= effectiveBoxB);
              if (fullyInside) {
                matchedList.push(f);
              }
            } else {
              // SolidWorks Crossing: Touches or crosses the ACTUAL CONTOUR or is enclosed
              const fb = getFieldBounds(f);
              const fullyInside = (fb.minX >= boxL && fb.maxX <= boxR && fb.minY >= boxT && fb.maxY <= boxB);
              if (fullyInside || doesBoxTouchContour(boxL, boxR, boxT, boxB, f)) {
                matchedList.push(f);
              }
            }
          }

          // Check pixelmaps for CAD Marquee (supports multiple pixel objects)
          const matchedPixelMaps = [];
          const allPixelMaps = window.JWPLCHMIPixelMaps?.getAll?.() || [];
          for (let i = 0; i < allPixelMaps.length; i++) {
            const pm = allPixelMaps[i];
            if (Number(pm.page || 0) !== activePage || pm.editorVisible === false || !pm.pixels?.length) continue;
            if (isLeftToRight) {
              // Blue Window: Must be completely enclosed in marquee box
              let pMinX = Infinity, pMinY = Infinity, pMaxX = -Infinity, pMaxY = -Infinity;
              pm.pixels.forEach((p) => {
                const px = pm.x + p.x;
                const py = pm.y + p.y;
                if (px < pMinX) pMinX = px;
                if (px > pMaxX) pMaxX = px;
                if (py < pMinY) pMinY = py;
                if (py > pMaxY) pMaxY = py;
              });
              if (pMinX !== Infinity && pMinX >= effectiveBoxL && pMaxX <= effectiveBoxR && pMinY >= effectiveBoxT && pMaxY <= effectiveBoxB) {
                matchedPixelMaps.push(pm);
              }
            } else {
              // Green Crossing ("modo verde"): Touches ANY drawn pixel
              const touchesPixel = pm.pixels.some((p) => {
                const px = pm.x + p.x;
                const py = pm.y + p.y;
                return px >= boxL && px <= boxR && py >= boxT && py <= boxB;
              });
              if (touchesPixel) {
                matchedPixelMaps.push(pm);
              }
            }
          }

          if (matchedList.length > 0 && matchedPixelMaps.length === 0) {
            setSelectedKeys(matchedList.map((f) => f.key));
            const primary = selectedField();
            selectedTool = primary ? toolForField(primary) : 'pointer';
            window.JWPLCHMIPixelMaps?.setSelectedKeys?.([]);
            syncInputsFromState();
            syncToolUI();
          } else if (matchedPixelMaps.length > 0 && matchedList.length === 0) {
            setSelectedKeys([]);
            window.JWPLCHMIPixelMaps?.setSelectedKeys?.(matchedPixelMaps.map((m) => m.key));
            selectedTool = 'pointer';
            syncToolUI();
          } else if (matchedList.length > 0 && matchedPixelMaps.length > 0) {
            setSelectedKeys(matchedList.map((f) => f.key));
            window.JWPLCHMIPixelMaps?.setSelectedKeys?.(matchedPixelMaps.map((m) => m.key), false);
            selectedTool = 'pointer';
            syncInputsFromState();
            syncToolUI();
          } else {
            setSelectedKeys([]);
            window.JWPLCHMIPixelMaps?.setSelectedKeys?.([]);
            syncToolUI();
          }
        }
      }
      isMarquee = false;
      marqueeStart = null;
      marqueeEnd = null;
      render();
    }

    if (wasDrawingShape) {
      selectedTool = 'pointer';
      syncToolUI();
      window.dispatchEvent(new CustomEvent('jwplc:tool-changed', { detail: { tool: 'pointer' } }));
      render();
    }

    if (changed) commitHistory();
  }

  if (canvasViewport) {
    canvasViewport.addEventListener('pointerdown', handlePointerDown);
  } else {
    displayCanvas.addEventListener('pointerdown', handlePointerDown);
  }
  window.addEventListener('pointermove', handlePointerMove);
  window.addEventListener('pointerup', endPointer);
  window.addEventListener('pointercancel', endPointer);

  canvasViewport?.addEventListener('pointerleave', () => {
    cursorStatus.textContent = 'X: — · Y: —';
    pixelStatus.textContent = 'Pixel: —';
    updateEyedropperLiveUI({ x: -1, y: -1 });
  });

  function isEditingTarget(target) {
    return target instanceof HTMLInputElement ||
      target instanceof HTMLTextAreaElement ||
      target instanceof HTMLSelectElement ||
      target?.isContentEditable;
  }

  function selectAllOnPage() {
    const pageFields = fieldsForPage(activePage).filter((f) => !f.hidden && !f.locked);
    const allPixelMaps = (window.JWPLCHMIPixelMaps?.getAll?.() || []).filter(
      (pm) => Number(pm.page || 0) === activePage && pm.editorVisible !== false && !pm.locked && pm.pixels?.length
    );

    setSelectedKeys(pageFields.map((f) => f.key));
    window.JWPLCHMIPixelMaps?.setSelectedKeys?.(allPixelMaps.map((m) => m.key), false);

    selectedTool = 'pointer';
    syncInputsFromState();
    syncToolUI();
    render();
  }

  function nudgeSelection(dx, dy) {
    const snapStep = getSnapStep();
    if (snapStep > 0) {
      dx = Math.sign(dx) * Math.max(Math.abs(dx), snapStep);
      dy = Math.sign(dy) * Math.max(Math.abs(dy), snapStep);
    }

    const fields = selectedFields();
    const selPms = (window.JWPLCHMIPixelMaps?.getSelectedMaps?.() || []).filter(
      (pm) => Number(pm.page || 0) === activePage && pm.editorVisible !== false && !pm.locked && pm.pixels?.length
    );

    let moved = false;
    if (fields.length > 0) {
      fields.forEach((field) => {
        if (field.locked) return;
        if (['LINE', 'RECT', 'ELLIPSE', 'TRIANGLE', 'POLYGON'].includes(field.type)) {
          field.x += dx;
          field.y += dy;
          if (field.x2 !== undefined) field.x2 += dx;
          if (field.y2 !== undefined) field.y2 += dy;
          moved = true;
        } else if (field.type === 'RAW_TEXT') {
          const b = getFieldBounds(field);
          let nextX = clamp(field.x + dx, 0, Math.max(0, WIDTH - b.width));
          let nextY = clamp(field.y + dy, 0, Math.max(0, HEIGHT - b.height));
          if (nextX !== field.x || nextY !== field.y) moved = true;
          field.x = nextX;
          field.y = nextY;
          rawState.x = nextX;
          rawState.y = nextY;
        } else {
          const g = computeFieldGeometry(field);
          let nextX = clamp(field.x + dx, 0, Math.max(0, WIDTH - g.fieldW));
          let nextY = clamp(field.y + dy, 0, Math.max(0, HEIGHT - g.fieldH));
          if (nextX !== field.x || nextY !== field.y) moved = true;
          field.x = nextX;
          field.y = nextY;
        }
      });
    }

    if (selPms.length > 0) {
      selPms.forEach((pm) => {
        const b = window.JWPLCHMIPixelMaps?.getBounds?.(pm);
        if (b) {
          pm.x = Math.max(0, Math.min(WIDTH - b.width, pm.x + dx));
          pm.y = Math.max(0, Math.min(HEIGHT - b.height, pm.y + dy));
          moved = true;
        }
      });
      window.JWPLCHMIPixelMaps?.syncInspector?.();
      window.JWPLCHMIPixelMaps?.notifyChanged?.();
    }

    if (moved) {
      syncInputsFromState();
      render();
      return true;
    }
    if (fields.length > 0) return false;

    if (selectedTool === 'rawText') {
      let nextX = clamp(rawState.x + dx, 0, WIDTH - 1);
      let nextY = clamp(rawState.y + dy, 0, HEIGHT - 1);
      if (snapStep > 0) {
        nextX = Math.round(nextX / snapStep) * snapStep;
        nextY = Math.round(nextY / snapStep) * snapStep;
        nextX = clamp(nextX, 0, WIDTH - 1);
        nextY = clamp(nextY, 0, HEIGHT - 1);
      }
      if (nextX === rawState.x && nextY === rawState.y) return false;
      rawState.x = nextX;
      rawState.y = nextY;
      syncInputsFromState();
      render();
      return true;
    }
    return false;
  }

  function alignSelectedShapes(alignType) {
    const fields = selectedFields();
    if (fields.length === 0) {
      if (selectedTool === 'rawText') {
        const b = rawTextBounds(rawState.value || '', rawState.size, rawState.font);
        const w = b.width;
        const h = b.height;
        const cx = Math.max(0, Math.round((WIDTH - w) / 2));
        const cy = Math.max(0, Math.round((HEIGHT - h) / 2));
        const rx = Math.max(0, WIDTH - w);
        const by = Math.max(0, HEIGHT - h);

        if (alignType === 'top-left') { rawState.x = 0; rawState.y = 0; }
        else if (alignType === 'top') { rawState.x = cx; rawState.y = 0; }
        else if (alignType === 'top-right') { rawState.x = rx; rawState.y = 0; }
        else if (alignType === 'left' || alignType === 'mid-left') { rawState.x = 0; rawState.y = cy; }
        else if (alignType === 'center' || alignType === 'mid-center') { rawState.x = cx; rawState.y = cy; }
        else if (alignType === 'right' || alignType === 'mid-right') { rawState.x = rx; rawState.y = cy; }
        else if (alignType === 'bottom-left') { rawState.x = 0; rawState.y = by; }
        else if (alignType === 'bottom') { rawState.x = cx; rawState.y = by; }
        else if (alignType === 'bottom-right') { rawState.x = rx; rawState.y = by; }
        else if (alignType === 'hcenter') { rawState.x = cx; }
        else if (alignType === 'vcenter') { rawState.y = cy; }
        syncInputsFromState();
        render();
      }
      return;
    }
    fields.forEach((f) => {
      const b = getFieldBounds(f);
      let dx = 0;
      let dy = 0;
      const hCenterX = Math.round((WIDTH - b.width) / 2) - b.minX;
      const vCenterY = Math.round((HEIGHT - b.height) / 2) - b.minY;
      const leftX = 0 - b.minX;
      const rightX = (WIDTH - 1) - b.maxX;
      const topY = 0 - b.minY;
      const bottomY = (HEIGHT - 1) - b.maxY;

      if (alignType === 'top-left') {
        dx = leftX; dy = topY;
      } else if (alignType === 'top') {
        dx = hCenterX; dy = topY;
      } else if (alignType === 'top-right') {
        dx = rightX; dy = topY;
      } else if (alignType === 'left' || alignType === 'mid-left') {
        dx = leftX; dy = vCenterY;
      } else if (alignType === 'center' || alignType === 'mid-center') {
        dx = hCenterX; dy = vCenterY;
      } else if (alignType === 'right' || alignType === 'mid-right') {
        dx = rightX; dy = vCenterY;
      } else if (alignType === 'bottom-left') {
        dx = leftX; dy = bottomY;
      } else if (alignType === 'bottom') {
        dx = hCenterX; dy = bottomY;
      } else if (alignType === 'bottom-right') {
        dx = rightX; dy = bottomY;
      } else if (alignType === 'hcenter') {
        dx = hCenterX;
      } else if (alignType === 'vcenter') {
        dy = vCenterY;
      }
      f.x += dx;
      f.y += dy;
      if (f.x2 !== undefined) f.x2 += dx;
      if (f.y2 !== undefined) f.y2 += dy;
      if (f.type === 'RAW_TEXT') {
        rawState.x = f.x;
        rawState.y = f.y;
      }
      if (f.type === 'NAV_BAR') {
        window.JWPLCHMIPages?.syncNavBarConfigFromField?.(f);
      }
    });
    syncShapeInspector();
    syncInputsFromState();
    render();
    commitHistory();
  }

  document.addEventListener('keydown', (event) => {
    const ctrl = event.ctrlKey || event.metaKey;
    if (!isEditingTarget(event.target)) {
      if (!ctrl && !event.altKey) {
        const key = event.key.toLowerCase();
        if (key === 'v') {
          selectedTool = 'pointer';
          syncToolUI();
          render();
          return;
        }
        if (key === 'b') {
          selectedTool = 'pixel';
          syncToolUI();
          render();
          return;
        }
        if (key === 'e') {
          selectedTool = 'erase';
          syncToolUI();
          render();
          return;
        }
        if (key === 'g') {
          activateTool('fill');
          return;
        }
        if (key === 'i') {
          activateTool('pick');
          return;
        }
      }

      if (ctrl && event.key.toLowerCase() === 'a') {
        event.preventDefault();
        selectAllOnPage();
        return;
      }
      if (event.code === 'Space' || event.key === ' ') {
        event.preventDefault();
        if (event.shiftKey) {
          fitSelection();
        } else {
          fitCanvas();
        }
        return;
      }
      if (ctrl && event.key.toLowerCase() === 'c') {
        event.preventDefault();
        const sources = selectedFields();
        if (sources.length > 0) clipboard = sources.map((f) => JSON.parse(JSON.stringify(f)));
        return;
      }
      if (ctrl && event.key.toLowerCase() === 'v') {
        event.preventDefault();
        if (clipboard.length === 0 || hmiFields.length + clipboard.length > MAX_FIELDS) return;
        const newKeys = [];
        clipboard.forEach((source) => {
          fieldSerial += 1;
          const copy = JSON.parse(JSON.stringify(source));
          copy.key = `${source.type.toLowerCase()}-${fieldSerial}`;
          delete copy.layerOrder;
          copy.name = nextSequential(source.name || source.type, 'name');
          copy.page = activePage;
          copy.x = (source.x || 0) + 10;
          copy.y = (source.y || 0) + 10;
          if (source.x2 !== undefined) copy.x2 = source.x2 + 10;
          if (source.y2 !== undefined) copy.y2 = source.y2 + 10;
          assignSequentialSymbols(copy, source);
          placeNewField(copy);
          assignNewLayerOrder(copy);
          hmiFields.push(copy);
          newKeys.push(copy.key);
        });
        setSelectedKeys(newKeys);
        const primary = selectedField();
        selectedTool = primary ? toolForField(primary) : 'pointer';
        syncInputsFromState(); syncToolUI(); render(); commitHistory();
        return;
      }
      if (ctrl && event.key.toLowerCase() === 'z') {
        event.preventDefault();
        if (event.shiftKey) redo();
        else undo();
        return;
      }
      if (ctrl && event.key.toLowerCase() === 'y') {
        event.preventDefault();
        redo();
        return;
      }
      if (ctrl && event.key.toLowerCase() === 'd') {
        event.preventDefault();
        duplicateSelectedField();
        return;
      }
      if (event.key === 'Delete' || event.key === 'Backspace') {
        event.preventDefault();
        if (selectedFields().some((f) => f.locked)) {
          showToastMessage('Objeto bloqueado: desbloquéalo en el árbol de objetos para borrarlo.', 'warning');
          return;
        }
        deleteSelectedField();
        return;
      }
      if (event.key === 'Escape') {
        setSelectedKeys([]);
        selectedTool = 'none';
        syncToolUI();
        render();
        return;
      }

      const step = event.shiftKey ? 10 : 1;
      const delta = {
        ArrowLeft: [-step, 0],
        ArrowRight: [step, 0],
        ArrowUp: [0, -step],
        ArrowDown: [0, step]
      }[event.key];
      if (delta) {
        event.preventDefault();
        keyboardNudgeChanged = nudgeSelection(delta[0], delta[1]) || keyboardNudgeChanged;
      }
    }
  });

  document.addEventListener('keyup', (event) => {
    if (['ArrowLeft', 'ArrowRight', 'ArrowUp', 'ArrowDown'].includes(event.key) && keyboardNudgeChanged) {
      keyboardNudgeChanged = false;
      commitHistory();
    }
  });

  buildColorSelect(rawTextColor, 'WHITE');
  buildColorSelect(rawTextBackground, 'BLACK', true);
  buildColorSelect(fieldFillColor, 'WHITE');
  buildColorSelect(fieldLabelColor, 'WHITE');
  buildColorSelect(fieldValueColor, 'CYAN');
  buildColorSelect(fieldBackgroundColor, 'BLACK', true);
  buildColorSelect(fieldFrameColor, 'WHITE');

  attachVisualColorPicker(rawTextColor,
    () => (selectedField()?.type === 'RAW_TEXT' ? (selectedField()?.textColor ?? 0xFFFF) : rawState.foreground),
    (val) => {
      const f = selectedField();
      if (f && f.type === 'RAW_TEXT') f.textColor = val;
      rawState.foreground = val;
    });

  attachVisualColorPicker(rawTextBackground,
    () => (selectedField()?.type === 'RAW_TEXT' ? (selectedField()?.backgroundColor ?? 0x0000) : rawState.background),
    (val) => {
      const f = selectedField();
      if (f && f.type === 'RAW_TEXT') f.backgroundColor = val;
      rawState.background = val;
    });

  attachVisualColorPicker(fieldFillColor,
    () => selectedField()?.fillColor ?? selectedColor.value,
    (val) => {
      const f = selectedField();
      if (f) f.fillColor = val;
    });

  attachVisualColorPicker(fieldLabelColor,
    () => selectedField()?.labelColor ?? 0xFFFF,
    (val) => {
      const f = selectedField();
      if (f) f.labelColor = val;
    });

  attachVisualColorPicker(fieldValueColor,
    () => selectedField()?.valueColor ?? 0x07FF,
    (val) => {
      const f = selectedField();
      if (f) f.valueColor = val;
    });

  attachVisualColorPicker(fieldBackgroundColor,
    () => selectedField()?.backgroundColor ?? 0x0000,
    (val) => {
      const f = selectedField();
      if (f) f.backgroundColor = val;
    });

  attachVisualColorPicker(fieldFrameColor,
    () => selectedField()?.frameColor ?? 0xFFFF,
    (val) => {
      const f = selectedField();
      if (f) f.frameColor = val;
    });

  clearRecentColorsBtn?.addEventListener('click', () => {
    recentColors = [0x0000, 0xFFFF, 0xF800, 0x07E0, 0x001F, 0x07FF, 0xFFE0, 0xFD20];
    renderRecentColors();
  });
  eyedropperUseFillBtn?.addEventListener('click', () => activateTool('fill'));
  document.getElementById('eyedropperUseBrushBtn')?.addEventListener('click', () => {
    window.JWPLCHMIPixelMaps?.prepareBrush?.(selectedColor.value & 0xFFFF);
    activateTool('pixel');
  });
  eyedropperSection?.querySelectorAll('[data-color-tool]').forEach((btn) => {
    btn.addEventListener('click', () => {
      if (btn.dataset.colorTool !== selectedTool) activateTool(btn.dataset.colorTool);
    });
  });
  eyedropperSection?.querySelectorAll('[data-fill-mode]').forEach((btn) => {
    btn.addEventListener('click', () => {
      colorToolPrefs.fillMode = btn.dataset.fillMode === 'GLOBAL' ? 'GLOBAL' : 'CONTIGUOUS';
      syncColorToolInspector();
    });
  });
  document.getElementById('fillShapesToggle')?.addEventListener('change', (event) => {
    colorToolPrefs.fillShapes = Boolean(event.target.checked);
  });
  document.getElementById('pickReturnToggle')?.addEventListener('change', (event) => {
    colorToolPrefs.pickReturn = Boolean(event.target.checked);
  });

  // ── Shape Inspector Events ──────────────────────────────────────────────
  let activeShapeMainTab = 'design';
  let activeShapeSubTab = 'form';

  function syncShapeTabPanes() {
    const subTabsEl = document.getElementById('shapeSubTabs');
    const paneForm = document.getElementById('shapePaneForm');
    const paneAppear = document.getElementById('shapePaneAppear');
    const paneLoc = document.getElementById('shapePaneLoc');
    const paneTech = document.getElementById('shapePaneTech');

    document.querySelectorAll('#shapeMainTabs [data-shape-tab]').forEach((btn) => {
      const isAct = btn.dataset.shapeTab === activeShapeMainTab;
      btn.classList.toggle('active', isAct);
      btn.classList.toggle('is-active', isAct);
    });
    document.querySelectorAll('#shapeSubTabs [data-shape-subtab]').forEach((btn) => {
      const isAct = btn.dataset.shapeSubtab === activeShapeSubTab;
      btn.classList.toggle('active', isAct);
      btn.classList.toggle('is-active', isAct);
    });

    if (activeShapeMainTab === 'tech') {
      if (subTabsEl) subTabsEl.hidden = true;
      if (paneForm) paneForm.hidden = true;
      if (paneAppear) paneAppear.hidden = true;
      if (paneLoc) paneLoc.hidden = true;
      if (paneTech) paneTech.hidden = false;
    } else {
      if (subTabsEl) subTabsEl.hidden = false;
      if (paneTech) paneTech.hidden = true;
      if (paneForm) paneForm.hidden = activeShapeSubTab !== 'form';
      if (paneAppear) paneAppear.hidden = activeShapeSubTab !== 'appear';
      if (paneLoc) paneLoc.hidden = activeShapeSubTab !== 'loc';
    }
  }

  document.querySelectorAll('#shapeMainTabs [data-shape-tab]').forEach((btn) => {
    btn.addEventListener('click', (e) => {
      e.preventDefault();
      activeShapeMainTab = btn.dataset.shapeTab || 'design';
      syncShapeTabPanes();
      syncShapeInspector();
    });
  });

  document.querySelectorAll('#shapeSubTabs [data-shape-subtab]').forEach((btn) => {
    btn.addEventListener('click', (e) => {
      e.preventDefault();
      activeShapeSubTab = btn.dataset.shapeSubtab || 'form';
      syncShapeTabPanes();
    });
  });

  document.querySelectorAll('#shapeTypeSelectorGrid [data-shape-type]').forEach((btn) => {
    btn.addEventListener('click', () => {
      const f = selectedField(); if (!f) return;
      const newType = btn.dataset.shapeType;
      if (!newType || f.type === newType) return;
      f.type = newType;
      if (newType === 'POLYGON' && (!f.sides || f.sides < 3)) f.sides = 5;
      syncInputsFromState();
      render();
      commitHistory();
    });
  });

  function applyShapeCornerRadius(val, commit = false) {
    const f = selectedField(); if (!f) return;
    const maxR = getMaxCornerRadius(f);
    const clamped = Math.max(0, Math.min(maxR, Math.round(Number(val) || 0)));
    f.cornerRadius = clamped;
    syncShapeInspector();
    render();
    if (commit) commitHistory();
  }

  const shapeCornerRadiusEl = document.getElementById('shapeCornerRadius');
  shapeCornerRadiusEl?.addEventListener('input', () => applyShapeCornerRadius(shapeCornerRadiusEl.value, false));
  shapeCornerRadiusEl?.addEventListener('change', () => applyShapeCornerRadius(shapeCornerRadiusEl.value, true));

  document.querySelectorAll('#shapeCornerPresetGrid [data-corner-preset]').forEach((btn) => {
    btn.addEventListener('click', () => {
      const f = selectedField(); if (!f) return;
      const preset = btn.dataset.cornerPreset;
      const maxR = getMaxCornerRadius(f);
      const val = preset === 'max' ? maxR : Number(preset) || 0;
      applyShapeCornerRadius(val, true);
    });
  });

  document.querySelectorAll('#shapeSidesPresetGrid [data-sides-preset]').forEach((btn) => {
    btn.addEventListener('click', () => {
      const f = selectedField(); if (!f) return;
      f.sides = Math.max(3, Math.min(12, Number(btn.dataset.sidesPreset) || 5));
      if (shapeSides2) shapeSides2.value = String(f.sides);
      if (fieldSides) fieldSides.value = String(f.sides);
      syncShapeInspector();
      render();
      commitHistory();
    });
  });

  document.querySelectorAll('#shapeStrokePresetGrid [data-stroke-preset]').forEach((btn) => {
    btn.addEventListener('click', () => {
      const f = selectedField(); if (!f) return;
      f.size = Math.max(1, Math.min(12, Number(btn.dataset.strokePreset) || 1));
      syncShapeInspector();
      render();
      commitHistory();
    });
  });

  document.querySelectorAll('#shapeRotPresetGrid [data-rot-preset]').forEach((btn) => {
    btn.addEventListener('click', () => {
      const f = selectedField(); if (!f) return;
      f.rotation = Number(btn.dataset.rotPreset) % 360;
      syncShapeInspector();
      render();
      commitHistory();
    });
  });

  document.querySelectorAll('[data-shape-color-target] .bar-palette-pill').forEach((pill) => {
    pill.addEventListener('click', () => {
      const f = selectedField(); if (!f) return;
      const group = pill.closest('[data-shape-color-target]');
      const target = group?.dataset.shapeColorTarget;
      const hex = pill.dataset.colorHex || pill.dataset.hex;
      if (!hex) return;
      const c = hex888ToRgb565(hex);
      if (target === 'border') {
        f.frameColor = c;
      } else if (target === 'fill') {
        f.fillColor = c;
      }
      syncShapeInspector();
      render();
      commitHistory();
    });
  });

  async function activateShapeEyedropper(target) {
    const f = selectedField(); if (!f) return;
    const borderBtn = document.getElementById('shapeBorderEyedropperBtn');
    const fillBtn = document.getElementById('shapeFillEyedropperBtn');

    if (window.EyeDropper) {
      try {
        if (target === 'border') borderBtn?.classList.add('active');
        if (target === 'fill') fillBtn?.classList.add('active');
        const eyeDropper = new window.EyeDropper();
        const res = await eyeDropper.open();
        if (res && res.sRGBHex) {
          const c565 = hex888ToRgb565(res.sRGBHex);
          if (target === 'border') {
            f.frameColor = c565;
          } else if (target === 'fill') {
            f.fillColor = c565;
          }
          addRecentColor(c565);
          syncShapeInspector();
          render();
          commitHistory();
        }
      } catch (e) {
        // Canceled or aborted by user
      } finally {
        borderBtn?.classList.remove('active');
        fillBtn?.classList.remove('active');
      }
      return;
    }

    // Fallback if native EyeDropper not supported: canvas pixel sampler
    activeShapeColorDropperTarget = target;
    if (target === 'border') borderBtn?.classList.add('active');
    if (target === 'fill') fillBtn?.classList.add('active');
    selectedTool = 'colorPicker';
    syncToolUI();
  }

  document.getElementById('shapeBorderEyedropperBtn')?.addEventListener('click', () => {
    activateShapeEyedropper('border');
  });

  document.getElementById('shapeFillEyedropperBtn')?.addEventListener('click', () => {
    activateShapeEyedropper('fill');
  });

  document.getElementById('shapeCopyCodeBtn')?.addEventListener('click', () => {
    const f = selectedField(); if (!f) return;
    const code = buildShapeCppSnippet(f);
    navigator.clipboard?.writeText(code);
    const btn = document.getElementById('shapeCopyCodeBtn');
    if (btn) {
      const prev = btn.textContent;
      btn.textContent = '¡Copiado!';
      setTimeout(() => { btn.textContent = prev; }, 1400);
    }
  });

  shapeInspectorName?.addEventListener('input', () => {
    const f = selectedField(); if (!f) return;
    f.name = shapeInspectorName.value;
    syncInputsFromState(); render();
  });
  function shapeGeomUpdate() {
    const f = selectedField(); if (!f) return;
    // shapeX/shapeY are CENTER coords; shapeW/shapeH are local width/height
    const cx = Number(shapeX?.value) || 0; const cy = Number(shapeY?.value) || 0;
    const w = Math.max(1, Number(shapeW?.value) || 1); const h = Math.max(1, Number(shapeH?.value) || 1);
    const hw = Math.round(w / 2); const hh = Math.round(h / 2);
    f.x = cx - hw; f.y = cy - hh; f.x2 = cx + hw; f.y2 = cy + hh;
    if (shapeSides2 && !shapeSidesWrap2?.hidden) {
      let val = parseInt(shapeSides2.value, 10);
      if (!isNaN(val)) {
        if (val > 12) {
          showToastMessage('El número máximo de lados para un polígono es 12.', 'warning');
          val = 12;
          shapeSides2.value = '12';
        } else if (val < 3) {
          val = 3;
          shapeSides2.value = '3';
        }
        f.sides = val;
        if (fieldSides) fieldSides.value = String(val);
      }
    }
    syncShapeInspector();
    render();
  }
  [shapeX, shapeY, shapeW, shapeH].forEach((el) => {
    el?.addEventListener('input', shapeGeomUpdate);
    el?.addEventListener('change', () => { shapeGeomUpdate(); commitHistory(); });
  });

  function handleShapeSidesInput(commit = false) {
    const f = selectedField(); if (!f) return;
    let val = parseInt(shapeSides2.value, 10);
    if (isNaN(val)) return;
    if (val > 12) {
      showToastMessage('El número máximo de lados para un polígono es 12.', 'warning');
      val = 12;
      shapeSides2.value = '12';
    } else if (commit && val < 3) {
      showToastMessage('El número mínimo de lados para un polígono es 3.', 'warning');
      val = 3;
      shapeSides2.value = '3';
    }
    f.sides = clamp(val, 3, 12);
    if (fieldSides) fieldSides.value = String(f.sides);
    syncShapeInspector();
    render();
    if (commit) commitHistory();
  }
  shapeSides2?.addEventListener('input', () => handleShapeSidesInput(false));
  shapeSides2?.addEventListener('change', () => handleShapeSidesInput(true));

  const mainPolySidesInput = document.getElementById('mainPolySidesInput');
  mainPolySidesInput?.addEventListener('input', () => {
    let val = parseInt(mainPolySidesInput.value, 10);
    if (isNaN(val)) return;
    if (val > 12) {
      showToastMessage('El número máximo de lados para un polígono es 12.', 'warning');
      mainPolySidesInput.value = '12';
    }
  });
  mainPolySidesInput?.addEventListener('change', () => {
    let val = parseInt(mainPolySidesInput.value, 10);
    if (isNaN(val) || val < 3) {
      showToastMessage('El número mínimo de lados para un polígono es 3.', 'warning');
      mainPolySidesInput.value = '3';
    } else if (val > 12) {
      showToastMessage('El número máximo de lados para un polígono es 12.', 'warning');
      mainPolySidesInput.value = '12';
    }
  });
  shapeRotation?.addEventListener('input', () => {
    const f = selectedField(); if (!f) return; f.rotation = Number(shapeRotation.value) % 360; syncShapeInspector(); render();
  });
  shapeRotation?.addEventListener('change', () => commitHistory());
  document.getElementById('shapeRotate90Btn')?.addEventListener('click', () => {
    const f = selectedField(); if (!f) return;
    f.rotation = ((Number(f.rotation) || 0) + 90) % 360;
    syncShapeInspector(); render(); commitHistory();
  });
  document.getElementById('shapeFlipHBtn')?.addEventListener('click', () => {
    selectedFields().forEach((f) => {
      if (!['LINE','RECT','ELLIPSE','TRIANGLE','POLYGON'].includes(f.type)) return;
      f.flipH = !f.flipH;
    });
    syncShapeInspector(); render(); commitHistory();
  });
  document.getElementById('shapeFlipVBtn')?.addEventListener('click', () => {
    selectedFields().forEach((f) => {
      if (!['LINE','RECT','ELLIPSE','TRIANGLE','POLYGON'].includes(f.type)) return;
      f.flipV = !f.flipV;
    });
    syncShapeInspector(); render(); commitHistory();
  });
  shapeBorderEnabled?.addEventListener('change', () => {
    const f = selectedField(); if (!f) return;
    f.borderEnabled = shapeBorderEnabled.checked;
    if (f.borderEnabled) shapeActiveColorKey = 'border';
    syncShapeInspector(); render(); commitHistory();
  });
  document.getElementById('shapeAppearanceCard')?.addEventListener('click', (e) => {
    if (e.target.closest('.bar-toggle-switch') || e.target.closest('input')) return;
    const row = e.target.closest('[data-shape-legend]');
    if (row) {
      shapeActiveColorKey = row.dataset.shapeLegend;
      const f = selectedField();
      if (shapeActiveColorKey === 'fill' && f && !f.fill && f.type !== 'LINE') {
        f.fill = true;
        render();
        commitHistory();
      }
      syncShapeInspector();
    }
  });
  shapeBorderSize?.addEventListener('input', () => {
    const f = selectedField(); if (!f) return; f.size = Number(shapeBorderSize.value) || 1; syncShapeInspector(); render();
  });
  shapeBorderSize?.addEventListener('change', () => commitHistory());
  shapeFillEnabled?.addEventListener('change', () => {
    const f = selectedField(); if (!f) return;
    f.fill = shapeFillEnabled.checked;
    if (f.fill) shapeActiveColorKey = 'fill';
    syncShapeInspector(); render(); commitHistory();
  });
  document.querySelectorAll('.align-btn[data-align], .shape-matrix-btn[data-align]').forEach((btn) => {
    btn.addEventListener('click', () => alignSelectedShapes(btn.dataset.align));
  });
  document.getElementById('shapeMergeBitmapBtn')?.addEventListener('click', () => {
    const keys = [...selectedFieldKeys];
    keys.forEach((key) => {
      const f = hmiFields.find((x) => x.key === key);
      if (f) drawShape(pixelLayer, f);
    });
    hmiFields = hmiFields.filter((f) => !keys.includes(f.key));
    setSelectedKeys([]);
    selectedTool = 'pointer';
    syncInputsFromState(); syncToolUI(); render(); commitHistory();
  });
  document.getElementById('shapeDuplicateBtn')?.addEventListener('click', duplicateSelectedField);
  document.getElementById('shapeDeleteBtn')?.addEventListener('click', deleteSelectedField);
  document.getElementById('sidebarPreviewFrame')?.addEventListener('click', () => {
    fitCanvas();
  });

  buildPalette();
  updateActiveColorUI();
  renderRecentColors();
  syncInputsFromState();
  syncToolUI();
  render();
  history.push(captureSnapshot());
  historyIndex = 0;
  updateHistoryButtons();

  // Cursor sobre el viewport (lo usa el bloque de paneo, fuera de este módulo).
  function trackViewportPointer(event) {
    updateCursor(pointFromPointer(event));
  }

  function clearViewportCursor(onlyIfActive = false) {
    if (onlyIfActive && !activeCursorPoint) return;
    activeCursorPoint = null;
    updateCrosshairs(null);
    drawRulers();
    cursorStatus.textContent = 'X: — · Y: —';
    pixelStatus.textContent = 'Pixel: —';
    updateEyedropperLiveUI({ x: -1, y: -1 });
  }

  window.JWPLCHMIEditor = {
    trackViewportPointer,
    clearViewportCursor,
    integratedBarLayers: true,
    getSelectedField: () => selectedField(),
    getSelectedFields: () => selectedFields(),
    getSelectedFieldKeys: () => [...selectedFieldKeys],
    setSelectedFieldKeys: (keys) => {
      setSelectedKeys(keys);
      syncInputsFromState();
      syncToolUI();
      render();
    },
    getZoom: () => zoom,
    getSelectedColor: () => (selectedColor ? selectedColor.value : 0xFFFF),
    getSelectedTool: () => selectedTool,
    getSelectedFieldType: () => selectedField()?.type || null,
    getAllFields: () => hmiFields,
    getFieldsForPage: (page = activePage) => fieldsForPage(page),
    getPages: () => hmiPages.map((page) => ({ ...page })),
    getActivePage: () => activePage,
    getMaxPages: () => MAX_PAGES,
    computeSelectedGeometry: () => computeFieldGeometry(selectedField()),
    selectAll: selectAllOnPage,
    selectAllOnPage,
    getSelectedFields: () => selectedFields(),
    hasFieldSelection: () => selectedFields().length > 0,
    getSelectionHandles: (fields = selectedFields()) => getSelectionHandles(fields),
    isPointInsideSelection: (p) => isPointInsideSelection(p),
    hitTestField: (p) => hitTestField(p),
    hitTestHandle,
    hasTextSelection: () => selectedField()?.type === 'TEXT' && selectedTool === 'textField',
    hasValueSelection: () => selectedField()?.type === 'VALUE' && selectedTool === 'valueField',
    setActivePage,
    addPage,
    renamePage,
    deletePage,
    moveSelectedFieldToPage,
    commitHistory,
    render,
    redrawCanvasOnly,
    renderCanvasOnly: redrawCanvasOnly,
    fitCanvas,
    fitSelection,
    applyZoom,
    setPan: (x, y) => {
      panX = x;
      panY = y;
      updateStageTransform();
    },
    getPan: () => ({ x: panX, y: panY }),
    undo,
    redo,
    duplicateSelectedField,
    deleteSelectedField,
    addTextField,
    addValueField,
    addRawTextField,
    addIconField,
    iconEntry,
    fillAt,
    fillRegion,
    pickColorAt,
    compositeColorAt,
    hitTestHandle: (e) => hitTestHandle(e),
    drawSelectionAndGuides: () => drawSelectionAndGuides(),
    ensureLayerOrder,
    assignNewLayerOrder,
    reorderPageObjects: reorderPageFields,
    renderFieldPreview,
    sortObjectListDOM,
    getFieldBounds: (f) => getFieldBounds(f),
    hitTestField: (pt) => hitTestField(pt),
    isPointInsideSelection: (pt) => isPointInsideSelection(pt),
    getShapeVertices: (f) => getShapeTransformedVertices(f),
    getMaxCornerRadius: (f) => getMaxCornerRadius(f),
    showToast: (msg, kind) => showToastMessage(msg, kind),
    nextSequentialName: (value) => nextSequential(value, 'name'),
    getPixelFieldOrder: (x, y) => {
      const px = Math.trunc(Number(x));
      const py = Math.trunc(Number(y));
      return inside(px, py) ? topFieldOrder[indexFor(px, py)] : Infinity;
    },
    // A15-2: la barra existe si y sólo si hay un objeto NAV_BAR visible.
    getNavBarConfig: () => {
      const navBarField = hmiFields.find((field) => field.type === 'NAV_BAR');
      return { ...navBarConfig, enabled: Boolean(navBarField && !navBarField.hidden) };
    },
    setNavBarConfig: (cfg) => {
      navBarConfig = { ...navBarConfig, ...cfg };
      render();
      commitHistory();
    },
    setPageMetadata
  };
  window.jwplc = window.JWPLCHMIEditor;
})();




// --- Viewport Panning & Vertical Toolbar Wire-up ---

  // --- Viewport Infinite Panning (Right-click or Middle-click) and Wheel Scroll ---
  const canvasViewport = document.getElementById('canvasViewport');
  let isPanning = false;
  let panStartX = 0;
  let panStartY = 0;
  let startPanX = 0;
  let startPanY = 0;

  if (canvasViewport) {
    canvasViewport.addEventListener('mousedown', (e) => {
      // Right-click (button 2) or Middle-click (button 1) for free infinite pan
      if (e.button === 2 || e.button === 1) {
        isPanning = true;
        panStartX = e.clientX;
        panStartY = e.clientY;
        const currentPan = window.JWPLCHMIEditor?.getPan?.() || { x: 0, y: 0 };
        startPanX = currentPan.x;
        startPanY = currentPan.y;
        document.body.classList.add('is-panning');
        canvasViewport.style.cursor = 'grabbing';
        e.preventDefault();
      }
    }, true);

    window.addEventListener('mousemove', (e) => {
      if (isPanning) {
        e.preventDefault();
        const dx = e.clientX - panStartX;
        const dy = e.clientY - panStartY;
        window.JWPLCHMIEditor?.setPan?.(startPanX + dx, startPanY + dy);
      }
    });

    window.addEventListener('mouseup', (e) => {
      if (isPanning && (e.button === 2 || e.button === 1)) {
        isPanning = false;
        document.body.classList.remove('is-panning');
        canvasViewport.style.cursor = '';
      }
    });

    canvasViewport.addEventListener('contextmenu', (e) => {
      e.preventDefault();
    });

    // Crosshairs & cursor tracking across the canvas stage
    // Este bloque está fuera del módulo del editor: usa su API pública (antes
    // llamaba funciones internas y fallaba con ReferenceError al salir del lienzo).
    canvasViewport.addEventListener('pointermove', (e) => {
      if (e.target.closest('.canvas-toolbar') || e.target.closest('.right-vertical-toolbar')) {
        window.JWPLCHMIEditor?.clearViewportCursor?.(true);
        return;
      }
      window.JWPLCHMIEditor?.trackViewportPointer?.(e);
    });

    canvasViewport.addEventListener('pointerleave', () => {
      window.JWPLCHMIEditor?.clearViewportCursor?.();
    });

    canvasViewport.addEventListener('wheel', (e) => {
      e.preventDefault();
      const activeTool = window.JWPLCHMIEditor?.getSelectedTool?.();
      if (activeTool === 'erase' || activeTool === 'pixel') {
        return;
      }
      if (e.ctrlKey) {
        // Ctrl + Wheel = Zoom in / Zoom out
        const curZoom = Number(document.getElementById('zoomSelect')?.value) || 1;
        const factor = e.deltaY < 0 ? 1.15 : 0.87;
        const rawNext = curZoom * factor;
        const nextZoom = Math.max(0.5, Math.min(16, Math.round(rawNext * 20) / 20));
        window.JWPLCHMIEditor?.applyZoom?.(nextZoom);
      } else if (e.shiftKey) {
        // Shift + Wheel = horizontal scroll
        const currentPan = window.JWPLCHMIEditor?.getPan?.() || { x: 0, y: 0 };
        const delta = e.deltaY || e.deltaX;
        window.JWPLCHMIEditor?.setPan?.(currentPan.x - delta, currentPan.y);
      } else {
        // Normal Wheel = vertical scroll (up/down) + horizontal scroll on trackpad
        const currentPan = window.JWPLCHMIEditor?.getPan?.() || { x: 0, y: 0 };
        const newX = e.deltaX ? currentPan.x - e.deltaX : currentPan.x;
        const newY = currentPan.y - e.deltaY;
        window.JWPLCHMIEditor?.setPan?.(newX, newY);
      }
    }, { passive: false });
  }

  // --- Vertical Toolbar & Header Fit Buttons Wire-up ---
  const vertToolbar = document.querySelector('.right-vertical-toolbar');
  if (vertToolbar) {
    vertToolbar.addEventListener('pointerdown', (e) => e.stopPropagation());
    vertToolbar.addEventListener('mousedown', (e) => e.stopPropagation());
  }

  document.getElementById('zoomInBtn')?.addEventListener('click', () => {
    const curZoom = Number(document.getElementById('zoomSelect')?.value) || 1;
    let nextZoom = Math.min(16, Math.floor((curZoom + 0.5) * 2) / 2);
    if (nextZoom <= curZoom) nextZoom = Math.min(16, curZoom + 0.5);
    window.JWPLCHMIEditor?.applyZoom?.(nextZoom);
  });

  document.getElementById('zoomOutBtn')?.addEventListener('click', () => {
    const curZoom = Number(document.getElementById('zoomSelect')?.value) || 1;
    let nextZoom = Math.max(0.5, Math.ceil((curZoom - 0.5) * 2) / 2);
    if (nextZoom >= curZoom) nextZoom = Math.max(0.5, curZoom - 0.5);
    window.JWPLCHMIEditor?.applyZoom?.(nextZoom);
  });

  document.getElementById('fitCanvasBtn')?.addEventListener('click', () => {
    window.JWPLCHMIEditor?.fitCanvas?.();
  });

  document.getElementById('fitSelectionBtn')?.addEventListener('click', () => {
    window.JWPLCHMIEditor?.fitSelection?.();
  });

  document.getElementById('fitButton')?.addEventListener('click', (e) => {
    e.preventDefault();
    window.JWPLCHMIEditor?.fitCanvas?.();
  });

  document.getElementById('fitSelectionButton')?.addEventListener('click', (e) => {
    e.preventDefault();
    window.JWPLCHMIEditor?.fitSelection?.();
  });

  document.getElementById('vertGeoToggle')?.addEventListener('click', function() {
    const toggle = document.getElementById('geometryToggle');
    if (toggle) {
      toggle.checked = !toggle.checked;
      this.classList.toggle('active', toggle.checked);
      toggle.dispatchEvent(new Event('change', { bubbles: true }));
    }
  });

// --- End PanZoom ---
