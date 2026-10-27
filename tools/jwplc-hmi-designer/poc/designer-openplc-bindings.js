/*
 * Modo enlaces del Designer dentro de OpenPLC (contrato jwplc-hmi-host/1.4).
 *
 * OpenPLC muestra en su pestaña «HMI Designer» una versión recortada del
 * Designer (desktop.html?host=openplc&view=enlaces): vista previa, pestañas de
 * páginas, árbol de objetos y, en el inspector, sólo «Datos» para elegir la
 * «Variable PLC» de cada objeto. El diseño se edita en la ventana completa
 * («Abrir HMI Designer para diseñar» → JWPLCHMIHost.openEditor()).
 *
 * Aquí no se dibuja, no se mueven ni borran objetos y no se crean páginas:
 *   - se ocultan las herramientas de dibujo, «Nuevo/Abrir/Guardar», «Actualizar
 *     HMI», Live, las pestañas Diseño/Técnico y el panel de código;
 *   - el botón de arriba abre la ventana completa (también tras cerrarla) o la
 *     trae al frente si sigue abierta;
 *   - un clic en el lienzo sólo selecciona; arrastrar, doble clic y las teclas
 *     de edición no hacen nada;
 *   - mientras la ventana completa está abierta (onEditorWindow) la vista queda
 *     en sólo lectura: OpenPLC la recarga cuando la ventana guarda.
 * El guardado lo hace OpenPLC (Ctrl+S, Guardar proyecto, antes de compilar).
 */
(() => {
  'use strict';

  const host = window.JWPLCHMIHost;
  if (!host || host.mode !== 'openplc') return;
  if (new URLSearchParams(location.search).get('view') !== 'enlaces') return;

  const editor = () => window.JWPLCHMIEditor || null;
  document.documentElement.classList.add('jwplc-view-bindings');

  const style = document.createElement('style');
  style.id = 'jwplcBindingsViewStyle';
  style.textContent = `
    .jwplc-view-bindings .canvas-toolbar,
    .jwplc-view-bindings #newProjectButton,
    .jwplc-view-bindings #openProjectButton,
    .jwplc-view-bindings #saveProjectButton,
    .jwplc-view-bindings #updateHmiButton,
    .jwplc-view-bindings #linkSketchButton,
    .jwplc-view-bindings #liveConnectButton,
    .jwplc-view-bindings #installAppButton,
    .jwplc-view-bindings #generateButton,
    .jwplc-view-bindings .page-tabs .add-tab,
    .jwplc-view-bindings .page-delete-btn,
    .jwplc-view-bindings .page-tab-edit-icon,
    .jwplc-view-bindings #objectsList .object-btn,
    .jwplc-view-bindings .inspector-actions,
    .jwplc-view-bindings .inspector-delete,
    .jwplc-view-bindings #barTechDuplicateBtn,
    .jwplc-view-bindings #barTechDeleteBtn,
    .jwplc-view-bindings .jwplc-insp-tabs-hidden,
    .jwplc-view-bindings [data-menu-action="duplicate"],
    .jwplc-view-bindings [data-menu-action="delete"],
    .jwplc-view-bindings .menu-tree-btns,
    .jwplc-view-bindings #idleConfigSection,
    .jwplc-view-bindings .jwplc-menu-design-only { display: none !important; }
    /* Sin panel de código: su fila del grid queda en 0. */
    html.jwplc-view-bindings body, html.jwplc-view-bindings body.bottom-collapsed {
      grid-template-rows: 58px minmax(0, 1fr) 0 28px !important; }
    @media (max-height: 820px) {
      html.jwplc-view-bindings body, html.jwplc-view-bindings body.bottom-collapsed {
        grid-template-rows: 52px minmax(0, 1fr) 0 26px !important; }
    }
    .jwplc-view-bindings .code-panel { height: 0 !important; min-height: 0 !important; border: 0 !important;
      overflow: hidden !important; visibility: hidden; }
    .jwplc-view-bindings #objectsList [draggable] { cursor: pointer; }
    .jwplc-bindings-bar { display: inline-flex; align-items: center; gap: 10px; margin-left: auto; }
    .jwplc-bindings-chip { font-size: 12px; color: #9fb3c2; white-space: nowrap; }
    .jwplc-bindings-edit { height: 32px; padding: 0 14px; border-radius: 8px; border: 1px solid #ff9a43;
      background: rgba(255, 154, 67, .14); color: #ffb877; font-weight: 600; cursor: pointer; white-space: nowrap; }
    .jwplc-bindings-edit:hover { background: rgba(255, 154, 67, .24); }
    .jwplc-bindings-lock { display: none; margin: 8px 10px 0; padding: 8px 10px; border-radius: 8px;
      border: 1px solid #3a5568; background: #142430; color: #cfe0ea; font-size: 12px; line-height: 1.4; }
    .jwplc-editor-open .jwplc-bindings-lock { display: block; }
    .jwplc-bindings-empty { margin: 14px 12px; padding: 14px; border: 1px dashed #3a5568; border-radius: 10px;
      color: #9fb3c2; font-size: 13px; line-height: 1.45; }
    .jwplc-editor-open .jwplc-plc-select { pointer-events: none; opacity: .55; }
  `;
  document.head.appendChild(style);

  // --- Barra superior: qué es esta vista y cómo editar el diseño -------------------

  function ensureBar() {
    if (document.querySelector('.jwplc-bindings-bar')) return;
    const anchor = document.getElementById('undoButton')?.closest('nav, header, .toolbar') || document.querySelector('nav.toolbar');
    if (!anchor) return;
    const bar = document.createElement('div');
    bar.className = 'jwplc-bindings-bar';
    bar.innerHTML =
      '<span class="jwplc-bindings-chip">Vista de enlaces · elige la variable PLC de cada objeto</span>' +
      '<button type="button" class="jwplc-bindings-edit" id="jwplcEditDesignButton"></button>';
    anchor.appendChild(bar);
    bar.querySelector('button').addEventListener('click', () => host.openEditor?.());
    syncEditButton();
  }

  // Abre la ventana del Designer (también después de cerrarla) o la trae al frente.
  function syncEditButton() {
    const button = document.getElementById('jwplcEditDesignButton');
    if (!button) return;
    const open = document.documentElement.classList.contains('jwplc-editor-open');
    const text = open ? 'Ir a la ventana del HMI Designer' : 'Abrir HMI Designer para diseñar';
    if (button.textContent !== text) button.textContent = text;
    button.title = open
      ? 'El diseño está abierto en su ventana: tráela al frente.'
      : 'Abre el HMI Designer completo en su ventana para dibujar la pantalla.';
  }

  function ensureLockNote() {
    const panel = document.getElementById('objectsNavBlock');
    if (!panel || panel.previousElementSibling?.classList?.contains('jwplc-bindings-lock')) return;
    const note = document.createElement('div');
    note.className = 'jwplc-bindings-lock';
    note.textContent = 'El diseño está abierto en la ventana del HMI Designer. Esta vista se actualiza cuando guardas allí.';
    panel.before(note);
  }

  // --- Inspector: siempre en «Datos» ---------------------------------------------

  // Pestaña de datos de cada inspector (campos, BAR, MENU, ICON) y las que se ocultan.
  const DATA_TABS = [
    { data: '#fieldInspectorV2 [data-insp-main="data"]', hide: '[data-insp-main="design"], [data-insp-main="tech"]' },
    { data: '.bar-main-tab[data-main-tab="plc"]', hide: '.bar-main-tab[data-main-tab="design"], .bar-main-tab[data-main-tab="tech"]' },
    { data: '[data-menu-main="tree"]', hide: '[data-menu-main="design"], [data-menu-main="tech"]' },
    { data: '[data-icon-main="data"]', hide: '[data-icon-main="design"], [data-icon-main="tech"]' }
  ];

  function showDataTab() {
    for (const tab of DATA_TABS) {
      document.querySelectorAll(tab.data).forEach((data) => {
        if (!data.offsetParent) return;
        if (!data.classList.contains('active') && data.getAttribute('aria-selected') !== 'true') data.click();
        // Con una sola pestaña, la fila de pestañas sobra.
        const row = data.parentElement;
        if (row && !row.classList.contains('jwplc-insp-tabs-hidden')) row.classList.add('jwplc-insp-tabs-hidden');
      });
    }
  }

  // MENU: del árbol sólo se elige el nodo; en su editor queda sólo la variable.
  function trimMenuEditor() {
    document.querySelectorAll('[data-menu-add]').forEach((button) => {
      button.closest('.bar-card')?.classList.add('jwplc-menu-design-only');
    });
    document.querySelectorAll('[data-menu-node-editor]').forEach((box) => {
      // Queda el título del nodo, «Variable PLC» y su nombre C++; el resto es diseño.
      [...box.children].forEach((child, index) => {
        const keep = index === 0 || child.matches('.jwplc-plc-binding') ||
          child.querySelector('.jwplc-plc-binding, [data-node-prop="variable"]');
        child.classList.toggle('jwplc-menu-design-only', !keep);
      });
      box.querySelectorAll('[data-node-prop]:not([data-node-prop="variable"])').forEach((input) => {
        input.closest('.field-label, label')?.classList.add('jwplc-menu-design-only');
      });
    });
  }

  // Sin objeto elegido: en vez de la configuración del proyecto, qué hacer aquí.
  function ensureEmptyHint() {
    const idle = document.getElementById('idleConfigSection');
    if (!idle) return;
    let hint = document.getElementById('jwplcBindingsEmpty');
    if (!hint) {
      hint = document.createElement('div');
      hint.id = 'jwplcBindingsEmpty';
      hint.className = 'jwplc-bindings-empty';
      hint.textContent = 'Elige un objeto en «Objetos» o en el lienzo para enlazarlo a una variable del PLC.';
      idle.after(hint);
    }
    hint.hidden = editor()?.hasFieldSelection?.() === true;
  }

  // --- Lienzo: un clic selecciona, nada más ----------------------------------------

  const canvasViewport = () => document.getElementById('canvasViewport');
  const displayCanvas = () => document.getElementById('displayCanvas');

  function pointOnScreen(event) {
    const canvas = displayCanvas();
    const rect = canvas.getBoundingClientRect();
    const zoom = editor()?.getZoom?.() || 1;
    return {
      x: Math.floor(((event.clientX - rect.left) * (canvas.width / rect.width)) / zoom),
      y: Math.floor(((event.clientY - rect.top) * (canvas.height / rect.height)) / zoom)
    };
  }

  const PASS_THROUGH = '.right-vertical-toolbar, button, input, select, textarea, label';

  window.addEventListener('pointerdown', (event) => {
    const viewport = canvasViewport();
    if (!viewport || !viewport.contains(event.target) || event.target.closest(PASS_THROUGH)) return;
    if (event.button !== 0) return; // clic derecho / central: desplazar la vista
    event.stopImmediatePropagation();
    event.preventDefault();
    if (event.target !== displayCanvas()) return;
    const field = editor()?.hitTestField?.(pointOnScreen(event));
    editor()?.setSelectedFieldKeys?.(field ? [field.key] : []);
  }, true);

  // Sin doble clic (renombrar páginas, ajustar a un objeto) ni arrastrar en el árbol.
  window.addEventListener('dblclick', (event) => {
    if (event.target.closest('.page-tabs, #objectsList, #canvasViewport')) {
      event.stopImmediatePropagation();
      event.preventDefault();
    }
  }, true);
  window.addEventListener('dragstart', (event) => {
    if (event.target.closest?.('#objectsList')) event.preventDefault();
  }, true);

  // Teclado: sólo navegación, deshacer/rehacer y guardar.
  const ALLOWED_PLAIN = new Set(['Tab', 'Escape', 'Shift', 'Control', 'Alt', 'Meta']);
  window.addEventListener('keydown', (event) => {
    const target = event.target;
    if (target?.closest?.('input, select, textarea, [contenteditable="true"]')) return;
    const key = event.key;
    if (ALLOWED_PLAIN.has(key) || /^F\d+$/.test(key)) return;
    if ((event.ctrlKey || event.metaKey) && /^[szy]$/i.test(key)) return;
    event.stopImmediatePropagation();
    event.preventDefault();
  }, true);

  // --- Ventana completa abierta: sólo lectura --------------------------------------

  function setEditorOpen(open) {
    document.documentElement.classList.toggle('jwplc-editor-open', Boolean(open));
    document.querySelectorAll('.jwplc-plc-select').forEach((select) => { select.disabled = Boolean(open); });
    syncEditButton();
  }
  host.onEditorWindow?.(setEditorOpen);

  let scheduled = false;
  function refresh() {
    scheduled = false;
    ensureBar();
    ensureLockNote();
    showDataTab();
    trimMenuEditor();
    ensureEmptyHint();
    if (document.documentElement.classList.contains('jwplc-editor-open')) setEditorOpen(true);
  }
  function schedule() {
    if (scheduled) return;
    scheduled = true;
    requestAnimationFrame(refresh);
  }

  function init() {
    new MutationObserver(schedule).observe(document.body, { childList: true, subtree: true });
    window.addEventListener('jwplc:editor-refresh', schedule);
    schedule();
    // La pantalla completa a la vista (el lienzo arranca con el zoom del editor).
    setTimeout(() => editor()?.fitCanvas?.(), 600);
    window.addEventListener('resize', () => editor()?.fitCanvas?.());
  }

  window.JWPLCHMIBindingsView = { setEditorOpen };

  if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', init, { once: true });
  else init();
})();
