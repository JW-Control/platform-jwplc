(() => {
  'use strict';

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
    return editor()?.getAllFields?.() || [];
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
    const variable = canonicalFor(field, 'variable', field.variable, index);
    if (field.type === 'VALUE' || field.type === 'BAR') return `float ${variable} = 0.0f;`;
    if (field.type === 'BOOL') return `bool ${variable} = false;`;
    const capacity = Math.max(1, Number(field.capacity) || 1);
    return `char ${variable}[${capacity + 1}] = {};`;
  }

  function setterHint(field, index) {
    const id = canonicalFor(field, 'id', field.id, index);
    const variable = canonicalFor(field, 'variable', field.variable, index);
    if (field.type === 'VALUE') return `// JWPLC_Display.setValue(${id}, ${variable});`;
    if (field.type === 'BOOL') return `// JWPLC_Display.setBool(${id}, ${variable});`;
    if (field.type === 'BAR') return `// JWPLC_Display.setBar(${id}, ${variable});`;
    return `// JWPLC_Display.setText(${id}, ${variable});`;
  }

  function groupedChunks(renderer, commentIndent = '') {
    const fields = allFields();
    const chunks = [];

    pages().forEach((page) => {
      const entries = fields
        .map((field, index) => ({ field, index }))
        .filter(({ field }) => Number(field.page || 0) === Number(page.id));
      if (!entries.length) return;
      const lines = entries.map(({ field, index }) => renderer(field, index));
      chunks.push(`${commentIndent}// ${pageComment(page)}\n${lines.join('\n')}`);
    });

    return chunks.join('\n\n');
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
    const fields = allFields();

    const enumLines = groupedChunks((field, index) => {
      const id = canonicalFor(field, 'id', field.id, index);
      return `    ${id} = ${index + 1},`;
    }, '    ');

    const variables = groupedChunks((field, index) => variableDeclaration(field, index));
    const setters = groupedChunks((field, index) => setterHint(field, index));

    text = text.replace(
      /enum HMIFieldId : uint8_t\n\{\n[\s\S]*?\n\};/,
      `enum HMIFieldId : uint8_t\n{\n${enumLines}\n};`);

    text = text.replace(
      /\/\/ Variables HMI\n[\s\S]*?\n\n\/\/ Definición declarativa/,
      `// Variables HMI\n${variables}\n\n// Definición declarativa`);

    text = text.replace(
      /(\/\/ Setters públicos que corresponden a este diseño:\n)[\s\S]*$/,
      `$1${setters}`);

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
    refresh: patchGeneratedCode
  };
})();
