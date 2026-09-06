(() => {
  'use strict';

  if (window.JWPLCHMIProject) return;

  const PROJECT_FORMAT = 'JWPLC_HMI_PROJECT';
  const PROJECT_VERSION = 1;
  const HEADER_NAME = 'JWPLC_HMI_Generated.h';
  const DB_NAME = 'jwplc-hmi-designer';
  const DB_STORE = 'handles';
  const SKETCH_HANDLE_KEY = 'linkedSketchDirectory';

  let projectFileHandle = null;
  let linkedSketchDirectory = null;
  let dirty = false;
  let suppressDirty = true;
  let projectName = 'HMI_ST7789';
  let installPrompt = null;

  function editor() {
    return window.JWPLCHMIEditor || null;
  }

  function codegen() {
    return window.JWPLCHMICodegen || null;
  }

  function wait(ms) {
    return new Promise((resolve) => setTimeout(resolve, ms));
  }

  async function waitFor(predicate, timeoutMs = 2500) {
    const started = performance.now();
    while ((performance.now() - started) < timeoutMs) {
      if (predicate()) return true;
      await wait(25);
    }
    return Boolean(predicate());
  }

  function toolbar() {
    return document.querySelector('.toolbar');
  }

  function statusbar() {
    return document.querySelector('.statusbar');
  }

  function findToolbarButton(label) {
    return [...(toolbar()?.querySelectorAll('button') || [])]
      .find((button) => button.textContent.trim() === label) || null;
  }

  function projectStatusNode() {
    return [...(statusbar()?.querySelectorAll('span') || [])]
      .find((span) => span.textContent.trim().startsWith('Proyecto:')) || null;
  }

  function setProjectStatus() {
    const node = projectStatusNode();
    if (!node) return;
    node.textContent = `Proyecto: ${projectName}${dirty ? ' *' : ''}`;
  }

  function setDirty(next = true) {
    dirty = Boolean(next);
    setProjectStatus();
  }

  function injectStyles() {
    if (document.getElementById('a11-project-integration-style')) return;
    const style = document.createElement('style');
    style.id = 'a11-project-integration-style';
    style.textContent = `
      .a11-project-button.linked {
        border-color:#3da98a !important;
        box-shadow:0 0 0 1px rgba(61,169,138,.22) inset;
      }
      .a11-project-button.update-ready {
        border-color:#ff8a2a !important;
      }
      .a11-project-status {
        display:inline-flex;
        align-items:center;
        gap:6px;
        max-width:300px;
        overflow:hidden;
        text-overflow:ellipsis;
        white-space:nowrap;
      }
      .a11-project-status::before {
        content:'●';
        font-size:9px;
        color:#768894;
      }
      .a11-project-status.linked::before { color:#4fc49c; }
      .a11-project-status.warning::before { color:#ffb45f; }
      .a11-project-toast {
        position:fixed;
        left:50%;
        bottom:38px;
        transform:translateX(-50%);
        z-index:5000;
        min-width:280px;
        max-width:min(620px, calc(100vw - 32px));
        padding:10px 14px;
        border:1px solid #506675;
        border-radius:7px;
        background:rgba(12,22,29,.97);
        color:#e5eef3;
        font:12px/1.45 'Segoe UI', sans-serif;
        box-shadow:0 9px 30px rgba(0,0,0,.34);
        opacity:0;
        pointer-events:none;
        transition:opacity .16s ease;
      }
      .a11-project-toast.show { opacity:1; }
      .a11-project-toast.error { border-color:#c85d58; color:#ffd7d4; }
      .a11-project-toast.ok { border-color:#3b9e80; }
    `;
    document.head.appendChild(style);
  }

  function toast(message, kind = '') {
    let node = document.getElementById('a11ProjectToast');
    if (!node) {
      node = document.createElement('div');
      node.id = 'a11ProjectToast';
      node.className = 'a11-project-toast';
      document.body.appendChild(node);
    }
    node.textContent = message;
    node.className = `a11-project-toast ${kind} show`.trim();
    clearTimeout(toast.timer);
    toast.timer = setTimeout(() => {
      node.classList.remove('show');
    }, 2800);
  }

  function ensureManifest() {
    if (!document.querySelector('link[rel="manifest"]')) {
      const link = document.createElement('link');
      link.rel = 'manifest';
      link.href = './manifest.webmanifest';
      document.head.appendChild(link);
    }
    if (!document.querySelector('meta[name="theme-color"]')) {
      const meta = document.createElement('meta');
      meta.name = 'theme-color';
      meta.content = '#101a21';
      document.head.appendChild(meta);
    }
  }

  function registerServiceWorker() {
    if (!('serviceWorker' in navigator)) return;
    const isLocalhost = ['localhost', '127.0.0.1', '[::1]'].includes(location.hostname);
    if (location.protocol !== 'https:' && !isLocalhost) return;
    navigator.serviceWorker.register('./service-worker.js').catch(() => {});
  }

  function openDatabase() {
    if (!('indexedDB' in window)) return Promise.resolve(null);
    return new Promise((resolve) => {
      const request = indexedDB.open(DB_NAME, 1);
      request.onupgradeneeded = () => {
        const db = request.result;
        if (!db.objectStoreNames.contains(DB_STORE)) db.createObjectStore(DB_STORE);
      };
      request.onsuccess = () => resolve(request.result);
      request.onerror = () => resolve(null);
    });
  }

  async function storeHandle(key, handle) {
    try {
      const db = await openDatabase();
      if (!db) return;
      await new Promise((resolve) => {
        const tx = db.transaction(DB_STORE, 'readwrite');
        tx.objectStore(DB_STORE).put(handle, key);
        tx.oncomplete = resolve;
        tx.onerror = resolve;
      });
      db.close();
    } catch (_) {}
  }

  async function loadHandle(key) {
    try {
      const db = await openDatabase();
      if (!db) return null;
      const value = await new Promise((resolve) => {
        const tx = db.transaction(DB_STORE, 'readonly');
        const request = tx.objectStore(DB_STORE).get(key);
        request.onsuccess = () => resolve(request.result || null);
        request.onerror = () => resolve(null);
      });
      db.close();
      return value;
    } catch (_) {
      return null;
    }
  }

  async function ensureDirectoryPermission(handle, ask = false) {
    if (!handle) return false;
    const options = { mode: 'readwrite' };
    try {
      if ((await handle.queryPermission(options)) === 'granted') return true;
      if (ask && (await handle.requestPermission(options)) === 'granted') return true;
    } catch (_) {}
    return false;
  }

  function sanitizeFileStem(value, fallback = 'JWPLC_HMI') {
    const normalized = String(value || '').trim().replace(/[<>:"/\\|?*\u0000-\u001F]/g, '_');
    return (normalized || fallback).slice(0, 80);
  }

  function canonicalProjectFileName() {
    const stem = linkedSketchDirectory?.name || projectName || 'JWPLC_HMI';
    return `${sanitizeFileStem(stem)}.jwhmi`;
  }

  async function directoryHasFile(directory, name) {
    try {
      await directory.getFileHandle(name, { create: false });
      return true;
    } catch (error) {
      if (error?.name === 'NotFoundError') return false;
      throw error;
    }
  }

  function serializeProject() {
    const api = editor();
    const pages = api?.getPages?.() || [{ id: 0, name: 'Principal' }];
    const fields = (api?.getAllFields?.() || []).map((field) => {
      const copy = { ...field };
      delete copy.key;
      return copy;
    });

    return {
      format: PROJECT_FORMAT,
      version: PROJECT_VERSION,
      target: {
        display: 'ST7789',
        width: 320,
        height: 170,
        rotation: 3,
        pixelFormat: 'RGB565'
      },
      projectName,
      savedAt: new Date().toISOString(),
      activePage: Number(api?.getActivePage?.() || 0),
      pages,
      fields,
      declarativeOnly: true
    };
  }

  function validateProject(data) {
    if (!data || data.format !== PROJECT_FORMAT || Number(data.version) !== PROJECT_VERSION) {
      throw new Error('El archivo no es un proyecto JWPLC HMI compatible con Alpha11.');
    }
    if (!Array.isArray(data.pages) || !data.pages.length || data.pages.length > 16) {
      throw new Error('El proyecto contiene una cantidad de páginas inválida.');
    }
    if (!Array.isArray(data.fields) || data.fields.length > 32) {
      throw new Error('El proyecto contiene una cantidad de campos inválida.');
    }

    const ids = data.pages.map((page) => Number(page.id)).sort((a, b) => a - b);
    if (ids.some((id, index) => id !== index)) {
      throw new Error('Alpha11 requiere páginas contiguas desde 0.');
    }

    return data;
  }

  function normalizeImportedFields(fields) {
    return fields.map((field, index) => ({
      ...field,
      key: `${String(field.type || 'TEXT').toLowerCase()}-${index + 1}`,
      page: Number(field.page || 0)
    }));
  }

  async function applyProject(data) {
    const api = editor();
    if (!api) throw new Error('El editor todavía no está listo.');

    const normalized = validateProject(data);
    const importedFields = normalizeImportedFields(normalized.fields);
    const pages = normalized.pages.slice().sort((a, b) => Number(a.id) - Number(b.id));

    suppressDirty = true;
    document.getElementById('newProjectButton')?.click();

    if (pages[0]?.name && pages[0].name !== 'Principal') {
      api.renamePage?.(0, pages[0].name);
    }
    for (let i = 1; i < pages.length; i += 1) {
      api.addPage?.(pages[i].name || `Página ${i + 1}`);
    }

    const desiredCount = Math.max(1, importedFields.length);
    const currentFields = api.getAllFields();
    while (currentFields.length < desiredCount) api.addTextField?.();

    const liveFields = api.getAllFields();
    liveFields.splice(0, liveFields.length, ...importedFields);

    let targetPage = Number(normalized.activePage || 0);
    if (!pages.some((page) => Number(page.id) === targetPage)) targetPage = 0;
    const currentPage = Number(api.getActivePage?.() || 0);
    if (pages.length > 1 && currentPage === targetPage) {
      const alternate = targetPage === 0 ? 1 : 0;
      api.setActivePage?.(alternate);
    }
    api.setActivePage?.(targetPage);
    api.render?.();
    await wait(0);
    document.querySelector('.object-item')?.click();
    api.commitHistory?.();

    projectName = String(normalized.projectName || projectName || 'HMI_ST7789').slice(0, 48);
    setDirty(false);
    suppressDirty = false;
    window.dispatchEvent(new CustomEvent('jwplc:project-loaded', { detail: { projectName } }));
  }

  async function writeFileHandle(handle, text) {
    const writable = await handle.createWritable();
    await writable.write(text);
    await writable.close();
  }

  async function saveProjectBesideSketch(data) {
    if (!linkedSketchDirectory) return false;
    if (!(await ensureDirectoryPermission(linkedSketchDirectory, true))) {
      updateLinkedUI('permission');
      toast('No se concedió permiso para guardar el proyecto junto al sketch.', 'error');
      return false;
    }

    const fileName = canonicalProjectFileName();
    const existing = await directoryHasFile(linkedSketchDirectory, fileName);
    if (existing && !window.confirm(`Se reemplazará ${fileName} en el sketch “${linkedSketchDirectory.name}”. ¿Continuar?`)) {
      return false;
    }

    projectFileHandle = await linkedSketchDirectory.getFileHandle(fileName, { create: true });
    await writeFileHandle(projectFileHandle, data);
    projectName = projectFileHandle.name.replace(/\.jwhmi$/i, '') || projectName;
    setDirty(false);
    updateLinkedUI('linked');
    toast(`${fileName} guardado junto al sketch.`, 'ok');
    window.dispatchEvent(new CustomEvent('jwplc:project-written', {
      detail: { sketch: linkedSketchDirectory.name, project: fileName }
    }));
    return true;
  }

  async function saveProjectAs() {
    const data = JSON.stringify(serializeProject(), null, 2) + '\n';

    if ('showSaveFilePicker' in window) {
      projectFileHandle = await window.showSaveFilePicker({
        suggestedName: `${projectName || 'JWPLC_HMI'}.jwhmi`,
        types: [{
          description: 'Proyecto JWPLC HMI',
          accept: { 'application/json': ['.jwhmi'] }
        }]
      });
      await writeFileHandle(projectFileHandle, data);
      projectName = projectFileHandle.name.replace(/\.jwhmi$/i, '') || projectName;
    } else {
      const blob = new Blob([data], { type: 'application/json' });
      const url = URL.createObjectURL(blob);
      const anchor = document.createElement('a');
      anchor.href = url;
      anchor.download = `${projectName || 'JWPLC_HMI'}.jwhmi`;
      anchor.click();
      URL.revokeObjectURL(url);
    }

    setDirty(false);
    toast('Proyecto HMI guardado.', 'ok');
  }

  async function saveProject() {
    try {
      const data = JSON.stringify(serializeProject(), null, 2) + '\n';
      if (projectFileHandle) {
        await writeFileHandle(projectFileHandle, data);
        setDirty(false);
        toast('Proyecto HMI actualizado.', 'ok');
        return;
      }
      if (linkedSketchDirectory && await saveProjectBesideSketch(data)) return;
      await saveProjectAs();
    } catch (error) {
      if (error?.name !== 'AbortError') toast(error?.message || 'No se pudo guardar el proyecto.', 'error');
    }
  }

  async function openProject() {
    try {
      let file;
      if ('showOpenFilePicker' in window) {
        const handles = await window.showOpenFilePicker({
          multiple: false,
          types: [{
            description: 'Proyecto JWPLC HMI',
            accept: { 'application/json': ['.jwhmi'] }
          }]
        });
        projectFileHandle = handles[0];
        file = await projectFileHandle.getFile();
      } else {
        file = await new Promise((resolve) => {
          const input = document.createElement('input');
          input.type = 'file';
          input.accept = '.jwhmi,application/json';
          input.onchange = () => resolve(input.files?.[0] || null);
          input.click();
        });
      }
      if (!file) return;
      const data = JSON.parse(await file.text());
      await applyProject(data);
      projectName = file.name.replace(/\.jwhmi$/i, '') || projectName;
      setProjectStatus();
      toast('Proyecto HMI abierto.', 'ok');
    } catch (error) {
      if (error?.name !== 'AbortError') toast(error?.message || 'No se pudo abrir el proyecto.', 'error');
    }
  }

  async function directoryContainsSketch(directory) {
    try {
      for await (const entry of directory.values()) {
        if (entry.kind === 'file' && /\.ino$/i.test(entry.name)) return true;
      }
    } catch (_) {}
    return false;
  }

  function linkedStatusNode() {
    return document.getElementById('sketchLinkStatus');
  }

  function linkButton() {
    return document.getElementById('linkSketchButton');
  }

  function updateButton() {
    return document.getElementById('updateHmiButton');
  }

  function updateLinkedUI(state = 'none') {
    const node = linkedStatusNode();
    const linked = Boolean(linkedSketchDirectory);
    if (node) {
      node.classList.toggle('linked', linked && state !== 'permission');
      node.classList.toggle('warning', state === 'permission');
      if (!linked) node.textContent = 'Sketch: sin vincular';
      else if (state === 'permission') node.textContent = `Sketch: ${linkedSketchDirectory.name} · requiere permiso`;
      else node.textContent = `Sketch: ${linkedSketchDirectory.name} · ${HEADER_NAME}`;
    }
    linkButton()?.classList.toggle('linked', linked);
    if (updateButton()) updateButton().disabled = !linked;
  }

  async function linkSketch() {
    if (!('showDirectoryPicker' in window)) {
      toast('Vincular sketch requiere Edge/Chrome de escritorio en contexto seguro.', 'error');
      return;
    }

    try {
      const directory = await window.showDirectoryPicker({ mode: 'readwrite' });
      if (!(await directoryContainsSketch(directory))) {
        toast('La carpeta seleccionada no contiene un archivo .ino.', 'error');
        return;
      }
      if (!(await ensureDirectoryPermission(directory, true))) {
        toast('No se concedió permiso de escritura para el sketch.', 'error');
        return;
      }
      linkedSketchDirectory = directory;
      if (!projectFileHandle && projectName === 'HMI_ST7789') {
        projectName = String(directory.name || projectName).slice(0, 48);
        setProjectStatus();
      }
      await storeHandle(SKETCH_HANDLE_KEY, directory);
      updateLinkedUI('linked');
      toast(`Sketch “${directory.name}” vinculado. Guardar creará ${canonicalProjectFileName()} junto al .ino.`, 'ok');
    } catch (error) {
      if (error?.name !== 'AbortError') toast(error?.message || 'No se pudo vincular el sketch.', 'error');
    }
  }

  async function generatedHeaderText() {
    const cg = codegen();
    if (!cg) throw new Error('El generador C++ todavía no está listo.');
    const issues = cg.validateIdentifiers?.() || [];
    if (issues.length) throw new Error('Corrige los identificadores C++ duplicados antes de actualizar la HMI.');

    await waitFor(() => window.JWPLCHMIBool && window.JWPLCHMIBar && window.JWPLCHMIPages, 2000);
    document.getElementById('contractTab')?.click();
    await wait(40);
    cg.refresh?.();
    await wait(0);

    const text = document.getElementById('codeOutput')?.textContent || '';
    if (!text.startsWith('// Código generado por JWPLC HMI Designer') || !text.includes('#pragma once')) {
      throw new Error('No se pudo obtener un JWPLC_HMI_Generated.h válido.');
    }
    return text.endsWith('\n') ? text : `${text}\n`;
  }

  async function updateGeneratedHeader() {
    if (!linkedSketchDirectory) {
      await linkSketch();
      if (!linkedSketchDirectory) return;
    }

    try {
      if (!(await ensureDirectoryPermission(linkedSketchDirectory, true))) {
        updateLinkedUI('permission');
        toast('El Designer necesita permiso para actualizar el sketch.', 'error');
        return;
      }

      let existing = false;
      try {
        await linkedSketchDirectory.getFileHandle(HEADER_NAME, { create: false });
        existing = true;
      } catch (error) {
        if (error?.name !== 'NotFoundError') throw error;
      }

      if (existing && !window.confirm(`Se reemplazará ${HEADER_NAME} en el sketch “${linkedSketchDirectory.name}”. ¿Continuar?`)) {
        return;
      }

      const text = await generatedHeaderText();
      const handle = await linkedSketchDirectory.getFileHandle(HEADER_NAME, { create: true });
      await writeFileHandle(handle, text);
      updateLinkedUI('linked');
      toast(`${HEADER_NAME} actualizado correctamente.`, 'ok');
      window.dispatchEvent(new CustomEvent('jwplc:header-written', {
        detail: { sketch: linkedSketchDirectory.name, header: HEADER_NAME }
      }));
    } catch (error) {
      toast(error?.message || `No se pudo actualizar ${HEADER_NAME}.`, 'error');
    }
  }

  async function restoreLinkedSketch() {
    linkedSketchDirectory = await loadHandle(SKETCH_HANDLE_KEY);
    if (!linkedSketchDirectory) {
      updateLinkedUI('none');
      return;
    }
    const granted = await ensureDirectoryPermission(linkedSketchDirectory, false);
    updateLinkedUI(granted ? 'linked' : 'permission');
  }

  function injectToolbar() {
    const bar = toolbar();
    if (!bar) return;

    const openButton = findToolbarButton('Abrir');
    const saveButton = findToolbarButton('Guardar');
    if (openButton) {
      openButton.id = 'openProjectButton';
      openButton.disabled = false;
      openButton.title = 'Abrir proyecto .jwhmi';
      openButton.addEventListener('click', openProject);
    }
    if (saveButton) {
      saveButton.id = 'saveProjectButton';
      saveButton.disabled = false;
      saveButton.title = 'Guardar proyecto .jwhmi';
      saveButton.addEventListener('click', saveProject);
    }

    const generateButton = document.getElementById('generateButton');
    if (generateButton && !document.getElementById('linkSketchButton')) {
      const separator = document.createElement('span');
      separator.className = 'separator';

      const link = document.createElement('button');
      link.id = 'linkSketchButton';
      link.className = 'a11-project-button';
      link.type = 'button';
      link.textContent = 'Vincular sketch…';
      link.title = 'Seleccionar la carpeta del sketch Arduino';
      link.addEventListener('click', linkSketch);

      const update = document.createElement('button');
      update.id = 'updateHmiButton';
      update.className = 'a11-project-button update-ready';
      update.type = 'button';
      update.textContent = 'Actualizar HMI';
      update.title = `Escribir ${HEADER_NAME} en el sketch vinculado`;
      update.disabled = true;
      update.addEventListener('click', updateGeneratedHeader);

      generateButton.insertAdjacentElement('afterend', update);
      update.insertAdjacentElement('afterend', link);
      link.insertAdjacentElement('afterend', separator);
    }

    if (!document.getElementById('installAppButton')) {
      const install = document.createElement('button');
      install.id = 'installAppButton';
      install.type = 'button';
      install.textContent = 'Instalar app';
      install.title = 'Instalar JWPLC HMI Designer como aplicación';
      install.hidden = true;
      install.addEventListener('click', async () => {
        if (!installPrompt) return;
        installPrompt.prompt();
        await installPrompt.userChoice.catch(() => null);
        installPrompt = null;
        install.hidden = true;
      });
      bar.appendChild(install);
    }
  }

  function injectStatus() {
    const bar = statusbar();
    if (!bar || document.getElementById('sketchLinkStatus')) return;
    const grow = bar.querySelector('.grow');
    const node = document.createElement('span');
    node.id = 'sketchLinkStatus';
    node.className = 'a11-project-status';
    node.textContent = 'Sketch: sin vincular';
    if (grow) grow.insertAdjacentElement('beforebegin', node);
    else bar.appendChild(node);
  }

  function bindDirtyTracking() {
    window.addEventListener('jwplc:editor-refresh', () => {
      if (!suppressDirty) setDirty(true);
    });
    document.getElementById('newProjectButton')?.addEventListener('click', () => {
      projectFileHandle = null;
      projectName = 'HMI_ST7789';
      if (!suppressDirty) setDirty(true);
    });
    setTimeout(() => {
      suppressDirty = false;
      setDirty(false);
    }, 250);
  }

  window.addEventListener('beforeinstallprompt', (event) => {
    event.preventDefault();
    installPrompt = event;
    const button = document.getElementById('installAppButton');
    if (button) button.hidden = false;
  });

  window.addEventListener('appinstalled', () => {
    installPrompt = null;
    const button = document.getElementById('installAppButton');
    if (button) button.hidden = true;
    toast('JWPLC HMI Designer instalado.', 'ok');
  });

  injectStyles();
  ensureManifest();
  registerServiceWorker();
  injectToolbar();
  injectStatus();
  bindDirtyTracking();
  restoreLinkedSketch();

  window.JWPLCHMIProject = {
    serialize: serializeProject,
    apply: applyProject,
    open: openProject,
    save: saveProject,
    saveAs: saveProjectAs,
    linkSketch,
    updateGeneratedHeader,
    linkedSketchName: () => linkedSketchDirectory?.name || null,
    linkedProjectFileName: canonicalProjectFileName,
    headerName: HEADER_NAME
  };
})();
