/*
 * Modo OpenPLC del Designer (contrato jwplc-hmi-host/1, Alpha14 F3).
 *
 * Sólo se activa cuando OpenPLC abre el Designer: existe window.JWPLCHMIHost
 * con mode "openplc". En la pestaña "Datos" (campos BOOL/VALUE, BAR, ICON,
 * TREND y nodos del MENU) agrega el selector "Variable PLC" con las variables
 * que entrega OpenPLC (listVariables). Al elegir una:
 *   - el nombre C++ se deriva solo (ya no se escribe a mano);
 *   - se guarda plcBinding { var, access } en el objeto (se guarda en el .jwhmi);
 *   - en los nodos del MENU se ajusta el tipo (int32 / float / bool) con copy-on-write.
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
  const INTEGER_TYPES = new Set(['SINT', 'USINT', 'BYTE', 'INT', 'UINT', 'WORD', 'DINT', 'UDINT', 'DWORD']);
  const TREND_CHANNELS = {
    variable: { varProp: 'variable', bindingProp: 'plcBinding', enabledProp: null },
    ch2Variable: { varProp: 'ch2Variable', bindingProp: 'ch2PlcBinding', enabledProp: 'ch2Enabled' },
    ch3Variable: { varProp: 'ch3Variable', bindingProp: 'ch3PlcBinding', enabledProp: 'ch3Enabled' }
  };
  let variables = [];
  const bindingBoxes = new WeakMap();
  let variablesVersion = 0;
  let scheduled = false;
  let lastDirtyReported = null;

  const editor = () => window.JWPLCHMIEditor || null;
  const menuApi = () => window.JWPLCHMIMenu || null;
  const escapeHtml = (text) => String(text).replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));

  function sanitize(name) {
    const cleaned = String(name || '').toLowerCase().replace(/[^a-z0-9_]/g, '_');
    if (!cleaned) return 'plc';
    return /^[a-z_]/.test(cleaned) ? cleaned : `_${cleaned}`;
  }

  /** Nombres C++ en uso por todos los objetos, salvo `(exceptObj, exceptProp)`. */
  function usedNames(exceptObj, exceptProp = 'variable') {
    const names = new Set();
    for (const field of editor()?.getAllFields?.() || []) {
      if ((field !== exceptObj || exceptProp !== 'variable') && field.variable) {
        names.add(String(field.variable));
      }
      if (field.type === 'TREND') {
        if (field.ch2Enabled && (field !== exceptObj || exceptProp !== 'ch2Variable') && field.ch2Variable) {
          names.add(String(field.ch2Variable));
        }
        if (field.ch3Enabled && (field !== exceptObj || exceptProp !== 'ch3Variable') && field.ch3Variable) {
          names.add(String(field.ch3Variable));
        }
      }
      for (const node of field.nodes || []) {
        if (node !== exceptObj && node.variable) names.add(String(node.variable));
      }
      if (Array.isArray(field.tierVariables)) {
        field.tierVariables.forEach((tv, idx) => {
          const tierObj = field.tierBindings?.[idx];
          if (tv && tierObj !== exceptObj && (field !== exceptObj || exceptProp !== `tier:${idx}`)) {
            names.add(String(tv));
          }
        });
      }
    }
    return names;
  }

  function uniqueName(base, exceptObj, exceptProp = 'variable') {
    const used = usedNames(exceptObj, exceptProp);
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
      return { kind: 'node', field, object: node, binding: node.plcBinding || null, wants };
    }
    if (input.matches('[data-trend-prop]')) {
      const channel = TREND_CHANNELS[input.dataset.trendProp];
      if (!channel || field.type !== 'TREND') return null;
      const enabled = channel.enabledProp ? Boolean(field[channel.enabledProp]) : true;
      return {
        kind: 'trend',
        field,
        object: field,
        varProp: channel.varProp,
        bindingProp: channel.bindingProp,
        binding: field[channel.bindingProp] || null,
        enabled,
        wants: { hmi: 'number', rw: false }
      };
    }
    if (input.id === 'barPlcVariable') {
      return { kind: 'bar', field, object: field, binding: field.plcBinding || null, wants: { hmi: 'number', rw: false } };
    }
    if (field.type === 'BOOL') {
      return { kind: 'field', field, object: field, binding: field.plcBinding || null, wants: { hmi: 'bool', rw: false } };
    }
    if (field.type === 'VALUE') {
      if (window.JWPLCHMICodegenCore?.isClockField?.(field)) {
        return { kind: 'field', field, object: field, binding: null, wants: null, note: 'Este VALUE muestra la fecha/hora del RTC y no lleva variable PLC.' };
      }
      return { kind: 'field', field, object: field, binding: field.plcBinding || null, wants: { hmi: 'number', rw: false } };
    }
    if (field.type === 'BAR') {
      return { kind: 'field', field, object: field, binding: field.plcBinding || null, wants: { hmi: 'number', rw: false } };
    }
    if (input.matches('[data-tier-prop="variable"]')) {
      const tierIndex = Number(input.dataset.tierIndex) || 0;
      const existing = Array.isArray(field.tierBindings) ? (field.tierBindings[tierIndex] || null) : null;
      return {
        kind: 'tier',
        field,
        object: existing,
        tierIndex,
        binding: existing?.plcBinding || null,
        wants: { hmi: 'bool', rw: false }
      };
    }
    if (field.type === 'ICON') {
      const isUint = field.iconVarType === 'uint8';
      return { kind: 'field', field, object: field, binding: field.plcBinding || null, wants: { hmi: isUint ? 'number' : 'bool', rw: false } };
    }
    return {
      kind: 'field', field, object: field, binding: null, wants: null,
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

  function incompatibleReason(variable, wants) {
    return variable.reason || (wants?.rw && variable.access === 'R' ? 'sólo lectura' : 'tipo no compatible');
  }

  function renderOptions(select, target) {
    const signature = `${target.kind}|${JSON.stringify(target.wants)}|${variablesVersion}`;
    if (select.dataset.signature === signature) return;
    select.dataset.signature = signature;
    const ok = variables.filter((v) => compatible(v, target.wants));
    const rest = variables.filter((v) => !compatible(v, target.wants));
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
          `<option disabled>${escapeHtml(`${v.name} · ${v.iecType} — ${incompatibleReason(v, target.wants)}`)}</option>`).join('')}</optgroup>`
        : '');
  }

  function fire(input, value) {
    input.value = value;
    input.dispatchEvent(new Event('input', { bubbles: true }));
    input.dispatchEvent(new Event('change', { bubbles: true }));
  }

  function syncDirty() {
    if (typeof host.setDirty !== 'function') return;
    const dirty = Boolean(window.JWPLCHMISaveFlow?.isDirty?.());
    if (dirty === lastDirtyReported) return;
    lastDirtyReported = dirty;
    host.setDirty(dirty);
  }

  function bind(input, target, variableName) {
    if (!variableName) {
      if (target.kind === 'node') {
        menuApi()?.mutateSelectedNode?.((node) => { delete node.plcBinding; }, true);
        syncDirty();
        schedule();
        return;
      }
      if (target.kind === 'tier') {
        if (Array.isArray(target.field.tierBindings)) {
          target.field.tierBindings = target.field.tierBindings.map((b) => (b ? { ...b } : b));
          delete target.field.tierBindings[target.tierIndex];
          if (target.field.tierBindings.every((b) => !b || !b.plcBinding)) {
            delete target.field.tierBindings;
          }
        }
        if (Array.isArray(target.field.tierVariables)) {
          target.field.tierVariables = [...target.field.tierVariables];
          target.field.tierVariables[target.tierIndex] = '';
        }
      } else if (target.kind === 'trend') {
        delete target.field[target.bindingProp];
      } else {
        delete target.object.plcBinding;
      }
      editor()?.commitHistory?.();
      syncDirty();
      schedule();
      return;
    }

    const variable = variables.find((v) => v.name === variableName);
    if (!variable) return;

    if (target.kind === 'node') {
      const kind = variable.hmiTypes.includes('bool') ? 'bool' : INTEGER_TYPES.has(variable.iecType) ? 'int32' : 'float';
      const name = uniqueName(sanitize(variable.name), target.object);
      const binding = { var: variable.name, access: target.wants?.rw ? 'RW' : 'R' };
      const allowed = target.object.type === 'EDIT_NUM'
        ? ['float', 'int32']
        : target.object.type === 'EDIT_BOOL'
          ? ['bool']
          : ['float', 'int32', 'bool'];
      menuApi()?.mutateSelectedNode?.((node) => {
        if (variable.hmiTypes.length && allowed.includes(kind) && node.valueType !== kind) {
          node.valueType = kind;
        }
        node.variable = name;
        node.plcBinding = binding;
      }, true);
      syncDirty();
      schedule();
      return;
    }

    if (target.kind === 'tier') {
      const name = uniqueName(sanitize(variable.name), target.field, `tier:${target.tierIndex}`);
      target.field.tierBindings = Array.isArray(target.field.tierBindings)
        ? target.field.tierBindings.map((b) => (b ? { ...b } : b))
        : [];
      target.field.tierBindings[target.tierIndex] = {
        plcBinding: { var: variable.name, access: target.wants?.rw ? 'RW' : 'R' }
      };
      if (input.value !== name) {
        fire(input, name);
      } else {
        target.field.tierVariables = Array.isArray(target.field.tierVariables) ? [...target.field.tierVariables] : [];
        target.field.tierVariables[target.tierIndex] = name;
        editor()?.commitHistory?.();
      }
      syncDirty();
      schedule();
      return;
    }

    if (target.kind === 'trend') {
      const name = uniqueName(sanitize(variable.name), target.field, target.varProp);
      target.field[target.bindingProp] = { var: variable.name, access: 'R' };
      if (input.value !== name) fire(input, name);
      else editor()?.commitHistory?.();
      syncDirty();
      schedule();
      return;
    }

    // ICON: ajuste de iconVarType según la variable PLC (bool vs uint8_t).
    if (target.field.type === 'ICON' && variable.hmiTypes.length) {
      if (variable.hmiTypes.includes('bool') && !INTEGER_TYPES.has(variable.iecType)) {
        target.field.iconVarType = 'bool';
      } else if (INTEGER_TYPES.has(variable.iecType)) {
        target.field.iconVarType = 'uint8';
      }
      window.JWPLCHMIIcons?.sync?.();
    }

    const name = uniqueName(sanitize(variable.name), target.object);
    target.object.plcBinding = { var: variable.name, access: target.wants?.rw ? 'RW' : 'R' };
    const liveInput = target.kind === 'field'
      ? (target.field.type === 'ICON' ? document.querySelector('#iconInspectorSection input[data-icon-prop="variable"]') : document.getElementById('fieldVariable'))
      : document.getElementById('barPlcVariable');
    if (liveInput && liveInput.value !== name) fire(liveInput, name);
    else editor()?.commitHistory?.();

    syncDirty();
    schedule();
  }

  function findVariableInputs() {
    return [
      ...document.querySelectorAll('.insp-panel[data-insp-panel="data"] input[data-proxy="fieldVariable"]'),
      ...document.querySelectorAll('#barPlcVariable'),
      ...document.querySelectorAll('[data-menu-node-editor] input[data-node-prop="variable"]'),
      ...document.querySelectorAll('#iconInspectorSection input[data-icon-prop="variable"]'),
      ...document.querySelectorAll('#iconInspectorSection input[data-tier-prop="variable"]'),
      ...document.querySelectorAll('#trendInspectorSection [data-trend-prop="variable"], #trendInspectorSection [data-trend-prop="ch2Variable"], #trendInspectorSection [data-trend-prop="ch3Variable"]'),
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
    // En OpenPLC los enlaces salen de Variable PLC, no de copiar variables C++ de TREND.
    document.querySelectorAll('[data-fi-link-trend-wrap], #fieldLinkTrendWrap').forEach((el) => {
      el.hidden = true;
    });

    for (const input of findVariableInputs()) {
      const target = describeTarget(input);
      if (!target) continue;
      input.readOnly = true;
      input.title = 'En un proyecto OpenPLC el nombre C++ se genera a partir de la variable PLC.';

      // El selector de cada input se recuerda (el de BAR y TREND no están dentro de un <label>).
      let box = bindingBoxes.get(input);
      if (!box?.isConnected) {
        box = document.createElement('div');
        bindingBoxes.set(input, box);
        box.className = 'jwplc-plc-binding';
        const channelLabel = target.kind === 'trend'
          ? (target.varProp === 'ch2Variable' ? 'Variable PLC · Canal 2' : target.varProp === 'ch3Variable' ? 'Variable PLC · Canal 3' : 'Variable PLC · Canal 1')
          : 'Variable PLC';
        box.innerHTML =
          `<label class="field-label">${channelLabel} <select class="field-input jwplc-plc-select"></select></label>` +
          '<div class="insp-note jwplc-plc-note"></div>';
        // Antes del <label> del input; si no tiene (BAR/TREND), antes de su fila.
        const row = input.parentElement && input.parentElement.children.length > 1 ? input.parentElement : input;
        (input.closest('label') || row).before(box);
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
      // Con la ventana del editor abierta, la vista de enlaces es de sólo lectura.
      const editorLocked = document.documentElement.classList.contains('jwplc-editor-open');
      const channelDisabled = target.kind === 'trend' && !target.enabled;
      select.disabled = editorLocked || channelDisabled;
      renderOptions(select, target);
      const bound = target.binding?.var || '';
      const match = bound ? variables.find((v) => v.name.toLowerCase() === bound.toLowerCase()) : null;
      const isCompatible = Boolean(match && compatible(match, target.wants));
      const value = isCompatible ? match.name : '';
      if (select.value !== value) select.value = value;
      const text = channelDisabled
        ? 'Canal desactivado.'
        : !bound
          ? 'Sin enlazar: elige una variable del PLC.'
          : !match
            ? `La variable "${bound}" ya no existe en el PLC.`
            : !isCompatible
              ? `La variable "${match.name}" (${match.iecType}) ya no es compatible (${incompatibleReason(match, target.wants)}).`
              : `Enlazada a ${match.name}${target.binding.access === 'RW' ? ' (lectura y escritura)' : ''}.`;
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
    setInterval(syncDirty, DIRTY_POLL_MS);
    window.addEventListener('jwplc:editor-refresh', syncDirty);
    window.addEventListener('jwplc:project-loaded', () => setTimeout(syncDirty, 0));
    window.addEventListener('jwplc:project-written', syncDirty);
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
