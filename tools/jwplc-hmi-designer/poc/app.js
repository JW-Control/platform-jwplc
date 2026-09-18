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
    const vertSnap = document.getElementById('vertSnapToggle');
    if (vertSnap) vertSnap.classList.toggle('active', enabled);
    render();
  }
  document.getElementById('vertSnapToggle')?.addEventListener('click', function() {
    setSnapEnabled(!snapToggle.checked);
  });
  snapInspectorToggle?.addEventListener('change', function() {
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
  const shapeX = document.getElementById('shapeX');
  const shapeY = document.getElementById('shapeY');
  const shapeW = document.getElementById('shapeW');
  const shapeH = document.getElementById('shapeH');
  const shapeSides2 = document.getElementById('shapeSides2');
  const shapeSidesWrap2 = document.getElementById('shapeSidesWrap2');
  const shapeRotation = document.getElementById('shapeRotation');
  const shapeBorderEnabled = document.getElementById('shapeBorderEnabled');
  const shapeBorderColorInput = document.getElementById('shapeBorderColorInput');
  const shapeBorderColorSwatch = document.getElementById('shapeBorderColorSwatch');
  const shapeBorderColorCode = document.getElementById('shapeBorderColorCode');
  const shapeBorderSize = document.getElementById('shapeBorderSize');
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
  const rawTextTransparent = document.getElementById('rawTextTransparent');
  const rawTextColor = document.getElementById('rawTextColor');
  const rawTextBackground = document.getElementById('rawTextBackground');
  const rawTextBackgroundWrap = document.getElementById('rawTextBackgroundWrap');
  const rawBoundsStatus = document.getElementById('rawBoundsStatus');

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
  let dragInitialPointer = null;
  let dragInitialFields = null;
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

  function updateStageTransform() {
    if (canvasStage) {
      canvasStage.style.transform = `translate(${panX}px, ${panY}px)`;
    }
    if (canvasViewport) {
      canvasViewport.style.backgroundPosition = `${panX}px ${panY}px, ${panX + 12}px ${panY + 12}px`;
    }
  }

  function applyZoom(nextZoom) {
    const val = Math.max(0.25, Math.min(16, Math.round(nextZoom * 100) / 100));
    zoom = val;
    let opt = zoomSelect.querySelector(`option[value="${val}"]`);
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
    const rightMargin = 56;

    const rulerTopH = 22;
    const rulerRightW = 36;

    const availW = Math.max(80, vpW - leftMargin - rightMargin - rulerRightW);
    const availH = Math.max(60, vpH - topMargin - bottomMargin - rulerTopH);

    const rawZoom = Math.min(availW / WIDTH, availH / HEIGHT);
    const targetZoom = Math.max(0.5, Math.min(10, Math.floor(rawZoom * 20) / 20));

    applyZoom(targetZoom);

    const desiredCenterX = leftMargin + (vpW - leftMargin - rightMargin) / 2;
    const desiredCenterY = topMargin + (vpH - topMargin - bottomMargin) / 2;

    const targetPanX = Math.round(desiredCenterX - ((vpW - WIDTH * targetZoom) / 2 - 18 + (WIDTH / 2) * targetZoom));
    const targetPanY = Math.round(desiredCenterY - ((vpH - HEIGHT * targetZoom) / 2 + 11 + (HEIGHT / 2) * targetZoom));

    panX = targetPanX;
    panY = targetPanY;
    updateStageTransform();
  }

  function fitSelection() {
    if (!canvasViewport) return;
    const fields = selectedFields();
    if (!fields || fields.length === 0) {
      fitCanvas();
      return;
    }

    const b = getSelectionBounds(fields);
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
    const rightMargin = 56;

    const availW = Math.max(80, vpW - leftMargin - rightMargin);
    const availH = Math.max(60, vpH - topMargin - bottomMargin);

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

    const targetPanX = Math.round(desiredCenterX - ((vpW - WIDTH * targetZoom) / 2 - 18 + selCenterX * targetZoom));
    const targetPanY = Math.round(desiredCenterY - ((vpH - HEIGHT * targetZoom) / 2 + 11 + selCenterY * targetZoom));

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

  const rawState = {
    x: 20,
    y: 20,
    size: 2,
    value: 'TEMP: 25.6 C',
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
    return `0x${value.toString(16).toUpperCase().padStart(4, '0')}`;
  }

  function colorByName(name) {
    return COLORS.find((color) => color.name === name) || COLORS[0];
  }

  function colorName(value) {
    const match = COLORS.find((color) => color.value === value);
    return match ? match.name : hex565(value);
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
    const { r, g, b } = rgb565ToRgb888(value);
    return `rgb(${r}, ${g}, ${b})`;
  }

  function rgb565ToHex888(value) {
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
    const val565 = Number(value) & 0xFFFF;
    const matched = COLORS.find((c) => c.value === val565);
    const name = matched ? matched.name : hex565(val565);
    let opt = [...select.options].find((o) => o.value === name || o.value === hex565(val565));
    if (!opt) {
      opt = document.createElement('option');
      opt.value = name;
      opt.textContent = `${name} · ${hex565(val565)}`;
      select.appendChild(opt);
    }
    select.value = opt.value;
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

  function getShapeTransformedVertices(field) {
    const x1 = field.x ?? 0; const y1 = field.y ?? 0;
    const x2 = field.x2 ?? x1; const y2 = field.y2 ?? y1;
    const cx = (x1 + x2) / 2;
    const cy = (y1 + y2) / 2;
    const angleDeg = Number(field.rotation) || 0;
    const flipH = Boolean(field.flipH);
    const flipV = Boolean(field.flipV);
    const tf = (p) => transformShapePoint(p.x, p.y, cx, cy, angleDeg, flipH, flipV);

    if (field.type === 'LINE') {
      return [tf({ x: x1, y: y1 }), tf({ x: x2, y: y2 })];
    }
    if (field.type === 'RECT') {
      const left = Math.min(x1, x2);
      const right = Math.max(x1, x2);
      const top = Math.min(y1, y2);
      const bottom = Math.max(y1, y2);
      return [
        tf({ x: left, y: top }),
        tf({ x: right, y: top }),
        tf({ x: right, y: bottom }),
        tf({ x: left, y: bottom })
      ];
    }
    if (field.type === 'TRIANGLE') {
      const topPt = { x: Math.round((x1 + x2) / 2), y: y1 };
      const bl = { x: x1, y: y2 };
      const br = { x: x2, y: y2 };
      return [tf(topPt), tf(bl), tf(br)];
    }
    if (field.type === 'POLYGON') {
      return getPolygonVertices(field).map(tf);
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

  function addRecentColor(color565) {
    color565 = Number(color565) & 0xFFFF;
    recentColors = [color565, ...recentColors.filter((c) => (c & 0xFFFF) !== color565)].slice(0, 16);
    renderRecentColors();
  }

  function updateEyedropperActiveUI() {
    if (!eyedropperActiveSwatch || !selectedColor) return;
    const val = selectedColor.value & 0xFFFF;
    eyedropperActiveSwatch.style.backgroundColor = rgb565ToCss(val);
    eyedropperActiveName.textContent = selectedColor.name;
    eyedropperActive565.textContent = hex565(val);
    if (eyedropperActiveHex) eyedropperActiveHex.textContent = rgb565ToHex888(val).toUpperCase();
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
    const color565 = framebuffer[indexFor(point.x, point.y)];
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
        const matched = COLORS.find((c) => c.value === colorVal);
        selectedColor = matched || { name: hex565(colorVal), value: colorVal };
        updateActiveColorUI();
        buildPalette();
        renderRecentColors();

        const field = selectedField();
        if (field) {
          if (['LINE', 'RECT', 'ELLIPSE', 'TRIANGLE', 'POLYGON'].includes(field.type)) {
            if (field.fill) {
              field.fillColor = selectedColor.value;
            } else {
              field.frameColor = selectedColor.value;
            }
          } else if (field.type === 'RAW_TEXT') {
            field.textColor = selectedColor.value;
          } else if (['TEXT', 'VALUE', 'BOOL', 'BAR'].includes(field.type)) {
            field.valueColor = selectedColor.value;
          }
          syncInputsFromState();
          render();
          commitHistory();
        }
      });
      recentColorsGrid.appendChild(btn);
    });
  }

  function pageExists(page) {
    return hmiPages.some((item) => item.id === Number(page));
  }

  function fieldsForPage(page = activePage) {
    return hmiFields.filter((field) => Number(field.page || 0) === Number(page));
  }

  function setSelectedKeys(keys) {
    selectedFieldKeys = Array.isArray(keys) ? Array.from(new Set(keys.filter(Boolean))) : (keys ? [keys] : []);
    selectedFieldKey = selectedFieldKeys[0] || null;
  }

  function selectedFields() {
    return hmiFields.filter((field) => selectedFieldKeys.includes(field.key) && Number(field.page || 0) === activePage);
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
    if (field.type === 'RAW_TEXT') return 'rawText';
    if (['LINE', 'RECT', 'ELLIPSE', 'TRIANGLE', 'POLYGON'].includes(field.type)) return 'pointer';
    return 'textField';
  }

  function setLayerPixel(x, y, value) {
    if (!inside(x, y)) return false;
    const index = indexFor(x, y);
    if (pixelLayer[index] === value) return false;
    pixelLayer[index] = value;
    return true;
  }

  function floodFill(startX, startY, replacement) {
    if (!inside(startX, startY)) return false;
    const target = framebuffer[indexFor(startX, startY)];
    if (target === replacement) return false;

    const queue = new Int32Array(WIDTH * HEIGHT);
    const visited = new Uint8Array(WIDTH * HEIGHT);
    let head = 0;
    let tail = 0;

    const startIdx = indexFor(startX, startY);
    queue[tail++] = (startX & 0xFFFF) | ((startY & 0xFFFF) << 16);
    visited[startIdx] = 1;

    let changed = false;
    while (head < tail) {
      const val = queue[head++];
      const x = val & 0xFFFF;
      const y = (val >> 16) & 0xFFFF;
      const idx = indexFor(x, y);

      pixelLayer[idx] = replacement;
      changed = true;

      // 4 neighbors
      if (x > 0) {
        const nIdx = idx - 1;
        if (!visited[nIdx] && framebuffer[nIdx] === target) {
          visited[nIdx] = 1;
          queue[tail++] = ((x - 1) & 0xFFFF) | ((y & 0xFFFF) << 16);
        }
      }
      if (x < WIDTH - 1) {
        const nIdx = idx + 1;
        if (!visited[nIdx] && framebuffer[nIdx] === target) {
          visited[nIdx] = 1;
          queue[tail++] = ((x + 1) & 0xFFFF) | ((y & 0xFFFF) << 16);
        }
      }
      if (y > 0) {
        const nIdx = idx - WIDTH;
        if (!visited[nIdx] && framebuffer[nIdx] === target) {
          visited[nIdx] = 1;
          queue[tail++] = (x & 0xFFFF) | (((y - 1) & 0xFFFF) << 16);
        }
      }
      if (y < HEIGHT - 1) {
        const nIdx = idx + WIDTH;
        if (!visited[nIdx] && framebuffer[nIdx] === target) {
          visited[nIdx] = 1;
          queue[tail++] = (x & 0xFFFF) | (((y + 1) & 0xFFFF) << 16);
        }
      }
    }
    return changed;
  }

  function pickColorAt(point) {
    if (!inside(point.x, point.y)) return false;
    const color565 = framebuffer[indexFor(point.x, point.y)];
    const matched = COLORS.find((c) => c.value === color565);
    selectedColor = matched || { name: hex565(color565), value: color565 };
    addRecentColor(color565);
    updateActiveColorUI();
    buildPalette();

    const field = selectedField();
    if (field) {
      if (['LINE', 'RECT', 'ELLIPSE', 'TRIANGLE', 'POLYGON'].includes(field.type)) {
        if (field.fill) {
          field.fillColor = selectedColor.value;
        } else {
          field.frameColor = selectedColor.value;
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

  function setBufferPixel(buffer, x, y, value) {
    if (inside(x, y)) buffer[indexFor(x, y)] = value;
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

  function drawBufferRect(buffer, x, y, width, height, value) {
    if (width <= 0 || height <= 0) return;
    fillBufferRect(buffer, x, y, width, 1, value);
    fillBufferRect(buffer, x, y + height - 1, width, 1, value);
    fillBufferRect(buffer, x, y, 1, height, value);
    fillBufferRect(buffer, x + width - 1, y, 1, height, value);
  }

  function rasterLineBuffer(buffer, x0, y0, x1, y1, value, size = 1) {
    let dx = Math.abs(x1 - x0);
    const sx = x0 < x1 ? 1 : -1;
    let dy = -Math.abs(y1 - y0);
    const sy = y0 < y1 ? 1 : -1;
    let error = dx + dy;
    let changed = false;
    while (true) {
      for (let oy = 0; oy < size; oy++) {
        for (let ox = 0; ox < size; ox++) {
          setBufferPixel(buffer, x0 + ox - Math.floor(size/2), y0 + oy - Math.floor(size/2), value);
        }
      }
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

  function isPointInsideShapeArea(px, py, field) {
    const cx = ((field.x ?? 0) + (field.x2 ?? field.x ?? 0)) / 2;
    const cy = ((field.y ?? 0) + (field.y2 ?? field.y ?? 0)) / 2;
    const angleDeg = Number(field.rotation) || 0;
    const local = inverseTransformShapePoint(px, py, cx, cy, angleDeg, Boolean(field.flipH), Boolean(field.flipV));
    px = local.x; py = local.y;
    const a = { x: field.x, y: field.y };
    const b = { x: field.x2 ?? field.x, y: field.y2 ?? field.y };
    if (field.type === 'RECT') {
      const left = Math.min(a.x, b.x);
      const right = Math.max(a.x, b.x);
      const top = Math.min(a.y, b.y);
      const bottom = Math.max(a.y, b.y);
      return px >= left && px <= right && py >= top && py <= bottom;
    }
    if (field.type === 'ELLIPSE') {
      const rx = Math.abs(b.x - a.x) / 2;
      const ry = Math.abs(b.y - a.y) / 2;
      if (rx <= 0 || ry <= 0) return false;
      const cx = (a.x + b.x) / 2;
      const cy = (a.y + b.y) / 2;
      const nx = (px - cx) / rx;
      const ny = (py - cy) / ry;
      return (nx * nx + ny * ny) <= 1.05;
    }
    if (field.type === 'TRIANGLE') {
      const topPt = { x: Math.round((a.x + b.x) / 2), y: a.y };
      const bl = { x: a.x, y: b.y };
      const br = { x: b.x, y: b.y };
      const d1 = (px - bl.x) * (topPt.y - bl.y) - (topPt.x - bl.x) * (py - bl.y);
      const d2 = (px - br.x) * (bl.y - br.y) - (bl.x - br.x) * (py - br.y);
      const d3 = (px - topPt.x) * (br.y - topPt.y) - (br.x - topPt.x) * (py - topPt.y);
      const hasNeg = (d1 < 0) || (d2 < 0) || (d3 < 0);
      const hasPos = (d1 > 0) || (d2 > 0) || (d3 > 0);
      return !(hasNeg && hasPos);
    }
    if (field.type === 'POLYGON') {
      const pts = getPolygonVertices(field);
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
    return false;
  }

  function drawShape(buffer, field) {
    const hasBorder = field.borderEnabled !== false;
    const hasFill = Boolean(field.fill);
    if (!hasBorder && !hasFill) return;

    const color = field.frameColor || 0xFFFF;
    const fillColor = field.fillColor ?? color;
    const size = field.size || 1;
    const pts = getShapeTransformedVertices(field);
    if (!pts || pts.length === 0) return;

    if (field.type === 'LINE') {
      if (hasBorder && pts.length >= 2) {
        rasterLineBuffer(buffer, pts[0].x, pts[0].y, pts[1].x, pts[1].y, color, size);
      }
      return;
    }

    if (field.type === 'RECT') {
      if (hasFill && pts.length >= 4) {
        fillShapePolygon(buffer, pts, fillColor);
      }
      if (hasBorder && pts.length >= 4) {
        rasterLineBuffer(buffer, pts[0].x, pts[0].y, pts[1].x, pts[1].y, color, size);
        rasterLineBuffer(buffer, pts[1].x, pts[1].y, pts[2].x, pts[2].y, color, size);
        rasterLineBuffer(buffer, pts[2].x, pts[2].y, pts[3].x, pts[3].y, color, size);
        rasterLineBuffer(buffer, pts[3].x, pts[3].y, pts[0].x, pts[0].y, color, size);
      }
      return;
    }

    if (field.type === 'TRIANGLE') {
      if (hasFill && pts.length >= 3) {
        fillShapeTriangle(buffer, pts[0], pts[1], pts[2], fillColor);
      }
      if (hasBorder && pts.length >= 3) {
        rasterLineBuffer(buffer, pts[0].x, pts[0].y, pts[1].x, pts[1].y, color, size);
        rasterLineBuffer(buffer, pts[1].x, pts[1].y, pts[2].x, pts[2].y, color, size);
        rasterLineBuffer(buffer, pts[2].x, pts[2].y, pts[0].x, pts[0].y, color, size);
      }
      return;
    }

    if (field.type === 'POLYGON' || field.type === 'ELLIPSE') {
      if (hasFill && pts.length >= 3) {
        fillShapePolygon(buffer, pts, fillColor);
      }
      if (hasBorder && pts.length >= 2) {
        for (let i = 0; i < pts.length; i++) {
          const p1 = pts[i];
          const p2 = pts[(i + 1) % pts.length];
          rasterLineBuffer(buffer, p1.x, p1.y, p2.x, p2.y, color, size);
        }
      }
      return;
    }
  }

  function drawClassicChar(buffer, x, y, charCode, foreground, background, size) {
    const font = window.JWPLCGfxClassicFont;
    const glyph = font.glyphFor(charCode);
    const scale = Math.max(1, Math.trunc(size));
    const isTransparent = (background === null || background === undefined || background === -1);
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

  function nominalTextBounds(text, size) {
    if (!text) return { width: 0, height: 0 };
    const scale = Math.max(1, Math.trunc(size || 1));
    return { width: text.length * 6 * scale - scale, height: 7 * scale };
  }

  function effectiveFieldPadding(field) {
    return Math.max(FIELD_PADDING, field.labelSize || 1, field.valueSize || 1);
  }

  function makeNumericSample(field) {
    const integerDigits = Math.max(1, Math.trunc(field.integerDigits || 1));
    const decimalDigits = Math.max(0, Math.trunc(field.decimalDigits || 0));
    return `${field.signedValue ? '-' : ''}${'8'.repeat(integerDigits)}${decimalDigits > 0 ? `.${'8'.repeat(decimalDigits)}` : ''}`;
  }

  function makeOverflowText(field) {
    let slots = Math.max(1, Math.trunc(field.integerDigits || 1));
    const decimals = Math.max(0, Math.trunc(field.decimalDigits || 0));
    if (decimals > 0) slots += 1 + decimals;
    if (field.signedValue) slots += 1;
    return '#'.repeat(slots);
  }

  function formatNumericPreview(field) {
    const raw = String(field.preview ?? '').trim();
    if (!raw) return '';
    const value = Number(raw);
    if (!Number.isFinite(value)) return makeOverflowText(field);
    const negative = value < 0;
    if (negative && !field.signedValue) return makeOverflowText(field);

    const decimals = Math.max(0, Math.trunc(field.decimalDigits || 0));
    const allowed = Math.max(1, Math.trunc(field.integerDigits || 1));
    let out;

    if (field.leadingZeros) {
      const magnitude = Math.abs(value).toFixed(decimals);
      const [integerPart, fractionPart] = magnitude.split('.');
      const paddedInteger = integerPart.padStart(allowed, '0');
      const body = decimals > 0 ? `${paddedInteger}.${fractionPart || ''.padEnd(decimals, '0')}` : paddedInteger;
      out = negative ? `-${body}` : body;
    } else {
      out = value.toFixed(decimals);
    }

    const unsigned = out.replace(/^[+-]/, '');
    const integerPart = unsigned.split('.')[0] || '';
    if (integerPart.length > allowed) return makeOverflowText(field);
    return out;
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
    if (!field) return { minX: 0, maxX: 0, minY: 0, maxY: 0, width: 0, height: 0 };
    if (field.type === 'RAW_TEXT') {
      const tb = nominalTextBounds(field.text || '', field.size || 1);
      const w = Math.max(6, tb.width);
      const h = Math.max(7, tb.height);
      return {
        minX: field.x,
        maxX: field.x + w - 1,
        minY: field.y,
        maxY: field.y + h - 1,
        width: w,
        height: h
      };
    }
    if (['LINE', 'RECT', 'ELLIPSE', 'TRIANGLE', 'POLYGON'].includes(field.type)) {
      const pts = getShapeTransformedVertices(field);
      let minX = Infinity, maxX = -Infinity, minY = Infinity, maxY = -Infinity;
      for (const p of pts) {
        if (p.x < minX) minX = p.x;
        if (p.x > maxX) maxX = p.x;
        if (p.y < minY) minY = p.y;
        if (p.y > maxY) maxY = p.y;
      }
      if (!Number.isFinite(minX)) {
        minX = field.x ?? 0; maxX = minX; minY = field.y ?? 0; maxY = minY;
      }
      return {
        minX,
        maxX,
        minY,
        maxY,
        width: Math.max(1, maxX - minX),
        height: Math.max(1, maxY - minY)
      };
    }
    const g = computeFieldGeometry(field);
    return {
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
    const pad = effectiveFieldPadding(field);
    const labelBounds = nominalTextBounds(field.label, field.labelSize);
    const unitBounds = nominalTextBounds(field.unit, field.labelSize);
    const valueBounds = nominalTextBounds(valueSampleForField(field), field.valueSize);
    let fieldW;
    let fieldH;
    let valueX;
    let valueY;

    if (field.layout === 'STACKED') {
      const valueAndUnitW = valueBounds.width + (unitBounds.width > 0 ? FIELD_GAP + unitBounds.width : 0);
      fieldW = 2 * pad + Math.max(labelBounds.width, valueAndUnitW);
      fieldH = 2 * pad + labelBounds.height + (labelBounds.height > 0 ? FIELD_GAP : 0) + Math.max(valueBounds.height, unitBounds.height);
      valueX = field.x + pad;
      valueY = field.y + pad + labelBounds.height + (labelBounds.height > 0 ? FIELD_GAP : 0);
    } else {
      fieldW = 2 * pad + labelBounds.width + (labelBounds.width > 0 ? FIELD_GAP : 0) + valueBounds.width + (unitBounds.width > 0 ? FIELD_GAP + unitBounds.width : 0);
      fieldH = 2 * pad + Math.max(labelBounds.height, valueBounds.height, unitBounds.height);
      valueX = field.x + pad + labelBounds.width + (labelBounds.width > 0 ? FIELD_GAP : 0);
      valueY = field.y + pad;
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
    const g = computeFieldGeometry(field);
    fillBufferRect(buffer, g.fieldX, g.fieldY, g.fieldW, g.fieldH, field.backgroundColor);
    if (field.frame && g.fieldW > 1 && g.fieldH > 1) {
      drawBufferRect(buffer, g.fieldX, g.fieldY, g.fieldW, g.fieldH, field.frameColor);
    }
    if (field.label) {
      drawClassicTextAt(buffer, field.label, field.x + g.pad, field.y + g.pad, field.labelColor, field.backgroundColor, field.labelSize);
    }
    if (field.unit) {
      drawClassicTextAt(buffer, field.unit, g.valueX + g.valueW + FIELD_GAP, g.valueY, field.labelColor, field.backgroundColor, field.labelSize);
    }
    const preview = previewTextForField(field);
    if (preview) {
      drawClassicTextAt(buffer, preview, alignedValueX(field, g), g.valueY, field.valueColor, field.backgroundColor, field.valueSize);
    }
    return g;
  }

  function composeFramebuffer() {
    framebuffer.set(pixelLayer);
    fieldsForPage(activePage).forEach((field) => {
      if (['TEXT', 'VALUE', 'BOOL', 'BAR'].includes(field.type)) {
        drawField(framebuffer, field);
      } else if (['LINE', 'RECT', 'ELLIPSE', 'TRIANGLE', 'POLYGON'].includes(field.type)) {
        drawShape(framebuffer, field);
      } else if (field.type === 'RAW_TEXT') {
        const bg = field.transparentBackground ? null : (field.backgroundColor ?? 0x0000);
        drawClassicTextAt(framebuffer, field.text || '', field.x, field.y, field.textColor ?? 0xFFFF, bg, field.size || 1);
      }
    });
    if (selectedTool === 'rawText' && !hmiFields.some((f) => f.type === 'RAW_TEXT' && Number(f.page || 0) === activePage)) {
      const bg = rawState.transparent ? null : rawState.background;
      drawClassicTextAt(framebuffer, rawState.value, rawState.x, rawState.y, rawState.foreground, bg, rawState.size);
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
  }

  function drawGrid() {
    if (!gridToggle.checked || zoom < 2) return;
    const gridSize = Number(gridSizeSelect.value) || 8;
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

  function cppString(value) {
    return String(value || '').replace(/\\/g, '\\\\').replace(/"/g, '\\"');
  }

  function cppBool(value) {
    return value ? 'true' : 'false';
  }

  function fieldFallbackId(field, index) {
    if (field.type === 'VALUE') return `FIELD_VALUE_${index + 1}`;
    if (field.type === 'BOOL') return `FIELD_BOOL_${index + 1}`;
    if (field.type === 'BAR') return `FIELD_BAR_${index + 1}`;
    return `FIELD_TEXT_${index + 1}`;
  }

  function variableFallback(field, index) {
    if (field.type === 'VALUE') return `valor${index + 1}`;
    if (field.type === 'BOOL') return `estado${index + 1}`;
    if (field.type === 'BAR') return `nivel${index + 1}`;
    return `texto${index + 1}`;
  }

  function variableDeclaration(field, index) {
    const variable = sanitizeSymbol(field.variable, variableFallback(field, index));
    if (field.type === 'VALUE' || field.type === 'BAR') return `float ${variable} = 0.0f;`;
    if (field.type === 'BOOL') return `bool ${variable} = false;`;
    return `char ${variable}[${Math.max(1, field.capacity) + 1}] = {};`;
  }

  function fieldContract(field, index) {
    const id = sanitizeSymbol(field.id, fieldFallbackId(field, index));
    const layout = `JWPLC_UI_LAYOUT_${field.layout}`;
    const align = `JWPLC_UI_ALIGN_${field.align}`;
    const frame = cppBool(field.frame);
    const label = field.label ? `"${cppString(field.label)}"` : 'nullptr';
    const unit = field.unit ? `"${cppString(field.unit)}"` : 'nullptr';
    const colors = `JWPLC_UIColors(\n            ${hex565(field.labelColor)},\n            ${hex565(field.valueColor)},\n            ${hex565(field.backgroundColor)},\n            ${hex565(field.frameColor)})`;

    if (field.type === 'VALUE') {
      return `    JWPLC_UIValueField(\n        ${id},\n        JWPLC_UIRect(${field.x}, ${field.y}),\n        JWPLC_UIText(${label}, ${unit}),\n        JWPLC_UIValueFormat(\n            ${Math.max(1, field.integerDigits)},\n            ${Math.max(0, field.decimalDigits)},\n            ${cppBool(field.signedValue)},\n            ${cppBool(field.leadingZeros)}),\n        JWPLC_UIValueStyle(\n            ${field.valueSize},\n            ${field.labelSize},\n            ${frame},\n            ${layout},\n            ${align}),\n        ${field.page},\n        ${colors})`;
    }

    return `    JWPLC_UITextField(\n        ${id},\n        JWPLC_UIRect(${field.x}, ${field.y}),\n        JWPLC_UIText(${label}, ${unit}, ${field.capacity}),\n        JWPLC_UITextFieldStyle(\n            ${field.valueSize},\n            ${field.labelSize},\n            ${frame},\n            ${layout},\n            ${align}),\n        ${field.page},\n        ${colors})`;
  }

  function setterHint(field, index) {
    const id = sanitizeSymbol(field.id, fieldFallbackId(field, index));
    const variable = sanitizeSymbol(field.variable, variableFallback(field, index));
    if (field.type === 'VALUE') return `// JWPLC_Display.setValue(${id}, ${variable});`;
    if (field.type === 'BOOL') return `// JWPLC_Display.setBool(${id}, ${variable});`;
    if (field.type === 'BAR') return `// JWPLC_Display.setBar(${id}, ${variable});`;
    return `// JWPLC_Display.setText(${id}, ${variable});`;
  }

  function buildContractText() {
    const validFields = hmiFields.filter(f => !['LINE', 'RECT', 'ELLIPSE', 'TRIANGLE', 'POLYGON', 'RAW_TEXT'].includes(f.type));
    if (validFields.length === 0) return '// Sin campos HMI. Agrega TEXT, VALUE, BOOL o BAR para comenzar.';
    const enumLines = validFields
      .map((field, index) => `    ${sanitizeSymbol(field.id, fieldFallbackId(field, index))} = ${index + 1}`)
      .join(',\n');
    const variables = validFields.map(variableDeclaration).join('\n');
    const fields = validFields.map(fieldContract).join(',\n');
    const setters = validFields.map(setterHint).join('\n');

    return `// Código generado por JWPLC HMI Designer\n// API pública JWPLC_UI · Alpha11 A11-3E\n\nenum HMIFieldId : uint8_t\n{\n${enumLines}\n};\n\n// Variables HMI\n${variables}\n\n// Definición declarativa\nstatic const JWPLC_UIField HMI_FIELDS[] =\n{\n${fields}\n};\n\nvoid jwplcHMISetup()\n{\n    JWPLC_Display.setFields(\n        HMI_FIELDS,\n        sizeof(HMI_FIELDS) / sizeof(HMI_FIELDS[0]));\n}\n\n// jwplcUIUpdate() NO se genera.\n// El usuario alimenta las variables anteriores dentro de su jwplcUIUpdate().\n// Setters públicos que corresponden a este diseño:\n${setters}`;
  }

  function buildStatusText() {
    const field = selectedField();
    const g = field ? computeFieldGeometry(field) : null;
    const textCount = hmiFields.filter((item) => item.type === 'TEXT').length;
    const valueCount = hmiFields.filter((item) => item.type === 'VALUE').length;
    const current = hmiPages.find((item) => item.id === activePage);
    return `A11 UX Foundation: PASS\nA11 UX-4 Edición: PASS\nA11-3A TEXT: PASS_BASE_DESIGNER\nA11-3B VALUE: PASS\nA11-3C BOOL: PASS\nA11-3D BAR: PASS\nA11-3E PAGES: IN_PROGRESS\n\n- Página activa: ${activePage} · ${current?.name || 'Página'}\n- Páginas: ${hmiPages.length}/${MAX_PAGES}\n- Campos globales: ${hmiFields.length}/${MAX_FIELDS}\n- Campos página: ${fieldsForPage().length}\n- TEXT: ${textCount}\n- VALUE: ${valueCount}\n- Selección: ${field ? `${field.type} ${field.name} · ${field.id}` : 'ninguna'}\n- Movimiento: flechas 1 px / Shift+flechas 10 px\n- Duplicar: Ctrl+D\n- Eliminar: Delete\n${g ? `- AUTO field: ${g.fieldW} × ${g.fieldH} px\n- X/Y: ${field.x}, ${field.y}\n- effectivePadding: ${g.pad} px` : ''}${field?.type === 'VALUE' ? `\n- sample reservado: ${makeNumericSample(field)}\n- preview formateado: ${formatNumericPreview(field) || '(vacío)'}` : ''}`;
  }

  function updateMetrics() {
    const selF = selectedField();
    if (selF && selF.type === 'RAW_TEXT') {
      const width = selF.text ? selF.text.length * 6 * (selF.size || 1) : 0;
      const height = selF.text ? 8 * (selF.size || 1) : 0;
      rawBoundsStatus.textContent = `${width} × ${height} px`;
      return;
    }
    if (selectedTool === 'rawText') {
      const width = rawState.value ? rawState.value.length * 6 * rawState.size : 0;
      const height = rawState.value ? 8 * rawState.size : 0;
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

  function renderObjectList() {
    objectList.querySelectorAll('.object-item').forEach((item) => item.remove());
    const visibleFields = fieldsForPage(activePage);
    visibleFields.forEach((field) => {
      const index = hmiFields.indexOf(field);
      const button = document.createElement('button');
      const active = selectedFieldKeys.includes(field.key);
      button.className = `object-item${active ? ' active' : ''}`;
      button.type = 'button';
      button.dataset.fieldKey = field.key;
      button.title = `${field.type} · ${field.name} · ${field.id}`;
      let icon = 'T';
      if (field.type === 'VALUE') icon = '123';
      else if (field.type === 'BOOL') icon = '○';
      else if (field.type === 'BAR') icon = '▥';
      else if (field.type === 'LINE') icon = '╱';
      else if (field.type === 'RECT') icon = '□';
      else if (field.type === 'ELLIPSE') icon = '⬭';
      else if (field.type === 'TRIANGLE') icon = '△';
      else if (field.type === 'POLYGON') icon = '⎔';
      else if (field.type === 'RAW_TEXT') icon = 'A';
      
      button.innerHTML = `<span class="object-icon">${icon}</span><span class="object-type">${field.type}</span><span class="object-name"></span><span class="object-id"></span><span class="object-eye">●</span>`;
      if (field.type === 'VALUE') {
        const iconNode = button.querySelector('.object-icon');
        iconNode.style.fontSize = '9.5px';
        iconNode.style.fontWeight = '800';
        iconNode.style.color = '#52c9ff';
      }
      button.querySelector('.object-name').textContent = field.name || `${field.type} ${index + 1}`;
      if (['LINE', 'RECT', 'ELLIPSE', 'TRIANGLE', 'POLYGON', 'RAW_TEXT'].includes(field.type)) {
        button.querySelector('.object-id').textContent = '';
      } else {
        button.querySelector('.object-id').textContent = field.id || fieldFallbackId(field, index);
      }
      button.addEventListener('click', (e) => {
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
      objectList.appendChild(button);
    });
    countBadge.textContent = String(visibleFields.length);
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
    drawGrid();

    if (previewCtx) {
      previewCtx.imageSmoothingEnabled = false;
      previewCtx.clearRect(0, 0, WIDTH, HEIGHT);
      previewCtx.drawImage(logicalCanvas, 0, 0);
    }

    renderObjectList();

    drawSelectionAndGuides();
    drawRulers();
  
    

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

  function getSelectionBounds(fields) {
    if (!fields || fields.length === 0) return null;
    let minX = Infinity, minY = Infinity, maxX = -Infinity, maxY = -Infinity;
    for (const f of fields) {
      const b = getFieldBounds(f);
      if (b.minX < minX) minX = b.minX;
      if (b.maxX > maxX) maxX = b.maxX;
      if (b.minY < minY) minY = b.minY;
      if (b.maxY > maxY) maxY = b.maxY;
    }
    if (minX > maxX || minY > maxY) return null;

    const screenX1 = minX * zoom;
    const screenY1 = minY * zoom;
    const screenX2 = (maxX + 1) * zoom;
    const screenY2 = (maxY + 1) * zoom;
    const screenW = screenX2 - screenX1;
    const screenH = screenY2 - screenY1;

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

  function getSelectionHandles(fields) {
    if (!fields || fields.length === 0) return [];

    // Single rotated shape: place handles at actual transformed corners/midpoints
    if (fields.length === 1) {
      const f = fields[0];
      const SHAPE_TYPES = ['LINE', 'RECT', 'ELLIPSE', 'TRIANGLE', 'POLYGON'];
      if (SHAPE_TYPES.includes(f.type) && (Number(f.rotation) || 0) !== 0) {
        const x1 = f.x ?? 0; const y1 = f.y ?? 0;
        const x2 = f.x2 ?? x1; const y2 = f.y2 ?? y1;
        const cx = (x1 + x2) / 2; const cy = (y1 + y2) / 2;
        const minX = Math.min(x1, x2); const maxX = Math.max(x1, x2);
        const minY = Math.min(y1, y2); const maxY = Math.max(y1, y2);
        const midX = (minX + maxX) / 2; const midY = (minY + maxY) / 2;
        const angleDeg = Number(f.rotation) || 0;
        const flipH = Boolean(f.flipH); const flipV = Boolean(f.flipV);
        const tp = (px, py) => {
          const r = transformShapePoint(px, py, cx, cy, angleDeg, flipH, flipV);
          return { x: r.x * zoom, y: r.y * zoom };
        };
        return [
          { id: 'tl', ...tp(minX, minY), cursor: 'crosshair' },
          { id: 'tc', ...tp(midX, minY), cursor: 'crosshair' },
          { id: 'tr', ...tp(maxX, minY), cursor: 'crosshair' },
          { id: 'rc', ...tp(maxX, midY), cursor: 'crosshair' },
          { id: 'br', ...tp(maxX, maxY), cursor: 'crosshair' },
          { id: 'bc', ...tp(midX, maxY), cursor: 'crosshair' },
          { id: 'bl', ...tp(minX, maxY), cursor: 'crosshair' },
          { id: 'lc', ...tp(minX, midY), cursor: 'crosshair' },
        ];
      }
    }

    // Default: AABB handles for unrotated or multi-selection
    const b = getSelectionBounds(fields);
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
    if (fields.length === 0) return null;
    const rect = displayCanvas.getBoundingClientRect();
    const scaleX = displayCanvas.width / rect.width;
    const scaleY = displayCanvas.height / rect.height;
    const mouseScreenX = (event.clientX - rect.left) * scaleX;
    const mouseScreenY = (event.clientY - rect.top) * scaleY;

    const handles = getSelectionHandles(fields);
    const hitRadius = 8;
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
    if (fields.length === 0) return false;
    const b = getSelectionBounds(fields);
    if (!b) return false;
    const margin = 1;
    return (
      point.x >= b.minX - margin &&
      point.x <= b.maxX + margin &&
      point.y >= b.minY - margin &&
      point.y <= b.maxY + margin
    );
  }

  function drawSelectionAndGuides() {
    const fields = selectedFields();
    if (fields.length === 0) return;

    const b = getSelectionBounds(fields);
    if (!b) return;

    displayCtx.save();

    // 1. Dotted projection lines to rulers (always use AABB for guides)
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

    // 2. Bounding outline — rotated rect for single rotated shape, AABB otherwise
    displayCtx.strokeStyle = '#0084ff';
    displayCtx.lineWidth = 1;
    const SHAPE_TYPES = ['LINE', 'RECT', 'ELLIPSE', 'TRIANGLE', 'POLYGON'];
    if (fields.length === 1 && SHAPE_TYPES.includes(fields[0].type) && (Number(fields[0].rotation) || 0) !== 0) {
      const f = fields[0];
      const x1 = f.x ?? 0; const y1 = f.y ?? 0;
      const x2 = f.x2 ?? x1; const y2 = f.y2 ?? y1;
      const cx = (x1 + x2) / 2; const cy = (y1 + y2) / 2;
      const minX = Math.min(x1, x2); const maxX = Math.max(x1, x2);
      const minY = Math.min(y1, y2); const maxY = Math.max(y1, y2);
      const angleDeg = Number(f.rotation) || 0;
      const flipH = Boolean(f.flipH); const flipV = Boolean(f.flipV);
      const corners = [
        [minX, minY], [maxX, minY], [maxX, maxY], [minX, maxY]
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
      displayCtx.strokeRect(b.screenX1 + 0.5, b.screenY1 + 0.5, b.screenW, b.screenH);
    }

    // 3. 8 Selection Handles
    const hs = 6;
    const handles = getSelectionHandles(fields);
    handles.forEach((h) => {
      const rx = Math.round(h.x - hs / 2);
      const ry = Math.round(h.y - hs / 2);
      displayCtx.fillStyle = '#060d13';
      displayCtx.fillRect(rx, ry, hs, hs);
      displayCtx.strokeStyle = '#0084ff';
      displayCtx.lineWidth = 1.5;
      displayCtx.strokeRect(rx + 0.5, ry + 0.5, hs - 1, hs - 1);
    });

    displayCtx.restore();
  }

  function drawRulers() {
    const topCanvas = document.getElementById('topRulerCanvas');
    const rightCanvas = document.getElementById('rightRulerCanvas');
    if (!topCanvas || !rightCanvas) return;

    const targetTopW = WIDTH * zoom;
    const targetRightH = HEIGHT * zoom;

    if (topCanvas.width !== targetTopW || topCanvas.height !== 22) {
      topCanvas.width = targetTopW;
      topCanvas.height = 22;
      topCanvas.style.width = `${targetTopW}px`;
      topCanvas.style.height = '22px';
    }
    if (rightCanvas.width !== 36 || rightCanvas.height !== targetRightH) {
      rightCanvas.width = 36;
      rightCanvas.height = targetRightH;
      rightCanvas.style.width = '36px';
      rightCanvas.style.height = `${targetRightH}px`;
    }

    const topCtx = topCanvas.getContext('2d');
    const rightCtx = rightCanvas.getContext('2d');

    topCtx.clearRect(0, 0, topCanvas.width, topCanvas.height);
    rightCtx.clearRect(0, 0, rightCanvas.width, rightCanvas.height);

    // Top ruler ticks
    topCtx.strokeStyle = '#5a6b78';
    topCtx.lineWidth = 1;
    topCtx.beginPath();
    for (let x = 0; x <= WIDTH; x += 2) {
      const px = Math.round(x * zoom) + 0.5;
      let tickLen = 0;
      if (x % 50 === 0) tickLen = 8;
      else if (x % 10 === 0) tickLen = 5;
      else if (x % 2 === 0 && zoom >= 3) tickLen = 3;
      if (tickLen > 0) {
        topCtx.moveTo(px, 22 - tickLen);
        topCtx.lineTo(px, 22);
      }
    }
    topCtx.stroke();

    // Right ruler ticks
    rightCtx.strokeStyle = '#5a6b78';
    rightCtx.lineWidth = 1;
    rightCtx.beginPath();
    for (let y = 0; y <= HEIGHT; y += 2) {
      const py = Math.round(y * zoom) + 0.5;
      let tickLen = 0;
      if (y % 50 === 0) tickLen = 8;
      else if (y % 10 === 0) tickLen = 5;
      else if (y % 2 === 0 && zoom >= 3) tickLen = 3;
      if (tickLen > 0) {
        rightCtx.moveTo(0, py);
        rightCtx.lineTo(tickLen, py);
      }
    }
    rightCtx.stroke();

    // Selected field coordinates
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

    topCtx.font = '10px Consolas, monospace';
    topCtx.fillStyle = '#9cb1c2';
    topCtx.textAlign = 'center';
    topCtx.textBaseline = 'top';

    rightCtx.font = '10px Consolas, monospace';
    rightCtx.fillStyle = '#9cb1c2';
    rightCtx.textAlign = 'left';
    rightCtx.textBaseline = 'middle';

    if (selBounds) {
      const px1 = selBounds.x1 * zoom;
      const px2 = selBounds.x2 * zoom;
      topCtx.fillStyle = '#dbe8f2';
      topCtx.fillText(String(selBounds.x1), px1, 2);
      if (px2 - px1 > 24) {
        topCtx.fillText(String(selBounds.x2), px2, 2);
      }

      // Highlight ticks at x1 and x2
      topCtx.strokeStyle = '#0084ff';
      topCtx.lineWidth = 2;
      topCtx.beginPath();
      topCtx.moveTo(px1 + 0.5, 12);
      topCtx.lineTo(px1 + 0.5, 22);
      topCtx.moveTo(px2 + 0.5, 12);
      topCtx.lineTo(px2 + 0.5, 22);
      topCtx.stroke();

      const py1 = selBounds.y1 * zoom;
      const py2 = selBounds.y2 * zoom;
      rightCtx.fillStyle = '#dbe8f2';
      rightCtx.fillText(String(selBounds.y1), 12, py1);
      if (py2 - py1 > 14) {
        rightCtx.fillText(String(selBounds.y2), 12, py2);
      }

      // Highlight ticks at y1 and y2
      rightCtx.strokeStyle = '#0084ff';
      rightCtx.lineWidth = 2;
      rightCtx.beginPath();
      rightCtx.moveTo(0, py1 + 0.5);
      rightCtx.lineTo(10, py1 + 0.5);
      rightCtx.moveTo(0, py2 + 0.5);
      rightCtx.lineTo(10, py2 + 0.5);
      rightCtx.stroke();
    } else {
      for (let x = 50; x < WIDTH; x += 50) {
        topCtx.fillText(String(x), x * zoom, 2);
      }
      for (let y = 50; y < HEIGHT; y += 50) {
        rightCtx.fillText(String(y), 12, y * zoom);
      }
    }
  }

  function updateCursor(point) {
    if (!inside(point.x, point.y)) {
      cursorStatus.textContent = 'X: — · Y: —';
      pixelStatus.textContent = 'Pixel: —';
      updateEyedropperLiveUI(point);
      return;
    }
    cursorStatus.textContent = `X: ${point.x} · Y: ${point.y}`;
    pixelStatus.textContent = `Pixel: ${hex565(framebuffer[indexFor(point.x, point.y)])}`;
    updateEyedropperLiveUI(point);
  }

  function syncToolUI() {
    document.querySelectorAll('.tool[data-tool]').forEach((button) => {
      button.classList.toggle('active', button.dataset.tool === selectedTool);
    });

    const field = selectedField();
    const isRaw = selectedTool === 'rawText' || field?.type === 'RAW_TEXT';
    const isColorTool = (selectedTool === 'pick' || selectedTool === 'fill');
    const fieldTools = ['textField', 'valueField', 'boolField', 'barField', 'pointer'];

    const SHAPE_TYPES = ['LINE', 'RECT', 'ELLIPSE', 'TRIANGLE', 'POLYGON'];
    const isShape = Boolean(field && SHAPE_TYPES.includes(field.type));

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
    }
    const showField = Boolean(field && fieldTools.includes(selectedTool) && field.type !== 'RAW_TEXT' && !isShape);
    if (fieldSection) {
      fieldSection.hidden = !showField;
      fieldSection.style.display = showField ? '' : 'none';
    }
    if (rawMetricsSection) rawMetricsSection.hidden = !isRaw;
    if (fieldMetricsSection) fieldMetricsSection.hidden = !showField;

    if (isShape) {
      syncShapeInspector();
    }

    if (isColorTool) {
      updateEyedropperActiveUI();
      renderRecentColors();
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
  }

  function updateActiveColorUI() {
    activeColorSwatch.style.background = rgb565ToCss(selectedColor.value);
    activeColorName.textContent = selectedColor.name;
    activeColorValue.textContent = hex565(selectedColor.value);
    updateEyedropperActiveUI();
    renderRecentColors();
  }

  function buildColorSelect(select, selectedName) {
    if (!select) return;
    select.innerHTML = '';
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
    const visual = document.createElement('input');
    visual.type = 'color';
    visual.title = 'Selector visual interactivo RGB';
    const swatch = document.createElement('span');
    swatch.className = 'a11-swatch';
    row.append(visual, swatch);
    host.appendChild(row);
    select.insertAdjacentElement('afterend', host);

    function sync() {
      const val = Number(getValue()) & 0xFFFF;
      const hex888 = rgb565ToHex888(val);
      visual.value = hex888;
      swatch.style.background = rgb565ToCss(val);
      swatch.title = `${hex565(val)} · ${hex888.toUpperCase()}`;
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
      const matched = COLORS.find((c) => c.name === select.value);
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
          } else if (field.type === 'RAW_TEXT') {
            field.textColor = color.value;
          } else if (['TEXT', 'VALUE', 'BOOL', 'BAR'].includes(field.type)) {
            field.valueColor = color.value;
          }
          syncInputsFromState();
        }
        updateActiveColorUI();
        buildPalette();
        render();
        commitHistory();
      });
      palette.appendChild(button);
    });
  }

  function syncShapeInspector() {
    const field = selectedField();
    if (!field) return;
    const SHAPE_ICONS = { LINE: '\u2571', RECT: '\u25a1', ELLIPSE: '\u2b2d', TRIANGLE: '\u25b3', POLYGON: '\u2394' };
    if (shapeInspectorIcon) shapeInspectorIcon.textContent = SHAPE_ICONS[field.type] || '\u25a1';
    if (shapeInspectorName) shapeInspectorName.value = field.name || (field.type.toLowerCase());
    const x1 = field.x ?? 0; const y1 = field.y ?? 0;
    const x2 = field.x2 ?? x1; const y2 = field.y2 ?? y1;
    // Show center position (invariant to rotation) and local width/height
    const cx = (x1 + x2) / 2; const cy = (y1 + y2) / 2;
    if (shapeX) shapeX.value = Math.round(cx);
    if (shapeY) shapeY.value = Math.round(cy);
    if (shapeW) shapeW.value = Math.abs(x2 - x1) || 1;
    if (shapeH) shapeH.value = Math.abs(y2 - y1) || 1;
    if (shapeSidesWrap2) shapeSidesWrap2.hidden = field.type !== 'POLYGON';
    if (shapeSides2) shapeSides2.value = field.sides || 5;
    if (shapeRotation) shapeRotation.value = field.rotation || 0;
    // Border
    if (shapeBorderEnabled) shapeBorderEnabled.checked = field.borderEnabled !== false;
    const fc = field.frameColor ?? 0xFFFF;
    if (shapeBorderColorSwatch) shapeBorderColorSwatch.style.background = rgb565ToCss(fc);
    if (shapeBorderColorInput) shapeBorderColorInput.value = rgb565ToHex888(fc);
    if (shapeBorderColorCode) shapeBorderColorCode.textContent = rgb565ToHex888(fc).toUpperCase();
    if (shapeBorderSize) shapeBorderSize.value = field.size || 1;
    // Fill
    if (shapeFillEnabled) shapeFillEnabled.checked = !!field.fill;
    const fillC = field.fillColor ?? 0xFFE0;
    if (shapeFillColorSwatch) shapeFillColorSwatch.style.background = rgb565ToCss(fillC);
    if (shapeFillColorInput) shapeFillColorInput.value = rgb565ToHex888(fillC);
    if (shapeFillColorCode) shapeFillColorCode.textContent = rgb565ToHex888(fillC).toUpperCase();
  }

  function syncInputsFromState() {
    rawTextInput.value = rawState.value;
    rawTextX.value = String(rawState.x);
    rawTextY.value = String(rawState.y);
    rawTextSize.value = String(rawState.size);
    if (rawTextTransparent) rawTextTransparent.value = rawState.transparent ? '1' : '0';
    if (rawTextColor) ensureSelectOption(rawTextColor, rawState.foreground);
    if (rawTextBackground) ensureSelectOption(rawTextBackground, rawState.background);
    if (rawTextBackgroundWrap) rawTextBackgroundWrap.hidden = Boolean(rawState.transparent);

    const field = selectedField();
    if (!field) {
      inspectorContract.textContent = 'Sin objeto seleccionado';
      syncAllColorControls();
      return;
    }

    if (field.type === 'RAW_TEXT') {
      rawTextInput.value = field.text || '';
      rawTextX.value = String(field.x);
      rawTextY.value = String(field.y);
      rawTextSize.value = String(field.size || 1);
      if (rawTextTransparent) rawTextTransparent.value = field.transparentBackground ? '1' : '0';
      if (rawTextColor) ensureSelectOption(rawTextColor, field.textColor ?? 0xFFFF);
      if (rawTextBackground) ensureSelectOption(rawTextBackground, field.backgroundColor ?? 0x0000);
      if (rawTextBackgroundWrap) rawTextBackgroundWrap.hidden = Boolean(field.transparentBackground);
      syncAllColorControls();
      inspectorContract.textContent = `// Texto RAW estático: JWPLC_Display.getTFT()->print(...)`;
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
    fieldLayout.value = field.layout || 'INLINE';
    fieldAlign.value = field.align || 'LEFT';
    fieldLabelColor.value = colorName(field.labelColor || 0xFFFF);
    fieldValueColor.value = colorName(field.valueColor || 0xFFFF);
    fieldBackgroundColor.value = colorName(field.backgroundColor || 0x0000);
    fieldFrameColor.value = colorName(field.frameColor || 0xFFFF);
    syncAllColorControls();

    if (field.type === 'VALUE') {
      if (fieldIntegerDigits) fieldIntegerDigits.value = String(field.integerDigits);
      if (fieldDecimalDigits) fieldDecimalDigits.value = String(field.decimalDigits);
      if (fieldSigned) fieldSigned.value = field.signedValue ? '1' : '0';
      if (fieldLeadingZeros) fieldLeadingZeros.value = field.leadingZeros ? '1' : '0';
      inspectorContract.textContent = `float ${sanitizeSymbol(field.variable, 'valor')} = 0.0f;`;
    } else if (field.type === 'BOOL') {
      inspectorContract.textContent = `bool ${sanitizeSymbol(field.variable, 'estado')} = false;`;
    } else if (field.type === 'BAR') {
      inspectorContract.textContent = `float ${sanitizeSymbol(field.variable, 'nivel')} = 0.0f;`;
    } else if (['LINE', 'RECT', 'ELLIPSE', 'TRIANGLE', 'POLYGON'].includes(field.type)) {
      inspectorContract.textContent = `// Objeto est\u00e1tico: no requiere variable de estado`;
      syncShapeInspector();
    } else {
      inspectorContract.textContent = `char ${sanitizeSymbol(field.variable, 'texto')}[${field.capacity + 1}] = {};`;
    }
  }

  function captureSnapshot() {
    return {
      fields: hmiFields.map((field) => ({ ...field })),
      pages: hmiPages.map((page) => ({ ...page })),
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
      name: `${typeLabel} ${fieldSerial}`,
      id: uniqueFieldSymbol(`FIELD_${type}_${fieldSerial}`),
      variable: '',
      x: startPoint.x,
      y: startPoint.y,
      x2: endPoint.x,
      y2: endPoint.y,
      page: activePage,
      frameColor: selectedColor ? selectedColor.value : 0xFFFF,
      fillColor: selectedColor ? selectedColor.value : 0xFFFF,
      backgroundColor: 0x0000,
      fill: false,
      size: 1,
      sides: Number(document.getElementById('mainPolySidesInput')?.value || 5)
    };
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
      transparentBackground: true,
      textColor: selectedColor ? selectedColor.value : (rawState.foreground ?? 0xFFFF),
      backgroundColor: rawState.background ?? 0x0000,
      page: activePage
    };
    hmiFields.push(field);
    setSelectedKeys([field.key]);
    selectedTool = 'rawText';
    syncInputsFromState();
    syncToolUI();
    render();
    commitHistory();
    return field;
  }

  function addTextField() {
    if (hmiFields.length >= MAX_FIELDS) return;
    fieldSerial += 1;
    const field = defaultTextField(`text-${fieldSerial}`);
    field.name = `TEXT ${fieldSerial}`;
    field.id = uniqueFieldSymbol(`FIELD_TEXT_${fieldSerial}`);
    field.variable = uniqueVariable(`texto${fieldSerial}`);
    field.label = `Texto ${fieldSerial}`;
    field.preview = 'READY';
    field.page = activePage;
    field.x = 20 + ((fieldSerial - 1) * 8) % 80;
    field.y = 20 + ((fieldSerial - 1) * 8) % 60;
    placeNewField(field);
    hmiFields.push(field);
    setSelectedKeys([field.key]);
    selectedTool = 'textField';
    syncInputsFromState();
    syncToolUI();
    render();
    commitHistory();
  }

  function addValueField() {
    if (hmiFields.length >= MAX_FIELDS) return;
    fieldSerial += 1;
    const field = defaultValueField(`value-${fieldSerial}`);
    field.name = `VALUE ${fieldSerial}`;
    field.id = uniqueFieldSymbol(`FIELD_VALUE_${fieldSerial}`);
    field.variable = uniqueVariable(`valor${fieldSerial}`);
    field.label = `Valor ${fieldSerial}`;
    field.page = activePage;
    field.x = 28 + ((fieldSerial - 1) * 8) % 90;
    field.y = 44 + ((fieldSerial - 1) * 8) % 70;
    placeNewField(field);
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
    if (sources.length === 0 || hmiFields.length + sources.length > MAX_FIELDS) return;
    const newKeys = [];
    sources.forEach((source) => {
      fieldSerial += 1;
      const copy = { ...source };
      const prefix = source.type.toLowerCase();
      copy.key = `${prefix}-${fieldSerial}`;
      copy.name = `${source.name || source.type} copia`;
      copy.id = uniqueFieldSymbol(`${sanitizeSymbol(source.id, fieldFallbackId(source, 0))}_COPY`);
      copy.variable = uniqueVariable(`${sanitizeSymbol(source.variable, variableFallback(source, 0))}Copy`);
      copy.page = activePage;
      copy.x = source.x + 8;
      copy.y = source.y + 8;
      if (source.x2 !== undefined) copy.x2 = source.x2 + 8;
      if (source.y2 !== undefined) copy.y2 = source.y2 + 8;
      placeNewField(copy);
      hmiFields.push(copy);
      newKeys.push(copy.key);
    });
    setSelectedKeys(newKeys);
    const primary = selectedField();
    selectedTool = primary ? toolForField(primary) : 'pointer';
    syncInputsFromState();
    syncToolUI();
    render();
    commitHistory();
  }

  function deleteSelectedField() {
    const keysToDelete = new Set(selectedFieldKeys);
    if (keysToDelete.size === 0) return;
    hmiFields = hmiFields.filter((field) => !keysToDelete.has(field.key));
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
      if (tool === 'rawText') {
        if (!field || field.type !== 'RAW_TEXT') addRawTextField();
        else {
          selectedTool = 'rawText';
          syncToolUI();
          render();
        }
        return;
      }
      selectedTool = tool;
      if (!['pointer', 'textField', 'valueField', 'boolField', 'barField', 'rawText'].includes(tool)) {
        setSelectedKeys([]);
      }
      syncInputsFromState();
      syncToolUI();
      render();
    });
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
  });
  bindRawInput(rawTextX, () => {
    rawState.x = clamp(Number(rawTextX.value) || 0, 0, WIDTH - 1);
    const field = selectedField();
    if (field && field.type === 'RAW_TEXT') field.x = rawState.x;
  });
  bindRawInput(rawTextY, () => {
    rawState.y = clamp(Number(rawTextY.value) || 0, 0, HEIGHT - 1);
    const field = selectedField();
    if (field && field.type === 'RAW_TEXT') field.y = rawState.y;
  });
  bindRawInput(rawTextSize, () => {
    rawState.size = Number(rawTextSize.value) || 1;
    const field = selectedField();
    if (field && field.type === 'RAW_TEXT') field.size = rawState.size;
  });
  bindRawInput(rawTextTransparent, () => {
    const isTrans = rawTextTransparent.value === '1';
    rawState.transparent = isTrans;
    const field = selectedField();
    if (field && field.type === 'RAW_TEXT') field.transparentBackground = isTrans;
    if (rawTextBackgroundWrap) rawTextBackgroundWrap.hidden = isTrans;
  });
  bindRawInput(rawTextColor, () => {
    const color = colorByName(rawTextColor.value).value;
    rawState.foreground = color;
    const field = selectedField();
    if (field && field.type === 'RAW_TEXT') field.textColor = color;
  });
  bindRawInput(rawTextBackground, () => {
    rawState.background = colorByName(rawTextBackground.value).value;
    const field = selectedField();
    if (field && field.type === 'RAW_TEXT') field.backgroundColor = rawState.background;
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
  bindFieldInput(fieldSides, (field) => { field.sides = clamp(Number(fieldSides.value) || 5, 3, 12); });
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
  bindFieldInput(fieldFrame, (field) => { field.frame = fieldFrame.value === '1'; });
  bindFieldInput(fieldLayout, (field) => { field.layout = fieldLayout.value; });
  bindFieldInput(fieldAlign, (field) => { field.align = fieldAlign.value; });
  bindFieldInput(fieldLabelColor, (field) => { field.labelColor = colorByName(fieldLabelColor.value).value; });
  bindFieldInput(fieldValueColor, (field) => { field.valueColor = colorByName(fieldValueColor.value).value; });
  bindFieldInput(fieldBackgroundColor, (field) => { field.backgroundColor = colorByName(fieldBackgroundColor.value).value; });
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
    let px = point.x;
    let py = point.y;
    if (['LINE','RECT','ELLIPSE','TRIANGLE','POLYGON'].includes(field.type)) {
      const rcx = ((field.x ?? 0) + (field.x2 ?? field.x ?? 0)) / 2;
      const rcy = ((field.y ?? 0) + (field.y2 ?? field.y ?? 0)) / 2;
      const rotAngle = Number(field.rotation) || 0;
      const local = inverseTransformShapePoint(px, py, rcx, rcy, rotAngle, Boolean(field.flipH), Boolean(field.flipV));
      px = local.x; py = local.y;
    }
    const a = { x: field.x, y: field.y };
    const b = { x: field.x2 ?? field.x, y: field.y2 ?? field.y };

    if (field.fill && isPointInsideShapeArea(px, py, field)) {
      return 0.0;
    }

    if (field.type === 'LINE') {
      return distToSegment(px, py, a.x, a.y, b.x, b.y);
    }

    if (field.type === 'RECT') {
      const left = Math.min(a.x, b.x);
      const right = Math.max(a.x, b.x);
      const top = Math.min(a.y, b.y);
      const bottom = Math.max(a.y, b.y);
      return Math.min(
        distToSegment(px, py, left, top, right, top),
        distToSegment(px, py, right, top, right, bottom),
        distToSegment(px, py, right, bottom, left, bottom),
        distToSegment(px, py, left, bottom, left, top)
      );
    }

    if (field.type === 'ELLIPSE') {
      const rx = Math.abs(b.x - a.x) / 2;
      const ry = Math.abs(b.y - a.y) / 2;
      if (rx < 1 || ry < 1) {
        return distToSegment(px, py, a.x, a.y, b.x, b.y);
      }
      const minX = Math.min(a.x, b.x);
      const maxX = Math.max(a.x, b.x);
      const minY = Math.min(a.y, b.y);
      const maxY = Math.max(a.y, b.y);
      const margin = 12;
      if (px < minX - margin || px > maxX + margin || py < minY - margin || py > maxY + margin) {
        const dx = Math.max(minX - px, 0, px - maxX);
        const dy = Math.max(minY - py, 0, py - maxY);
        return Math.hypot(dx, dy);
      }

      const cx = (a.x + b.x) / 2;
      const cy = (a.y + b.y) / 2;
      let minDist = Infinity;
      let prevX = cx + rx * ELLIPSE_UNIT_SAMPLES[0].c;
      let prevY = cy + ry * ELLIPSE_UNIT_SAMPLES[0].s;

      for (let i = 1; i <= ELLIPSE_SAMPLE_STEPS; i++) {
        const curX = cx + rx * ELLIPSE_UNIT_SAMPLES[i].c;
        const curY = cy + ry * ELLIPSE_UNIT_SAMPLES[i].s;
        const d = distToSegment(px, py, prevX, prevY, curX, curY);
        if (d < minDist) minDist = d;
        prevX = curX;
        prevY = curY;
      }
      return minDist;
    }

    if (field.type === 'TRIANGLE') {
      const topPt = { x: Math.round((a.x + b.x) / 2), y: a.y };
      const bl = { x: a.x, y: b.y };
      const br = { x: b.x, y: b.y };
      return Math.min(
        distToSegment(px, py, topPt.x, topPt.y, bl.x, bl.y),
        distToSegment(px, py, bl.x, bl.y, br.x, br.y),
        distToSegment(px, py, br.x, br.y, topPt.x, topPt.y)
      );
    }

    if (field.type === 'POLYGON') {
      const pts = getPolygonVertices(field);
      let minDist = Infinity;
      const numSides = pts.length;
      for (let i = 0; i < numSides; i++) {
        const p1 = pts[i];
        const p2 = pts[(i + 1) % numSides];
        minDist = Math.min(minDist, distToSegment(px, py, p1.x, p1.y, p2.x, p2.y));
      }
      return minDist;
    }

    // For TEXT, VALUE, BOOL, BAR, etc.
    const g = computeFieldGeometry(field);
    if (
      px >= g.fieldX && px <= g.fieldX + g.fieldW &&
      py >= g.fieldY && py <= g.fieldY + g.fieldH
    ) {
      return 0;
    }
    return Math.min(
      distToSegment(px, py, g.fieldX, g.fieldY, g.fieldX + g.fieldW, g.fieldY),
      distToSegment(px, py, g.fieldX + g.fieldW, g.fieldY, g.fieldX + g.fieldW, g.fieldY + g.fieldH),
      distToSegment(px, py, g.fieldX + g.fieldW, g.fieldY + g.fieldH, g.fieldX, g.fieldY + g.fieldH),
      distToSegment(px, py, g.fieldX, g.fieldY + g.fieldH, g.fieldX, g.fieldY)
    );
  }

  function hitTestField(point) {
    let bestField = null;
    let bestDist = Infinity;
    const tolerance = 2.0;

    for (let index = hmiFields.length - 1; index >= 0; index -= 1) {
      const field = hmiFields[index];
      if (Number(field.page || 0) !== activePage) continue;

      const dist = distanceToFieldBorder(point, field);
      if (dist <= tolerance && dist < bestDist) {
        bestDist = dist;
        bestField = field;
      }
    }
    return bestField;
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
      event.target.id === 'rightRulerCanvas'
    ) {
      return;
    }

    const point = pointFromPointer(event);
    const isTargetDisplay = (event.target === displayCanvas);
    gestureChanged = false;

    if (isTargetDisplay && (selectedTool === 'pick' || event.altKey)) {
      if (pickColorAt(point)) {
        render();
      }
      return;
    }

    if (isTargetDisplay && selectedTool === 'fill') {
      let shapeHit = null;
      for (let index = hmiFields.length - 1; index >= 0; index--) {
        const f = hmiFields[index];
        if (Number(f.page || 0) !== activePage) continue;
        if (['RECT', 'ELLIPSE', 'TRIANGLE', 'POLYGON'].includes(f.type)) {
          if (isPointInsideShapeArea(point.x, point.y, f) || distanceToFieldBorder(point, f) <= 3.0) {
            shapeHit = f;
            break;
          }
        }
      }

      if (shapeHit) {
        shapeHit.fill = true;
        shapeHit.fillColor = selectedColor.value;
        setSelectedKeys([shapeHit.key]);
        syncInputsFromState();
        syncToolUI();
        render();
        commitHistory();
        return;
      }

      if (floodFill(point.x, point.y, selectedColor.value)) {
        render();
        commitHistory();
      }
      return;
    }

    if (isTargetDisplay && (selectedTool === 'pixel' || selectedTool === 'erase')) {
      if (!inside(point.x, point.y)) return;
      drawing = true;
      lastPoint = point;
      const value = selectedTool === 'erase' ? 0x0000 : selectedColor.value;
      gestureChanged = setLayerPixel(point.x, point.y, value);
      render();
      return;
    }

    if (['line', 'rect', 'ellipse', 'triangle', 'polygon'].includes(selectedTool) && isTargetDisplay) {
      const type = selectedTool.toUpperCase();
      addShapeField(type, point, point);
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
    if (handle) {
      resizingHandle = handle;
      resizeInitialBounds = getSelectionBounds(selectedFields());
      resizeInitialPointer = point;
      resizeInitialFields = selectedFields().map((f) => ({ ...f }));
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
      draggingObject = true;
      dragInitialPointer = { ...point, rawX: point.x, rawY: point.y };
      dragInitialFields = selectedFields().map((f) => ({ ...f }));
      render();
      return;
    }

    // 4. Check if clicking directly on any field's contour (1.5px tolerance)
    const hit = hitTestField(point);
    if (hit) {
      setSelectedKeys([hit.key]);
      selectedTool = toolForField(hit);
      draggingObject = true;
      dragInitialPointer = { ...point, rawX: hit.x, rawY: hit.y };
      dragInitialFields = [{ ...hit }];
      syncInputsFromState();
      syncToolUI();
      render();
      return;
    }

    // 5. Empty space clicked: Clear selection and start CAD Marquee box
    setSelectedKeys([]);
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

      // ---- Rotation-aware resize for a single rotated shape ----
      if (fields.length === 1 && ['LINE', 'RECT', 'ELLIPSE', 'TRIANGLE', 'POLYGON'].includes(fields[0].type)) {
        const f = fields[0];
        const orig = resizeInitialFields?.[0];
        if (orig) {
          const angleDeg = Number(orig.rotation) || 0;
          if (angleDeg !== 0) {
            // Shape center is fixed during resize (only size changes, unless moving an opposite corner)
            const ox1 = orig.x ?? 0; const oy1 = orig.y ?? 0;
            const ox2 = orig.x2 ?? ox1; const oy2 = orig.y2 ?? oy1;
            const origCx = (ox1 + ox2) / 2;
            const origCy = (oy1 + oy2) / 2;

            // Un-rotate cursor into shape's local space
            const local = inverseTransformShapePoint(curX, curY, origCx, origCy, angleDeg, false, false);
            const lx = local.x; const ly = local.y;

            const hid = resizingHandle.id;
            let nx1 = ox1; let ny1 = oy1; let nx2 = ox2; let ny2 = oy2;

            // Each handle controls which edge(s) move
            if (hid === 'tl') { nx1 = lx; ny1 = ly; }
            else if (hid === 'tc') { ny1 = ly; }
            else if (hid === 'tr') { nx2 = lx; ny1 = ly; }
            else if (hid === 'rc') { nx2 = lx; }
            else if (hid === 'br') { nx2 = lx; ny2 = ly; }
            else if (hid === 'bc') { ny2 = ly; }
            else if (hid === 'bl') { nx1 = lx; ny2 = ly; }
            else if (hid === 'lc') { nx1 = lx; }

            // Ensure min size of 1px
            if (Math.abs(nx2 - nx1) < 1) nx2 = nx1 + (nx2 >= nx1 ? 1 : -1);
            if (Math.abs(ny2 - ny1) < 1) ny2 = ny1 + (ny2 >= ny1 ? 1 : -1);

            f.x = Math.round(nx1); f.y = Math.round(ny1);
            f.x2 = Math.round(nx2); f.y2 = Math.round(ny2);

            gestureChanged = true;
            syncShapeInspector();
            render();
            return;
          }
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
        } else {
          displayCanvas.style.cursor = (['pixel', 'erase', 'fill', 'pick'].includes(selectedTool) ? 'crosshair' : 'default');
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
      if (snapStep > 0) {
        dx = Math.round(dx / snapStep) * snapStep;
        dy = Math.round(dy / snapStep) * snapStep;
      }

      if (selectedTool === 'rawText' && selectedFields().length === 0) {
        let nextX = (dragInitialPointer?.rawX ?? 0) + dx;
        let nextY = (dragInitialPointer?.rawY ?? 0) + dy;
        nextX = clamp(nextX, 0, WIDTH - 1);
        nextY = clamp(nextY, 0, HEIGHT - 1);
        gestureChanged = gestureChanged || nextX !== rawState.x || nextY !== rawState.y;
        rawState.x = nextX;
        rawState.y = nextY;
      } else {
        const fields = selectedFields();
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
          gestureChanged = true;
        });
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
    dragInitialPointer = null;
    dragInitialFields = null;
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

        if (boxR - boxL >= 2 || boxB - boxT >= 2) {
          const matchedList = [];
          for (let i = 0; i < hmiFields.length; i++) {
            const f = hmiFields[i];
            if (Number(f.page || 0) !== activePage) continue;

            if (isLeftToRight) {
              const fb = getFieldBounds(f);
              const fullyInside = (fb.minX >= boxL && fb.maxX <= boxR && fb.minY >= boxT && fb.maxY <= boxB);
              if (fullyInside) {
                matchedList.push(f);
              }
            } else {
              // SolidWorks Crossing: Touches or crosses the ACTUAL CONTOUR or is enclosed
              if (doesBoxTouchContour(boxL, boxR, boxT, boxB, f)) {
                matchedList.push(f);
              }
            }
          }

          if (matchedList.length > 0) {
            setSelectedKeys(matchedList.map((f) => f.key));
            const primary = selectedField();
            selectedTool = primary ? toolForField(primary) : 'pointer';
            syncInputsFromState();
            syncToolUI();
          } else {
            setSelectedKeys([]);
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

  function nudgeSelection(dx, dy) {
    const snapStep = getSnapStep();
    if (snapStep > 0) {
      dx = Math.sign(dx) * Math.max(Math.abs(dx), snapStep);
      dy = Math.sign(dy) * Math.max(Math.abs(dy), snapStep);
    }

    const fields = selectedFields();
    if (fields.length > 0) {
      let moved = false;
      fields.forEach((field) => {
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
      if (moved) {
        syncInputsFromState();
        render();
        return true;
      }
      return false;
    }

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
    const SHAPE_TYPES = ['LINE', 'RECT', 'ELLIPSE', 'TRIANGLE', 'POLYGON'];
    const fields = selectedFields().filter((f) => SHAPE_TYPES.includes(f.type));
    if (fields.length === 0) return;
    fields.forEach((f) => {
      const b = getFieldBounds(f);
      let dx = 0;
      let dy = 0;
      if (alignType === 'left') {
        dx = 0 - b.minX;
      } else if (alignType === 'hcenter') {
        dx = Math.round((WIDTH - b.width) / 2) - b.minX;
      } else if (alignType === 'right') {
        dx = (WIDTH - 1) - b.maxX;
      } else if (alignType === 'top') {
        dy = 0 - b.minY;
      } else if (alignType === 'vcenter') {
        dy = Math.round((HEIGHT - b.height) / 2) - b.minY;
      } else if (alignType === 'bottom') {
        dy = (HEIGHT - 1) - b.maxY;
      }
      f.x += dx;
      f.y += dy;
      if (f.x2 !== undefined) f.x2 += dx;
      if (f.y2 !== undefined) f.y2 += dy;
    });
    syncShapeInspector(); render(); commitHistory();
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
          selectedTool = 'fill';
          syncToolUI();
          render();
          return;
        }
        if (key === 'i') {
          selectedTool = 'pick';
          syncToolUI();
          render();
          return;
        }
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
          copy.name = (source.name || source.type) + ' copia';
          copy.page = activePage;
          copy.x = (source.x || 0) + 10;
          copy.y = (source.y || 0) + 10;
          if (source.x2 !== undefined) copy.x2 = source.x2 + 10;
          if (source.y2 !== undefined) copy.y2 = source.y2 + 10;
          if (!['LINE','RECT','ELLIPSE','TRIANGLE','POLYGON','RAW_TEXT'].includes(source.type)) {
            copy.id = uniqueFieldSymbol(sanitizeSymbol(source.id, fieldFallbackId(source, 0)) + '_COPY');
            copy.variable = uniqueVariable(sanitizeSymbol(source.variable, variableFallback(source, 0)) + 'Copy');
          }
          placeNewField(copy);
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
  buildColorSelect(rawTextBackground, 'BLACK');
  buildColorSelect(fieldFillColor, 'WHITE');
  buildColorSelect(fieldLabelColor, 'WHITE');
  buildColorSelect(fieldValueColor, 'CYAN');
  buildColorSelect(fieldBackgroundColor, 'BLACK');
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
  eyedropperUseFillBtn?.addEventListener('click', () => {
    selectedTool = 'fill';
    syncToolUI();
    render();
  });

  // ── Shape Inspector Events ──────────────────────────────────────────────
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
    if (shapeSides2 && !shapeSidesWrap2?.hidden) f.sides = Number(shapeSides2.value);
    render();
  }
  [shapeX, shapeY, shapeW, shapeH].forEach((el) => {
    el?.addEventListener('input', shapeGeomUpdate);
    el?.addEventListener('change', () => { shapeGeomUpdate(); commitHistory(); });
  });
  shapeSides2?.addEventListener('input', () => {
    const f = selectedField(); if (!f) return; f.sides = Number(shapeSides2.value); render();
  });
  shapeSides2?.addEventListener('change', () => commitHistory());
  shapeRotation?.addEventListener('input', () => {
    const f = selectedField(); if (!f) return; f.rotation = Number(shapeRotation.value) % 360; render();
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
    const f = selectedField(); if (!f) return; f.borderEnabled = shapeBorderEnabled.checked; render(); commitHistory();
  });
  function triggerColorPicker(input) {
    if (!input) return;
    if (typeof input.showPicker === 'function') {
      try { input.showPicker(); return; } catch (e) {}
    }
    input.click();
  }
  shapeBorderColorSwatch?.addEventListener('click', () => triggerColorPicker(shapeBorderColorInput));
  shapeBorderColorInput?.addEventListener('input', () => {
    const f = selectedField(); if (!f) return;
    const c = hex888ToRgb565(shapeBorderColorInput.value); f.frameColor = c;
    if (shapeBorderColorSwatch) shapeBorderColorSwatch.style.background = rgb565ToCss(c);
    if (shapeBorderColorCode) shapeBorderColorCode.textContent = rgb565ToHex888(c).toUpperCase();
    render();
  });
  shapeBorderColorInput?.addEventListener('change', () => commitHistory());
  shapeBorderSize?.addEventListener('input', () => {
    const f = selectedField(); if (!f) return; f.size = Number(shapeBorderSize.value) || 1; render();
  });
  shapeBorderSize?.addEventListener('change', () => commitHistory());
  shapeFillEnabled?.addEventListener('change', () => {
    const f = selectedField(); if (!f) return; f.fill = shapeFillEnabled.checked;
    syncShapeInspector(); render(); commitHistory();
  });
  shapeFillColorSwatch?.addEventListener('click', () => triggerColorPicker(shapeFillColorInput));
  shapeFillColorInput?.addEventListener('input', () => {
    const f = selectedField(); if (!f) return;
    const c = hex888ToRgb565(shapeFillColorInput.value); f.fillColor = c;
    if (shapeFillColorSwatch) shapeFillColorSwatch.style.background = rgb565ToCss(c);
    if (shapeFillColorCode) shapeFillColorCode.textContent = rgb565ToHex888(c).toUpperCase();
    render();
  });
  shapeFillColorInput?.addEventListener('change', () => commitHistory());
  document.querySelectorAll('.align-btn[data-align]').forEach((btn) => {
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
  // ── End Shape Inspector ─────────────────────────────────────────────────

  buildPalette();
  updateActiveColorUI();
  renderRecentColors();
  syncInputsFromState();
  syncToolUI();
  render();
  history.push(captureSnapshot());
  historyIndex = 0;
  updateHistoryButtons();

  window.JWPLCHMIEditor = {
    getSelectedField: () => selectedField(),
    getSelectedFields: () => selectedFields(),
    getSelectedFieldKeys: () => [...selectedFieldKeys],
    setSelectedFieldKeys: (keys) => setSelectedKeys(keys),
    getSelectedTool: () => selectedTool,
    getSelectedFieldType: () => selectedField()?.type || null,
    getAllFields: () => hmiFields,
    getFieldsForPage: (page = activePage) => fieldsForPage(page),
    getPages: () => hmiPages.map((page) => ({ ...page })),
    getActivePage: () => activePage,
    getMaxPages: () => MAX_PAGES,
    computeSelectedGeometry: () => computeFieldGeometry(selectedField()),
    hasFieldSelection: () => Boolean(selectedField()) && ['textField', 'valueField', 'boolField', 'barField'].includes(selectedTool),
    hasTextSelection: () => selectedField()?.type === 'TEXT' && selectedTool === 'textField',
    hasValueSelection: () => selectedField()?.type === 'VALUE' && selectedTool === 'valueField',
    setActivePage,
    addPage,
    renamePage,
    moveSelectedFieldToPage,
    commitHistory,
    render,
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
    floodFill,
    pickColorAt
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

    // Crosshairs tracking across the viewport
    canvasViewport.addEventListener('pointermove', (e) => {
      const crosshairX = document.getElementById('crosshairX');
      const crosshairY = document.getElementById('crosshairY');
      if (crosshairX && crosshairY) {
        const rect = canvasViewport.getBoundingClientRect();
        const cx = e.clientX - rect.left;
        const cy = e.clientY - rect.top;
        crosshairX.style.display = 'block';
        crosshairY.style.display = 'block';
        crosshairX.style.left = `${cx}px`;
        crosshairY.style.top = `${cy}px`;
      }
    });

    canvasViewport.addEventListener('pointerleave', () => {
      const crosshairX = document.getElementById('crosshairX');
      const crosshairY = document.getElementById('crosshairY');
      if (crosshairX) crosshairX.style.display = 'none';
      if (crosshairY) crosshairY.style.display = 'none';
    });

    // Mouse Wheel: Scroll up/down, Shift+Scroll left/right, Ctrl+Scroll Zoom
    canvasViewport.addEventListener('wheel', (e) => {
      e.preventDefault();
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

  document.getElementById('vertGridToggle')?.addEventListener('click', function() {
    const toggle = document.getElementById('gridToggle');
    if (toggle) {
      toggle.checked = !toggle.checked;
      this.classList.toggle('active', toggle.checked);
      toggle.dispatchEvent(new Event('change', { bubbles: true }));
    }
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
