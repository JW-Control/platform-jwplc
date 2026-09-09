(() => {
  'use strict';

  if (window.JWPLCHMIProjectIntegrationFix) return;

  const HEADER_NAME = 'JWPLC_HMI_Generated.h';
  const MAX_PIXEL_MAPS = 16;
  const native = window.JWPLCHMINative || null;

  let openButtonInstalled = false;
  let nativeUpdateInstalled = false;

  function wait(ms) {
    return new Promise((resolve) => setTimeout(resolve, ms));
  }

  async function waitFor(predicate, timeoutMs = 3500) {
    const started = performance.now();
    while ((performance.now() - started) < timeoutMs) {
      if (predicate()) return true;
      await wait(25);
    }
    return Boolean(predicate());
  }

  function toast(message, kind = '') {
    let node = document.getElementById('a11ProjectIntegrationToast');
    if (!node) {
      node = document.createElement('div');
      node.id = 'a11ProjectIntegrationToast';
      Object.assign(node.style, {
        position: 'fixed',
        left: '50%',
        bottom: '38px',
        transform: 'translateX(-50%)',
        zIndex: '8000',
        minWidth: '300px',
        maxWidth: 'min(700px, calc(100vw - 32px))',
        padding: '10px 14px',
        border: '1px solid #506675',
        borderRadius: '7px',
        background: 'rgba(12,22,29,.98)',
        color: '#e5eef3',
        font: "12px/1.45 'Segoe UI', sans-serif",
        boxShadow: '0 9px 30px rgba(0,0,0,.34)',
        pointerEvents: 'none',
        opacity: '0',
        transition: 'opacity .16s ease'
      });
      document.body.appendChild(node);
    }

    node.textContent = message;
    node.style.borderColor = kind === 'error' ? '#c85d58' : kind === 'ok' ? '#3b9e80' : '#506675';
    node.style.color = kind === 'error' ? '#ffd7d4' : '#e5eef3';
    node.style.opacity = '1';
    clearTimeout(toast.timer);
    toast.timer = setTimeout(() => { node.style.opacity = '0'; }, 3200);
  }

  function validatePixelMaps(input) {
    if (input == null) return [];
    if (!Array.isArray(input)) throw new Error('El bloque pixelMaps del proyecto es inválido.');
    if (input.length > MAX_PIXEL_MAPS) throw new Error(`El proyecto supera el máximo de ${MAX_PIXEL_MAPS} PixelMaps.`);

    return input.map((map, index) => {
      if (!map || typeof map !== 'object') throw new Error(`PixelMap ${index + 1} inválido.`);
      if (!Array.isArray(map.pixels)) throw new Error(`PixelMap ${index + 1} no contiene una lista de píxeles válida.`);
      return map;
    });
  }

  function installProjectPersistence() {
    const project = window.JWPLCHMIProject;
    if (!project || project.__a11PixelPersistenceWrapped) return false;

    const baseSerialize = project.serialize?.bind(project);
    const baseApply = project.apply?.bind(project);
    if (!baseSerialize || !baseApply) return false;

    project.serialize = () => {
      const data = baseSerialize();
      return {
        ...data,
        pixelMaps: window.JWPLCHMIPixelMaps?.export?.() || []
      };
    };

    project.apply = async (data) => {
      const pixelMaps = validatePixelMaps(data?.pixelMaps);
      const result = await baseApply(data);

      await waitFor(() => window.JWPLCHMIPixelMaps?.import, 2000);
      window.JWPLCHMIPixelMaps?.import?.(pixelMaps);
      window.JWPLCHMIPixelMaps?.refresh?.();
      window.dispatchEvent(new CustomEvent('jwplc:pixelmap-changed'));
      window.dispatchEvent(new CustomEvent('jwplc:editor-refresh'));
      return result;
    };

    project.__a11PixelPersistenceWrapped = true;
    return true;
  }

  async function chooseProjectFile() {
    if ('showOpenFilePicker' in window) {
      const handles = await window.showOpenFilePicker({
        multiple: false,
        types: [{
          description: 'Proyecto JWPLC HMI',
          accept: { 'application/json': ['.jwhmi'] }
        }]
      });
      const handle = handles?.[0];
      return handle ? await handle.getFile() : null;
    }

    return await new Promise((resolve) => {
      const input = document.createElement('input');
      input.type = 'file';
      input.accept = '.jwhmi,application/json';
      input.onchange = () => resolve(input.files?.[0] || null);
      input.click();
    });
  }

  async function openProjectFixed() {
    try {
      const file = await chooseProjectFile();
      if (!file) return;
      const data = JSON.parse(await file.text());
      await window.JWPLCHMIProject.apply(data);
      toast(`${file.name} abierto. PixelMaps restaurados: ${Array.isArray(data.pixelMaps) ? data.pixelMaps.length : 0}.`, 'ok');
    } catch (error) {
      if (error?.name !== 'AbortError') toast(error?.message || 'No se pudo abrir el proyecto .jwhmi.', 'error');
    }
  }

  function installOpenButton() {
    const oldButton = document.getElementById('openProjectButton');
    if (!oldButton || oldButton.dataset.a11ProjectFix === '1') return false;

    const button = oldButton.cloneNode(true);
    button.dataset.a11ProjectFix = '1';
    oldButton.replaceWith(button);
    button.addEventListener('click', openProjectFixed);
    openButtonInstalled = true;
    return true;
  }

  async function nativeLinkedInfo() {
    if (!native?.isNative || !native.getLinkedSketch) return null;
    try {
      const info = await native.getLinkedSketch();
      return info?.ok ? info : null;
    } catch (_) {
      return null;
    }
  }

  function syncNativeLinkedUi(info) {
    if (!native?.isNative) return;

    const linkButton = document.getElementById('linkSketchButton');
    const updateButton = document.getElementById('updateHmiButton');
    const status = document.getElementById('sketchLinkStatus');
    const linked = Boolean(info?.ok);

    if (linkButton) {
      linkButton.textContent = linked ? `Sketch: ${info.name}` : 'Vincular sketch…';
      linkButton.classList.toggle('linked', linked);
      linkButton.title = linked ? `Sketch vinculado: ${info.path}` : 'Seleccionar la carpeta del sketch Arduino';
    }
    if (updateButton) updateButton.disabled = !linked;
    if (status) {
      status.textContent = linked ? `Sketch: ${info.name} · ${HEADER_NAME}` : 'Sketch: sin vincular';
      status.classList.toggle('linked', linked);
      status.classList.remove('warning');
    }
  }

  function isValidGeneratedHeader(text) {
    const value = String(text || '');
    return value.startsWith('// Código generado por JWPLC HMI Designer') &&
      value.includes('#pragma once');
  }

  async function stableGeneratedHeaderText() {
    const cg = window.JWPLCHMICodegen;
    if (!cg) throw new Error('El generador C++ todavía no está listo.');

    const issues = cg.validateIdentifiers?.() || [];
    if (issues.length) throw new Error('Corrige los identificadores C++ duplicados antes de actualizar la HMI.');

    await waitFor(() => window.JWPLCHMIBool && window.JWPLCHMIBar && window.JWPLCHMIPages, 2000);
    document.getElementById('contractTab')?.click();
    await wait(40);
    cg.refresh?.();

    // Pixel-only necesita completar el header después del refresh base. Forzamos
    // aquí el mismo pipeline que usa Generar C++ antes de validar/escribir.
    for (let attempt = 0; attempt < 12; attempt += 1) {
      window.JWPLCHMIPixelStability?.ensurePixelCode?.();
      window.JWPLCHMIPixelCodegenGuard?.apply?.();
      await wait(attempt === 0 ? 0 : 20);

      const text = document.getElementById('codeOutput')?.textContent || '';
      if (isValidGeneratedHeader(text)) {
        return text.endsWith('\n') ? text : `${text}\n`;
      }
    }

    throw new Error('No se pudo obtener un JWPLC_HMI_Generated.h válido después de completar el codegen PixelMap.');
  }

  async function linkSketchIfNeeded() {
    let info = await nativeLinkedInfo();
    if (info) return info;
    if (!native?.selectSketchDirectory) return null;

    const selected = await native.selectSketchDirectory();
    if (selected?.canceled) return null;
    if (!selected?.ok) throw new Error(selected?.error || 'No se pudo vincular el sketch.');
    info = selected;
    syncNativeLinkedUi(info);
    return info;
  }

  async function updateHeaderNativeFixed() {
    try {
      const info = await linkSketchIfNeeded();
      if (!info) return;

      const text = await stableGeneratedHeaderText();
      let result = await native.writeGeneratedHeader(text, false);
      if (result?.requiresOverwrite) {
        if (!window.confirm(`Se reemplazará ${HEADER_NAME} en el sketch “${info.name}”. ¿Continuar?`)) return;
        result = await native.writeGeneratedHeader(text, true);
      }
      if (!result?.ok) throw new Error(result?.error || `No se pudo actualizar ${HEADER_NAME}.`);

      syncNativeLinkedUi(await nativeLinkedInfo());
      toast(`${HEADER_NAME} actualizado correctamente.`, 'ok');
      window.dispatchEvent(new CustomEvent('jwplc:header-written', {
        detail: { sketch: info.name, header: HEADER_NAME }
      }));
    } catch (error) {
      toast(error?.message || `No se pudo actualizar ${HEADER_NAME}.`, 'error');
    }
  }

  function installNativeUpdateButton() {
    if (!native?.isNative) return false;
    const oldButton = document.getElementById('updateHmiButton');
    if (!oldButton || oldButton.dataset.a11ProjectFix === '1') return false;

    const button = oldButton.cloneNode(true);
    button.dataset.a11ProjectFix = '1';
    oldButton.replaceWith(button);
    button.addEventListener('click', updateHeaderNativeFixed);
    nativeUpdateInstalled = true;

    if (window.JWPLCHMIProject) {
      window.JWPLCHMIProject.updateGeneratedHeader = updateHeaderNativeFixed;
    }
    return true;
  }

  async function restoreNativeState() {
    if (!native?.isNative) return;
    const info = await nativeLinkedInfo();
    syncNativeLinkedUi(info);
  }

  async function init() {
    await waitFor(() => window.JWPLCHMIProject && window.JWPLCHMIPixelMaps, 4000);
    if (!window.JWPLCHMIProject) return;

    installProjectPersistence();
    installOpenButton();

    if (native?.isNative) {
      // designer-native-project.js inicializa de forma asíncrona. Esperamos un
      // instante y reemplazamos únicamente Actualizar HMI con el pipeline estable.
      await wait(120);
      installNativeUpdateButton();
      await restoreNativeState();

      // El restaurador web de designer-project.js puede terminar después y
      // deshabilitar el botón por no tener FileSystemHandle. Reafirmamos el
      // estado nativo una vez que ambas inicializaciones hayan terminado.
      setTimeout(restoreNativeState, 350);
      setTimeout(restoreNativeState, 900);
    }

    window.addEventListener('jwplc:project-loaded', () => {
      setTimeout(restoreNativeState, 0);
      setTimeout(restoreNativeState, 250);
    });
    window.addEventListener('jwplc:header-written', () => setTimeout(restoreNativeState, 0));

    const guardTimer = setInterval(() => {
      installProjectPersistence();
      if (!openButtonInstalled) installOpenButton();
      if (native?.isNative && !document.getElementById('updateHmiButton')?.dataset?.a11ProjectFix) {
        installNativeUpdateButton();
      }
    }, 250);
    setTimeout(() => clearInterval(guardTimer), 3500);
  }

  window.JWPLCHMIProjectIntegrationFix = {
    open: openProjectFixed,
    updateHeader: updateHeaderNativeFixed,
    buildHeader: stableGeneratedHeaderText,
    restoreLinked: restoreNativeState
  };

  init().catch((error) => console.error('[JWPLC HMI project integration fix]', error));
})();