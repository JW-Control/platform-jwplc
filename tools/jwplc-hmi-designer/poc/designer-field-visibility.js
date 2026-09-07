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

// A11-7: compat se registra antes del objeto PIXEL para capturar el framebuffer
// base y preservar el orden visual físico PixelMaps -> fields. Después se carga
// el editor de capas/colores RGB565.
(() => {
  if (document.querySelector('script[data-a11-pixel-compat]')) return;
  const compat = document.createElement('script');
  compat.src = './designer-pixelmap-compat.js';
  compat.async = false;
  compat.dataset.a11PixelCompat = '1';
  compat.onload = () => {
    if (window.JWPLCHMIPixelMaps || document.querySelector('script[data-a11-pixelmap]')) return;
    const pixel = document.createElement('script');
    pixel.src = './designer-pixelmap.js';
    pixel.async = false;
    pixel.dataset.a11Pixelmap = '1';
    document.body.appendChild(pixel);
  };
  document.body.appendChild(compat);
})();
