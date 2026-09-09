(() => {
  'use strict';

  if (window.JWPLCHMIInspectorState) return;

  const fieldSection = document.getElementById('textFieldControlsSection');
  const rawSection = document.getElementById('rawTextControlsSection');
  const pixelInspector = document.getElementById('pixelMapControlsSection');
  const legacyPixelSection = document.querySelector('.pixel-controls');
  const rightPanel = document.querySelector('.right-panel');

  if (!rightPanel) return;

  const style = document.createElement('style');
  style.id = 'a11-inspector-state-style';
  style.textContent = `
    .right-panel .inspector-section[hidden],
    .right-panel .pixel-controls[hidden],
    .right-panel details[hidden],
    .right-panel label[hidden],
    .right-panel div[hidden] {
      display: none !important;
    }
  `;
  document.head.appendChild(style);

  function editor() {
    return window.JWPLCHMIEditor || null;
  }

  function pixels() {
    return window.JWPLCHMIPixelMaps || null;
  }

  function setSectionVisible(section, visible) {
    if (!section) return;
    section.hidden = !visible;
    if (visible) section.style.removeProperty('display');
    else section.style.setProperty('display', 'none', 'important');
  }

  function currentMode() {
    if (pixels()?.getSelected?.()) return 'PIXEL';
    const selectedTool = editor()?.getSelectedTool?.() || 'none';
    if (selectedTool === 'rawText') return 'RAW';
    if (editor()?.getSelectedField?.()) return 'FIELD';
    return 'NONE';
  }

  function syncInspectorState() {
    const mode = currentMode();

    setSectionVisible(fieldSection, mode === 'FIELD');
    setSectionVisible(rawSection, mode === 'RAW');
    setSectionVisible(pixelInspector, mode === 'PIXEL');

    // A11-7 ya dispone de color contextual en PIXEL y en Apariencia.
    // La sección global heredada mezcla estados y queda fuera del flujo normal.
    setSectionVisible(legacyPixelSection, false);

    rightPanel.dataset.inspectorMode = mode.toLowerCase();
  }

  window.addEventListener('jwplc:editor-refresh', syncInspectorState);
  window.addEventListener('jwplc:pixelmap-changed', syncInspectorState);
  window.addEventListener('jwplc:project-loaded', syncInspectorState);

  document.querySelector('.left-panel')?.addEventListener('click', () => {
    setTimeout(syncInspectorState, 0);
  });
  rightPanel.addEventListener('click', () => setTimeout(syncInspectorState, 0));

  syncInspectorState();

  window.JWPLCHMIInspectorState = {
    sync: syncInspectorState,
    mode: currentMode
  };
})();
