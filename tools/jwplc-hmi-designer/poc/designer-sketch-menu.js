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
//   «Crear sketch nuevo…» crea <nombre>/<nombre>.ino con el sketch base de la
//                         guía, lo vincula y hace «Escribir en sketch».
  //   «Generar C++» se oculta: el panel «Código generado» ya muestra el header.
  //
  // Funciona con la app de escritorio (window.JWPLCHMINative, IPC de
  // electron/main.js) y con el navegador (File System Access en
  // designer-project.js). Los clics se toman en fase de captura en document:
  // otros módulos reemplazan los botones (cloneNode) y así no importa cuál
  // quedó en el DOM.
  //
  // Dentro de OpenPLC (window.JWPLCHMIHost.mode === 'openplc') el «sketch»
  // vinculado es la carpeta hmi/ del proyecto: el botón es «Actualizar HMI»,
  // no hay guía JWPLC_HMI_LEEME.md (no hay .ino) y el menú sólo ofrece
  // recargar el HMI guardado y abrir la carpeta.

  if (window.JWPLCHMISketchMenu) return;

  const HEADER_NAME = 'JWPLC_HMI_Generated.h';
  const native = window.JWPLCHMINative?.isNative ? window.JWPLCHMINative : null;
  const openplc = window.JWPLCHMIHost?.mode === 'openplc';
  const target = (info) => (openplc ? `el proyecto OpenPLC «${info.name}»` : `el sketch «${info.name}»`);

  const wait = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
  // Regla de nombres del Arduino IDE 2 (la misma que valida electron/main.js).
  const SKETCH_NAME = /^[A-Za-z0-9][A-Za-z0-9_.-]{0,62}$/;

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
    // App generada antes de «Crear sketch»: main.js/preload.cjs van dentro del .exe.
    canCreate: Boolean(native.createSketch),
    async create(name, inoText) {
      const result = await native.createSketch(name, inoText);
      if (result?.canceled) return null;
      if (!result?.ok) throw new Error(result?.error || 'No se pudo crear el sketch.');
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
    // App generada antes de la guía: main.js/preload.cjs van dentro del .exe.
    async writeGuide(text) {
      if (!native.writeGuide) return 'old-app';
      const result = await native.writeGuide(text);
      if (result?.foreign) return 'foreign';
      if (!result?.ok) throw new Error(result?.error || 'No se pudo escribir la guía del sketch.');
      return 'ok';
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
    canCreate: true,
    async create(name, inoText) {
      const typed = await window.JWPLCDialog.prompt('Nombre del sketch nuevo (letras, números, _ . -):', name);
      if (typed === null) return null;
      const clean = typed.trim().replace(/\.ino$/i, '');
      if (!SKETCH_NAME.test(clean)) throw new Error(`Nombre de sketch inválido: «${clean}». Empieza con letra o número, máximo 63.`);
      if (!(await project()?.createSketch?.(clean, inoText))) return null;
      return this.info();
    },
    async readProject() {
      const info = await this.info();
      if (!info?.projectExists) throw new Error(`El sketch no tiene ${info?.projectFile || '.jwhmi'}.`);
      return project().readLinkedFile(info.projectFile);
    },
    write: (name, text) => project().writeLinkedFile(name, text),
    async writeGuide(text) {
      const guide = window.JWPLCSketchGuide;
      if (await project().linkedFileExists(guide.GUIDE_NAME)) {
        const current = await project().readLinkedFile(guide.GUIDE_NAME);
        if (!current.startsWith(guide.GUIDE_MARK)) return 'foreign';
      }
      await project().writeLinkedFile(guide.GUIDE_NAME, text);
      return 'ok';
    }
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
        await window.JWPLCDialog.confirm(`El sketch «${info.name}» ya tiene un HMI guardado (${info.projectFile}).\n\n¿Abrirlo? El diseño que está en pantalla se reemplaza.`, { okLabel: 'Abrir HMI', cancelLabel: 'Ahora no' })) {
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

  function suggestedSketchName() {
    const stem = String(project()?.serialize?.().projectName || '').replace(/[^A-Za-z0-9_.-]+/g, '_').replace(/^[^A-Za-z0-9]+/, '').slice(0, 63);
    return stem && stem !== 'HMI_ST7789' ? stem : 'JWPLC_HMI';
  }

  // Sketch base con las variables del diseño; si el header todavía no se puede
  // generar (p. ej. identificadores duplicados), el base mínimo.
  async function baseSketchText() {
    const guide = window.JWPLCSketchGuide;
    let header = '';
    try { header = await headerText(); } catch (_) {}
    return `${guide.baseSketch(header)}\n`;
  }

  async function createFlow() {
    if (!backend.canCreate || !window.JWPLCSketchGuide) {
      toast('Para crear sketches desde el Designer, vuelve a generar la app de escritorio.', 'error');
      return null;
    }
    let info;
    try {
      info = await backend.create(suggestedSketchName(), await baseSketchText());
    } catch (error) {
      toast(error?.message || 'No se pudo crear el sketch.', 'error');
      return null;
    }
    if (!info) return null;
    window.dispatchEvent(new CustomEvent('jwplc:sketch-linked', { detail: { sketch: info.name, created: true } }));
    ownSketches.add(sketchKey(info));
    refreshButtons(info);
    // El .ino incluye el header: se escribe ya para que compile al abrirlo.
    await writeToSketch();
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

  // Guía JWPLC_HMI_LEEME.md (sketch base con las variables del header). Nunca
  // bloquea la escritura del header: si falla, sólo se avisa.
  async function writeGuide(info, header) {
    const guide = window.JWPLCSketchGuide;
    if (!guide || openplc) return '';
    try {
      const text = guide.build({ sketchName: info.name, projectFile: info.projectFile, header });
      const status = await backend.writeGuide(text);
      if (status === 'ok') return ` y ${guide.GUIDE_NAME} (sketch base)`;
      if (status === 'foreign') return `. ${guide.GUIDE_NAME} no se tocó: no lo escribió el Designer`;
      return `. Para la guía ${guide.GUIDE_NAME}, vuelve a generar la app de escritorio`;
    } catch (error) {
      return `. Guía no escrita: ${error?.message || error}`;
    }
  }

  // Sketches cuyo HMI es el que está en pantalla (se abrió, se creó o ya se
  // escribió en esta sesión): «Guardar» los reescribe sin preguntar cada vez.
  const ownSketches = new Set();
  const sketchKey = (info) => info?.path || info?.name || '';
  window.addEventListener('jwplc:project-loaded', (event) => { if (event.detail?.sketch) backend.info().then((info) => info && ownSketches.add(sketchKey(info))); });
  window.addEventListener('jwplc:sketch-own', (event) => { if (event.detail?.key) ownSketches.add(event.detail.key); });

  // «Escribir en sketch»: header + .jwhmi (+ guía). Confirma sólo la primera
  // vez que reemplaza archivos de un sketch ajeno al diseño en pantalla.
  // Devuelve true si escribió.
  async function writeToSketch() {
    try {
      let info = await backend.info();
      if (!info) info = await linkFlow();
      if (!info) return false;

      const header = await headerText();
      const data = project().serialize();
      data.projectName = info.name;
      data.savedAt = new Date().toISOString();
      const projectText = `${JSON.stringify(data, null, 2)}\n`;

      const replaced = [info.headerExists && HEADER_NAME, info.projectExists && info.projectFile].filter(Boolean);
      if (replaced.length && !ownSketches.has(sketchKey(info)) &&
          !await window.JWPLCDialog.confirm(`Se reemplazará en ${target(info)}:\n\n${replaced.map((n) => `• ${n}`).join('\n')}\n\n¿Continuar?`, { okLabel: 'Reemplazar' })) {
        return false;
      }

      await backend.write(HEADER_NAME, header);
      await backend.write(info.projectFile, projectText);
      const guideNote = await writeGuide(info, header);
      toast(`Escrito en «${info.name}»: ${HEADER_NAME}, ${info.projectFile}${guideNote}.`, 'ok');
      window.dispatchEvent(new CustomEvent('jwplc:header-written', { detail: { sketch: info.name, header: HEADER_NAME } }));
      window.dispatchEvent(new CustomEvent('jwplc:project-written', { detail: { sketch: info.name, project: info.projectFile } }));
      ownSketches.add(sketchKey(info));
      refreshButtons(await backend.info());
      return true;
    } catch (error) {
      toast(error?.message || 'No se pudo escribir en el sketch.', 'error');
      return false;
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
    const escape = (v) => String(v ?? '').replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));
    menu = document.createElement('div');
    menu.className = 'sketch-menu';
    menu.setAttribute('role', 'menu');
    menu.innerHTML = openplc && info ? `
      <div class="sketch-menu-head">
        <strong>OpenPLC · ${escape(info.name)}</strong>
        ${info.path ? `<small title="${escape(info.path)}">${escape(info.path)}</small>` : ''}
        <div class="sketch-menu-files">
          <span class="${info.headerExists ? 'is-on' : ''}">${info.headerExists ? '✓' : '—'} ${HEADER_NAME}</span>
          <span class="${info.projectExists ? 'is-on' : ''}">${info.projectExists ? '✓' : '—'} ${escape(info.projectFile)}</span>
        </div>
      </div>
      ${menuItem('open-project', '⟲', 'Recargar HMI guardado', info.projectExists ? `hmi/${escape(info.projectFile)}` : 'Todavía no hay .jwhmi: usa «Actualizar HMI»', !info.projectExists)}
      ${menuItem('open-folder', '🗀', 'Abrir carpeta hmi/', 'Explorador de archivos')}` : !info ? `
      ${menuItem('change', '🗀', 'Elegir sketch existente…', 'Una carpeta con un .ino')}
      ${menuItem('create', '+', 'Crear sketch nuevo…', 'Carpeta y .ino base con las variables del HMI', !backend.canCreate)}` : `
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
      ${menuItem('change', '⇄', 'Cambiar sketch…', 'Vincular otra carpeta de sketch')}
      ${menuItem('create', '+', 'Crear sketch nuevo…', 'Carpeta y .ino base con las variables del HMI', !backend.canCreate)}`;
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
          if (await window.JWPLCDialog.confirm(`¿Abrir el HMI guardado en ${target(info)}? El diseño que está en pantalla se reemplaza.`, { okLabel: 'Abrir HMI' })) await openSketchProject(info);
        } else if (action === 'open-ino') {
          const result = await backend.openIno();
          if (!result?.ok) throw new Error(result?.error || 'No se pudo abrir el .ino.');
          toast('Abriendo el sketch en el Arduino IDE…', 'ok');
        } else if (action === 'open-folder') {
          const result = await backend.openFolder();
          if (!result?.ok) throw new Error(result?.error || 'No se pudo abrir la carpeta.');
        } else if (action === 'change') {
          await linkFlow();
        } else if (action === 'create') {
          await createFlow();
        }
      } catch (error) {
        toast(error?.message || 'No se pudo completar la acción.', 'error');
      }
    });
  }

  // --- Botones -----------------------------------------------------------------

  function refreshButtons(info) {
    const link = document.getElementById('linkSketchButton');
    if (openplc && info) {
      refreshButtonsOpenplc(info, link);
      return;
    }
    if (link) {
      const label = info ? `Sketch: ${info.name}` : 'Vincular sketch…';
      // En modo compacto desktop-responsive.js acorta el texto: sólo se corrige
      // si quedó el nombre de otro sketch.
      if (!link.textContent.includes(info ? info.name : 'Vincular')) link.textContent = label;
      link.classList.toggle('linked', Boolean(info));
      link.setAttribute('aria-haspopup', 'menu');
      link.classList.toggle('sketch-menu-button', Boolean(info));
      link.title = info ? `Sketch vinculado${info.path ? `: ${info.path}` : ''} · clic para abrir el menú` : 'Elegir un sketch Arduino existente o crear uno nuevo';
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

  // OpenPLC: designer-native-project.js, designer-project-integration-fix.js y
  // desktop-responsive.js también escriben estos botones, con los mismos textos.
  function refreshButtonsOpenplc(info, link) {
    if (link) {
      if (!link.textContent.startsWith('OpenPLC')) link.textContent = `OpenPLC: ${info.name}`;
      link.classList.add('linked', 'sketch-menu-button');
      link.setAttribute('aria-haspopup', 'menu');
      link.title = `HMI del proyecto OpenPLC${info.path ? `: ${info.path}` : ''} · clic para abrir el menú`;
    }
    const status = document.getElementById('sketchLinkStatus');
    if (status) {
      status.textContent = `OpenPLC: ${info.name} · hmi/${HEADER_NAME}`;
      status.classList.add('linked');
      status.classList.remove('warning');
    }
    const update = document.getElementById('updateHmiButton');
    if (update) {
      update.disabled = false;
      update.title = `Escribir ${HEADER_NAME} y ${info.projectFile} en la carpeta hmi/ del proyecto OpenPLC «${info.name}»`;
      // En modo compacto desktop-responsive.js lo acorta a «Actualizar».
      if (!/^Actualizar/.test(update.textContent)) update.textContent = 'Actualizar HMI';
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
  window.addEventListener('jwplc:sketch-linked', async () => refreshButtons(await backend.info().catch(() => null)));
  window.addEventListener('jwplc:sketch-unlinked', async () => {
    closeMenu();
    // En OpenPLC «Nuevo» no desvincula: la carpeta hmi/ sigue siendo el destino.
    refreshButtons(openplc ? await backend.info().catch(() => null) : null);
  });

  window.JWPLCHMISketchMenu = {
    writeToSketch, linkFlow, createFlow, openMenu, closeMenu, backend, baseSketchText, suggestedSketchName,
    markOwn: (info) => ownSketches.add(sketchKey(info)),
    refresh: async () => refreshButtons(await backend.info().catch(() => null))
  };
})();
