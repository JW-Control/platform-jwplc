(() => {
  'use strict';

  const WIDTH = 320;
  const HEIGHT = 170;
  const FIELD_PADDING = 3;
  const FIELD_GAP = 4;
  const DEFAULT_BAR_WIDTH = 80;
  const DEFAULT_BAR_HEIGHT = 12;

  const barButton = [...document.querySelectorAll('.component-tool')]
    .find((button) => button.querySelector('strong')?.textContent.trim() === 'BAR');
  const gate = document.querySelector('.page-tabs .gate');
  const bottomSummary = document.querySelector('.bottom-summary');
  const fieldSection = document.getElementById('textFieldControlsSection');
  const numericFormatDetails = document.getElementById('numericFormatDetails');
  const boolDetails = document.getElementById('boolTextDetails');
  const fieldCapacityWrap = document.getElementById('fieldCapacityWrap');
  const fieldCppType = document.getElementById('fieldCppType');
  const fieldPreview = document.getElementById('fieldPreview');
  const fieldPreviewWrap = fieldPreview?.closest('label');
  const fieldName = document.getElementById('fieldName');
  const fieldLabel = document.getElementById('fieldLabel');
  const fieldUnit = document.getElementById('fieldUnit');
  const fieldX = document.getElementById('fieldX');
  const fieldY = document.getElementById('fieldY');
  const fieldValueSize = document.getElementById('fieldValueSize');
  const fieldValueSizeWrap = fieldValueSize?.closest('label');
  const fieldLabelSize = document.getElementById('fieldLabelSize');
  const fieldAlign = document.getElementById('fieldAlign');
  const fieldLayout = document.getElementById('fieldLayout');
  const fieldFrame = document.getElementById('fieldFrame');
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
  const codeOutput = document.getElementById('codeOutput');
  const contractTab = document.getElementById('contractTab');
  const statusTab = document.getElementById('statusTab');
  const generateButton = document.getElementById('generateButton');
  const displayCanvas = document.getElementById('displayCanvas');
  const previewCanvas = document.getElementById('previewCanvas');
  const zoomSelect = document.getElementById('zoomSelect');
  const gridToggle = document.getElementById('gridToggle');

  if (!barButton || !fieldSection || !displayCanvas || !previewCanvas) return;

  const barFields = new Set();
  let barDragging = null;
  let barDragOffset = { x: 0, y: 0 };
  let barKeyboardChanged = false;

  const barDetails = document.createElement('details');
  barDetails.id = 'barRangeDetails';
  barDetails.open = true;
  barDetails.hidden = true;
  barDetails.innerHTML = `
    <summary>Rango BAR</summary>
    <div class="inspector-body two-cols">
      <label class="field-label">Mínimo
        <input id="fieldBarMin" class="field-input" type="number" step="any" value="0" />
      </label>
      <label class="field-label">Máximo
        <input id="fieldBarMax" class="field-input" type="number" step="any" value="100" />
      </label>
      <label class="field-label">Valor de prueba
        <input id="fieldBarValue" class="field-input" type="number" step="any" value="50" />
      </label>
      <div class="readout"><span>Relleno</span><strong id="barPercentStatus">50.0 %</strong></div>
      <label class="field-label">Ancho total
        <select id="fieldBarWidthMode" class="field-input">
          <option value="AUTO" selected>AUTO</option>
          <option value="FIXED">FIJO</option>
        </select>
      </label>
      <label class="field-label">Ancho fijo (px)
        <input id="fieldBarWidth" class="field-input" type="number" min="24" max="320" value="110" disabled />
      </label>
      <div class="readout"><span>Altura BAR</span><strong>12 px</strong></div>
      <div class="readout"><span>Ancho región</span><strong id="barValueWidthStatus">80 px</strong></div>
    </div>`;
  (boolDetails || numericFormatDetails).insertAdjacentElement('afterend', barDetails);

  const fieldBarMin = document.getElementById('fieldBarMin');
  const fieldBarMax = document.getElementById('fieldBarMax');
  const fieldBarValue = document.getElementById('fieldBarValue');
  const fieldBarWidthMode = document.getElementById('fieldBarWidthMode');
  const fieldBarWidth = document.getElementById('fieldBarWidth');
  const barPercentStatus = document.getElementById('barPercentStatus');
  const barValueWidthStatus = document.getElementById('barValueWidthStatus');

  function editor() {
    return window.JWPLCHMIEditor || null;
  }

  function selectedField() {
    return editor()?.getSelectedField?.() || null;
  }

  function isBar(field = selectedField()) {
    return field?.type === 'BAR';
  }

  function serialFor(field) {
    const match = String(field?.key || '').match(/-(\d+)$/);
    return match ? Number(match[1]) : 1;
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

  function cppFloat(value) {
    const number = Number(value);
    if (!Number.isFinite(number)) return '0.0f';
    return `${Number.isInteger(number) ? number.toFixed(1) : String(number)}f`;
  }

  function hex565(value) {
    return `0x${Number(value || 0).toString(16).toUpperCase().padStart(4, '0')}`;
  }

  function rgb565ToCss(value) {
    const color = Number(value || 0);
    const r = Math.round((((color >> 11) & 0x1F) * 255) / 31);
    const g = Math.round((((color >> 5) & 0x3F) * 255) / 63);
    const b = Math.round(((color & 0x1F) * 255) / 31);
    return `rgb(${r}, ${g}, ${b})`;
  }

  function nominalTextBounds(text, size) {
    const value = String(text || '');
    if (!value) return { width: 0, height: 0 };
    const scale = Math.max(1, Math.trunc(Number(size) || 1));
    return { width: value.length * 6 * scale - scale, height: 7 * scale };
  }

  function ensureBarState(field) {
    if (!field || field.type !== 'BAR') return;
    barFields.add(field);
    if (typeof field.barLabel !== 'string') field.barLabel = field.label || 'Nivel';
    if (typeof field.barUnit !== 'string') field.barUnit = field.unit || '%';
    if (!Number.isFinite(Number(field.barMin))) field.barMin = 0;
    if (!Number.isFinite(Number(field.barMax))) field.barMax = 100;
    if (!Number.isFinite(Number(field.barValue))) field.barValue = 50;
    if (typeof field.barAutoWidth !== 'boolean') field.barAutoWidth = true;
    if (!Number.isFinite(Number(field.barWidth))) field.barWidth = 110;

    // Placeholder mínimo para el core. BAR se compone pixel-perfect después
    // del render base, usando exactamente la geometría del runtime JWPLC_UI.
    field.label = '';
    field.unit = '';
    field.preview = '';
    field.capacity = 1;
    field.valueSize = 1;
    field.align = 'LEFT';
  }

  function normalizedFor(field) {
    const min = Number(field.barMin);
    let max = Number(field.barMax);
    const value = Number(field.barValue);
    if (max <= min) max = min + 1;
    const raw = (value - min) / (max - min);
    return Math.max(0, Math.min(1, Number.isFinite(raw) ? raw : 0));
  }

  function barGeometry(field) {
    ensureBarState(field);
    const pad = Math.max(FIELD_PADDING, Math.max(1, Math.trunc(Number(field.labelSize) || 1)));
    const labelBounds = nominalTextBounds(field.barLabel, field.labelSize);
    const unitBounds = nominalTextBounds(field.barUnit, field.labelSize);
    let valueW = DEFAULT_BAR_WIDTH;
    const valueH = DEFAULT_BAR_HEIGHT;
    let fieldW;
    let fieldH;
    let valueX;
    let valueY;

    if (!field.barAutoWidth) {
      fieldW = Math.max(24, Math.min(WIDTH, Math.trunc(Number(field.barWidth) || 110)));
      let available = fieldW - 2 * pad;
      if (field.layout === 'INLINE' && labelBounds.width > 0) available -= labelBounds.width + FIELD_GAP;
      if (unitBounds.width > 0) available -= unitBounds.width + FIELD_GAP;
      valueW = Math.max(1, available);
    }

    if (field.layout === 'STACKED') {
      const valueAndUnitW = valueW + (unitBounds.width > 0 ? FIELD_GAP + unitBounds.width : 0);
      if (field.barAutoWidth) fieldW = 2 * pad + Math.max(labelBounds.width, valueAndUnitW);
      fieldH = 2 * pad + labelBounds.height + (labelBounds.height > 0 ? FIELD_GAP : 0) + Math.max(valueH, unitBounds.height);
      valueX = field.x + pad;
      valueY = field.y + pad + labelBounds.height + (labelBounds.height > 0 ? FIELD_GAP : 0);
    } else {
      if (field.barAutoWidth) {
        fieldW = 2 * pad + labelBounds.width + (labelBounds.width > 0 ? FIELD_GAP : 0) + valueW + (unitBounds.width > 0 ? FIELD_GAP + unitBounds.width : 0);
      }
      fieldH = 2 * pad + Math.max(labelBounds.height, valueH, unitBounds.height);
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
      valueW,
      valueH,
      labelBounds,
      unitBounds
    };
  }

  function isTrackedActive(field) {
    if (!field || field.type !== 'BAR') return false;
    return [...document.querySelectorAll('.object-id')]
      .some((node) => node.textContent.trim() === String(field.id || '').trim());
  }

  function activeBars() {
    return [...barFields].filter(isTrackedActive);
  }

  function drawClassicText(ctx, text, x, y, size, color, scale) {
    const font = window.JWPLCGfxClassicFont;
    if (!font || !text) return;
    const textScale = Math.max(1, Math.trunc(Number(size) || 1));
    ctx.fillStyle = rgb565ToCss(color);
    let cursorX = x;
    for (const character of String(text)) {
      const glyph = font.glyphFor(character.codePointAt(0));
      for (let column = 0; column < font.cellWidth; column += 1) {
        const bits = column < font.bytesPerGlyph ? glyph[column] : 0;
        for (let row = 0; row < font.cellHeight; row += 1) {
          if (column < font.bytesPerGlyph && ((bits >> row) & 0x01) !== 0) {
            ctx.fillRect(
              (cursorX + column * textScale) * scale,
              (y + row * textScale) * scale,
              textScale * scale,
              textScale * scale);
          }
        }
      }
      cursorX += font.cellWidth * textScale;
    }
  }

  function drawGridOverField(ctx, g, scale) {
    if (!gridToggle?.checked || scale < 3) return;
    ctx.save();
    ctx.beginPath();
    ctx.rect(g.fieldX * scale, g.fieldY * scale, g.fieldW * scale, g.fieldH * scale);
    ctx.clip();
    ctx.strokeStyle = 'rgba(118, 151, 176, 0.18)';
    ctx.lineWidth = 1;
    ctx.beginPath();
    for (let x = g.fieldX; x <= g.fieldX + g.fieldW; x += 1) {
      const px = x * scale + 0.5;
      ctx.moveTo(px, g.fieldY * scale);
      ctx.lineTo(px, (g.fieldY + g.fieldH) * scale);
    }
    for (let y = g.fieldY; y <= g.fieldY + g.fieldH; y += 1) {
      const py = y * scale + 0.5;
      ctx.moveTo(g.fieldX * scale, py);
      ctx.lineTo((g.fieldX + g.fieldW) * scale, py);
    }
    ctx.stroke();
    ctx.restore();
  }

  function drawBarField(ctx, field, scale, grid) {
    const g = barGeometry(field);
    ctx.fillStyle = rgb565ToCss(field.backgroundColor);
    ctx.fillRect(g.fieldX * scale, g.fieldY * scale, g.fieldW * scale, g.fieldH * scale);

    if (field.frame && g.fieldW > 1 && g.fieldH > 1) {
      ctx.fillStyle = rgb565ToCss(field.frameColor);
      ctx.fillRect(g.fieldX * scale, g.fieldY * scale, g.fieldW * scale, scale);
      ctx.fillRect(g.fieldX * scale, (g.fieldY + g.fieldH - 1) * scale, g.fieldW * scale, scale);
      ctx.fillRect(g.fieldX * scale, g.fieldY * scale, scale, g.fieldH * scale);
      ctx.fillRect((g.fieldX + g.fieldW - 1) * scale, g.fieldY * scale, scale, g.fieldH * scale);
    }

    drawClassicText(ctx, field.barLabel, field.x + g.pad, field.y + g.pad, field.labelSize, field.labelColor, scale);
    drawClassicText(ctx, field.barUnit, g.valueX + g.valueW + FIELD_GAP, g.valueY, field.labelSize, field.labelColor, scale);

    const fillW = Math.round(normalizedFor(field) * g.valueW);
    if (fillW > 0) {
      ctx.fillStyle = rgb565ToCss(field.valueColor);
      ctx.fillRect(g.valueX * scale, g.valueY * scale, fillW * scale, g.valueH * scale);
    }

    if (grid) drawGridOverField(ctx, g, scale);
    return g;
  }

  function patchCanvases() {
    const bars = activeBars();
    if (!bars.length) return;
    const displayCtx = displayCanvas.getContext('2d', { alpha: false });
    const previewCtx = previewCanvas.getContext('2d', { alpha: false });
    const zoom = Math.max(1, Number(zoomSelect?.value) || 3);
    bars.forEach((field) => {
      drawBarField(previewCtx, field, 1, false);
      drawBarField(displayCtx, field, zoom, true);
    });
  }

  function triggerCoreRender(commit = false) {
    const field = selectedField();
    if (!isBar(field)) return;
    fieldX.value = String(field.x);
    fieldY.value = String(field.y);
    fieldX.dispatchEvent(new Event(commit ? 'change' : 'input', { bubbles: true }));
  }

  function updateBarFromControls(commit = false) {
    const field = selectedField();
    if (!isBar(field)) return;
    field.barMin = Number(fieldBarMin.value);
    field.barMax = Number(fieldBarMax.value);
    field.barValue = Number(fieldBarValue.value);
    field.barAutoWidth = fieldBarWidthMode.value === 'AUTO';
    field.barWidth = Math.max(24, Math.min(WIDTH, Number(fieldBarWidth.value) || 110));
    triggerCoreRender(commit);
  }

  [fieldBarMin, fieldBarMax, fieldBarValue, fieldBarWidth].forEach((input) => {
    input.addEventListener('input', () => updateBarFromControls(false));
    input.addEventListener('change', () => updateBarFromControls(true));
  });
  fieldBarWidthMode.addEventListener('change', () => updateBarFromControls(true));

  function bindBarTextControl(input, property) {
    if (!input) return;
    ['input', 'change'].forEach((eventName) => {
      input.addEventListener(eventName, (event) => {
        const field = selectedField();
        if (!isBar(field)) return;
        event.stopImmediatePropagation();
        field[property] = input.value;
        triggerCoreRender(eventName === 'change');
      }, true);
    });
  }

  bindBarTextControl(fieldLabel, 'barLabel');
  bindBarTextControl(fieldUnit, 'barUnit');

  function createBarField() {
    const api = editor();
    const current = selectedField();
    if (current?.type === 'BAR') return;
    api?.addTextField?.();
    const field = selectedField();
    if (!field) return;

    const serial = serialFor(field);
    field.type = 'BAR';
    field.name = `BAR ${serial}`;
    field.id = `FIELD_BAR_${serial}`;
    field.variable = `nivel${serial}`;
    field.barLabel = 'Nivel';
    field.barUnit = '%';
    field.barMin = 0;
    field.barMax = 100;
    field.barValue = 50;
    field.barAutoWidth = true;
    field.barWidth = 110;
    field.labelSize = 1;
    field.frame = false;
    field.layout = 'STACKED';
    field.labelColor = 0xFFFF;
    field.valueColor = 0x07FF;
    field.backgroundColor = 0x0000;
    field.frameColor = 0xFFFF;
    ensureBarState(field);

    fieldName.value = field.name;
    fieldName.dispatchEvent(new Event('change', { bubbles: true }));
  }

  barButton.disabled = false;
  barButton.classList.add('tool');
  barButton.dataset.tool = 'barField';
  barButton.title = 'Agregar / seleccionar barra de nivel';
  const barDescription = barButton.querySelector('span:last-child');
  if (barDescription) barDescription.textContent = 'Barra de nivel';
  barButton.addEventListener('click', (event) => {
    event.preventDefault();
    createBarField();
    setTimeout(patchUI, 0);
  });

  function patchObjectList() {
    document.querySelectorAll('.object-item').forEach((item) => {
      if (item.querySelector('.object-type')?.textContent.trim() !== 'BAR') return;
      const icon = item.querySelector('.object-icon');
      if (icon) {
        icon.textContent = '▥';
        icon.style.fontSize = '13px';
        icon.style.fontWeight = '700';
        icon.style.color = '#52c9ff';
      }
    });
  }

  function patchInspector(field) {
    const activeBar = isBar(field);
    barDetails.hidden = !activeBar;
    if (fieldValueSizeWrap) fieldValueSizeWrap.hidden = activeBar;
    if (fieldAlign) fieldAlign.disabled = activeBar;
    if (!activeBar) return;

    ensureBarState(field);
    if (numericFormatDetails) numericFormatDetails.hidden = true;
    if (boolDetails) boolDetails.hidden = true;
    if (fieldCapacityWrap) fieldCapacityWrap.hidden = true;
    if (fieldPreviewWrap) fieldPreviewWrap.hidden = true;
    if (fieldCppType) fieldCppType.value = 'float';
    if (fieldLabel) fieldLabel.value = field.barLabel;
    if (fieldUnit) fieldUnit.value = field.barUnit;
    if (fieldAlign) fieldAlign.value = 'LEFT';

    fieldBarMin.value = String(field.barMin);
    fieldBarMax.value = String(field.barMax);
    fieldBarValue.value = String(field.barValue);
    fieldBarWidthMode.value = field.barAutoWidth ? 'AUTO' : 'FIXED';
    fieldBarWidth.value = String(field.barWidth);
    fieldBarWidth.disabled = field.barAutoWidth;

    const g = barGeometry(field);
    if (barPercentStatus) barPercentStatus.textContent = `${(normalizedFor(field) * 100).toFixed(1)} %`;
    if (barValueWidthStatus) barValueWidthStatus.textContent = `${g.valueW} px`;
    if (fieldPadStatus) fieldPadStatus.textContent = `${g.pad} px`;
    if (fieldBoundsStatus) fieldBoundsStatus.textContent = `${g.fieldW} × ${g.fieldH} px`;
    if (fieldValueBoundsStatus) fieldValueBoundsStatus.textContent = `${g.valueW} × ${g.valueH} px`;
    if (fieldValueXYStatus) fieldValueXYStatus.textContent = `${g.valueX}, ${g.valueY}`;
    if (fieldLayoutStatus) fieldLayoutStatus.textContent = field.layout;
    if (inspectorContract) inspectorContract.textContent = `float ${sanitizeSymbol(field.variable, 'nivel')} = 0.0f;`;

    document.querySelectorAll('.component-tool').forEach((button) => button.classList.remove('active'));
    barButton.classList.add('active');
  }

  function barFieldBlock(field) {
    ensureBarState(field);
    const id = sanitizeSymbol(field.id, `FIELD_BAR_${serialFor(field)}`);
    const label = field.barLabel ? `"${cppString(field.barLabel)}"` : 'nullptr';
    const unit = field.barUnit ? `"${cppString(field.barUnit)}"` : 'nullptr';
    const rect = field.barAutoWidth
      ? `JWPLC_UIRect(${field.x}, ${field.y})`
      : `JWPLC_UIRect(${field.x}, ${field.y}, ${Math.max(24, Math.min(WIDTH, Math.trunc(Number(field.barWidth) || 110)))}, JWPLC_UI_AUTO)`;

    return `    JWPLC_UIBarField(\n        ${id},\n        ${rect},\n        JWPLC_UIText(${label}, ${unit}),\n        JWPLC_UIRange(${cppFloat(field.barMin)}, ${cppFloat(field.barMax)}),\n        JWPLC_UIBarStyle(\n            ${field.labelSize},\n            ${cppBool(field.frame)},\n            JWPLC_UI_LAYOUT_${field.layout}),\n        ${field.page || 0},\n        JWPLC_UIColors(\n            ${hex565(field.labelColor)},\n            ${hex565(field.valueColor)},\n            ${hex565(field.backgroundColor)},\n            ${hex565(field.frameColor)}))`;
  }

  function replaceHelperCall(text, id, replacement) {
    const marker = `    JWPLC_UITextField(\n        ${id},`;
    const start = text.indexOf(marker);
    if (start < 0) return text;
    const open = text.indexOf('(', start);
    if (open < 0) return text;
    let depth = 0;
    let inString = false;
    let escaped = false;
    for (let index = open; index < text.length; index += 1) {
      const ch = text[index];
      if (inString) {
        if (escaped) escaped = false;
        else if (ch === '\\') escaped = true;
        else if (ch === '"') inString = false;
        continue;
      }
      if (ch === '"') { inString = true; continue; }
      if (ch === '(') depth += 1;
      else if (ch === ')') {
        depth -= 1;
        if (depth === 0) return text.slice(0, start) + replacement + text.slice(index + 1);
      }
    }
    return text;
  }

  function activeBarFieldsInCode(text) {
    return activeBars().filter((field) => text.includes(String(field.id || '')));
  }

  function patchGeneratedCode() {
    if (!codeOutput?.textContent.startsWith('// Código generado por JWPLC HMI Designer')) return;
    let text = codeOutput.textContent
      .replace('// API pública JWPLC_UI · Alpha11 A11-3B', '// API pública JWPLC_UI · Alpha11 A11-3D')
      .replace('// API pública JWPLC_UI · Alpha11 A11-3C', '// API pública JWPLC_UI · Alpha11 A11-3D');

    activeBarFieldsInCode(text).forEach((field) => {
      ensureBarState(field);
      const id = sanitizeSymbol(field.id, `FIELD_BAR_${serialFor(field)}`);
      const variable = sanitizeSymbol(field.variable, `nivel${serialFor(field)}`);
      const legacyDeclaration = `char ${variable}[${Math.max(1, field.capacity) + 1}] = {};`;
      text = text.replace(legacyDeclaration, `float ${variable} = 0.0f;`);
      text = replaceHelperCall(text, id, barFieldBlock(field));
      text = text.replace(
        `// JWPLC_Display.setText(${id}, ${variable});`,
        `// JWPLC_Display.setBar(${id}, ${variable});`);
    });
    codeOutput.textContent = text;
  }

  function patchStatusText() {
    if (!codeOutput?.textContent.startsWith('A11 UX Foundation:')) return;
    const barCount = activeBars().length;
    let text = codeOutput.textContent
      .replace('A11-3C BOOL: IN_PROGRESS', 'A11-3C BOOL: PASS')
      .replace('A11-3D BAR permanece pendiente.', 'A11-3D BAR: IN_PROGRESS');

    if (!text.includes('A11-3D BAR: IN_PROGRESS')) {
      const marker = text.includes('A11-3C BOOL: PASS') ? 'A11-3C BOOL: PASS' : 'A11-3B VALUE: PASS';
      text = text.replace(marker, `${marker}\nA11-3D BAR: IN_PROGRESS`);
    }
    if (!text.includes('\n- BAR:')) {
      text = text.replace(/(\n- BOOL: \d+)/, `$1\n- BAR: ${barCount}`);
    } else {
      text = text.replace(/\n- BAR: \d+/, `\n- BAR: ${barCount}`);
    }
    text = text.replace(/\n\nA11-3D BAR permanece pendiente\.?/, '');
    codeOutput.textContent = text;
  }

  function pointFromPointer(event) {
    const rect = displayCanvas.getBoundingClientRect();
    const zoom = Math.max(1, Number(zoomSelect?.value) || 3);
    const scaleX = displayCanvas.width / rect.width;
    const scaleY = displayCanvas.height / rect.height;
    return {
      x: Math.floor(((event.clientX - rect.left) * scaleX) / zoom),
      y: Math.floor(((event.clientY - rect.top) * scaleY) / zoom)
    };
  }

  function hitBar(point) {
    const bars = activeBars();
    for (let index = bars.length - 1; index >= 0; index -= 1) {
      const field = bars[index];
      const g = barGeometry(field);
      if (point.x >= g.fieldX && point.x < g.fieldX + g.fieldW &&
          point.y >= g.fieldY && point.y < g.fieldY + g.fieldH) return field;
    }
    return null;
  }

  function selectBar(field) {
    if (selectedField() === field) return;
    const item = [...document.querySelectorAll('.object-item')]
      .find((node) => node.querySelector('.object-id')?.textContent.trim() === String(field.id || '').trim());
    item?.click();
  }

  displayCanvas.addEventListener('pointerdown', (event) => {
    const point = pointFromPointer(event);
    const hit = hitBar(point);
    if (!hit) return;
    event.preventDefault();
    event.stopImmediatePropagation();
    selectBar(hit);
    barDragging = hit;
    barDragOffset = { x: point.x - hit.x, y: point.y - hit.y };
    try { displayCanvas.setPointerCapture(event.pointerId); } catch (_) {}
  }, true);

  displayCanvas.addEventListener('pointermove', (event) => {
    if (!barDragging) return;
    event.preventDefault();
    event.stopImmediatePropagation();
    const point = pointFromPointer(event);
    const g = barGeometry(barDragging);
    barDragging.x = Math.max(0, Math.min(WIDTH - g.fieldW, point.x - barDragOffset.x));
    barDragging.y = Math.max(0, Math.min(HEIGHT - g.fieldH, point.y - barDragOffset.y));
    if (selectedField() === barDragging) triggerCoreRender(false);
  }, true);

  function finishBarDrag(event) {
    if (!barDragging) return;
    event?.preventDefault?.();
    event?.stopImmediatePropagation?.();
    if (selectedField() === barDragging) triggerCoreRender(true);
    barDragging = null;
  }

  displayCanvas.addEventListener('pointerup', finishBarDrag, true);
  displayCanvas.addEventListener('pointercancel', finishBarDrag, true);

  document.addEventListener('keydown', (event) => {
    if (!isBar() || event.target instanceof HTMLInputElement ||
        event.target instanceof HTMLTextAreaElement || event.target instanceof HTMLSelectElement) return;
    const step = event.shiftKey ? 10 : 1;
    const delta = {
      ArrowLeft: [-step, 0], ArrowRight: [step, 0],
      ArrowUp: [0, -step], ArrowDown: [0, step]
    }[event.key];
    if (!delta) return;
    event.preventDefault();
    event.stopImmediatePropagation();
    const field = selectedField();
    const g = barGeometry(field);
    field.x = Math.max(0, Math.min(WIDTH - g.fieldW, field.x + delta[0]));
    field.y = Math.max(0, Math.min(HEIGHT - g.fieldH, field.y + delta[1]));
    barKeyboardChanged = true;
    triggerCoreRender(false);
  }, true);

  document.addEventListener('keyup', (event) => {
    if (!barKeyboardChanged || !['ArrowLeft', 'ArrowRight', 'ArrowUp', 'ArrowDown'].includes(event.key)) return;
    event.preventDefault();
    event.stopImmediatePropagation();
    barKeyboardChanged = false;
    triggerCoreRender(true);
  }, true);

  const api = editor();
  if (api) {
    const originalGeometry = api.computeSelectedGeometry?.bind(api);
    const originalHasFieldSelection = api.hasFieldSelection?.bind(api);
    api.computeSelectedGeometry = () => isBar() ? barGeometry(selectedField()) : originalGeometry?.();
    api.hasFieldSelection = () => isBar() || Boolean(originalHasFieldSelection?.());
    api.hasBarSelection = () => isBar();
    api.addBarField = createBarField;
  }

  function patchUI() {
    const field = selectedField();
    if (field?.type === 'BAR') ensureBarState(field);
    if (gate) gate.textContent = 'Gate: A11-3D BAR · API pública';
    if (bottomSummary) bottomSummary.textContent = 'A11-3D · BAR sobre API pública';
    patchObjectList();
    patchInspector(field);
    patchCanvases();
    patchGeneratedCode();
    patchStatusText();
  }

  window.addEventListener('jwplc:editor-refresh', patchUI);
  contractTab?.addEventListener('click', () => setTimeout(patchGeneratedCode, 0));
  statusTab?.addEventListener('click', () => setTimeout(patchStatusText, 0));
  generateButton?.addEventListener('click', () => setTimeout(patchGeneratedCode, 0));
  fieldLayout?.addEventListener('change', () => setTimeout(patchUI, 0));
  fieldFrame?.addEventListener('change', () => setTimeout(patchUI, 0));
  fieldLabelSize?.addEventListener('change', () => setTimeout(patchUI, 0));
  [fieldLabelColor, fieldValueColor, fieldBackgroundColor, fieldFrameColor]
    .filter(Boolean)
    .forEach((input) => input.addEventListener('change', () => setTimeout(patchUI, 0)));

  patchUI();

  window.JWPLCHMIBar = {
    addBarField: createBarField,
    hasBarSelection: () => isBar(),
    trackedCount: () => activeBars().length,
    computeGeometry: (field) => barGeometry(field)
  };
})();
