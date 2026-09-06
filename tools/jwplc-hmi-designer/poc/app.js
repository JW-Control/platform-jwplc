(() => {
  'use strict';

  const WIDTH = 320;
  const HEIGHT = 170;
  const FIELD_PADDING = 3;
  const FIELD_GAP = 4;
  const MAX_FIELDS = 32;
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
  const previewCtx = previewCanvas.getContext('2d', { alpha: false });
  const logicalCanvas = document.createElement('canvas');
  logicalCanvas.width = WIDTH;
  logicalCanvas.height = HEIGHT;
  const logicalCtx = logicalCanvas.getContext('2d', { alpha: false });

  const zoomSelect = document.getElementById('zoomSelect');
  const gridToggle = document.getElementById('gridToggle');
  const clearButton = document.getElementById('clearButton');
  const newProjectButton = document.getElementById('newProjectButton');
  const demoButton = document.getElementById('demoButton');
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

  const rawState = {
    x: 20,
    y: 20,
    size: 2,
    value: 'TEMP: 25.6 C',
    foreground: 0xF800,
    background: 0xFFFF
  };

  function defaultField(key = 'text-1') {
    return {
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

  let textFields = [defaultField()];

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

  function selectedField() {
    return textFields.find((field) => field.key === selectedFieldKey) || null;
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

  function computeTextFieldGeometry(field) {
    if (!field) return null;
    const pad = effectiveFieldPadding(field);
    const labelBounds = nominalTextBounds(field.label, field.labelSize);
    const unitBounds = nominalTextBounds(field.unit, field.labelSize);
    const valueBounds = nominalTextBounds('W'.repeat(Math.max(1, field.capacity)), field.valueSize);
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
      pad, fieldX: field.x, fieldY: field.y, fieldW, fieldH,
      valueX, valueY, valueW: valueBounds.width, valueH: valueBounds.height,
      labelBounds, unitBounds
    };
  }

  function alignedValueX(field, geometry) {
    const current = nominalTextBounds(field.preview, field.valueSize).width;
    if (current >= geometry.valueW) return geometry.valueX;
    const free = geometry.valueW - current;
    if (field.align === 'CENTER') return geometry.valueX + Math.floor(free / 2);
    if (field.align === 'RIGHT') return geometry.valueX + free;
    return geometry.valueX;
  }

  function drawTextField(buffer, field) {
    const g = computeTextFieldGeometry(field);
    fillBufferRect(buffer, g.fieldX, g.fieldY, g.fieldW, g.fieldH, field.backgroundColor);
    if (field.frame && g.fieldW > 1 && g.fieldH > 1) drawBufferRect(buffer, g.fieldX, g.fieldY, g.fieldW, g.fieldH, field.frameColor);
    if (field.label) drawClassicTextAt(buffer, field.label, field.x + g.pad, field.y + g.pad, field.labelColor, field.backgroundColor, field.labelSize);
    if (field.unit) drawClassicTextAt(buffer, field.unit, g.valueX + g.valueW + FIELD_GAP, g.valueY, field.labelColor, field.backgroundColor, field.labelSize);
    const preview = field.preview.slice(0, field.capacity);
    if (preview) drawClassicTextAt(buffer, preview, alignedValueX(field, g), g.valueY, field.valueColor, field.backgroundColor, field.valueSize);
    return g;
  }

  function composeFramebuffer() {
    framebuffer.set(pixelLayer);
    textFields.forEach((field) => drawTextField(framebuffer, field));
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
      bytes[offset] = r; bytes[offset + 1] = g; bytes[offset + 2] = b; bytes[offset + 3] = 255;
    }
    logicalCtx.putImageData(image, 0, 0);
  }

  function drawGrid() {
    if (!gridToggle.checked || zoom < 3) return;
    displayCtx.save();
    displayCtx.strokeStyle = 'rgba(118, 151, 176, 0.18)';
    displayCtx.lineWidth = 1;
    displayCtx.beginPath();
    for (let x = 0; x <= WIDTH; x += 1) { const px = x * zoom + 0.5; displayCtx.moveTo(px, 0); displayCtx.lineTo(px, HEIGHT * zoom); }
    for (let y = 0; y <= HEIGHT; y += 1) { const py = y * zoom + 0.5; displayCtx.moveTo(0, py); displayCtx.lineTo(WIDTH * zoom, py); }
    displayCtx.stroke();
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

  function fieldContract(field, index) {
    const id = sanitizeSymbol(field.id, `FIELD_TEXT_${index + 1}`);
    const layout = `JWPLC_UI_LAYOUT_${field.layout}`;
    const align = `JWPLC_UI_ALIGN_${field.align}`;
    const frame = field.frame ? 'true' : 'false';
    const label = field.label ? `"${cppString(field.label)}"` : 'nullptr';
    const unit = field.unit ? `"${cppString(field.unit)}"` : 'nullptr';
    return `    JWPLC_UITextField(\n        ${id},\n        JWPLC_UIRect(${field.x}, ${field.y}),\n        JWPLC_UIText(${label}, ${unit}, ${field.capacity}),\n        JWPLC_UITextFieldStyle(\n            ${field.valueSize},\n            ${field.labelSize},\n            ${frame},\n            ${layout},\n            ${align}),\n        ${field.page},\n        JWPLC_UIColors(\n            ${colorName(field.labelColor)},\n            ${colorName(field.valueColor)},\n            ${colorName(field.backgroundColor)},\n            ${colorName(field.frameColor)}))`;
  }

  function buildContractText() {
    if (textFields.length === 0) return '// Sin campos HMI. Agrega un componente TEXT para comenzar.';
    const enumLines = textFields.map((field, index) => `    ${sanitizeSymbol(field.id, `FIELD_TEXT_${index + 1}`)} = ${index + 1}`).join(',\n');
    const variables = textFields.map((field, index) => `char ${sanitizeSymbol(field.variable, `texto${index + 1}`)}[${Math.max(1, field.capacity) + 1}] = {};`).join('\n');
    const fields = textFields.map(fieldContract).join(',\n');
    return `// Contrato generado por el Designer (A11 UX-4)\n\nenum HMIFieldId : uint8_t\n{\n${enumLines}\n};\n\n${variables}\n\nstatic const JWPLC_UIField HMI_FIELDS[] =\n{\n${fields}\n};\n\nvoid jwplcHMISetup()\n{\n    JWPLC_Display.setFields(\n        HMI_FIELDS,\n        sizeof(HMI_FIELDS) / sizeof(HMI_FIELDS[0]));\n}\n\n// jwplcUIUpdate() NO se genera.\n// El usuario alimenta las variables declaradas y llama los setters públicos.`;
  }

  function buildStatusText() {
    const field = selectedField();
    const g = field ? computeTextFieldGeometry(field) : null;
    return `A11 UX Foundation: PASS visual\nA11 UX-4 Edición: IN_PROGRESS\n\n- Undo/Redo: ${historyIndex > 0 ? 'disponible' : 'sin historial previo'}\n- TEXT fields: ${textFields.length}/${MAX_FIELDS}\n- Selección: ${field ? `${field.name} · ${field.id}` : 'ninguna'}\n- Movimiento: flechas 1 px / Shift+flechas 10 px\n- Duplicar: Ctrl+D\n- Eliminar: Delete\n${g ? `- AUTO field seleccionado: ${g.fieldW} × ${g.fieldH} px\n- X/Y: ${field.x}, ${field.y}\n- effectivePadding: ${g.pad} px` : ''}\n\nVALUE/BOOL/BAR siguen bloqueados hasta cerrar UX-4.`;
  }

  function updateMetrics() {
    if (selectedTool === 'rawText') {
      const width = rawState.value ? rawState.value.length * 6 * rawState.size : 0;
      const height = rawState.value ? 8 * rawState.size : 0;
      rawBoundsStatus.textContent = `${width} × ${height} px`;
      return;
    }
    const field = selectedField();
    if (selectedTool === 'textField' && field) {
      const g = computeTextFieldGeometry(field);
      fieldPadStatus.textContent = `${g.pad} px`;
      fieldBoundsStatus.textContent = `${g.fieldW} × ${g.fieldH} px`;
      fieldValueBoundsStatus.textContent = `${g.valueW} × ${g.valueH} px`;
      fieldValueXYStatus.textContent = `${g.valueX}, ${g.valueY}`;
      fieldLayoutStatus.textContent = field.layout;
    }
  }

  function updateCodePanel() {
    codeOutput.textContent = codeMode === 'contract' ? buildContractText() : buildStatusText();
  }

  function renderObjectList() {
    objectList.querySelectorAll('.object-item').forEach((item) => item.remove());
    textFields.forEach((field, index) => {
      const button = document.createElement('button');
      button.className = `object-item${field.key === selectedFieldKey && selectedTool === 'textField' ? ' active' : ''}`;
      button.type = 'button';
      button.dataset.fieldKey = field.key;
      button.title = `${field.name} · ${field.id}`;
      button.innerHTML = `<span class="object-icon">T</span><span class="object-type">TEXT</span><span class="object-name"></span><span class="object-id"></span><span class="object-eye">●</span>`;
      button.querySelector('.object-name').textContent = field.name || `TEXT ${index + 1}`;
      button.querySelector('.object-id').textContent = field.id || `FIELD_TEXT_${index + 1}`;
      button.addEventListener('click', () => {
        selectedFieldKey = field.key;
        selectedTool = 'textField';
        syncInputsFromState();
        syncToolUI();
        render();
      });
      objectList.appendChild(button);
    });
    countBadge.textContent = String(textFields.length);
    if (fieldsStatus) fieldsStatus.textContent = `Campos: ${textFields.length}/${MAX_FIELDS}`;
  }

  function updateHistoryButtons() {
    if (undoButton) undoButton.disabled = historyIndex <= 0;
    if (redoButton) redoButton.disabled = historyIndex < 0 || historyIndex >= history.length - 1;
    duplicateButton.disabled = !selectedField() || textFields.length >= MAX_FIELDS;
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
    previewCtx.imageSmoothingEnabled = false;
    previewCtx.clearRect(0, 0, WIDTH, HEIGHT);
    previewCtx.drawImage(logicalCanvas, 0, 0);
    renderObjectList();
    updateMetrics();
    updateCodePanel();
    updateHistoryButtons();
    window.dispatchEvent(new CustomEvent('jwplc:editor-refresh', {
      detail: { selectedTool, hasTextSelection: Boolean(selectedField()) }
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
      return;
    }
    cursorStatus.textContent = `X: ${point.x} · Y: ${point.y}`;
    pixelStatus.textContent = `Pixel: ${hex565(framebuffer[indexFor(point.x, point.y)])}`;
  }

  function syncToolUI() {
    document.querySelectorAll('.tool[data-tool]').forEach((button) => button.classList.toggle('active', button.dataset.tool === selectedTool));
    rawSection.hidden = selectedTool !== 'rawText';
    fieldSection.hidden = selectedTool !== 'textField' || !selectedField();
    rawMetricsSection.hidden = selectedTool !== 'rawText';
    fieldMetricsSection.hidden = selectedTool !== 'textField' || !selectedField();
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
    fieldName.value = field.name;
    fieldId.value = field.id;
    fieldVariable.value = field.variable;
    fieldCapacity.value = String(field.capacity);
    fieldX.value = String(field.x);
    fieldY.value = String(field.y);
    fieldPreview.value = field.preview;
    fieldLabel.value = field.label;
    fieldUnit.value = field.unit;
    fieldValueSize.value = String(field.valueSize);
    fieldLabelSize.value = String(field.labelSize);
    fieldFrame.value = field.frame ? '1' : '0';
    fieldLayout.value = field.layout;
    fieldAlign.value = field.align;
    fieldLabelColor.value = colorName(field.labelColor);
    fieldValueColor.value = colorName(field.valueColor);
    fieldBackgroundColor.value = colorName(field.backgroundColor);
    fieldFrameColor.value = colorName(field.frameColor);
    inspectorContract.textContent = `char ${sanitizeSymbol(field.variable, 'estadoTexto')}[${field.capacity + 1}];`;
  }

  function captureSnapshot() {
    return {
      fields: textFields.map((field) => ({ ...field })),
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
    if (a.selectedFieldKey !== b.selectedFieldKey || a.selectedTool !== b.selectedTool || a.serial !== b.serial) return false;
    if (JSON.stringify(a.fields) !== JSON.stringify(b.fields)) return false;
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
    textFields = snapshot.fields.map((field) => ({ ...field }));
    selectedFieldKey = snapshot.selectedFieldKey;
    selectedTool = snapshot.selectedTool;
    Object.assign(rawState, snapshot.raw);
    pixelLayer.set(snapshot.pixels);
    fieldSerial = snapshot.serial;
    if (selectedFieldKey && !selectedField()) selectedFieldKey = textFields[0]?.key || null;
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
    const used = new Set(textFields.map((field) => field.id));
    let candidate = base;
    let suffix = 2;
    while (used.has(candidate)) { candidate = `${base}_${suffix}`; suffix += 1; }
    return candidate;
  }

  function uniqueVariable(base) {
    const used = new Set(textFields.map((field) => field.variable));
    let candidate = base;
    let suffix = 2;
    while (used.has(candidate)) { candidate = `${base}${suffix}`; suffix += 1; }
    return candidate;
  }

  function addTextField() {
    if (textFields.length >= MAX_FIELDS) return;
    fieldSerial += 1;
    const field = defaultField(`text-${fieldSerial}`);
    field.name = `Texto ${fieldSerial}`;
    field.id = uniqueFieldSymbol(`FIELD_TEXT_${fieldSerial}`);
    field.variable = uniqueVariable(`texto${fieldSerial}`);
    field.label = field.name;
    field.preview = 'READY';
    field.x = 20 + ((fieldSerial - 1) * 8) % 80;
    field.y = 20 + ((fieldSerial - 1) * 8) % 60;
    textFields.push(field);
    selectedFieldKey = field.key;
    selectedTool = 'textField';
    syncInputsFromState();
    syncToolUI();
    render();
    commitHistory();
  }

  function duplicateSelectedField() {
    const source = selectedField();
    if (!source || textFields.length >= MAX_FIELDS) return;
    fieldSerial += 1;
    const copy = { ...source };
    copy.key = `text-${fieldSerial}`;
    copy.name = `${source.name || 'TEXT'} copia`;
    copy.id = uniqueFieldSymbol(`${sanitizeSymbol(source.id, 'FIELD_TEXT')}_COPY`);
    copy.variable = uniqueVariable(`${sanitizeSymbol(source.variable, 'texto')}Copy`);
    copy.x = clamp(source.x + 8, 0, WIDTH - 1);
    copy.y = clamp(source.y + 8, 0, HEIGHT - 1);
    const g = computeTextFieldGeometry(copy);
    copy.x = clamp(copy.x, 0, Math.max(0, WIDTH - g.fieldW));
    copy.y = clamp(copy.y, 0, Math.max(0, HEIGHT - g.fieldH));
    textFields.push(copy);
    selectedFieldKey = copy.key;
    selectedTool = 'textField';
    syncInputsFromState();
    syncToolUI();
    render();
    commitHistory();
  }

  function deleteSelectedField() {
    const index = textFields.findIndex((field) => field.key === selectedFieldKey);
    if (index < 0) return;
    textFields.splice(index, 1);
    const replacement = textFields[Math.min(index, textFields.length - 1)] || null;
    selectedFieldKey = replacement ? replacement.key : null;
    selectedTool = replacement ? 'textField' : 'none';
    syncInputsFromState();
    syncToolUI();
    render();
    commitHistory();
  }

  function resetProject() {
    pixelLayer.fill(0x0000);
    fieldSerial = 1;
    textFields = [defaultField('text-1')];
    selectedFieldKey = 'text-1';
    selectedTool = 'textField';
    syncInputsFromState();
    syncToolUI();
    render();
    commitHistory();
  }

  function demoTextField() {
    pixelLayer.fill(0x0000);
    const demo = defaultField('text-1');
    Object.assign(demo, {
      name: 'Estado de máquina', id: 'FIELD_STATUS', variable: 'estadoTexto', capacity: 12,
      x: 18, y: 28, preview: 'PRODUCCION', label: 'Estado', valueSize: 2, labelSize: 1,
      frame: true, layout: 'STACKED', align: 'CENTER', labelColor: 0xFFFF,
      valueColor: 0x07E0, backgroundColor: 0x0000, frameColor: 0xFD20
    });
    fieldSerial = 1;
    textFields = [demo];
    selectedFieldKey = demo.key;
    selectedTool = 'textField';
    syncInputsFromState();
    syncToolUI();
    render();
    commitHistory();
  }

  function bindRawInput(element, handler) {
    element.addEventListener('input', () => { handler(); render(); });
    element.addEventListener('change', () => { handler(); render(); commitHistory(); });
  }

  function bindFieldInput(element, handler) {
    element.addEventListener('input', () => {
      const field = selectedField();
      if (!field) return;
      handler(field);
      syncInputsFromState();
      render();
    });
    element.addEventListener('change', () => {
      const field = selectedField();
      if (!field) return;
      handler(field);
      syncInputsFromState();
      render();
      commitHistory();
    });
  }

  document.querySelectorAll('.tool[data-tool]').forEach((button) => {
    button.addEventListener('click', () => {
      const tool = button.dataset.tool;
      if (tool === 'textField' && !selectedField()) addTextField();
      else {
        selectedTool = tool;
        syncToolUI();
        render();
      }
    });
  });

  zoomSelect.addEventListener('change', () => { zoom = Number(zoomSelect.value); render(); });
  gridToggle.addEventListener('change', render);
  newProjectButton.addEventListener('click', resetProject);
  demoButton.addEventListener('click', demoTextField);
  clearButton.addEventListener('click', () => {
    pixelLayer.fill(0x0000);
    if (selectedTool === 'rawText') rawState.value = '';
    const field = selectedField();
    if (field && selectedTool === 'textField') field.preview = '';
    syncInputsFromState(); render(); commitHistory();
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
    field.capacity = clamp(Number(fieldCapacity.value) || 1, 1, 39);
    field.preview = field.preview.slice(0, field.capacity);
  });
  bindFieldInput(fieldX, (field) => { field.x = clamp(Number(fieldX.value) || 0, 0, WIDTH - 1); });
  bindFieldInput(fieldY, (field) => { field.y = clamp(Number(fieldY.value) || 0, 0, HEIGHT - 1); });
  bindFieldInput(fieldPreview, (field) => { field.preview = fieldPreview.value.slice(0, field.capacity); });
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

  statusTab.addEventListener('click', () => {
    codeMode = 'status'; statusTab.classList.add('active'); contractTab.classList.remove('active'); updateCodePanel();
  });
  contractTab.addEventListener('click', () => {
    codeMode = 'contract'; contractTab.classList.add('active'); statusTab.classList.remove('active'); updateCodePanel();
  });

  function hitTestTextField(point) {
    for (let index = textFields.length - 1; index >= 0; index -= 1) {
      const field = textFields[index];
      const g = computeTextFieldGeometry(field);
      if (point.x >= field.x && point.x < field.x + g.fieldW && point.y >= field.y && point.y < field.y + g.fieldH) return field;
    }
    return null;
  }

  displayCanvas.addEventListener('pointerdown', (event) => {
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
    } else {
      const hit = hitTestTextField(point);
      if (hit) {
        selectedTool = 'textField';
        selectedFieldKey = hit.key;
        draggingObject = true;
        dragOffset = { x: point.x - hit.x, y: point.y - hit.y };
        syncInputsFromState();
        syncToolUI();
      } else {
        selectedFieldKey = null;
        selectedTool = 'none';
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
      const value = selectedTool === 'erase' ? 0x0000 : selectedColor.value;
      if (lastPoint) gestureChanged = rasterLine(lastPoint.x, lastPoint.y, point.x, point.y, value) || gestureChanged;
      else gestureChanged = setLayerPixel(point.x, point.y, value) || gestureChanged;
      lastPoint = point;
      render();
    } else if (draggingObject) {
      if (selectedTool === 'rawText') {
        const nextX = clamp(point.x - dragOffset.x, 0, WIDTH - 1);
        const nextY = clamp(point.y - dragOffset.y, 0, HEIGHT - 1);
        gestureChanged = gestureChanged || nextX !== rawState.x || nextY !== rawState.y;
        rawState.x = nextX; rawState.y = nextY;
      } else {
        const field = selectedField();
        if (!field) return;
        const g = computeTextFieldGeometry(field);
        const nextX = clamp(point.x - dragOffset.x, 0, Math.max(0, WIDTH - g.fieldW));
        const nextY = clamp(point.y - dragOffset.y, 0, Math.max(0, HEIGHT - g.fieldH));
        gestureChanged = gestureChanged || nextX !== field.x || nextY !== field.y;
        field.x = nextX; field.y = nextY;
      }
      syncInputsFromState();
      render();
    }
  });

  function endPointer() {
    const changed = gestureChanged;
    drawing = false; draggingObject = false; lastPoint = null; gestureChanged = false;
    if (changed) commitHistory();
  }
  displayCanvas.addEventListener('pointerup', endPointer);
  displayCanvas.addEventListener('pointercancel', endPointer);
  displayCanvas.addEventListener('pointerleave', () => {
    cursorStatus.textContent = 'X: — · Y: —';
    pixelStatus.textContent = 'Pixel: —';
  });

  function isEditingTarget(target) {
    return target instanceof HTMLInputElement || target instanceof HTMLTextAreaElement || target instanceof HTMLSelectElement || target?.isContentEditable;
  }

  function nudgeSelection(dx, dy) {
    const field = selectedField();
    if (selectedTool === 'textField' && field) {
      const g = computeTextFieldGeometry(field);
      const nextX = clamp(field.x + dx, 0, Math.max(0, WIDTH - g.fieldW));
      const nextY = clamp(field.y + dy, 0, Math.max(0, HEIGHT - g.fieldH));
      if (nextX === field.x && nextY === field.y) return false;
      field.x = nextX; field.y = nextY;
      syncInputsFromState(); render(); return true;
    }
    if (selectedTool === 'rawText') {
      const nextX = clamp(rawState.x + dx, 0, WIDTH - 1);
      const nextY = clamp(rawState.y + dy, 0, HEIGHT - 1);
      if (nextX === rawState.x && nextY === rawState.y) return false;
      rawState.x = nextX; rawState.y = nextY;
      syncInputsFromState(); render(); return true;
    }
    return false;
  }

  document.addEventListener('keydown', (event) => {
    const ctrl = event.ctrlKey || event.metaKey;
    if (!isEditingTarget(event.target)) {
      if (ctrl && event.key.toLowerCase() === 'z') {
        event.preventDefault();
        if (event.shiftKey) redo(); else undo();
        return;
      }
      if (ctrl && event.key.toLowerCase() === 'y') { event.preventDefault(); redo(); return; }
      if (ctrl && event.key.toLowerCase() === 'd') { event.preventDefault(); duplicateSelectedField(); return; }
      if (event.key === 'Delete' || event.key === 'Backspace') { event.preventDefault(); deleteSelectedField(); return; }
      if (event.key === 'Escape') {
        selectedFieldKey = null; selectedTool = 'none'; syncToolUI(); render(); return;
      }
      const step = event.shiftKey ? 10 : 1;
      const delta = {
        ArrowLeft: [-step, 0], ArrowRight: [step, 0], ArrowUp: [0, -step], ArrowDown: [0, step]
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
    computeSelectedGeometry: () => computeTextFieldGeometry(selectedField()),
    hasTextSelection: () => Boolean(selectedField()) && selectedTool === 'textField',
    undo,
    redo,
    duplicateSelectedField,
    deleteSelectedField,
    addTextField
  };
})();