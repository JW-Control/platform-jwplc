(() => {
  'use strict';

  const boolButton = document.querySelector('[data-tool="boolField"]') ||
    [...document.querySelectorAll('.component-tool, .canvas-tool')]
      .find((button) => button.querySelector('strong')?.textContent.trim() === 'BOOL');
  const gate = document.querySelector('.page-tabs .gate');
  const bottomSummary = document.querySelector('.bottom-summary');
  const fieldSection = document.getElementById('textFieldControlsSection');
  const numericFormatDetails = document.getElementById('numericFormatDetails');
  const fieldCapacityWrap = document.getElementById('fieldCapacityWrap');
  const fieldCppType = document.getElementById('fieldCppType');
  const fieldPreview = document.getElementById('fieldPreview');
  const fieldPreviewWrap = fieldPreview?.closest('label');
  const fieldName = document.getElementById('fieldName');
  const inspectorContract = document.getElementById('inspectorContract');
  const codeOutput = document.getElementById('codeOutput');
  const contractTab = document.getElementById('contractTab');
  const statusTab = document.getElementById('statusTab');
  const generateButton = document.getElementById('generateButton');
  const clearButton = document.getElementById('clearButton');

  if (!boolButton || !fieldSection || !numericFormatDetails || !fieldPreview) return;

  const boolDetails = document.createElement('details');
  boolDetails.id = 'boolTextDetails';
  boolDetails.open = true;
  boolDetails.hidden = true;
  boolDetails.innerHTML = `
    <summary>Texto booleano</summary>
    <div class="inspector-body two-cols">
      <label class="field-label">Texto FALSE
        <input id="fieldFalseText" class="field-input" type="text" value="OFF" maxlength="39" />
      </label>
      <label class="field-label">Texto TRUE
        <input id="fieldTrueText" class="field-input" type="text" value="ON" maxlength="39" />
      </label>
      <label class="field-label">Estado de prueba
        <select id="fieldBoolPreview" class="field-input">
          <option value="0" selected>false</option>
          <option value="1">true</option>
        </select>
      </label>
      <div class="readout"><span>Reserva geométrica</span><strong id="boolReserveStatus">OFF</strong></div>
    </div>`;
  numericFormatDetails.insertAdjacentElement('afterend', boolDetails);

  const fieldFalseText = document.getElementById('fieldFalseText');
  const fieldTrueText = document.getElementById('fieldTrueText');
  const fieldBoolPreview = document.getElementById('fieldBoolPreview');
  const boolReserveStatus = document.getElementById('boolReserveStatus');

  function editor() {
    return window.JWPLCHMIEditor || null;
  }

  function selectedField() {
    return editor()?.getSelectedField?.() || null;
  }

  function isBool(field = selectedField()) {
    return field?.type === 'BOOL';
  }

  function allBoolFields() {
    return (editor()?.getAllFields?.() || []).filter((field) => field?.type === 'BOOL');
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

  function ensureBoolState(field) {
    if (!field || field.type !== 'BOOL') return;
    if (typeof field.falseText !== 'string') field.falseText = 'OFF';
    if (typeof field.trueText !== 'string') field.trueText = 'ON';
    if (typeof field.boolValue !== 'boolean') field.boolValue = false;
    field.capacity = Math.max(1, field.falseText.length, field.trueText.length);
    const expected = field.boolValue ? field.trueText : field.falseText;
    if (field.preview !== expected) field.preview = expected;
  }

  function forceBoolRender(commit = false) {
    const field = selectedField();
    if (!isBool(field)) return;
    ensureBoolState(field);
    fieldPreview.value = field.preview;
    fieldPreview.dispatchEvent(new Event(commit ? 'change' : 'input', { bubbles: true }));
  }

  function updateBoolFromControls(commit = false) {
    const field = selectedField();
    if (!isBool(field)) return;
    field.falseText = fieldFalseText.value;
    field.trueText = fieldTrueText.value;
    field.boolValue = fieldBoolPreview.value === '1';
    ensureBoolState(field);
    forceBoolRender(commit);
  }

  [fieldFalseText, fieldTrueText].forEach((input) => {
    input.addEventListener('input', () => updateBoolFromControls(false));
    input.addEventListener('change', () => updateBoolFromControls(true));
  });
  fieldBoolPreview.addEventListener('change', () => updateBoolFromControls(true));

  function createBoolField() {
    const api = editor();
    const current = selectedField();
    if (current?.type === 'BOOL') return;
    api?.addTextField?.();
    const field = selectedField();
    if (!field) return;

    const serial = serialFor(field);
    field.type = 'BOOL';
    field.name = `BOOL ${serial}`;
    field.id = `FIELD_BOOL_${serial}`;
    field.variable = `estado${serial}`;
    field.label = 'Estado';
    field.unit = '';
    field.falseText = 'OFF';
    field.trueText = 'ON';
    field.boolValue = false;
    field.preview = 'OFF';
    field.capacity = 3;
    field.valueSize = 2;
    field.labelSize = 1;
    field.frame = false;
    field.layout = 'INLINE';
    field.align = 'CENTER';
    ensureBoolState(field);

    fieldName.value = field.name;
    fieldName.dispatchEvent(new Event('change', { bubbles: true }));
  }

  boolButton.disabled = false;
  boolButton.classList.add('tool');
  boolButton.dataset.tool = 'boolField';
  boolButton.title = 'Agregar / seleccionar campo booleano';
  const boolDescription = boolButton.querySelector('span:last-child');
  if (boolDescription) boolDescription.textContent = 'Estado booleano';
  boolButton.addEventListener('click', (event) => {
    event.preventDefault();
    createBoolField();
    setTimeout(patchUI, 0);
  });

  function patchObjectList() {
    document.querySelectorAll('.object-item').forEach((item) => {
      if (item.querySelector('.object-type')?.textContent.trim() !== 'BOOL') return;
      const icon = item.querySelector('.object-icon');
      if (icon) {
        icon.textContent = '○';
        icon.style.fontSize = '13px';
        icon.style.fontWeight = '700';
        icon.style.color = '#52c9ff';
      }
    });
  }

  function patchInspector(field) {
    const activeBool = isBool(field);
    boolDetails.hidden = !activeBool;
    if (fieldPreviewWrap) fieldPreviewWrap.hidden = activeBool;

    if (!activeBool) return;
    ensureBoolState(field);
    fieldCapacityWrap.hidden = true;
    fieldCppType.value = 'bool';
    fieldFalseText.value = field.falseText;
    fieldTrueText.value = field.trueText;
    fieldBoolPreview.value = field.boolValue ? '1' : '0';
    boolReserveStatus.textContent = field.falseText.length >= field.trueText.length
      ? (field.falseText || '—')
      : (field.trueText || '—');
    inspectorContract.textContent = `bool ${sanitizeSymbol(field.variable, 'estado')} = false;`;

    document.querySelectorAll('.component-tool, .canvas-tool').forEach((button) => button.classList.remove('active'));
    boolButton.classList.add('active');
  }

  // A15-5: la definición C++ vive en designer-codegen-core.js.
  function boolFieldBlock(field) {
    ensureBoolState(field);
    const id = sanitizeSymbol(field.id, `FIELD_BOOL_${serialFor(field)}`);
    return window.JWPLCHMICodegenCore.boolFieldBlock(field, id);
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

  function activeBoolFieldsInCode(text) {
    return allBoolFields().filter((field) => {
      const id = sanitizeSymbol(field.id, `FIELD_BOOL_${serialFor(field)}`);
      return text.includes(id);
    });
  }

  function patchGeneratedCode() {
    if (!codeOutput?.textContent.startsWith('// Código generado por JWPLC HMI Designer')) return;
    let text = codeOutput.textContent
      .replace('// API pública JWPLC_UI · Alpha11 A11-3B', '// API pública JWPLC_UI · Alpha11 A11-3C');

    activeBoolFieldsInCode(text).forEach((field) => {
      ensureBoolState(field);
      const id = sanitizeSymbol(field.id, `FIELD_BOOL_${serialFor(field)}`);
      const variable = sanitizeSymbol(field.variable, `estado${serialFor(field)}`);
      const legacyDeclaration = `bool ${variable} = false;`;
      if (!text.includes(legacyDeclaration)) {
        const textDeclaration = window.JWPLCHMICodegenCore.textDeclaration(field, variable);
        text = text.replace(textDeclaration, legacyDeclaration);
      }
      text = replaceHelperCall(text, id, boolFieldBlock(field));
      text = text.replace(
        `// JWPLC_Display.setText(${id}, ${variable});`,
        `// JWPLC_Display.setBool(${id}, ${variable});`);
    });
    codeOutput.textContent = text;
  }

  function patchStatusText() {
    // Resumen del panel «Estado» (app.js): se agrega el conteo de BOOL.
    if (!codeOutput?.textContent.startsWith('JWPLC HMI Designer ')) return;
    const activePage = Number(editor()?.getActivePage?.() ?? 0);
    const boolCount = allBoolFields().filter((field) => Number(field.page || 0) === activePage).length;

    let text = codeOutput.textContent;
    if (!text.includes('\n- BOOL:')) {
      text = text.replace(/(\n- VALUE: \d+)/, `$1\n- BOOL: ${boolCount}`);
    } else {
      text = text.replace(/\n- BOOL: \d+/, `\n- BOOL: ${boolCount}`);
    }
    codeOutput.textContent = text;
  }

  function patchUI() {
    const field = selectedField();
    if (field?.type === 'BOOL') ensureBoolState(field);
    if (gate) gate.textContent = 'v2.1.0-alpha.15';
    if (bottomSummary) bottomSummary.textContent = 'BOOL sobre API pública';
    patchObjectList();
    patchInspector(field);
    patchGeneratedCode();
    patchStatusText();
  }

  window.addEventListener('jwplc:editor-refresh', patchUI);
  contractTab?.addEventListener('click', () => setTimeout(patchGeneratedCode, 0));
  statusTab?.addEventListener('click', () => setTimeout(patchStatusText, 0));
  generateButton?.addEventListener('click', () => setTimeout(patchGeneratedCode, 0));
  clearButton?.addEventListener('click', () => {
    setTimeout(() => {
      const field = selectedField();
      if (!isBool(field)) return;
      ensureBoolState(field);
      forceBoolRender(true);
    }, 0);
  });

  patchUI();

  window.JWPLCHMIBool = {
    addBoolField: createBoolField,
    hasBoolSelection: () => isBool(),
    trackedCount: () => allBoolFields().length
  };
})();
