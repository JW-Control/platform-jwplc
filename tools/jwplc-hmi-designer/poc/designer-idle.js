(() => {
  'use strict';

  // A15-7: "Pantalla y arranque" — cómo se entra y se vuelve de la pantalla
  // IDLE (estado del equipo) a las páginas del HMI.
  //
  // Configuración de proyecto (se guarda en el .jwhmi como `display`) que se
  // emite al final de jwplcHMISetup() con la API pública de JWPLC_Display:
  //   setIdleWakeMode / setIdleWakeButton       IDLE -> HMI
  //   setIdleReturnMode / setIdleReturnButton    HMI -> IDLE
  //   setIdleTimeoutMs                           vuelta por inactividad
  //   enterUserUI()                              arrancar en el HMI
  //
  // Firmware (JWPLC_Display.cpp:handleIdleWakeAndTimeout): con vuelta "por
  // inactividad" ESC no vuelve a la IDLE; con varias páginas, ESC primero
  // sale del contenido al selector de páginas (Alpha11).

  const BUTTONS = [
    { id: 'BTN_UP', label: '▲', name: 'Arriba', cell: 'up' },
    { id: 'BTN_LEFT', label: '◀', name: 'Izquierda', cell: 'left' },
    { id: 'BTN_OK', label: 'OK', name: 'OK', cell: 'ok' },
    { id: 'BTN_RIGHT', label: '▶', name: 'Derecha', cell: 'right' },
    { id: 'BTN_DOWN', label: '▼', name: 'Abajo', cell: 'down' },
    { id: 'BTN_ESC', label: 'ESC', name: 'ESC', cell: 'esc' }
  ];
  const BUTTON_IDS = new Set(BUTTONS.map((b) => b.id));
  const TIMEOUTS_S = [10, 15, 30, 60, 120];

  const DEFAULTS = Object.freeze({
    start: 'IDLE',          // IDLE | HMI
    wake: 'ANY',            // ANY | BUTTON | NONE
    wakeButton: 'BTN_OK',
    back: 'ESC',            // ESC | TIMEOUT | NONE (BUTTON retirado: chocaba con ◀ ▶ del selector)
    timeoutS: 15
  });

  let config = { ...DEFAULTS };

  function normalize(raw) {
    const c = { ...DEFAULTS, ...(raw && typeof raw === 'object' ? raw : {}) };
    if (!['IDLE', 'HMI'].includes(c.start)) c.start = DEFAULTS.start;
    if (!['ANY', 'BUTTON', 'NONE'].includes(c.wake)) c.wake = DEFAULTS.wake;
    // "Con otra tecla" se retiró (se cruzaba con LEFT/RIGHT/OK del selector de
    // páginas y del contenido); un proyecto que lo tenía pasa a ESC.
    if (!['ESC', 'TIMEOUT', 'NONE'].includes(c.back)) c.back = DEFAULTS.back;
    if (!BUTTON_IDS.has(c.wakeButton)) c.wakeButton = DEFAULTS.wakeButton;
    delete c.backButton;
    c.timeoutS = Math.max(1, Math.min(3600, Math.trunc(Number(c.timeoutS) || DEFAULTS.timeoutS)));
    return c;
  }

  // --- Codegen ----------------------------------------------------------------

  const MARKER = '    // Pantalla IDLE (Designer · Pantalla y arranque)\n';

  function setupLines(c = config) {
    const D = 'JWPLC_DisplayClass::';
    const lines = [];
    lines.push(`    JWPLC_Display.setIdleWakeMode(${D}${{ ANY: 'IDLE_WAKE_ANY_BUTTON', BUTTON: 'IDLE_WAKE_BUTTON_ONLY', NONE: 'IDLE_WAKE_DISABLED' }[c.wake]});`);
    if (c.wake === 'BUTTON') lines.push(`    JWPLC_Display.setIdleWakeButton(${c.wakeButton});`);
    lines.push(`    JWPLC_Display.setIdleReturnMode(${D}${{ ESC: 'IDLE_RETURN_ESC_ONLY', TIMEOUT: 'IDLE_RETURN_TIMEOUT', NONE: 'IDLE_RETURN_DISABLED' }[c.back]});`);
    if (c.back === 'TIMEOUT') lines.push(`    JWPLC_Display.setIdleTimeoutMs(${c.timeoutS * 1000}UL);`);
    if (c.start === 'HMI') lines.push('    JWPLC_Display.enterUserUI();');
    return `${MARKER}${lines.join('\n')}\n`;
  }

  // Inserta el bloque al final de jwplcHMISetup() (después de páginas, campos
  // y menús). Idempotente: si ya está, lo reemplaza.
  function patchHeader(text) {
    const head = text.indexOf('void jwplcHMISetup()\n{\n');
    if (head < 0) return text;
    const bodyStart = head + 'void jwplcHMISetup()\n{\n'.length;
    const end = text.indexOf('\n}\n', bodyStart);
    if (end < 0) return text;
    let body = text.slice(bodyStart, end + 1);
    const at = body.indexOf(MARKER);
    if (at >= 0) body = body.slice(0, at);
    body = body.replace(/\n*$/, '\n');
    return `${text.slice(0, bodyStart)}${body}\n${setupLines()}${text.slice(end + 1)}`;
  }

  window.JWPLCHMIIdle = {
    get: () => ({ ...config }),
    set: (raw) => { config = normalize(raw); sync(); },
    defaults: () => ({ ...DEFAULTS }),
    setupLines,
    patchHeader
  };

  // --- Inspector ---------------------------------------------------------------

  const panel = document.querySelector('.right-panel');
  if (!panel) return;

  function editor() {
    return window.JWPLCHMIEditor || null;
  }

  function screenSvg(kind) {
    // Mini pantalla 320×170: IDLE (estado de E/S) o una página del HMI.
    const idle = '<rect x="6" y="6" width="9" height="5" rx="1" fill="currentColor" opacity="0.9"/><rect x="17" y="6" width="9" height="5" rx="1" fill="currentColor" opacity="0.35"/><rect x="28" y="6" width="9" height="5" rx="1" fill="currentColor" opacity="0.9"/><rect x="6" y="14" width="31" height="2" fill="currentColor" opacity="0.5"/>';
    const hmi = '<rect x="6" y="5" width="14" height="3" fill="currentColor" opacity="0.6"/><rect x="6" y="10" width="20" height="6" fill="currentColor"/><rect x="29" y="12" width="8" height="4" fill="currentColor" opacity="0.6"/>';
    return `<svg width="44" height="22" viewBox="0 0 44 22"><rect x="1" y="1" width="42" height="20" rx="2" stroke-width="1.2" fill="none" stroke="currentColor"/>${kind === 'IDLE' ? idle : hmi}</svg>`;
  }

  function keypad(group, selected) {
    return `<div class="idle-keypad" data-idle-keypad="${group}">${BUTTONS.map((b) =>
      `<button type="button" class="idle-key idle-key-${b.cell}${b.id === selected ? ' active' : ''}" data-idle-key="${group}" data-value="${b.id}" title="${b.name} (${b.id})">${b.label}</button>`).join('')}</div>`;
  }

  function buttonName(id) {
    return BUTTONS.find((b) => b.id === id)?.name || id;
  }

  const section = document.createElement('section');
  section.id = 'idleConfigSection';
  section.className = 'inspector-section';
  section.hidden = true;
  section.innerHTML = `
    <div class="inspector-title"><h2>Proyecto · Pantalla y arranque</h2></div>
    <div class="insp-body">
      <div class="bar-card" data-idle-card="flow" style="order: 1;">
        <div class="bar-card-section-title">Flujo de pantallas</div>
        <div class="idle-flow" data-idle-flow></div>
      </div>
      <div class="bar-card" data-idle-card="start" style="order: 2;">
        <div class="bar-card-section-title">Al encender</div>
        <div class="bar-visual-grid-2">
          <button type="button" class="bar-visual-btn" data-idle-set="start" data-value="IDLE">${screenSvg('IDLE')}<span>Pantalla IDLE (Estado)</span></button>
          <button type="button" class="bar-visual-btn" data-idle-set="start" data-value="HMI">${screenSvg('HMI')}<span>Primera página del HMI</span></button>
        </div>
      </div>
      <div class="bar-card" data-idle-card="wake" style="order: 3;">
        <div class="bar-card-section-title" data-idle-title="wake">Entrar al HMI desde la IDLE</div>
        <div class="bar-visual-grid-3">
          <button type="button" class="bar-visual-btn" data-idle-set="wake" data-value="ANY"><strong class="insp-mono">✱</strong><span>Cualquier tecla</span></button>
          <button type="button" class="bar-visual-btn" data-idle-set="wake" data-value="BUTTON"><strong class="insp-mono">⌗</strong><span>Una tecla</span></button>
          <button type="button" class="bar-visual-btn" data-idle-set="wake" data-value="NONE"><strong class="insp-mono">⊘</strong><span>Sólo desde el sketch</span></button>
        </div>
        <div class="idle-keypad-row" data-idle-show="wake:BUTTON">
          <span class="bar-visual-group-title">Tecla para entrar</span>
          <div data-idle-keypad-host="wake"></div>
        </div>
        <div class="insp-note" data-idle-show="wake:NONE">El sketch entra con <code>JWPLC_Display.enterUserUI()</code>.</div>
        <div class="insp-note" data-idle-wake-hint hidden></div>
      </div>
      <div class="bar-card" data-idle-card="back" style="order: 4;">
        <div class="bar-card-section-title" data-idle-title="back">Volver a la IDLE</div>
        <div class="bar-visual-grid-3">
          <button type="button" class="bar-visual-btn" data-idle-set="back" data-value="ESC"><strong class="insp-mono">ESC</strong><span>Con ESC</span></button>
          <button type="button" class="bar-visual-btn" data-idle-set="back" data-value="TIMEOUT"><strong class="insp-mono">⏱</strong><span>Por inactividad</span></button>
          <button type="button" class="bar-visual-btn" data-idle-set="back" data-value="NONE"><strong class="insp-mono">⊘</strong><span>Nunca</span></button>
        </div>
        <div class="insp-note" data-idle-show="back:ESC" data-idle-pages-note>ESC retrocede paso a paso en el HMI: edición → submenú → selector de páginas (NN/TT) → y finalmente sale a la IDLE.</div>
        <div data-idle-show="back:TIMEOUT">
          <span class="bar-visual-group-title">Sin tocar teclas durante</span>
          <div class="insp-chip-row">${TIMEOUTS_S.map((s) => `<button type="button" class="insp-chip" data-idle-set="timeoutS" data-value="${s}">${s < 60 ? `${s} s` : `${s / 60} min`}</button>`).join('')}</div>
          <div class="insp-note">Vuelve en cualquier sección (selector o contenido). En este modo ESC no vuelve a la IDLE: sólo retrocede dentro del HMI.</div>
        </div>
        <div class="insp-note" data-idle-show="back:NONE" data-idle-back-none-note></div>
      </div>
      <div class="bar-card" data-idle-card="code" style="order: 5;">
        <div class="bar-card-header"><span class="bar-card-title">Se genera en jwplcHMISetup()</span></div>
        <pre class="insp-contract" data-idle-code></pre>
      </div>
    </div>`;
  const anchor = panel.querySelector('.pixel-controls');
  panel.insertBefore(section, anchor || null);

  function render() {
    section.querySelectorAll('[data-idle-set]').forEach((b) =>
      b.classList.toggle('active', String(config[b.dataset.idleSet]) === b.dataset.value));
    section.querySelectorAll('[data-idle-show]').forEach((el) => {
      const [key, value] = el.dataset.idleShow.split(':');
      el.hidden = config[key] !== value;
    });
    ['wake'].forEach((group) => {
      const host = section.querySelector(`[data-idle-keypad-host="${group}"]`);
      const selected = config.wakeButton;
      if (host.dataset.selected !== selected) {
        host.dataset.selected = selected;
        host.innerHTML = keypad(group, selected);
      }
    });

    const isStartHmi = config.start === 'HMI';
    const wakeCard = section.querySelector('[data-idle-card="wake"]');
    const backCard = section.querySelector('[data-idle-card="back"]');
    const wakeTitle = section.querySelector('[data-idle-title="wake"]');
    const backTitle = section.querySelector('[data-idle-title="back"]');
    const wakeHint = section.querySelector('[data-idle-wake-hint]');
    const backNoneNote = section.querySelector('[data-idle-back-none-note]');

    // Reordenamiento contextual de tarjetas según el punto de arranque:
    if (isStartHmi) {
      if (backCard) backCard.style.order = '3';
      if (wakeCard) wakeCard.style.order = '4';
      if (backTitle) backTitle.textContent = 'Salir a la pantalla de estado (IDLE)';
      if (wakeTitle) wakeTitle.textContent = 'Reingresar al HMI desde la IDLE';
      if (wakeHint) {
        const isBackNone = config.back === 'NONE';
        wakeHint.hidden = !isBackNone;
        if (isBackNone) {
          wakeHint.innerHTML = 'Aplica sólo si el sketch pasa a la pantalla IDLE mediante <code>JWPLC_Display.goIdle()</code>.';
        }
      }
      if (backNoneNote) {
        backNoneNote.innerHTML = 'El HMI permanece siempre activo en pantalla. La pantalla de diagnóstico del PLC no se mostrará automáticamente (salvo si el sketch llama a <code>JWPLC_Display.goIdle()</code> o desde un menú).';
      }
    } else {
      if (wakeCard) wakeCard.style.order = '3';
      if (backCard) backCard.style.order = '4';
      if (wakeTitle) wakeTitle.textContent = 'Entrar al HMI desde la IDLE';
      if (backTitle) backTitle.textContent = 'Volver a la IDLE';
      if (wakeHint) wakeHint.hidden = true;
      if (backNoneNote) {
        backNoneNote.innerHTML = 'Sólo se vuelve a la IDLE con un ítem «⏎ Ir al inicio» de un MENU (Árbol y datos) o desde el sketch con <code>JWPLC_Display.goIdle()</code>.';
      }
    }

    // Con una sola página no hay selector: ESC vuelve desde la raíz del contenido.
    const pages = editor()?.getPages?.().length || 1;
    const escNote = section.querySelector('[data-idle-pages-note]');
    if (!escNote.hidden) {
      escNote.textContent = pages > 1
        ? 'ESC retrocede paso a paso en el HMI: edición → submenú → selector de páginas (NN/TT) → y finalmente sale a la IDLE.'
        : 'ESC retrocede paso a paso: edición → submenú → raíz → y finalmente sale a la IDLE (con una sola página no hay selector NN/TT).';
    }

    // Diagrama "Flujo de pantallas":
    // El nodo inicial (marcado como inicio) siempre va a la izquierda para lectura natural de izquierda a derecha.
    const enterText = { ANY: 'cualquier tecla', BUTTON: `tecla ${buttonName(config.wakeButton)}`, NONE: 'enterUserUI()' }[config.wake];
    const backText = { ESC: 'con ESC', TIMEOUT: `${config.timeoutS} s inactivo`, NONE: 'sólo sketch / menú' }[config.back];

    if (isStartHmi) {
      // HMI (inicio) ---> IDLE ---> HMI
      const toIdleEdge = config.back === 'NONE'
        ? '<div class="idle-edge is-off">sin retorno auto <b>→</b></div>'
        : `<div class="idle-edge">${backText} <b>→</b></div>`;
      const toHmiEdge = `<div class="idle-edge"><b>←</b> ${enterText}</div>`;

      section.querySelector('[data-idle-flow]').innerHTML = `
        <div class="idle-node is-start">${screenSvg('HMI')}<span>HMI</span></div>
        <div class="idle-edges">
          ${toIdleEdge}
          ${toHmiEdge}
        </div>
        <div class="idle-node">${screenSvg('IDLE')}<span>IDLE</span></div>`;
    } else {
      // IDLE (inicio) ---> HMI ---> IDLE
      const toHmiEdge = `<div class="idle-edge">${enterText} <b>→</b></div>`;
      const toIdleEdge = config.back === 'NONE'
        ? '<div class="idle-edge is-off"><b>←</b> sin retorno auto</div>'
        : `<div class="idle-edge"><b>←</b> ${backText}</div>`;

      section.querySelector('[data-idle-flow]').innerHTML = `
        <div class="idle-node is-start">${screenSvg('IDLE')}<span>IDLE</span></div>
        <div class="idle-edges">
          ${toHmiEdge}
          ${toIdleEdge}
        </div>
        <div class="idle-node">${screenSvg('HMI')}<span>HMI</span></div>`;
    }

    section.querySelector('[data-idle-code]').textContent = setupLines().replace(MARKER, '').replace(/^ {4}/gm, '').trimEnd();
  }

  function sync() {
    const selected = editor()?.getSelectedField?.();
    const hasPixel = Boolean(window.JWPLCHMIPixelMaps?.getSelected?.());
    const currentTool = editor()?.getSelectedTool?.() || 'pointer';
    const isPixelTool = (currentTool === 'pixel' || currentTool === 'erase');
    const inspectorMode = window.JWPLCHMIInspectorState?.mode?.() || 'NONE';
    section.hidden = Boolean(selected) || hasPixel || isPixelTool || inspectorMode !== 'NONE';
    if (!section.hidden) render();
  }

  function update(patch) {
    config = normalize({ ...config, ...patch });
    render();
    window.JWPLCHMICodegen?.refresh?.();
    window.dispatchEvent(new CustomEvent('jwplc:editor-refresh'));
  }

  section.addEventListener('click', (e) => {
    const set = e.target.closest('[data-idle-set]');
    if (set) {
      update({ [set.dataset.idleSet]: set.dataset.idleSet === 'timeoutS' ? Number(set.dataset.value) : set.dataset.value });
      return;
    }
    const key = e.target.closest('[data-idle-key]');
    if (key) update({ wakeButton: key.dataset.value });
  });

  // Proyecto nuevo: valores por defecto (applyProject pulsa este botón antes de cargar).
  document.getElementById('newProjectButton')?.addEventListener('click', () => { config = { ...DEFAULTS }; sync(); });
  window.addEventListener('jwplc:editor-refresh', sync);
  setTimeout(sync, 0);
})();
