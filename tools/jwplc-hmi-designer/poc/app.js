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
  const snapToggle = document.getElementById('snapToggle');
  const geometryToggle = document.getElementById('geometryToggle'); document.getElementById('vertSnapToggle')?.addEventListener('click', function() { snapToggle.checked = !snapToggle.checked; this.classList.toggle('active', snapToggle.checked); render(); });
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

  const rawSection = document.getElementById('rawTextControlsSection');
  const fieldSection = document.getElementById('textFieldControlsSection');
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
  const rawTextBackground = document.getElementById('rawTextBackground');
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
  let selectedFieldKey = 'text-1';
  let fieldSerial = 1;
  let drawing = false;
  let draggingObject = false;
  let dragOffset = { x: 0, y: 0 };
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

  function indexFor(x, y) { return y * WIDTH + x; }
  function inside(x, y) { return x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT; }

  function pageExists(page) {
    return hmiPages.some((item) => item.id === Number(page));
  }

  function fieldsForPage(page = activePage) {
    return hmiFields.filter((field) => Number(field.page || 0) === Number(page));
  }

  function selectedField() {
    return hmiFields.find((field) => field.key === selectedFieldKey) || null;
  }

  function toolForField(field) {
    if (!field) return 'none';
    if (field.type === 'VALUE') return 'valueField';
    if (field.type === 'BOOL') return 'boolField';
    if (field.type === 'BAR') return 'barField';
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
    const rx = Math.abs(b.x - a.x) / 2;
    const ry = Math.abs(b.y - a.y) / 2;
    const cx = Math.min(a.x, b.x) + rx;
    const cy = Math.min(a.y, b.y) + ry;

    let x = 0;
    let y = Math.round(ry);
    let rx2 = rx * rx;
    let ry2 = ry * ry;
    let tworx2 = 2 * rx2;
    let twory2 = 2 * ry2;
    let p;
    let px = 0;
    let py = tworx2 * y;

    if (rx === 0 || ry === 0) return rasterLineBuffer(buffer, a.x, a.y, b.x, b.y, color, size);

    const plot = (cx, cy, x, y) => {
      for (let oy = 0; oy < size; oy++) {
        for (let ox = 0; ox < size; ox++) {
          let cxo = ox - Math.floor(size/2);
          let cyo = oy - Math.floor(size/2);
          setBufferPixel(buffer, Math.round(cx + x) + cxo, Math.round(cy + y) + cyo, color);
          setBufferPixel(buffer, Math.round(cx - x) + cxo, Math.round(cy + y) + cyo, color);
          setBufferPixel(buffer, Math.round(cx + x) + cxo, Math.round(cy - y) + cyo, color);
          setBufferPixel(buffer, Math.round(cx - x) + cxo, Math.round(cy - y) + cyo, color);
        }
      }
    };

    p = Math.round(ry2 - (rx2 * ry) + (0.25 * rx2));
    while (px < py) {
      plot(cx, cy, x, y);
      x++;
      px += twory2;
      if (p < 0) p += ry2 + px;
      else { y--; py -= tworx2; p += ry2 + px - py; }
    }
    p = Math.round(ry2 * (x + 0.5) * (x + 0.5) + rx2 * (y - 1) * (y - 1) - rx2 * ry2);
    while (y >= 0) {
      plot(cx, cy, x, y);
      y--;
      py -= tworx2;
      if (p > 0) p += rx2 - py;
      else { x++; px += twory2; p += rx2 - py + px; }
    }
  }

  function drawShape(buffer, field) {
    const color = field.frameColor || 0xFFFF;
    const size = field.size || 1;
    const a = { x: field.x, y: field.y };
    const b = { x: field.x2, y: field.y2 };

    if (field.type === 'LINE') {
      rasterLineBuffer(buffer, a.x, a.y, b.x, b.y, color, size);
    } else if (field.type === 'RECT') {
      const left = Math.min(a.x, b.x);
      const right = Math.max(a.x, b.x);
      const top = Math.min(a.y, b.y);
      const bottom = Math.max(a.y, b.y);
      rasterLineBuffer(buffer, left, top, right, top, color, size);
      rasterLineBuffer(buffer, right, top, right, bottom, color, size);
      rasterLineBuffer(buffer, right, bottom, left, bottom, color, size);
      rasterLineBuffer(buffer, left, bottom, left, top, color, size);
    } else if (field.type === 'ELLIPSE') {
      drawShapeEllipse(buffer, a, b, color, size);
    } else if (field.type === 'TRIANGLE') {
      const topPt = { x: Math.round((a.x + b.x) / 2), y: a.y };
      const bl = { x: a.x, y: b.y };
      const br = { x: b.x, y: b.y };
      rasterLineBuffer(buffer, topPt.x, topPt.y, bl.x, bl.y, color, size);
      rasterLineBuffer(buffer, bl.x, bl.y, br.x, br.y, color, size);
      rasterLineBuffer(buffer, br.x, br.y, topPt.x, topPt.y, color, size);
    } else if (field.type === 'POLYGON') {
      const cx = (a.x + b.x) / 2;
      const cy = (a.y + b.y) / 2;
      const rx = Math.abs(b.x - a.x) / 2;
      const ry = Math.abs(b.y - a.y) / 2;
      const numSides = Math.max(3, Math.min(12, Math.trunc(Number(field.sides) || 5)));
      const pts = [];
      for (let i = 0; i < numSides; i++) {
        const angle = (i * 2 * Math.PI / numSides) - Math.PI / 2;
        pts.push({
          x: Math.round(cx + rx * Math.cos(angle)),
          y: Math.round(cy + ry * Math.sin(angle))
        });
      }
      for (let i = 0; i < numSides; i++) {
        const p1 = pts[i];
        const p2 = pts[(i + 1) % numSides];
        rasterLineBuffer(buffer, p1.x, p1.y, p2.x, p2.y, color, size);
      }
    }
  }

  function drawClassicChar(buffer, x, y, charCode, foreground, background, size) {
    const font = window.JWPLCGfxClassicFont;
    const glyph = font.glyphFor(charCode);
    const scale = Math.max(1, Math.trunc(size));
    for (let column = 0; column < font.cellWidth; column += 1) {
      const bits = column < font.bytesPerGlyph ? glyph[column] : 0;
      for (let row = 0; row < font.cellHeight; row += 1) {
        const on = column < font.bytesPerGlyph && ((bits >> row) & 0x01) !== 0;
        fillBufferRect(buffer, x + column * scale, y + row * scale, scale, scale, on ? foreground : background);
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

  function computeFieldGeometry(field) {
    if (!field) return null;
    if (['LINE', 'RECT', 'ELLIPSE', 'TRIANGLE', 'POLYGON'].includes(field.type)) {
      return {
        pad: 0,
        fieldX: Math.min(field.x, field.x2),
        fieldY: Math.min(field.y, field.y2),
        fieldW: Math.abs(field.x2 - field.x),
        fieldH: Math.abs(field.y2 - field.y),
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
      }
    });
    if (selectedTool === 'rawText') {
      drawClassicTextAt(framebuffer, rawState.value, rawState.x, rawState.y, rawState.foreground, rawState.background, rawState.size);
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
    displayCtx.save();
    displayCtx.fillStyle = 'rgba(118, 151, 176, 0.4)';
    const dotSize = Math.max(1, Math.floor(zoom / 3));
    const offset = Math.floor(dotSize / 2);
    for (let x = 0; x <= WIDTH; x += gridSize) {
      for (let y = 0; y <= HEIGHT; y += gridSize) {
        displayCtx.fillRect(Math.floor(x * zoom) - offset, Math.floor(y * zoom) - offset, dotSize, dotSize);
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
    const validFields = hmiFields.filter(f => !['LINE', 'RECT', 'ELLIPSE', 'TRIANGLE', 'POLYGON'].includes(f.type));
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
      const active = field.key === selectedFieldKey && toolForField(field) === selectedTool;
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
      
      button.innerHTML = `<span class="object-icon">${icon}</span><span class="object-type">${field.type}</span><span class="object-name"></span><span class="object-id"></span><span class="object-eye">●</span>`;
      if (field.type === 'VALUE') {
        const iconNode = button.querySelector('.object-icon');
        iconNode.style.fontSize = '9.5px';
        iconNode.style.fontWeight = '800';
        iconNode.style.color = '#52c9ff';
      }
      button.querySelector('.object-name').textContent = field.name || `${field.type} ${index + 1}`;
      if (['LINE', 'RECT', 'ELLIPSE', 'TRIANGLE', 'POLYGON'].includes(field.type)) {
        button.querySelector('.object-id').textContent = '';
      } else {
        button.querySelector('.object-id').textContent = field.id || fieldFallbackId(field, index);
      }
      button.addEventListener('click', () => {
        selectedFieldKey = field.key;
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

    const sel = selectedField();
    if (sel && sel.type !== 'PIXELMAP') {
      let left, top, w, h;
      if (['LINE', 'RECT', 'ELLIPSE', 'TRIANGLE', 'POLYGON'].includes(sel.type)) {
        left = Math.min(sel.x, sel.x2 ?? sel.x);
        top = Math.min(sel.y, sel.y2 ?? sel.y);
        w = Math.max(1, Math.abs((sel.x2 ?? sel.x) - sel.x));
        h = Math.max(1, Math.abs((sel.y2 ?? sel.y) - sel.y));
      } else {
        const g = computeFieldGeometry(sel);
        left = g.fieldX;
        top = g.fieldY;
        w = g.fieldW;
        h = g.fieldH;
      }
      displayCtx.save();
      displayCtx.strokeStyle = '#ff9a43';
      displayCtx.lineWidth = 1;
      displayCtx.setLineDash([3, 3]);
      displayCtx.strokeRect(left * zoom - 1.5, top * zoom - 1.5, w * zoom + 3, h * zoom + 3);
      displayCtx.fillStyle = '#ff9a43';
      const s = 4;
      displayCtx.fillRect(left * zoom - 2, top * zoom - 2, s, s);
      displayCtx.fillRect((left + w) * zoom - 2, top * zoom - 2, s, s);
      displayCtx.fillRect(left * zoom - 2, (top + h) * zoom - 2, s, s);
      displayCtx.fillRect((left + w) * zoom - 2, (top + h) * zoom - 2, s, s);
      displayCtx.restore();
    }
  
    


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

  function updateCursor(point) {
    if (!inside(point.x, point.y)) {
      cursorStatus.textContent = 'X: — · Y: —';
      pixelStatus.textContent = 'Pixel: —';
      const rulersCanvas = document.getElementById('rulersCanvas');
      if (rulersCanvas) {
        const ctx = rulersCanvas.getContext('2d');
        ctx.clearRect(0, 0, rulersCanvas.width, rulersCanvas.height);
      }
      return;
    }
    cursorStatus.textContent = `X: ${point.x} · Y: ${point.y}`;
    pixelStatus.textContent = `Pixel: ${hex565(framebuffer[indexFor(point.x, point.y)])}`;
    
    const rulersCanvas = document.getElementById('rulersCanvas');
    if (rulersCanvas) {
      if (rulersCanvas.width !== displayCanvas.width) rulersCanvas.width = displayCanvas.width;
      if (rulersCanvas.height !== displayCanvas.height) rulersCanvas.height = displayCanvas.height;
      const ctx = rulersCanvas.getContext('2d');
      ctx.clearRect(0, 0, rulersCanvas.width, rulersCanvas.height);
      
      const px = (point.x + 0.5) * zoom;
      const py = (point.y + 0.5) * zoom;
      
      ctx.save();
      
      // Ruler ticks
      ctx.fillStyle = '#a4b9c9';
      ctx.strokeStyle = '#a4b9c9';
      ctx.lineWidth = 1;
      ctx.font = '10px monospace';
      
      // Top ruler
      ctx.beginPath();
      ctx.moveTo(0, 14);
      ctx.lineTo(rulersCanvas.width, 14);
      for(let x=0; x<=WIDTH; x+=10) {
        let tickX = x * zoom;
        if(x % 50 === 0) {
          ctx.moveTo(tickX, 0);
          ctx.lineTo(tickX, 14);
          ctx.textAlign = 'center';
          ctx.textBaseline = 'top';
          ctx.fillText(x, tickX, 2);
        } else {
          ctx.moveTo(tickX, 8);
          ctx.lineTo(tickX, 14);
        }
      }
      // Left ruler
      ctx.moveTo(14, 0);
      ctx.lineTo(14, rulersCanvas.height);
      for(let y=0; y<=HEIGHT; y+=10) {
        let tickY = y * zoom;
        if(y % 50 === 0) {
          ctx.moveTo(0, tickY);
          ctx.lineTo(14, tickY);
          ctx.textAlign = 'left';
          ctx.textBaseline = 'middle';
          ctx.fillText(y, 16, tickY);
        } else {
          ctx.moveTo(8, tickY);
          ctx.lineTo(14, tickY);
        }
      }
      ctx.stroke();
      
      // Crosshairs
      ctx.strokeStyle = 'rgba(255, 255, 255, 0.4)';
      ctx.setLineDash([2, 4]);
      ctx.beginPath();
      ctx.moveTo(px, 14);
      ctx.lineTo(px, rulersCanvas.height);
      ctx.moveTo(14, py);
      ctx.lineTo(rulersCanvas.width, py);
      ctx.stroke();
      
      // Highlight on rulers
      ctx.fillStyle = '#fff';
      ctx.fillRect(px - 14, 0, 28, 14);
      ctx.fillRect(0, py - 7, 14, 14);
      ctx.fillStyle = '#000';
      ctx.textAlign = 'center';
      ctx.textBaseline = 'top';
      ctx.fillText(point.x, px, 2);
      ctx.textBaseline = 'middle';
      ctx.fillText(point.y, 7, py);
      
      ctx.restore();
    }
  }

  function syncToolUI() {
    document.querySelectorAll('.tool[data-tool]').forEach((button) => {
      button.classList.toggle('active', button.dataset.tool === selectedTool);
    });

    const field = selectedField();
    const fieldTools = ['textField', 'valueField', 'boolField', 'barField', 'pointer'];
    rawSection.hidden = selectedTool !== 'rawText';
    fieldSection.hidden = !field || !fieldTools.includes(selectedTool);
    rawMetricsSection.hidden = selectedTool !== 'rawText';
    fieldMetricsSection.hidden = !field || !fieldTools.includes(selectedTool);

    if (field) {
      const isValue = field.type === 'VALUE';
      const isShape = ['LINE', 'RECT', 'ELLIPSE', 'TRIANGLE', 'POLYGON'].includes(field.type);
      if (fieldInspectorTitle) fieldInspectorTitle.textContent = `Inspector · ${field.type} field`;
      if (numericFormatDetails) numericFormatDetails.hidden = !isValue;
      if (fieldCapacityWrap) fieldCapacityWrap.hidden = isValue || isShape;
      if (fieldCppType) fieldCppType.value = isValue ? 'float' : (isShape ? 'none' : 'char[]');
      
      if (fieldX2Wrap) fieldX2Wrap.hidden = !isShape;
      if (fieldY2Wrap) fieldY2Wrap.hidden = !isShape;
      if (fieldSidesWrap) fieldSidesWrap.hidden = field.type !== 'POLYGON';
      const fieldValueBoundsWrap = document.getElementById('fieldValueBoundsWrap');
      if (fieldValueBoundsWrap) fieldValueBoundsWrap.hidden = isShape;
      if (fieldMetricsSection) fieldMetricsSection.hidden = isShape || !fieldTools.includes(selectedTool);
    }
  }

  function updateActiveColorUI() {
    activeColorSwatch.style.background = rgb565ToCss(selectedColor.value);
    activeColorName.textContent = selectedColor.name;
    activeColorValue.textContent = hex565(selectedColor.value);
  }

  function buildColorSelect(select, selectedName) {
    select.innerHTML = '';
    COLORS.forEach((color) => {
      const option = document.createElement('option');
      option.value = color.name;
      option.textContent = `${color.name} · ${hex565(color.value)}`;
      option.selected = color.name === selectedName;
      select.appendChild(option);
    });
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
        updateActiveColorUI();
        buildPalette();
        render();
        commitHistory();
      });
      palette.appendChild(button);
    });
  }

  function syncInputsFromState() {
    rawTextInput.value = rawState.value;
    rawTextX.value = String(rawState.x);
    rawTextY.value = String(rawState.y);
    rawTextSize.value = String(rawState.size);
    rawTextBackground.value = colorName(rawState.background);

    const field = selectedField();
    if (!field) {
      inspectorContract.textContent = 'Sin objeto seleccionado';
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
      inspectorContract.textContent = `// Objeto estático: no requiere variable de estado`;
    } else {
      inspectorContract.textContent = `char ${sanitizeSymbol(field.variable, 'texto')}[${field.capacity + 1}] = {};`;
    }
  }

  function captureSnapshot() {
    return {
      fields: hmiFields.map((field) => ({ ...field })),
      pages: hmiPages.map((page) => ({ ...page })),
      activePage,
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
    selectedFieldKey = snapshot.selectedFieldKey;
    selectedTool = snapshot.selectedTool;
    Object.assign(rawState, snapshot.raw);
    pixelLayer.set(snapshot.pixels);
    fieldSerial = snapshot.serial;
    const selected = selectedField();
    if (!selected || Number(selected.page || 0) !== activePage) {
      const replacement = fieldsForPage(activePage)[0] || null;
      selectedFieldKey = replacement?.key || null;
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
    selectedFieldKey = first?.key || null;
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
    selectedFieldKey = null;
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
    selectedFieldKey = field.key;
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
      backgroundColor: 0x0000,
      fill: false,
      size: 1,
      sides: Number(document.getElementById('mainPolySidesInput')?.value || 5)
    };
    hmiFields.push(field);
    selectedFieldKey = field.key;
    syncInputsFromState();
    syncToolUI();
    render();
    commitHistory();
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
    selectedFieldKey = field.key;
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
    selectedFieldKey = field.key;
    selectedTool = 'valueField';
    syncInputsFromState();
    syncToolUI();
    render();
    commitHistory();
  }

  function duplicateSelectedField() {
    const source = selectedField();
    if (!source || hmiFields.length >= MAX_FIELDS) return;
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
    placeNewField(copy);
    hmiFields.push(copy);
    selectedFieldKey = copy.key;
    selectedTool = toolForField(copy);
    syncInputsFromState();
    syncToolUI();
    render();
    commitHistory();
  }

  function deleteSelectedField() {
    const index = hmiFields.findIndex((field) => field.key === selectedFieldKey);
    if (index < 0) return;
    hmiFields.splice(index, 1);
    const replacement = fieldsForPage(activePage)[0] || null;
    selectedFieldKey = replacement?.key || null;
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
    selectedFieldKey = 'text-1';
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
    selectedFieldKey = demo.key;
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
    selectedFieldKey = demo.key;
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
      selectedTool = tool;
      if (!['pointer', 'textField', 'valueField', 'boolField', 'barField'].includes(tool)) {
        selectedFieldKey = null;
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

  bindRawInput(rawTextInput, () => { rawState.value = rawTextInput.value; });
  bindRawInput(rawTextX, () => { rawState.x = clamp(Number(rawTextX.value) || 0, 0, WIDTH - 1); });
  bindRawInput(rawTextY, () => { rawState.y = clamp(Number(rawTextY.value) || 0, 0, HEIGHT - 1); });
  bindRawInput(rawTextSize, () => { rawState.size = Number(rawTextSize.value) || 1; });
  bindRawInput(rawTextBackground, () => { rawState.background = colorByName(rawTextBackground.value).value; });

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

  function hitTestField(point) {
    for (let index = hmiFields.length - 1; index >= 0; index -= 1) {
      const field = hmiFields[index];
      if (Number(field.page || 0) !== activePage) continue;
      
      if (['LINE', 'RECT', 'ELLIPSE', 'TRIANGLE', 'POLYGON'].includes(field.type)) {
        const left = Math.min(field.x, field.x2);
        const right = Math.max(field.x, field.x2);
        const top = Math.min(field.y, field.y2);
        const bottom = Math.max(field.y, field.y2);
        if (point.x >= left && point.x <= right && point.y >= top && point.y <= bottom) return field;
      } else {
        const g = computeFieldGeometry(field);
        if (
          point.x >= field.x && point.x < field.x + g.fieldW &&
          point.y >= field.y && point.y < field.y + g.fieldH
        ) return field;
      }
    }
    return null;
  }

    
displayCanvas.addEventListener('pointerdown', (event) => {
    if (event.button !== 0) return;
    const point = pointFromPointer(event);
    if (!inside(point.x, point.y)) return;
    displayCanvas.setPointerCapture(event.pointerId);
    gestureChanged = false;

    if (selectedTool === 'pixel' || selectedTool === 'erase') {
      drawing = true;
      lastPoint = point;
      const value = selectedTool === 'erase' ? 0x0000 : selectedColor.value;
      gestureChanged = setLayerPixel(point.x, point.y, value);
    } else if (selectedTool === 'rawText') {
      draggingObject = true;
      dragOffset = { x: point.x - rawState.x, y: point.y - rawState.y };
      } else if (['line', 'rect', 'ellipse', 'triangle', 'polygon'].includes(selectedTool)) {
        const type = selectedTool.toUpperCase();
        addShapeField(type, point, point);
        drawing = true; // reusing drawing state for shape creation
      } else {
        const hit = hitTestField(point);
        if (hit) {
          selectedFieldKey = hit.key;
          selectedTool = toolForField(hit);
          draggingObject = true;
          dragOffset = { x: point.x - hit.x, y: point.y - hit.y };
          syncInputsFromState();
          syncToolUI();
        } else {
          selectedFieldKey = null;
          selectedTool = 'pointer';
          syncToolUI();
        }
      }
    render();
  });

  displayCanvas.addEventListener('pointermove', (event) => {
    const point = pointFromPointer(event);
    updateCursor(point);
    if (!inside(point.x, point.y)) return;

    if (drawing) {
      if (['line', 'rect', 'ellipse', 'triangle', 'polygon'].includes(selectedTool) || ['LINE', 'RECT', 'ELLIPSE', 'TRIANGLE', 'POLYGON'].includes(selectedField()?.type)) {
        const field = selectedField();
        if (field) {
          let nextX = point.x;
          let nextY = point.y;
          if (snapToggle && snapToggle.checked) {
            const gridSize = Number(gridSizeSelect.value) || 8;
            nextX = Math.round(nextX / gridSize) * gridSize;
            nextY = Math.round(nextY / gridSize) * gridSize;
          }
          field.x2 = clamp(nextX, 0, WIDTH - 1);
          field.y2 = clamp(nextY, 0, HEIGHT - 1);
          gestureChanged = true;
          syncInputsFromState();
          render();
        }
      } else {
        const value = selectedTool === 'erase' ? 0x0000 : selectedColor.value;
        if (lastPoint) gestureChanged = rasterLine(lastPoint.x, lastPoint.y, point.x, point.y, value) || gestureChanged;
        else gestureChanged = setLayerPixel(point.x, point.y, value) || gestureChanged;
        lastPoint = point;
        render();
      }
    } else if (draggingObject) {
      const isSnap = snapToggle && snapToggle.checked;
      const gridSize = Number(gridSizeSelect.value) || 8;
      
      if (selectedTool === 'rawText') {
        let nextX = point.x - dragOffset.x;
        let nextY = point.y - dragOffset.y;
        if (isSnap) {
          nextX = Math.round(nextX / gridSize) * gridSize;
          nextY = Math.round(nextY / gridSize) * gridSize;
        }
        nextX = clamp(nextX, 0, WIDTH - 1);
        nextY = clamp(nextY, 0, HEIGHT - 1);
        
        gestureChanged = gestureChanged || nextX !== rawState.x || nextY !== rawState.y;
        rawState.x = nextX;
        rawState.y = nextY;
      } else {
        const field = selectedField();
        if (!field || Number(field.page || 0) !== activePage) return;
        
        let nextX = point.x - dragOffset.x;
        let nextY = point.y - dragOffset.y;
        if (isSnap) {
          nextX = Math.round(nextX / gridSize) * gridSize;
          nextY = Math.round(nextY / gridSize) * gridSize;
        }
        
        if (['LINE', 'RECT', 'ELLIPSE', 'TRIANGLE', 'POLYGON'].includes(field.type)) {
          const dx = nextX - field.x;
          const dy = nextY - field.y;
          field.x += dx;
          field.y += dy;
          field.x2 += dx;
          field.y2 += dy;
          gestureChanged = true;
        } else {
          const g = computeFieldGeometry(field);
          nextX = clamp(nextX, 0, Math.max(0, WIDTH - g.fieldW));
          nextY = clamp(nextY, 0, Math.max(0, HEIGHT - g.fieldH));
          gestureChanged = gestureChanged || nextX !== field.x || nextY !== field.y;
          field.x = nextX;
          field.y = nextY;
        }
      }
      syncInputsFromState();
      render();
    }
  });

  function endPointer() {
    const changed = gestureChanged;
    drawing = false;
    draggingObject = false;
    lastPoint = null;
    gestureChanged = false;
    if (changed) commitHistory();
  }

  displayCanvas.addEventListener('pointerup', endPointer);
  displayCanvas.addEventListener('pointercancel', endPointer);
  displayCanvas.addEventListener('pointerleave', () => {
    cursorStatus.textContent = 'X: — · Y: —';
    pixelStatus.textContent = 'Pixel: —';
  });

  function isEditingTarget(target) {
    return target instanceof HTMLInputElement ||
      target instanceof HTMLTextAreaElement ||
      target instanceof HTMLSelectElement ||
      target?.isContentEditable;
  }

  function nudgeSelection(dx, dy) {
    const isSnap = snapToggle && snapToggle.checked;
    const gridSize = Number(gridSizeSelect.value) || 8;
    
    // Scale delta if snapping is enabled
    if (isSnap) {
      dx = Math.sign(dx) * Math.max(Math.abs(dx), gridSize);
      dy = Math.sign(dy) * Math.max(Math.abs(dy), gridSize);
    }

    const field = selectedField();
    if (field && Number(field.page || 0) === activePage && ['textField', 'valueField', 'boolField', 'barField'].includes(selectedTool)) {
      const g = computeFieldGeometry(field);
      let nextX = clamp(field.x + dx, 0, Math.max(0, WIDTH - g.fieldW));
      let nextY = clamp(field.y + dy, 0, Math.max(0, HEIGHT - g.fieldH));
      
      if (isSnap) {
        nextX = Math.round(nextX / gridSize) * gridSize;
        nextY = Math.round(nextY / gridSize) * gridSize;
        nextX = clamp(nextX, 0, Math.max(0, WIDTH - g.fieldW));
        nextY = clamp(nextY, 0, Math.max(0, HEIGHT - g.fieldH));
      }

      if (nextX === field.x && nextY === field.y) return false;
      field.x = nextX;
      field.y = nextY;
      syncInputsFromState();
      render();
      return true;
    }

    if (selectedTool === 'rawText') {
      let nextX = clamp(rawState.x + dx, 0, WIDTH - 1);
      let nextY = clamp(rawState.y + dy, 0, HEIGHT - 1);
      
      if (isSnap) {
        nextX = Math.round(nextX / gridSize) * gridSize;
        nextY = Math.round(nextY / gridSize) * gridSize;
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

  document.addEventListener('keydown', (event) => {
    const ctrl = event.ctrlKey || event.metaKey;
    if (!isEditingTarget(event.target)) {
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
        selectedFieldKey = null;
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

  buildColorSelect(rawTextBackground, 'WHITE');
  buildColorSelect(fieldLabelColor, 'WHITE');
  buildColorSelect(fieldValueColor, 'CYAN');
  buildColorSelect(fieldBackgroundColor, 'BLACK');
  buildColorSelect(fieldFrameColor, 'WHITE');
  buildPalette();
  updateActiveColorUI();
  syncInputsFromState();
  syncToolUI();
  render();
  history.push(captureSnapshot());
  historyIndex = 0;
  updateHistoryButtons();

  window.JWPLCHMIEditor = {
    getSelectedField: () => selectedField(),
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
    undo,
    redo,
    duplicateSelectedField,
    deleteSelectedField,
    addTextField,
    addValueField
  };
})();




// --- Viewport Panning & Vertical Toolbar Wire-up ---

  // --- Viewport Panning (Right-click or Middle-click) and Zoom (Ctrl+Scroll) ---
  const canvasViewport = document.getElementById('canvasViewport');
  let isPanning = false;
  let panStartX = 0;
  let panStartY = 0;
  let viewportScrollLeft = 0;
  let viewportScrollTop = 0;

  if (canvasViewport) {
    canvasViewport.addEventListener('mousedown', (e) => {
      if (e.button === 2 || e.button === 1) {
        isPanning = true;
        panStartX = e.clientX;
        panStartY = e.clientY;
        viewportScrollLeft = canvasViewport.scrollLeft;
        viewportScrollTop = canvasViewport.scrollTop;
        canvasViewport.style.cursor = 'grabbing';
        e.preventDefault();
      }
    });

    window.addEventListener('mousemove', (e) => {
      if (isPanning) {
        const dx = e.clientX - panStartX;
        const dy = e.clientY - panStartY;
        canvasViewport.scrollLeft = viewportScrollLeft - dx;
        canvasViewport.scrollTop = viewportScrollTop - dy;
        e.preventDefault();
      }
    });

    window.addEventListener('mouseup', (e) => {
      if (isPanning && (e.button === 2 || e.button === 1)) {
        isPanning = false;
        canvasViewport.style.cursor = '';
      }
    });

    canvasViewport.addEventListener('contextmenu', (e) => {
      e.preventDefault();
    });

    canvasViewport.addEventListener('wheel', (e) => {
      if (e.ctrlKey) {
        e.preventDefault();
        const sel = document.getElementById('zoomSelect');
        if (!sel) return;
        let idx = sel.selectedIndex;
        if (e.deltaY < 0) {
          idx = Math.min(sel.options.length - 1, idx + 1);
        } else {
          idx = Math.max(0, idx - 1);
        }
        if (idx !== sel.selectedIndex) {
          sel.selectedIndex = idx;
          sel.dispatchEvent(new Event('change', { bubbles: true }));
        }
      }
    }, { passive: false });
  }

  // --- Vertical Toolbar Discrete Buttons Wire-up ---
  document.getElementById('zoomInBtn')?.addEventListener('click', () => {
    const sel = document.getElementById('zoomSelect');
    if (sel && sel.selectedIndex < sel.options.length - 1) {
      sel.selectedIndex += 1;
      sel.dispatchEvent(new Event('change', { bubbles: true }));
    }
  });

  document.getElementById('zoomOutBtn')?.addEventListener('click', () => {
    const sel = document.getElementById('zoomSelect');
    if (sel && sel.selectedIndex > 0) {
      sel.selectedIndex -= 1;
      sel.dispatchEvent(new Event('change', { bubbles: true }));
    }
  });

  document.getElementById('fitCanvasBtn')?.addEventListener('click', () => {
    document.getElementById('fitButton')?.click();
  });

  document.getElementById('vertGridToggle')?.addEventListener('click', function() {
    const toggle = document.getElementById('gridToggle');
    if (toggle) {
      toggle.checked = !toggle.checked;
      this.classList.toggle('active', toggle.checked);
      toggle.dispatchEvent(new Event('change', { bubbles: true }));
    }
  });

  document.getElementById('vertSnapToggle')?.addEventListener('click', function() {
    const toggle = document.getElementById('snapToggle');
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
