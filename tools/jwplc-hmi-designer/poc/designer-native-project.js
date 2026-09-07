(() => {
  'use strict';

  const native = window.JWPLCHMINative;
  if (!native?.isNative) return;

  const HEADER_NAME = 'JWPLC_HMI_Generated.h';
  let linked = null;

  function wait(ms) {
    return new Promise((resolve) => setTimeout(resolve, ms));
  }

  async function waitFor(predicate, timeoutMs = 3000) {
    const started = performance.now();
    while ((performance.now() - started) < timeoutMs) {
      if (predicate()) return true;
      await wait(25);
    }
    return Boolean(predicate());
  }

  function toast(message, kind = '') {
    let node = document.getElementById('a11NativeProjectToast');
    if (!node) {
      node = document.createElement('div');
      node.id = 'a11NativeProjectToast';
      Object.assign(node.style, {
        position: 'fixed',
        left: '50%',
        bottom: '38px',
        transform: 'translateX(-50%)',
        zIndex: '7000',
        minWidth: '300px',
        maxWidth: 'min(680px, calc(100vw - 32px))',
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
    toast.timer = setTimeout(() => { node.style.opacity = '0'; }, 3000);
  }

  function updateUi() {
    const linkButton = document.getElementById('linkSketchButton');
    const updateButton = document.getElementById('updateHmiButton');
    const status = document.getElementById('sketchLinkStatus');

    if (linkButton) {
      linkButton.textContent = linked?.ok ? `Sketch: ${linked.name}` : 'Vincular sketch…';
      linkButton.classList.toggle('linked', Boolean(linked?.ok));
      linkButton.title = linked?.ok
        ? `Sketch vinculado: ${linked.path}`
        : 'Seleccionar la carpeta del sketch Arduino';
    }
    if (updateButton) updateButton.disabled = !linked?.ok;
    if (status) {
      status.textContent = linked?.ok
        ? `Sketch: ${linked.name} · ${HEADER_NAME}`
        : 'Sketch: sin vincular';
      status.classList.toggle('linked', Boolean(linked?.ok));
      status.classList.remove('warning');
    }
  }

  async function linkSketchNative() {
    const result = await native.selectSketchDirectory();
    if (result?.canceled) return false;
    if (!result?.ok) {
      toast(result?.error || 'No se pudo vincular el sketch.', 'error');
      return false;
    }

    linked = result;
    updateUi();
    toast(`Sketch “${linked.name}” vinculado.`, 'ok');
    return true;
  }

  async function generatedHeaderText() {
    const cg = window.JWPLCHMICodegen;
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

  async function updateHeaderNative() {
    try {
      if (!linked?.ok && !(await linkSketchNative())) return;
      const text = await generatedHeaderText();
      let result = await native.writeGeneratedHeader(text, false);

      if (result?.requiresOverwrite) {
        if (!window.confirm(`Se reemplazará ${HEADER_NAME} en el sketch “${linked.name}”. ¿Continuar?`)) return;
        result = await native.writeGeneratedHeader(text, true);
      }

      if (!result?.ok) throw new Error(result?.error || `No se pudo actualizar ${HEADER_NAME}.`);
      toast(`${HEADER_NAME} actualizado correctamente.`, 'ok');
      window.dispatchEvent(new CustomEvent('jwplc:header-written', {
        detail: { sketch: linked.name, header: HEADER_NAME }
      }));
    } catch (error) {
      toast(error?.message || `No se pudo actualizar ${HEADER_NAME}.`, 'error');
    }
  }

  async function saveCanonicalProjectNative(fallbackSave) {
    if (!linked?.ok) {
      await fallbackSave?.();
      return;
    }

    try {
      const project = window.JWPLCHMIProject?.serialize?.();
      if (!project) throw new Error('El proyecto HMI todavía no está listo.');
      project.projectName = linked.name;
      project.savedAt = new Date().toISOString();
      const text = `${JSON.stringify(project, null, 2)}\n`;
      const fileName = `${linked.name}.jwhmi`;

      let result = await native.writeProject(fileName, text, false);
      if (result?.requiresOverwrite) {
        if (!window.confirm(`Se reemplazará ${fileName} en el sketch “${linked.name}”. ¿Continuar?`)) return;
        result = await native.writeProject(fileName, text, true);
      }
      if (!result?.ok) throw new Error(result?.error || `No se pudo guardar ${fileName}.`);
      toast(`${fileName} guardado junto al sketch.`, 'ok');
      window.dispatchEvent(new CustomEvent('jwplc:project-written', {
        detail: { sketch: linked.name, project: fileName }
      }));
    } catch (error) {
      toast(error?.message || 'No se pudo guardar el proyecto.', 'error');
    }
  }

  function replaceButton(id, handler) {
    const oldButton = document.getElementById(id);
    if (!oldButton) return null;
    const button = oldButton.cloneNode(true);
    oldButton.replaceWith(button);
    button.addEventListener('click', handler);
    return button;
  }

  async function init() {
    await waitFor(() => window.JWPLCHMIProject && document.getElementById('linkSketchButton'), 3500);
    if (!window.JWPLCHMIProject) return;

    const browserSave = window.JWPLCHMIProject.save?.bind(window.JWPLCHMIProject);

    replaceButton('linkSketchButton', linkSketchNative);
    replaceButton('updateHmiButton', updateHeaderNative);
    replaceButton('saveProjectButton', () => saveCanonicalProjectNative(browserSave));

    window.JWPLCHMIProject.linkSketch = linkSketchNative;
    window.JWPLCHMIProject.updateGeneratedHeader = updateHeaderNative;
    window.JWPLCHMIProject.linkedSketchName = () => linked?.name || null;

    const restored = await native.getLinkedSketch();
    if (restored?.ok) linked = restored;
    updateUi();
  }

  init().catch((error) => {
    console.error('[JWPLC HMI native project]', error);
    toast('No se pudo inicializar la integración nativa con el sketch.', 'error');
  });
})();
