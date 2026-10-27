/*
 * Modo OpenPLC del Designer (contrato jwplc-hmi-host/1, Alpha14 F3).
 *
 * Sólo se activa cuando OpenPLC abre el Designer: existe window.JWPLCHMIHost
 * con mode "openplc". En la pestaña "Datos" (campos BOOL/VALUE, BAR y nodos
 * del MENU) agrega el selector "Variable PLC" con las variables que entrega
 * OpenPLC (listVariables). Al elegir una:
 *   - el nombre C++ se deriva solo (ya no se escribe a mano);
 *   - se guarda plcBinding { var, access } en el objeto (se guarda en el .jwhmi);
 *   - en los nodos del MENU se ajusta el tipo (int32 / float / bool).
 * Si OpenPLC avisa que cambiaron las variables del proyecto
 * (onVariablesChanged), se vuelven a pedir. Dentro de la pestaña de OpenPLC
 * (embedded) se informa si el diseño tiene cambios sin guardar (setDirty),
 * para que la pestaña lo muestre como cualquier archivo del proyecto.
 * El modo Arduino y el .exe no cambian.
 *
 * Contrato: platform-jwplc docs/v2.1.0-alpha.14/A14_F3_CONTRATO_HMI_OPENPLC_20261017.md
 */
(() => {
  'use strict';

  const host = window.JWPLCHMIHost;
  if (!host || host.mode !== 'openplc') return;

  const DIRTY_POLL_MS = 600;
  const INTEGER_TYPES = new Set(['INT', 'UINT', 'WORD', 'DINT', 'UDINT', 'DWORD']);
  let variables = [];
  let variablesVersion = 0;
  let scheduled = false;

  const editor = () => window.JWPLCHMIEditor || null;
  const menuApi = () => window.JWPLCHMIMenu || null;
  const escapeHtml = (text) => String(text).replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));

  function sanitize(name) {
    const cleaned = String(name || '').toLowerCase().replace(/[^a-z0-9_]/g, '_');
    if (!cleaned) return 'plc';
    return /^[a-z_]/.test(cleaned) ? cleaned : `_${cleaned}`;
  }

  /** Nombres C++ en uso por todos los objetos, salvo `except`. */
  function usedNames(except) {
    const names = new Set();
    for (const field of editor()?.getAllFields?.() || []) {
      if (field !== except && field.variable) names.add(String(field.variable));
      for (const node of field.nodes || []) {
        if (node !== except && node.variable) names.add(String(node.variable));
      }
    }
    return names;
  }

  function uniqueName(base, except) {
    const used = usedNames(except);
    if (!used.has(base)) return base;
    for (let i = 2; ; i += 1) if (!used.has(`${base}_${i}`)) return `${base}_${i}`;
  }

  /** Qué objeto edita un input de "Variable C++" y qué variable PLC acepta. */
  function describeTarget(input) {
    const field = editor()?.getSelectedField?.();
    if (!field) return null;
    if (input.matches('[data-node-prop="variable"]')) {
      const node = menuApi()?.selectedNode?.(field);
      if (!node) return null;
      const wants = node.type === 'EDIT_NUM' ? { hmi: 'number', rw: true }
        : node.type === 'EDIT_BOOL' ? { hmi: 'bool', rw: true }
          : { hmi: 'any', rw: false };
      return { kind: 'node', field, object: node, wants };
    }
    if (input.id === 'barPlcVariable') return { kind: 'bar', field, object: field, wants: { hmi: 'number', rw: false } };
    if (field.type === 'BOOL') return { kind: 'field', field, object: field, wants: { hmi: 'bool', rw: false } };
    if (field.type === 'VALUE' || field.type === 'BAR') return { kind: 'field', field, object: field, wants: { hmi: 'number', rw: false } };
    return {
      kind: 'field', field, object: field, wants: null,
      note: field.type === 'TEXT'
        ? 'TEXT todavía no se enlaza a variables PLC: para mostrar una variable usa VALUE (números), BOOL (estados) o BAR.'
        : 'Este objeto todavía no se enlaza a variables PLC.'
    };
  }

  function compatible(variable, wants) {
    if (!wants || variable.access === null) return false;
    if (wants.rw && variable.access !== 'RW') return false;
    if (wants.hmi === 'bool') return variable.hmiTypes.includes('bool');
    if (wants.hmi === 'number') return variable.hmiTypes.includes('float');
    return variable.hmiTypes.length > 0;
  }

  function optionLabel(variable) {
    // Sin dirección: OpenPLC la enlaza por la tabla de depuración del programa (contrato 1.2).
    const parts = [variable.name, variable.iecType, variable.address || (variable.access ? 'variable del programa' : 'sin dirección')];
    if (variable.access === 'RW') parts.push('RW');
    return parts.join(' · ');
  }

  function renderOptions(select, target) {
    const signature = `${target.kind}|${JSON.stringify(target.wants)}|${variablesVersion}`;
    if (select.dataset.signature === signature) return;
    select.dataset.signature = signature;
    const ok = variables.filter((v) => compatible(v, target.wants));
    const rest = variables.filter((v) => !compatible(v, target.wants));
    const why = (v) => v.reason || (target.wants?.rw && v.access === 'R' ? 'sólo lectura' : 'tipo no compatible');
    if (!variables.length) {
      select.innerHTML =
        '<option value="">— Sin enlazar —</option>' +
        '<option disabled>No hay variables: decláralas en la tabla de variables del programa (main) o en las globales</option>';
      return;
    }
    select.innerHTML =
      '<option value="">— Sin enlazar —</option>' +
      ok.map((v) => `<option value="${escapeHtml(v.name)}">${escapeHtml(optionLabel(v))}</option>`).join('') +
      (rest.length
        ? `<optgroup label="No disponibles">${rest.map((v) =>
          `<option disabled>${escapeHtml(`${v.name} · ${v.iecType} — ${why(v)}`)}</option>`).join('')}</optgroup>`
        : '');
  }

  function fire(input, value) {
    input.value = value;
    input.dispatchEvent(new Event('input', { bubbles: true }));
    input.dispatchEvent(new Event('change', { bubbles: true }));
  }

  function bind(input, target, variableName) {
    if (!variableName) {
      delete target.object.plcBinding;
      editor()?.commitHistory?.();
      schedule();
      return;
    }
    const variable = variables.find((v) => v.name === variableName);
    if (!variable) return;

    // MENU: tipo del nodo según la variable PLC (puede redibujar el inspector).
    if (target.kind === 'node' && variable.hmiTypes.length) {
      const kind = variable.hmiTypes.includes('bool') ? 'bool' : INTEGER_TYPES.has(variable.iecType) ? 'int32' : 'float';
      if (target.object.valueType !== kind) {
        document.querySelector(`[data-menu-node-editor] [data-node-kind="${kind}"]`)?.click();
      }
    }

    const fresh = describeTarget(input.isConnected ? input : findVariableInputs()[0] || input);
    const object = fresh?.object || target.object;
    const name = uniqueName(sanitize(variable.name), object);
    const liveInput = target.kind === 'field'
      ? document.getElementById('fieldVariable')
      : target.kind === 'bar'
        ? document.getElementById('barPlcVariable')
        : document.querySelector('[data-menu-node-editor] input[data-node-prop="variable"]');
    if (liveInput && liveInput.value !== name) fire(liveInput, name);

    // El MENU clona sus nodos al editar: se toma el nodo vigente.
    const finalObject = target.kind === 'node' ? (menuApi()?.selectedNode?.(target.field) || object) : object;
    finalObject.plcBinding = { var: variable.name, access: target.wants?.rw ? 'RW' : 'R' };
    editor()?.commitHistory?.();
    schedule();
  }

  function findVariableInputs() {
    return [
      ...document.querySelectorAll('.insp-panel[data-insp-panel="data"] input[data-proxy="fieldVariable"]'),
      ...document.querySelectorAll('#barPlcVariable'),
      ...document.querySelectorAll('[data-menu-node-editor] input[data-node-prop="variable"]'),
    ];
  }

  function ensure() {
    scheduled = false;
    document.querySelectorAll('.insp-mode-chip').forEach((chip) => {
      if (chip.textContent !== 'OpenPLC') chip.textContent = 'OpenPLC';
    });
    document.querySelectorAll('.insp-note').forEach((note) => {
      if (note.textContent.includes('A15-6')) note.hidden = true;
    });

    for (const input of findVariableInputs()) {
      const target = describeTarget(input);
      if (!target) continue;
      input.readOnly = true;
      input.title = 'En un proyecto OpenPLC el nombre C++ se genera a partir de la variable PLC.';

      let box = input.closest('label')?.previousElementSibling;
      if (!box?.classList?.contains('jwplc-plc-binding')) {
        box = document.createElement('div');
        box.className = 'jwplc-plc-binding';
        box.innerHTML =
          '<label class="field-label">Variable PLC <select class="field-input jwplc-plc-select"></select></label>' +
          '<div class="insp-note jwplc-plc-note"></div>';
        (input.closest('label') || input).before(box);
        box.querySelector('select').addEventListener('change', (event) => {
          const current = describeTarget(input);
          if (current) bind(input, current, event.target.value);
        });
      }

      const select = box.querySelector('select');
      const note = box.querySelector('.jwplc-plc-note');
      if (!target.wants) {
        select.disabled = true;
        select.innerHTML = '<option>—</option>';
        select.dataset.signature = '';
        note.textContent = target.note;
        continue;
      }
      select.disabled = false;
      renderOptions(select, target);
      const bound = target.object.plcBinding?.var || '';
      const match = variables.find((v) => v.name.toLowerCase() === bound.toLowerCase());
      const value = match ? match.name : '';
      if (select.value !== value) select.value = value;
      const text = !bound
        ? 'Sin enlazar: elige una variable del PLC.'
        : match
          ? `Enlazada a ${match.name}${target.object.plcBinding.access === 'RW' ? ' (lectura y escritura)' : ''}.`
          : `La variable "${bound}" ya no existe en el PLC.`;
      if (note.textContent !== text) note.textContent = text;
    }
  }

  function schedule() {
    if (scheduled) return;
    scheduled = true;
    requestAnimationFrame(ensure);
  }

  async function loadVariables() {
    try {
      const list = await host.listVariables();
      variables = Array.isArray(list) ? list : [];
    } catch (error) {
      console.error('[JWPLC HMI OpenPLC] listVariables', error);
      variables = [];
    }
    variablesVersion += 1;
    schedule();
  }

  // Cambios sin guardar del diseño -> pestaña de OpenPLC.
  function watchDirty() {
    let last = null;
    setInterval(() => {
      const dirty = Boolean(window.JWPLCHMISaveFlow?.isDirty?.());
      if (dirty === last) return;
      last = dirty;
      host.setDirty(dirty);
    }, DIRTY_POLL_MS);
  }

  async function init() {
    await loadVariables();
    host.onVariablesChanged?.(loadVariables);
    if (typeof host.setDirty === 'function') watchDirty();
    window.addEventListener('jwplc:editor-refresh', schedule);
    new MutationObserver(schedule).observe(document.body, { childList: true, subtree: true });
    schedule();
  }

  if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', init, { once: true });
  else init();
})();
