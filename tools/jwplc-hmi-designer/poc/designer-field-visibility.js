(() => {
  'use strict';

  const fieldCapacity = document.getElementById('fieldCapacity');
  const fieldPreview = document.getElementById('fieldPreview');
  const fieldValueSize = document.getElementById('fieldValueSize');

  const capacityWrap = fieldCapacity?.closest('label');
  const previewWrap = fieldPreview?.closest('label');
  const valueSizeWrap = fieldValueSize?.closest('label');

  function editor() {
    return window.JWPLCHMIEditor || null;
  }

  function setVisible(element, visible) {
    if (!element) return;
    if (visible) element.style.removeProperty('display');
    else element.style.setProperty('display', 'none', 'important');
  }

  function syncFieldVisibility() {
    const field = editor()?.getSelectedField?.() || null;
    const type = field?.type || null;

    // Capacidad sólo pertenece a TEXT.
    setVisible(capacityWrap, type === 'TEXT');

    // El preview genérico pertenece a TEXT/VALUE. BOOL y BAR tienen
    // controles de prueba específicos dentro de sus secciones semánticas.
    setVisible(previewWrap, type === 'TEXT' || type === 'VALUE');

    // BAR no tiene tamaño de valor textual: su región visible es una barra
    // fija de 12 px de alto en el runtime. TEXT/VALUE/BOOL sí lo usan.
    setVisible(valueSizeWrap, type !== 'BAR');
  }

  window.addEventListener('jwplc:editor-refresh', syncFieldVisibility);
  document.querySelector('.left-panel')?.addEventListener('click', () => {
    setTimeout(syncFieldVisibility, 0);
  });

  syncFieldVisibility();
})();
