(() => {
  'use strict';

  // Flujo de guardado (pedido del usuario, 2026-10-23):
  //
  //   Cambios sin guardar  se compara el diseño con lo último guardado o abierto
  //                        (zoom, selección o herramienta no cuentan). «Nuevo»,
  //                        «Abrir» y cerrar la app preguntan si guardar.
  //   Guardar / Ctrl+S     con sketch vinculado = «Escribir en sketch» (header,
  //                        .jwhmi y guía). Sin sketch: cuadro «Guardar» en
  //                        Documentos\Arduino; si ahí ya hay un sketch se vincula,
  //                        si no se ofrece crearlo o guardar sólo el .jwhmi.
  //   Abrir ▾              (escritorio) «Abrir archivo…» y los proyectos
  //                        recientes; la lista la guarda main.js.
  //
  // Los clics se toman en fase de captura en document (otros módulos clonan
  // los botones). Necesita designer-dialog.js y designer-sketch-menu.js.

  if (window.JWPLCHMISaveFlow) return;

  const native = window.JWPLCHMINative?.isNative ? window.JWPLCHMINative : null;
  const project = () => window.JWPLCHMIProject;
  const sketchMenu = () => window.JWPLCHMISketchMenu;
  const dialogs = () => window.JWPLCDialog;
  const wait = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

  function toast(message, kind = '') {
    let node = document.getElementById('a15SketchToast');
    if (!node) {
      node = document.createElement('div');
      node.id = 'a15SketchToast';
      node.className = 'sketch-toast';
      document.body.appendChild(node);
    }
    node.textContent = message;
    node.dataset.kind = kind;
    node.classList.add('is-visible');
    clearTimeout(toast.timer);
    toast.timer = setTimeout(() => node.classList.remove('is-visible'), Math.max(3400, message.length * 45));
  }

  // --- Cambios sin guardar -------------------------------------------------------

  // Lo que define el diseño: sin la fecha, el nombre ni la página activa.
  function fingerprint() {
    try {
      const data = project()?.serialize?.();
      if (!data) return null;
      delete data.savedAt;
      delete data.projectName;
      delete data.activePage;
      return JSON.stringify(data);
    } catch (_) {
      return null;
    }
  }

  let baseline = null;
  let applying = 0;

  function markSaved() {
    baseline = fingerprint();
    updateStatus();
  }

  function isDirty() {
    if (baseline === null) return false;
    const now = fingerprint();
    return now !== null && now !== baseline;
  }

  // Al abrir o empezar un proyecto: lo que queda en pantalla es «lo guardado».
  // Los PixelMaps se importan después de apply, por eso la espera.
  function markSavedSoon() {
    clearTimeout(markSavedSoon.timer);
    markSavedSoon.timer = setTimeout(markSaved, 600);
  }

  let statusTimer = null;
  function updateStatus() {
    project()?.setDirtyFlag?.(isDirty());
  }
  function scheduleStatus() {
    clearTimeout(statusTimer);
    statusTimer = setTimeout(updateStatus, 500);
  }

  // Pregunta antes de perder cambios. true = seguir con la acción.
  async function guardChanges(actionText) {
    if (!isDirty()) return true;
    const choice = await dialogs().choose(
      `El diseño tiene cambios sin guardar.\n\n¿Guardarlos antes de ${actionText}?`,
      [
        { id: 'cancel', label: 'Cancelar' },
        { id: 'discard', label: 'No guardar' },
        { id: 'save', label: 'Guardar', primary: true }
      ],
      { cancelId: 'cancel' }
    );
    if (choice === 'discard') return true;
    if (choice === 'save') return save();
    return false;
  }

  // --- Guardar -------------------------------------------------------------------

  async function linkedInfo() {
    try { return await sketchMenu()?.backend?.info?.(); } catch (_) { return null; }
  }

  function projectText(name) {
    const data = project().serialize();
    data.projectName = name;
    data.savedAt = new Date().toISOString();
    return `${JSON.stringify(data, null, 2)}\n`;
  }

  async function writeLinked() {
    const ok = await sketchMenu().writeToSketch();
    if (ok) markSaved();
    return ok;
  }

  // Sin sketch en el escritorio: elegir destino y crear/vincular el sketch.
  async function saveWithoutSketchNative() {
    const target = await native.chooseSaveTarget(sketchMenu().suggestedSketchName());
    if (target?.canceled) return false;
    if (!target?.ok) throw new Error(target?.error || 'No se pudo elegir dónde guardar.');

    if (target.kind === 'existing') {
      const info = await native.applySaveTarget('existing');
      if (!info?.ok) throw new Error(info?.error || 'No se pudo vincular el sketch.');
      window.dispatchEvent(new CustomEvent('jwplc:sketch-linked', { detail: { sketch: info.name } }));
      return writeLinked();
    }

    const reason = target.occupied
      ? `La carpeta «${target.name}» ya existe y tiene otros archivos: no se crea el sketch ahí.`
      : `«${target.name}» no sirve como nombre de sketch (letras, números, _ . -; empieza con letra o número, máximo 63).`;
    const choice = await dialogs().choose(
      target.canCreate
        ? `En «${target.folder}» no hay un sketch «${target.name}».\n\n¿Crear el sketch? Se crea la carpeta «${target.name}» con ${target.name}.ino (sketch base con las variables del HMI), se vincula este proyecto y se escriben el header, el .jwhmi y la guía.`
        : `${reason}\n\n¿Guardar sólo el proyecto ${target.name}.jwhmi, sin sketch?`,
      [
        { id: 'cancel', label: 'Cancelar' },
        { id: 'jwhmi-only', label: 'Solo guardar .jwhmi', primary: !target.canCreate },
        ...(target.canCreate ? [{ id: 'create', label: 'Crear sketch', primary: true }] : [])
      ],
      { cancelId: 'cancel' }
    );
    if (choice === 'create') {
      const info = await native.applySaveTarget('create', { inoText: await sketchMenu().baseSketchText() });
      if (!info?.ok) throw new Error(info?.error || 'No se pudo crear el sketch.');
      window.dispatchEvent(new CustomEvent('jwplc:sketch-linked', { detail: { sketch: info.name, created: true } }));
      sketchMenu().markOwn(info);
      return writeLinked();
    }
    if (choice === 'jwhmi-only') {
      const result = await native.applySaveTarget('jwhmi-only', { text: projectText(target.name) });
      if (!result?.ok) throw new Error(result?.error || 'No se pudo guardar el proyecto.');
      window.dispatchEvent(new CustomEvent('jwplc:sketch-unlinked'));
      markSaved();
      toast(`${result.fileName} guardado (sin sketch vinculado).`, 'ok');
      return true;
    }
    return false;
  }

  let saving = null;
  async function save() {
    if (saving) return saving;
    saving = (async () => {
      try {
        if (await linkedInfo()) return await writeLinked();
        if (native?.chooseSaveTarget) return await saveWithoutSketchNative();
        // Navegador o app generada antes de este flujo: el guardado de siempre.
        await project()?.save?.();
        markSaved();
        return true;
      } catch (error) {
        toast(error?.message || 'No se pudo guardar.', 'error');
        return false;
      } finally {
        saving = null;
      }
    })();
    return saving;
  }

  // --- Abrir ▾ (recientes) --------------------------------------------------------

  let recentMenu = null;

  function closeRecentMenu() {
    recentMenu?.remove();
    recentMenu = null;
    document.getElementById('openProjectButton')?.setAttribute('aria-expanded', 'false');
  }

  const escapeHtml = (v) => String(v ?? '').replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));

  function recentItem(action, icon, label, hint, extra = '', disabled = false) {
    return `<button type="button" class="sketch-menu-item" role="menuitem" data-open-action="${action}"${extra}${disabled ? ' disabled' : ''}>` +
      `<span class="sketch-menu-icon" aria-hidden="true">${icon}</span><span class="sketch-menu-text"><strong>${escapeHtml(label)}</strong>` +
      `${hint ? `<small title="${escapeHtml(hint)}">${escapeHtml(hint)}</small>` : ''}</span></button>`;
  }

  async function openRecentMenu(button) {
    closeRecentMenu();
    const result = await native.listRecentProjects().catch(() => null);
    const items = result?.items || [];
    recentMenu = document.createElement('div');
    recentMenu.className = 'sketch-menu recent-menu';
    recentMenu.setAttribute('role', 'menu');
    recentMenu.innerHTML = `
      ${recentItem('file', '🗀', 'Abrir archivo…', 'Elegir un .jwhmi en el explorador')}
      <div class="sketch-menu-sep"></div>
      <div class="sketch-menu-head"><strong>Recientes</strong>${items.length ? '' : '<small>Todavía no hay proyectos recientes</small>'}</div>
      ${items.map((item, index) => recentItem('recent', item.sketch ? '⧉' : '◇',
        item.fileName.replace(/\.jwhmi$/i, ''),
        item.exists ? `${item.sketch ? `Sketch ${item.sketch} · ` : ''}${item.folder}` : `No existe: ${item.folder}`,
        ` data-recent-index="${index}"`)).join('')}
      ${items.length ? `<div class="sketch-menu-sep"></div>${recentItem('clear', '✕', 'Limpiar recientes', '')}` : ''}`;
    document.body.appendChild(recentMenu);
    const r = button.getBoundingClientRect();
    recentMenu.style.top = `${Math.round(r.bottom + 6)}px`;
    recentMenu.style.left = `${Math.round(Math.max(8, Math.min(window.innerWidth - recentMenu.offsetWidth - 8, r.left)))}px`;
    button.setAttribute('aria-expanded', 'true');
    recentMenu.querySelector('.sketch-menu-item:not([disabled])')?.focus();

    recentMenu.addEventListener('click', async (event) => {
      const item = event.target.closest('[data-open-action]');
      if (!item || item.disabled) return;
      const action = item.dataset.openAction;
      const recent = items[Number(item.dataset.recentIndex)];
      closeRecentMenu();
      if (action === 'clear') {
        await native.clearRecentProjects();
        toast('Lista de recientes vaciada.', 'ok');
        return;
      }
      if (!(await guardChanges('abrir otro proyecto'))) return;
      if (action === 'file') {
        clickBypassing('openProjectButton');
        return;
      }
      try {
        const opened = await native.openRecentProject(recent.path);
        if (!opened?.ok) throw new Error(opened?.error || 'No se pudo abrir el proyecto.');
        await window.JWPLCHMIProjectIntegrationFix.applyOpened(opened);
      } catch (error) {
        toast(error?.message || 'No se pudo abrir el proyecto.', 'error');
      }
    });
  }

  // --- Botones, atajo y cierre -------------------------------------------------------

  let bypass = false;

  function clickBypassing(id) {
    const button = document.getElementById(id);
    if (!button) return;
    bypass = true;
    try { button.click(); } finally { bypass = false; }
  }

  async function guardedClick(id, actionText) {
    if (!(await guardChanges(actionText))) return;
    clickBypassing(id);
    if (id === 'newProjectButton') markSavedSoon();
  }

  const recentsAvailable = () => Boolean(native?.listRecentProjects && window.JWPLCHMIProjectIntegrationFix?.applyOpened);

  document.addEventListener('click', (event) => {
    const button = event.target.closest?.('#newProjectButton, #openProjectButton, #saveProjectButton');
    if (recentMenu && !recentMenu.contains(event.target) && button?.id !== 'openProjectButton') closeRecentMenu();
    if (!button || bypass || applying) return;
    if (button.id === 'openProjectButton' && recentsAvailable()) {
      event.preventDefault();
      event.stopImmediatePropagation();
      if (recentMenu) closeRecentMenu();
      else openRecentMenu(button);
      return;
    }
    if (button.id === 'saveProjectButton') {
      event.preventDefault();
      event.stopImmediatePropagation();
      save();
      return;
    }
    if (button.id === 'newProjectButton' && !isDirty()) { markSavedSoon(); return; }
    if (!isDirty()) return;
    event.preventDefault();
    event.stopImmediatePropagation();
    guardedClick(button.id, button.id === 'newProjectButton' ? 'empezar un proyecto nuevo' : 'abrir otro proyecto');
  }, true);

  document.addEventListener('keydown', (event) => {
    if (event.key === 'Escape' && recentMenu) { closeRecentMenu(); return; }
    if (!(event.ctrlKey || event.metaKey) || event.altKey || event.key.toLowerCase() !== 's') return;
    if (document.querySelector('.jw-dialog')) return;
    event.preventDefault();
    event.stopImmediatePropagation();
    save();
  }, true);

  window.addEventListener('jwplc:project-loaded', markSavedSoon);
  window.addEventListener('jwplc:project-written', markSaved);
  window.addEventListener('jwplc:editor-refresh', scheduleStatus);
  window.addEventListener('resize', closeRecentMenu);

  native?.registerCloseGuard?.(async () => {
    if (await guardChanges('cerrar la app')) native.confirmClose();
  });

  async function init() {
    for (let i = 0; i < 120 && !(project()?.apply && sketchMenu() && window.JWPLCHMIPages); i++) await wait(50);
    const api = project();
    if (!api) return;
    // apply() pulsa «Nuevo» por dentro: ese clic no es del usuario.
    const wrap = () => {
      const current = api.apply;
      if (!current || current.__saveFlowWrapped) return;
      const wrapped = async (...args) => {
        applying++;
        try { return await current.apply(api, args); } finally { applying--; }
      };
      wrapped.__saveFlowWrapped = true;
      api.apply = wrapped;
    };
    wrap();
    // designer-project-integration-fix.js vuelve a envolver apply al iniciar.
    const timer = setInterval(wrap, 250);
    setTimeout(() => clearInterval(timer), 4000);
    api.isDirty = isDirty;
    // Lo que queda en pantalla al arrancar (diseño vacío o el HMI del sketch).
    await wait(1500);
    if (baseline === null) markSaved();
  }

  window.JWPLCHMISaveFlow = { save, isDirty, markSaved, guardChanges };
  init().catch((error) => console.error('[JWPLC HMI save flow]', error));
})();
