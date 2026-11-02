/*
 * Proyecto «Runtime» del Designer (Alpha16).
 *
 * Tercer tipo de proyecto, junto a Arduino IDE y OpenPLC: el diseño se muestra
 * como página HMI dentro de JWPLC_Runtime (programación en el panel, estilo
 * LOGO!). Cada objeto se enlaza a una variable del Runtime (I, Q, M, salida,
 * tiempo o cuenta de un bloque) en vez de a una variable C++ del sketch.
 *
 *   - Proyecto · Pantalla y arranque: tarjeta «Tipo de proyecto». En Runtime
 *     la pantalla IDLE es la RUN del Runtime, así que sus tarjetas se ocultan.
 *   - Datos: selector «Variable del Runtime» sobre el nombre de la variable
 *     (rt_q1, rt_b02_t...: ver designer-runtime-core.js).
 *   - Código: patchHeader() convierte el header (jwplcRuntimeHMISetup /
 *     PageEnter / Update). Se guarda en el .jwhmi como projectType.
 *
 * Dentro de OpenPLC (JWPLCHMIHost.mode === 'openplc') no se activa.
 */
(() => {
  'use strict';

  if (window.JWPLCHMIHost?.mode === 'openplc') return;
  const core = window.JWPLCHMIRuntimeCore;
  if (!core) return;

  const editor = () => window.JWPLCHMIEditor || null;
  let projectType = 'arduino';
  const boxes = new WeakMap();
  let scheduled = false;

  const escapeHtml = (text) => String(text).replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));
  const isRuntime = () => projectType === 'runtime';

  // --- Estilos -----------------------------------------------------------------
  const style = document.createElement('style');
  style.textContent = `
    .jwplc-rt-binding { margin-bottom: 8px; }
    .jwplc-rt-binding .insp-note { margin-top: 4px; }
    html.jwplc-project-runtime #idleConfigSection [data-idle-card] { display: none !important; }
    .jwplc-rt-type .bar-visual-btn span small { display: block; opacity: .7; font-size: 11px; margin-top: 2px; }
  `;
  document.head.appendChild(style);

  // --- Tarjeta «Tipo de proyecto» ---------------------------------------------
  const card = document.createElement('div');
  card.className = 'bar-card jwplc-rt-type';
  card.style.order = '0';
  card.innerHTML = `
    <div class="bar-card-section-title">Tipo de proyecto</div>
    <div class="bar-visual-grid-2">
      <button type="button" class="bar-visual-btn" data-rt-type="arduino"><strong class="insp-mono">.ino</strong><span>Arduino IDE<small>El sketch declara las variables</small></span></button>
      <button type="button" class="bar-visual-btn" data-rt-type="runtime"><strong class="insp-mono">RUN</strong><span>Runtime<small>Páginas del programa del panel</small></span></button>
    </div>
    <div class="insp-note" data-rt-note hidden>
      Página HMI del <code>JWPLC_Runtime</code>: en la pantalla RUN, ► abre la primera página y ◄ la última;
      dentro, ◄ ► pasan de página y ESC abre el menú del Runtime. Cada objeto toma su valor del programa
      (Datos &gt; Variable del Runtime). El sketch solo incluye el header y llama a <code>JWPLC_Runtime.begin()</code>.
      MENU y EVENT_LOG no se admiten (el Runtime tiene los suyos).
    </div>`;

  function attachCard() {
    const body = document.querySelector('#idleConfigSection .insp-body');
    if (body && card.parentElement !== body) body.prepend(card);
    return Boolean(body);
  }

  card.addEventListener('click', (event) => {
    const button = event.target.closest('[data-rt-type]');
    if (!button) return;
    set(button.dataset.rtType);
    editor()?.commitHistory?.();
  });

  function renderCard() {
    card.querySelectorAll('[data-rt-type]').forEach((b) => b.classList.toggle('active', b.dataset.rtType === projectType));
    card.querySelector('[data-rt-note]').hidden = !isRuntime();
    document.documentElement.classList.toggle('jwplc-project-runtime', isRuntime());
  }

  // --- Datos: «Variable del Runtime» -----------------------------------------
  function usedNames(except) {
    const names = new Set();
    for (const field of editor()?.getAllFields?.() || []) {
      if (field !== except && field.variable) names.add(String(field.variable).toLowerCase());
    }
    return names;
  }

  function uniqueName(base, except) {
    const used = usedNames(except);
    if (!used.has(base)) return base;
    for (let i = 2; ; i += 1) if (!used.has(`${base}_${i}`)) return `${base}_${i}`;
  }

  function variableInputs() {
    return [
      ...document.querySelectorAll('.insp-panel[data-insp-panel="data"] input[data-proxy="fieldVariable"]'),
      ...document.querySelectorAll('#barPlcVariable'),
      ...document.querySelectorAll('#iconInspectorSection input[data-icon-prop="variable"]')
    ];
  }

  function wantsFor(field) {
    if (!field) return null;
    if (field.type === 'BOOL') return 'bool';
    if (field.type === 'VALUE' || field.type === 'BAR') return 'number';
    if (field.type === 'ICON') return field.iconVarType === 'uint8' ? 'number' : 'bool';
    return null;
  }

  function fire(input, value) {
    input.value = value;
    input.dispatchEvent(new Event('input', { bubbles: true }));
    input.dispatchEvent(new Event('change', { bubbles: true }));
  }

  function ensureBox(input) {
    let box = boxes.get(input);
    if (box?.isConnected) return box;
    box = document.createElement('div');
    box.className = 'jwplc-rt-binding';
    box.innerHTML =
      '<label class="field-label">Variable del Runtime <select class="field-input jwplc-rt-select"></select></label>' +
      '<div class="insp-note jwplc-rt-note"></div>';
    const row = input.parentElement && input.parentElement.children.length > 1 ? input.parentElement : input;
    (input.closest('label') || row).before(box);
    boxes.set(input, box);
    box.querySelector('select').addEventListener('change', (event) => {
      const field = editor()?.getSelectedField?.();
      const symbol = event.target.value;
      if (!field || !symbol) return;
      fire(input, uniqueName(symbol, field));
      editor()?.commitHistory?.();
      schedule();
    });
    return box;
  }

  function renderBox(input) {
    const box = ensureBox(input);
    box.hidden = !isRuntime();
    if (!isRuntime()) return;
    const field = editor()?.getSelectedField?.();
    const wants = wantsFor(field);
    const select = box.querySelector('select');
    const note = box.querySelector('.jwplc-rt-note');
    if (!wants) {
      select.disabled = true;
      select.innerHTML = '<option>—</option>';
      select.dataset.signature = '';
      note.textContent = field?.type === 'TEXT'
        ? 'El Runtime no tiene variables de texto: usa VALUE (números), BOOL (estados) o BAR.'
        : 'Este objeto no se enlaza a variables del Runtime.';
      return;
    }
    select.disabled = false;
    const signature = wants;
    if (select.dataset.signature !== signature) {
      select.dataset.signature = signature;
      select.innerHTML = '<option value="">— Elegir —</option>' + core.catalog(wants, 32).map((group) =>
        `<optgroup label="${escapeHtml(group.label)}">${group.options.map((o) =>
          `<option value="${escapeHtml(o.symbol)}">${escapeHtml(o.label)}</option>`).join('')}</optgroup>`).join('');
    }
    const parsed = core.parseSymbol(field.variable);
    const base = parsed ? core.symbolFor(parsed.key) : '';
    if (select.value !== base) select.value = base;
    const text = parsed
      ? `Lee ${parsed.key} con ${core.expressionFor(field.variable)}.`
      : 'Sin enlazar: elige una variable del programa del Runtime.';
    if (note.textContent !== text) note.textContent = text;
  }

  function ensure() {
    scheduled = false;
    attachCard();
    renderCard();
    document.querySelectorAll('.insp-mode-chip').forEach((chip) => {
      const text = isRuntime() ? 'Runtime' : 'Arduino';
      if (chip.textContent !== text) chip.textContent = text;
    });
    // La nota de OpenPLC (A15-6) no aplica a un proyecto Runtime.
    document.querySelectorAll('.insp-note').forEach((note) => {
      if (note.textContent.includes('A15-6') && note.hidden !== isRuntime()) note.hidden = isRuntime();
    });
    variableInputs().forEach(renderBox);
  }

  function schedule() {
    if (scheduled) return;
    scheduled = true;
    requestAnimationFrame(ensure);
  }

  // --- Codegen -----------------------------------------------------------------
  function patchHeader(text) {
    if (!isRuntime()) return text;
    const pageCount = editor()?.getPages?.()?.length || 1;
    return core.toRuntimeHeader(text, { pageCount }).text;
  }

  // --- Proyecto ----------------------------------------------------------------
  function set(type) {
    const next = type === 'runtime' ? 'runtime' : 'arduino';
    if (next === projectType) return;
    projectType = next;
    ensure();
    window.JWPLCHMICodegen?.refresh?.();
    window.dispatchEvent(new CustomEvent('jwplc:project-type', { detail: { type: projectType } }));
  }

  window.addEventListener('jwplc:editor-refresh', schedule);
  new MutationObserver(schedule).observe(document.body, { childList: true, subtree: true });
  ensure();

  window.JWPLCHMIRuntime = {
    get: () => projectType,
    set,
    patchHeader
  };
})();
