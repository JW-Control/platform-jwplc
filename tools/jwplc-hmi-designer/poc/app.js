(() => {
  'use strict';

  const WIDTH = 320;
  const HEIGHT = 170;
  const FIELD_PADDING = 3;
  const FIELD_GAP = 4;

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

  const statusTab = document.getElementById('statusTab');
  const contractTab = document.getElementById('contractTab');
  const codeOutput = document.getElementById('codeOutput');

  let zoom = Number(zoomSelect.value);
  let selectedColor = COLORS.find((color) => color.name === 'ORANGE');
  let selectedTool = 'textField';
  let drawing = false;
  let draggingObject = false;
  let lastPoint = null;
  let codeMode = 'status';

  const rawState = {
    x: 20,
    y: 20,
    size: 2,
    value: 'TEMP: 25.6 C',
    foreground: 0xF800,
    background: 0xFFFF
  };

  const fieldState = {
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

  function indexFor(x, y) {
    return y * WIDTH + x;
  }

  function inside(x, y) {
    return x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT;
  }

  function setLayerPixel(x, y, value) {
    if (!inside(x, y)) return false;
    const index = indexFor(x, y);
    if (pixelLayer[index] === value) return false;
    pixelLayer[index] = value;
    return true;
  }

  function setBufferPixel(buffer, x, y, value) {
    if (!inside(x, y)) return;
    buffer[indexFor(x, y)] = value;
  }

  function fillBufferRect(buffer, x, y, width, height, value) {
    const x0 = Math.max(0, x);
    const y0 = Math.max(0, y);
    const x1 = Math.min(WIDTH, x + width);
    const y1 = Math.min(HEIGHT, y + height);
    for (let py = y0; py < y1; py += 1) {
      for (let px = x0; px < x1; px += 1) {
        setBufferPixel(buffer, px, py, value);
      }
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
      if (e2 >= dy) {
        error += dy;
        x0 += sx;
      }
      if (e2 <= dx) {
        error += dx;
        y0 += sy;
      }
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
        fillBufferRect(
          buffer,
          x + column * scale,
          y + row * scale,
          scale,
          scale,
          on ? foreground : background
        );
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
      if (character === '\n') {
        cursorX = x;
        cursorY += font.cellHeight * scale;
        continue;
      }
      if (character === '\r') continue;
      drawClassicChar(buffer, cursorX, cursorY, character.codePointAt(0), foreground, background, scale);
      cursorX += font.cellWidth * scale;
    }
  }

  // Equivale al textBounds() corregido del runtime Alpha11:
  // getTextBounds() 6x8 menos una escala en ancho/alto => cuerpo nominal 5x7.
  function nominalTextBounds(text, size) {
    if (!text) return { width: 0, height: 0 };
    const scale = Math.max(1, Math.trunc(size));
    return {
      width: text.length * 6 * scale - scale,
      height: 7 * scale
    };
  }

  function effectiveFieldPadding() {
    return Math.max(FIELD_PADDING, fieldState.labelSize || 1, fieldState.valueSize || 1);
  }

  function computeTextFieldGeometry() {
    const pad = effectiveFieldPadding();
    const labelBounds = nominalTextBounds(fieldState.label, fieldState.labelSize);
    const unitBounds = nominalTextBounds(fieldState.unit, fieldState.labelSize);
    const sample = 'W'.repeat(Math.max(1, fieldState.capacity));
    const valueBounds = nominalTextBounds(sample, fieldState.valueSize);

    let fieldW;
    let fieldH;
    let valueX;
    let valueY;

    if (fieldState.layout === 'STACKED') {
      const valueAndUnitW = valueBounds.width + (unitBounds.width > 0 ? FIELD_GAP + unitBounds.width : 0);
      fieldW = 2 * pad + Math.max(labelBounds.width, valueAndUnitW);
      fieldH = 2 * pad + labelBounds.height + (labelBounds.height > 0 ? FIELD_GAP : 0) + Math.max(valueBounds.height, unitBounds.height);
      valueX = fieldState.x + pad;
      valueY = fieldState.y + pad + labelBounds.height + (labelBounds.height > 0 ? FIELD_GAP : 0);
    } else {
      fieldW = 2 * pad + labelBounds.width + (labelBounds.width > 0 ? FIELD_GAP : 0) + valueBounds.width + (unitBounds.width > 0 ? FIELD_GAP + unitBounds.width : 0);
      fieldH = 2 * pad + Math.max(labelBounds.height, valueBounds.height, unitBounds.height);
      valueX = fieldState.x + pad + labelBounds.width + (labelBounds.width > 0 ? FIELD_GAP : 0);
      valueY = fieldState.y + pad;
    }

    return {
      pad,
      fieldX: fieldState.x,
      fieldY: fieldState.y,
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

  function alignedValueX(geometry) {
    const current = nominalTextBounds(fieldState.preview, fieldState.valueSize).width;
    if (current >= geometry.valueW) return geometry.valueX;
    const free = geometry.valueW - current;
    if (fieldState.align === 'CENTER') return geometry.valueX + Math.floor(free / 2);
    if (fieldState.align === 'RIGHT') return geometry.valueX + free;
    return geometry.valueX;
  }

  function drawTextField(buffer) {
    const g = computeTextFieldGeometry();
    fillBufferRect(buffer, g.fieldX, g.fieldY, g.fieldW, g.fieldH, fieldState.backgroundColor);
    if (fieldState.frame && g.fieldW > 1 && g.fieldH > 1) {
      drawBufferRect(buffer, g.fieldX, g.fieldY, g.fieldW, g.fieldH, fieldState.frameColor);
    }

    if (fieldState.label) {
      drawClassicTextAt(
        buffer,
        fieldState.label,
        fieldState.x + g.pad,
        fieldState.y + g.pad,
        fieldState.labelColor,
        fieldState.backgroundColor,
        fieldState.labelSize
      );
    }

    if (fieldState.unit) {
      drawClassicTextAt(
        buffer,
        fieldState.unit,
        g.valueX + g.valueW + FIELD_GAP,
        g.valueY,
        fieldState.labelColor,
        fieldState.backgroundColor,
        fieldState.labelSize
      );
    }

    const preview = fieldState.preview.slice(0, fieldState.capacity);
    if (preview) {
      drawClassicTextAt(
        buffer,
        preview,
        alignedValueX(g),
        g.valueY,
        fieldState.valueColor,
        fieldState.backgroundColor,
        fieldState.valueSize
      );
    }

    return g;
  }

  function composeFramebuffer() {
    framebuffer.set(pixelLayer);
    if (selectedTool === 'rawText') {
      drawClassicTextAt(
        framebuffer,
        rawState.value,
        rawState.x,
        rawState.y,
        rawState.foreground,
        rawState.background,
        rawState.size
      );
    } else if (selectedTool === 'textField') {
      drawTextField(framebuffer);
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
    if (!gridToggle.checked || zoom < 3) return;
    displayCtx.save();
    displayCtx.strokeStyle = 'rgba(118, 151, 176, 0.18)';
    displayCtx.lineWidth = 1;
    displayCtx.beginPath();
    for (let x = 0; x <= WIDTH; x += 1) {
      const px = x * zoom + 0.5;
      displayCtx.moveTo(px, 0);
      displayCtx.lineTo(px, HEIGHT * zoom);
    }
    for (let y = 0; y <= HEIGHT; y += 1) {
      const py = y * zoom + 0.5;
      displayCtx.moveTo(0, py);
      displayCtx.lineTo(WIDTH * zoom, py);
    }
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

  function buildContractText() {
    const id = sanitizeSymbol(fieldState.id, 'FIELD_STATUS');
    const variable = sanitizeSymbol(fieldState.variable, 'estadoTexto');
    const capacity = Math.max(1, fieldState.capacity);
    const layout = `JWPLC_UI_LAYOUT_${fieldState.layout}`;
    const align = `JWPLC_UI_ALIGN_${fieldState.align}`;
    const frame = fieldState.frame ? 'true' : 'false';
    const label = fieldState.label ? `"${cppString(fieldState.label)}"` : 'nullptr';
    const unit = fieldState.unit ? `"${cppString(fieldState.unit)}"` : 'nullptr';

    return `// Contrato generado por el Designer (A11-3A)\n\nenum HMIFieldId : uint8_t\n{\n    ${id} = 1\n};\n\nchar ${variable}[${capacity + 1}] = {};\n\nstatic const JWPLC_UIField HMI_FIELDS[] =\n{\n    JWPLC_UITextField(\n        ${id},\n        JWPLC_UIRect(${fieldState.x}, ${fieldState.y}),\n        JWPLC_UIText(${label}, ${unit}, ${capacity}),\n        JWPLC_UITextFieldStyle(\n            ${fieldState.valueSize},\n            ${fieldState.labelSize},\n            ${frame},\n            ${layout},\n            ${align}),\n        ${fieldState.page},\n        JWPLC_UIColors(\n            ${colorName(fieldState.labelColor)},\n            ${colorName(fieldState.valueColor)},\n            ${colorName(fieldState.backgroundColor)},\n            ${colorName(fieldState.frameColor)}))\n};\n\nvoid jwplcHMISetup()\n{\n    JWPLC_Display.setFields(\n        HMI_FIELDS,\n        sizeof(HMI_FIELDS) / sizeof(HMI_FIELDS[0]));\n}\n\n// jwplcUIUpdate() NO se genera.\n// El usuario alimenta ${variable} y usa JWPLC_Display.setText(${id}, ${variable});`;
  }

  function buildStatusText() {
    const g = computeTextFieldGeometry();
    return `A11-0 Arquitectura: PASS\nA11-1 Pixel Canvas: PASS\nA11-2 Texto / métricas balanceadas source: PASS\n\nA11-3A TEXT field: IN_PROGRESS\n- API objetivo: JWPLC_UITextField(...)\n- runtime duplicado: NO\n- métrica layout: 5×7 nominal\n- raster: GFX clásico 6×8\n- FIELD_PADDING mínimo: 3 px\n- effectivePadding: ${g.pad} px\n- FIELD_GAP: 4 px\n- AUTO field: ${g.fieldW} × ${g.fieldH} px\n- value region: ${g.valueW} × ${g.valueH} px\n- variable: ${sanitizeSymbol(fieldState.variable, 'estadoTexto')}[${fieldState.capacity + 1}]\n\nPendiente de A11-3A: validar Designer ↔ TFT usando JWPLC_UITextField.`;
  }

  function updateMetrics() {
    if (selectedTool === 'rawText') {
      const width = rawState.value ? rawState.value.length * 6 * rawState.size : 0;
      const height = rawState.value ? 8 * rawState.size : 0;
      rawBoundsStatus.textContent = `${width} × ${height} px`;
      return;
    }

    if (selectedTool === 'textField') {
      const g = computeTextFieldGeometry();
      fieldPadStatus.textContent = `${g.pad} px`;
      fieldBoundsStatus.textContent = `${g.fieldW} × ${g.fieldH} px`;
      fieldValueBoundsStatus.textContent = `${g.valueW} × ${g.valueH} px`;
      fieldValueXYStatus.textContent = `${g.valueX}, ${g.valueY}`;
      fieldLayoutStatus.textContent = fieldState.layout;
    }
  }

  function updateCodePanel() {
    codeOutput.textContent = codeMode === 'contract' ? buildContractText() : buildStatusText();
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

    updateMetrics();
    updateCodePanel();
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
    document.querySelectorAll('.tool[data-tool]').forEach((button) => {
      button.classList.toggle('active', button.dataset.tool === selectedTool);
    });
    rawSection.hidden = selectedTool !== 'rawText';
    fieldSection.hidden = selectedTool !== 'textField';
    rawMetricsSection.hidden = selectedTool !== 'rawText';
    fieldMetricsSection.hidden = selectedTool !== 'textField';
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
      button.addEventListener('click', () => {
        selectedColor = color;
        if (selectedTool === 'rawText') rawState.foreground = color.value;
        updateActiveColorUI();
        buildPalette();
        render();
      });
      button.classList.toggle('active', selectedColor.name === color.name);
      palette.appendChild(button);
    });
  }

  function syncInputsFromState() {
    rawTextInput.value = rawState.value;
    rawTextX.value = String(rawState.x);
    rawTextY.value = String(rawState.y);
    rawTextSize.value = String(rawState.size);
    rawTextBackground.value = colorName(rawState.background);

    fieldName.value = fieldState.name;
    fieldId.value = fieldState.id;
    fieldVariable.value = fieldState.variable;
    fieldCapacity.value = String(fieldState.capacity);
    fieldX.value = String(fieldState.x);
    fieldY.value = String(fieldState.y);
    fieldPreview.value = fieldState.preview;
    fieldLabel.value = fieldState.label;
    fieldUnit.value = fieldState.unit;
    fieldValueSize.value = String(fieldState.valueSize);
    fieldLabelSize.value = String(fieldState.labelSize);
    fieldFrame.value = fieldState.frame ? '1' : '0';
    fieldLayout.value = fieldState.layout;
    fieldAlign.value = fieldState.align;
    fieldLabelColor.value = colorName(fieldState.labelColor);
    fieldValueColor.value = colorName(fieldState.valueColor);
    fieldBackgroundColor.value = colorName(fieldState.backgroundColor);
    fieldFrameColor.value = colorName(fieldState.frameColor);
  }

  function resetProject() {
    pixelLayer.fill(0x0000);
    selectedTool = 'textField';
    Object.assign(fieldState, {
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
    });
    syncInputsFromState();
    syncToolUI();
    render();
  }

  function demoTextField() {
    pixelLayer.fill(0x0000);
    selectedTool = 'textField';
    Object.assign(fieldState, {
      name: 'Estado de máquina',
      id: 'FIELD_STATUS',
      variable: 'estadoTexto',
      capacity: 12,
      x: 18,
      y: 28,
      preview: 'PRODUCCION',
      label: 'Estado',
      unit: '',
      valueSize: 2,
      labelSize: 1,
      frame: true,
      layout: 'STACKED',
      align: 'CENTER',
      labelColor: 0xFFFF,
      valueColor: 0x07E0,
      backgroundColor: 0x0000,
      frameColor: 0xFD20
    });
    syncInputsFromState();
    syncToolUI();
    render();
  }

  function bindInput(element, handler) {
    element.addEventListener('input', () => {
      handler();
      render();
    });
    element.addEventListener('change', () => {
      handler();
      render();
    });
  }

  document.querySelectorAll('.tool[data-tool]').forEach((button) => {
    button.addEventListener('click', () => {
      selectedTool = button.dataset.tool;
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
  clearButton.addEventListener('click', () => {
    pixelLayer.fill(0x0000);
    if (selectedTool === 'rawText') rawState.value = '';
    if (selectedTool === 'textField') fieldState.preview = '';
    syncInputsFromState();
    render();
  });

  bindInput(rawTextInput, () => { rawState.value = rawTextInput.value; });
  bindInput(rawTextX, () => { rawState.x = clamp(Number(rawTextX.value) || 0, 0, WIDTH - 1); });
  bindInput(rawTextY, () => { rawState.y = clamp(Number(rawTextY.value) || 0, 0, HEIGHT - 1); });
  bindInput(rawTextSize, () => { rawState.size = Number(rawTextSize.value) || 1; });
  bindInput(rawTextBackground, () => { rawState.background = colorByName(rawTextBackground.value).value; });

  bindInput(fieldName, () => { fieldState.name = fieldName.value; });
  bindInput(fieldId, () => { fieldState.id = fieldId.value; });
  bindInput(fieldVariable, () => { fieldState.variable = fieldVariable.value; });
  bindInput(fieldCapacity, () => {
    fieldState.capacity = clamp(Number(fieldCapacity.value) || 1, 1, 39);
    fieldState.preview = fieldState.preview.slice(0, fieldState.capacity);
    fieldPreview.value = fieldState.preview;
  });
  bindInput(fieldX, () => { fieldState.x = clamp(Number(fieldX.value) || 0, 0, WIDTH - 1); });
  bindInput(fieldY, () => { fieldState.y = clamp(Number(fieldY.value) || 0, 0, HEIGHT - 1); });
  bindInput(fieldPreview, () => {
    fieldState.preview = fieldPreview.value.slice(0, fieldState.capacity);
    if (fieldPreview.value !== fieldState.preview) fieldPreview.value = fieldState.preview;
  });
  bindInput(fieldLabel, () => { fieldState.label = fieldLabel.value; });
  bindInput(fieldUnit, () => { fieldState.unit = fieldUnit.value; });
  bindInput(fieldValueSize, () => { fieldState.valueSize = Number(fieldValueSize.value) || 1; });
  bindInput(fieldLabelSize, () => { fieldState.labelSize = Number(fieldLabelSize.value) || 1; });
  bindInput(fieldFrame, () => { fieldState.frame = fieldFrame.value === '1'; });
  bindInput(fieldLayout, () => { fieldState.layout = fieldLayout.value; });
  bindInput(fieldAlign, () => { fieldState.align = fieldAlign.value; });
  bindInput(fieldLabelColor, () => { fieldState.labelColor = colorByName(fieldLabelColor.value).value; });
  bindInput(fieldValueColor, () => { fieldState.valueColor = colorByName(fieldValueColor.value).value; });
  bindInput(fieldBackgroundColor, () => { fieldState.backgroundColor = colorByName(fieldBackgroundColor.value).value; });
  bindInput(fieldFrameColor, () => { fieldState.frameColor = colorByName(fieldFrameColor.value).value; });

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

  displayCanvas.addEventListener('pointerdown', (event) => {
    const point = pointFromPointer(event);
    if (!inside(point.x, point.y)) return;
    displayCanvas.setPointerCapture(event.pointerId);
    if (selectedTool === 'pixel' || selectedTool === 'erase') {
      drawing = true;
      lastPoint = null;
      const value = selectedTool === 'erase' ? 0x0000 : selectedColor.value;
      lastPoint = point;
      setLayerPixel(point.x, point.y, value);
    } else {
      draggingObject = true;
      if (selectedTool === 'rawText') {
        rawState.x = point.x;
        rawState.y = point.y;
      } else if (selectedTool === 'textField') {
        fieldState.x = point.x;
        fieldState.y = point.y;
      }
      syncInputsFromState();
    }
    render();
  });

  displayCanvas.addEventListener('pointermove', (event) => {
    const point = pointFromPointer(event);
    updateCursor(point);
    if (!inside(point.x, point.y)) return;
    if (drawing) {
      const value = selectedTool === 'erase' ? 0x0000 : selectedColor.value;
      if (lastPoint) rasterLine(lastPoint.x, lastPoint.y, point.x, point.y, value);
      else setLayerPixel(point.x, point.y, value);
      lastPoint = point;
      render();
    } else if (draggingObject) {
      if (selectedTool === 'rawText') {
        rawState.x = point.x;
        rawState.y = point.y;
      } else if (selectedTool === 'textField') {
        fieldState.x = point.x;
        fieldState.y = point.y;
      }
      syncInputsFromState();
      render();
    }
  });

  function endPointer() {
    drawing = false;
    draggingObject = false;
    lastPoint = null;
  }
  displayCanvas.addEventListener('pointerup', endPointer);
  displayCanvas.addEventListener('pointercancel', endPointer);
  displayCanvas.addEventListener('pointerleave', () => {
    cursorStatus.textContent = 'X: — · Y: —';
    pixelStatus.textContent = 'Pixel: —';
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
})();
