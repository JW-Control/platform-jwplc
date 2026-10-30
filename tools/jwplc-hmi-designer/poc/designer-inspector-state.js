(() => {
  'use strict';

  if (window.JWPLCHMIInspectorState) return;

  const fieldSection = document.getElementById('textFieldControlsSection');
  const shapeSection = document.getElementById('shapeInspectorSection');
  const rawSection = document.getElementById('rawTextControlsSection');
  const pixelInspector = document.getElementById('pixelMapControlsSection');
  const legacyPixelSection = document.querySelector('.pixel-controls');
  const navBarSection = document.getElementById('navBarInspectorSection');
  const rightPanel = document.querySelector('.right-panel');
  const SHAPE_TYPES = ['LINE', 'RECT', 'ELLIPSE', 'TRIANGLE', 'POLYGON'];

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
    const tool = editor()?.getSelectedTool?.();
    if (tool === 'fill' || tool === 'pick') return 'COLOR';
    const field = editor()?.getSelectedField?.();
    const hasFields = Boolean(editor()?.hasFieldSelection?.() && field);
    const hasPixels = Boolean(pixels()?.getSelected?.());

    if (hasFields) {
      if (field.type === 'RAW_TEXT') return 'RAW';
      if (field.type === 'NAV_BAR') return 'NAV_BAR';
      if (field.type === 'MENU') return 'MENU';
      if (field.type === 'EVENT_LOG') return 'EVENT_LOG';
      if (field.type === 'ICON') return 'ICON';
      if (SHAPE_TYPES.includes(field.type)) return 'SHAPE';
      return 'FIELD';
    }

    if (hasPixels) return 'PIXEL';

    const selectedTool = editor()?.getSelectedTool?.() || 'none';
    if (selectedTool === 'rawText') return 'RAW';
    if (selectedTool === 'navBarField') return 'NAV_BAR';
    if (selectedTool === 'pixel' || selectedTool === 'erase') return 'PIXEL';
    return 'NONE';
  }

  function syncInspectorState() {
    const mode = currentMode();

    setSectionVisible(navBarSection, mode === 'NAV_BAR');
    setSectionVisible(document.getElementById('menuInspectorSection'), mode === 'MENU');
    setSectionVisible(document.getElementById('eventLogInspectorSection'), mode === 'EVENT_LOG');
    setSectionVisible(document.getElementById('iconInspectorSection'), mode === 'ICON');
    setSectionVisible(fieldSection, mode === 'FIELD');
    setSectionVisible(shapeSection, mode === 'SHAPE');
    setSectionVisible(rawSection, mode === 'RAW');
    setSectionVisible(pixelInspector, mode === 'PIXEL');
    setSectionVisible(document.getElementById('eyedropperSection'), mode === 'COLOR');
    setSectionVisible(document.getElementById('idleConfigSection'), mode === 'NONE');

    if (mode === 'NAV_BAR' && window.JWPLCHMIPages?.syncNavBarInspector) {
      window.JWPLCHMIPages.syncNavBarInspector();
    }

    // A11-7 ya dispone de color contextual en PIXEL y en Apariencia.
    // La sección global heredada mezcla estados y queda fuera del flujo normal.
    setSectionVisible(legacyPixelSection, false);

    rightPanel.dataset.inspectorMode = mode.toLowerCase();
  }

  window.addEventListener('jwplc:editor-refresh', syncInspectorState);
  window.addEventListener('jwplc:pixelmap-changed', syncInspectorState);
  window.addEventListener('jwplc:project-loaded', syncInspectorState);
  window.addEventListener('jwplc:tool-changed', () => setTimeout(syncInspectorState, 0));

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
