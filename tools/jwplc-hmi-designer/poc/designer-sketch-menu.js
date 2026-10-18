(() => {
  'use strict';

  // Alpha15 · Flujo con el sketch Arduino (decisión del jefe, 2026-10-18):
  //
  //   «Escribir en sketch»  escribe JWPLC_HMI_Generated.h y <sketch>.jwhmi en la
  //                         carpeta del sketch, con una sola confirmación: lo que
  //                         se reabre es exactamente lo que generó el header.
  //   «Sketch: nombre ▾»    menú: Abrir HMI del sketch · Abrir en Arduino IDE ·
  //                         Abrir carpeta · Cambiar sketch…
  //   Al vincular un sketch que ya tiene .jwhmi, pregunta si abrirlo.
  //   «Generar C++» se oculta: el panel «Código generado» ya muestra el header.
  //
  // Funciona con la app de escritorio (window.JWPLCHMINative, IPC de
  // electron/main.js) y con el navegador (File System Access en
  // designer-project.js). Los clics se toman en fase de captura en document:
  // otros módulos reemplazan los botones (cloneNode) y así no importa cuál
  // quedó en el DOM.

  if (window.JWPLCHMISketchMenu) return;

  const HEADER_NAME = 'JWPLC_HMI_Generated.h';
  const native = window.JWPLCHMINative?.isNative ? window.JWPLCHMINative : null;

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
    toast.timer = setTimeout(() => node.classList.remove('is-visible'), 3400);
  }

  // --- Acceso al sketch: escritorio o navegador ------------------------------

  const project = () => window.JWPLCHMIProject;

  const backend = native ? {
    canOpenExternally: true,
    async info() {
      const info = await native.getLinkedSketch();
      return info?.ok ? info : null;
    },
    async link() {
      const result = await native.selectSketchDirectory();
      if (result?.canceled) return null;
      if (!result?.ok) throw new Error(result?.error || 'No se pudo vincular el sketch.');
      return result;
    },
    async readProject() {
      const result = await native.readProject();
      if (!result?.ok) throw new Error(result?.missing ? `El sketch no tiene ${result.fileName}.` : (result?.error || 'No se pudo leer el proyecto del sketch.'));
      return result.text;
    },
    async write(name, text) {
      const result = name === HEADER_NAME
        ? await native.writeGeneratedHeader(text, true)
        : await native.writeProject(name, text, true);
      if (!result?.ok) throw new Error(result?.error || `No se pudo escribir ${name}.`);
    },
    openIno: () => native.openSketchIno(),
    openFolder: () => native.openSketchFolder()
  } : {
    canOpenExternally: false,
    async info() {
      const name = project()?.linkedSketchName?.();
      if (!name) return null;
      const projectFile = `${name}.jwhmi`;
      return {
        ok: true,
        name,
        projectFile,
        headerExists: await project().linkedFileExists(HEADER_NAME),
        projectExists: await project().linkedFileExists(projectFile)
      };
    },
    async link() {
      const before = project()?.linkedSketchName?.() || null;
      await project()?.linkSketch?.();
      const after = project()?.linkedSketchName?.() || null;
      // Selector cancelado: sigue el mismo sketch, no hay nada nuevo que abrir.
      if (!after || after === before) return null;
      return this.info();
    },
    async readProject() {
      const info = await this.info();
      if (!info?.projectExists) throw new Error(`El sketch no tiene ${info?.projectFile || '.jwhmi'}.`);
      return project().readLinkedFile(info.projectFile);
    },
    write: (name, text) => project().writeLinkedFile(name, text)
  };

  // --- Acciones ---------------------------------------------------------------

  async function openSketchProject(info) {
    const text = await backend.readProject();
    await project().apply(JSON.parse(text));
    window.dispatchEvent(new CustomEvent('jwplc:project-loaded', { detail: { sketch: info.name } }));
    toast(`HMI del sketch «${info.name}» abierto (${info.projectFile}).`, 'ok');
  }

  // Vincula (o cambia) el sketch y, si ya tiene su .jwhmi, ofrece abrirlo.
  async function linkFlow() {
    let info;
    try {
      info = await backend.link();
    } catch (error) {
      toast(error?.message || 'No se pudo vincular el sketch.', 'error');
      return null;
    }
    if (!info) return null;
    window.dispatchEvent(new CustomEvent('jwplc:sketch-linked', { detail: { sketch: info.name } }));
    refreshButtons(info);
    if (info.projectExists &&
        window.confirm(`El sketch «${info.name}» ya tiene un HMI guardado (${info.projectFile}).\n\n¿Abrirlo? El diseño que está en pantalla se reemplaza.`)) {
      try {
        await openSketchProject(info);
      } catch (error) {
        toast(error?.message || 'No se pudo abrir el HMI del sketch.', 'error');
      }
    } else {
      toast(`Sketch «${info.name}» vinculado.`, 'ok');
    }
    return info;
  }

  async function headerText() {
    const build = window.JWPLCHMIProjectIntegrationFix?.buildHeader;
    if (build) return build();
    // Respaldo: el mismo camino que la pestaña «Código generado».
    const issues = window.JWPLCHMICodegen?.validateIdentifiers?.() || [];
    if (issues.length) throw new Error('Corrige los identificadores C++ duplicados antes de escribir en el sketch.');
    document.getElementById('contractTab')?.click();
    await wait(40);
    window.JWPLCHMICodegen?.refresh?.();
    await wait(0);
    const text = document.getElementById('codeOutput')?.textContent || '';
    if (!text.startsWith('// Código generado por JWPLC HMI Designer') || !text.includes('#pragma once')) {
      throw new Error(`No se pudo obtener un ${HEADER_NAME} válido.`);
    }
    return text.endsWith('\n') ? text : `${text}\n`;
  }

  // «Escribir en sketch»: header + .jwhmi, una sola confirmación.
  async function writeToSketch() {
    try {
      let info = await backend.info();
      if (!info) info = await linkFlow();
      if (!info) return;

      const header = await headerText();
      const data = project().serialize();
      data.projectName = info.name;
      data.savedAt = new Date().toISOString();
      const projectText = `${JSON.stringify(data, null, 2)}\n`;

      const replaced = [info.headerExists && HEADER_NAME, info.projectExists && info.projectFile].filter(Boolean);
      if (replaced.length &&
          !window.confirm(`Se reemplazará en el sketch «${info.name}»:\n\n${replaced.map((n) => `• ${n}`).join('\n')}\n\n¿Continuar?`)) {
        return;
      }

      await backend.write(HEADER_NAME, header);
      await backend.write(info.projectFile, projectText);
      toast(`Escrito en «${info.name}»: ${HEADER_NAME} y ${info.projectFile}.`, 'ok');
      window.dispatchEvent(new CustomEvent('jwplc:header-written', { detail: { sketch: info.name, header: HEADER_NAME } }));
      window.dispatchEvent(new CustomEvent('jwplc:project-written', { detail: { sketch: info.name, project: info.projectFile } }));
      refreshButtons(await backend.info());
    } catch (error) {
      toast(error?.message || 'No se pudo escribir en el sketch.', 'error');
    }
  }

  // --- Menú «Sketch: nombre ▾» ------------------------------------------------

  let menu = null;

  function closeMenu() {
    menu?.remove();
    menu = null;
    document.getElementById('linkSketchButton')?.setAttribute('aria-expanded', 'false');
  }

  function menuItem(action, icon, label, hint, disabled = false) {
    return `<button type="button" class="sketch-menu-item" role="menuitem" data-sketch-action="${action}"${disabled ? ' disabled' : ''}>` +
      `<span class="sketch-menu-icon" aria-hidden="true">${icon}</span><span class="sketch-menu-text"><strong>${label}</strong>` +
      `${hint ? `<small>${hint}</small>` : ''}</span></button>`;
  }

  async function openMenu(button) {
    closeMenu();
    const info = await backend.info();
    if (!info) { await linkFlow(); return; }
    const escape = (v) => String(v ?? '').replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));
    menu = document.createElement('div');
    menu.className = 'sketch-menu';
    menu.setAttribute('role', 'menu');
    menu.innerHTML = `
      <div class="sketch-menu-head">
        <strong>${escape(info.name)}</strong>
        ${info.path ? `<small title="${escape(info.path)}">${escape(info.path)}</small>` : ''}
        <div class="sketch-menu-files">
          <span class="${info.headerExists ? 'is-on' : ''}">${info.headerExists ? '✓' : '—'} ${HEADER_NAME}</span>
          <span class="${info.projectExists ? 'is-on' : ''}">${info.projectExists ? '✓' : '—'} ${escape(info.projectFile)}</span>
        </div>
      </div>
      ${menuItem('open-project', '⟲', 'Abrir HMI del sketch', info.projectExists ? escape(info.projectFile) : 'Todavía no hay .jwhmi: usa «Escribir en sketch»', !info.projectExists)}
      ${backend.canOpenExternally ? menuItem('open-ino', '⧉', 'Abrir en Arduino IDE', 'Abre el .ino con el programa asociado') : ''}
      ${backend.canOpenExternally ? menuItem('open-folder', '🗀', 'Abrir carpeta', 'Explorador de archivos') : ''}
      <div class="sketch-menu-sep"></div>
      ${menuItem('change', '⇄', 'Cambiar sketch…', 'Vincular otra carpeta de sketch')}`;
    document.body.appendChild(menu);
    const r = button.getBoundingClientRect();
    menu.style.top = `${Math.round(r.bottom + 6)}px`;
    menu.style.left = `${Math.round(Math.max(8, Math.min(window.innerWidth - menu.offsetWidth - 8, r.right - menu.offsetWidth)))}px`;
    button.setAttribute('aria-expanded', 'true');
    menu.querySelector('.sketch-menu-item:not([disabled])')?.focus();

    menu.addEventListener('click', async (event) => {
      const item = event.target.closest('[data-sketch-action]');
      if (!item || item.disabled) return;
      const action = item.dataset.sketchAction;
      closeMenu();
      try {
        if (action === 'open-project') {
          if (window.confirm(`¿Abrir el HMI guardado en «${info.name}»? El diseño que está en pantalla se reemplaza.`)) await openSketchProject(info);
        } else if (action === 'open-ino') {
          const result = await backend.openIno();
          if (!result?.ok) throw new Error(result?.error || 'No se pudo abrir el .ino.');
          toast('Abriendo el sketch en el Arduino IDE…', 'ok');
        } else if (action === 'open-folder') {
          const result = await backend.openFolder();
          if (!result?.ok) throw new Error(result?.error || 'No se pudo abrir la carpeta.');
        } else if (action === 'change') {
          await linkFlow();
        }
      } catch (error) {
        toast(error?.message || 'No se pudo completar la acción.', 'error');
      }
    });
  }

  // --- Botones -----------------------------------------------------------------

  function refreshButtons(info) {
    const link = document.getElementById('linkSketchButton');
    if (link) {
      const label = info ? `Sketch: ${info.name}` : 'Vincular sketch…';
      // En modo compacto desktop-responsive.js acorta el texto: sólo se corrige
      // si quedó el nombre de otro sketch.
      if (!link.textContent.includes(info ? info.name : 'Vincular')) link.textContent = label;
      link.classList.toggle('linked', Boolean(info));
      link.setAttribute('aria-haspopup', 'menu');
      link.classList.toggle('sketch-menu-button', Boolean(info));
      link.title = info ? `Sketch vinculado${info.path ? `: ${info.path}` : ''} · clic para abrir el menú` : 'Seleccionar la carpeta del sketch Arduino';
    }
    const status = document.getElementById('sketchLinkStatus');
    if (status) {
      status.textContent = info ? `Sketch: ${info.name} · ${HEADER_NAME}` : 'Sketch: sin vincular';
      status.classList.toggle('linked', Boolean(info));
      status.classList.remove('warning');
    }
    const update = document.getElementById('updateHmiButton');
    if (update) {
      update.disabled = !info;
      update.title = `Escribir ${HEADER_NAME} y el proyecto .jwhmi en la carpeta del sketch${info ? ` «${info.name}»` : ''}`;
      if (!/Escribir/.test(update.textContent)) update.textContent = 'Escribir en sketch';
    }
  }

  document.addEventListener('click', (event) => {
    const link = event.target.closest?.('#linkSketchButton');
    const update = event.target.closest?.('#updateHmiButton');
    if (!link && !update) {
      if (menu && !menu.contains(event.target)) closeMenu();
      return;
    }
    event.preventDefault();
    event.stopImmediatePropagation();
    if (update) { closeMenu(); writeToSketch(); return; }
    if (menu) { closeMenu(); return; }
    openMenu(link);
  }, true);

  document.addEventListener('keydown', (event) => {
    if (event.key === 'Escape' && menu) closeMenu();
  });
  window.addEventListener('resize', closeMenu);

  // Los otros módulos reescriben los botones al iniciar; se reafirma un rato.
  let ticks = 0;
  const timer = setInterval(async () => {
    refreshButtons(await backend.info().catch(() => null));
    if (++ticks >= 12) clearInterval(timer);
  }, 300);
  window.addEventListener('jwplc:header-written', async () => refreshButtons(await backend.info().catch(() => null)));

  window.JWPLCHMISketchMenu = { writeToSketch, linkFlow, openMenu, closeMenu, backend };
})();
