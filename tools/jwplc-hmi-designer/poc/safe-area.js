(() => {
  'use strict';

  const WIDTH = 320;
  const HEIGHT = 170;
  const SAFE_MARGIN = 3;

  const displayCanvas = document.getElementById('displayCanvas');
  const safeAreaGuide = document.getElementById('safeAreaGuide');
  const safeAreaToggle = document.getElementById('safeAreaToggle');
  const safeAreaStatus = document.getElementById('safeAreaStatus');
  const zoomSelect = document.getElementById('zoomSelect');
  const newProjectButton = document.getElementById('newProjectButton');

  const textInput = document.getElementById('textInput');
  const textXInput = document.getElementById('textX');
  const textYInput = document.getElementById('textY');
  const textSizeSelect = document.getElementById('textSize');

  function numericValue(input, fallback) {
    const value = Number(input.value);
    return Number.isFinite(value) ? value : fallback;
  }

  function textBounds() {
    const font = window.JWPLCGfxClassicFont;
    const scale = Math.max(1, Math.trunc(numericValue(textSizeSelect, 1)));
    const text = textInput.value || '';

    return {
      width: text.length * font.cellWidth * scale,
      height: font.cellHeight * scale
    };
  }

  function insideRecommendedArea() {
    const x = Math.trunc(numericValue(textXInput, 0));
    const y = Math.trunc(numericValue(textYInput, 0));
    const bounds = textBounds();

    return x >= SAFE_MARGIN &&
      y >= SAFE_MARGIN &&
      x + bounds.width <= WIDTH - SAFE_MARGIN &&
      y + bounds.height <= HEIGHT - SAFE_MARGIN;
  }

  function updateSafeAreaStatus() {
    if (!safeAreaStatus) return;

    if (insideRecommendedArea()) {
      safeAreaStatus.textContent = 'Dentro de guía recomendada · 3 px';
      safeAreaStatus.classList.remove('warning');
      safeAreaStatus.classList.add('ok');
    } else {
      safeAreaStatus.textContent = 'Aviso: elemento fuera de la guía recomendada de 3 px';
      safeAreaStatus.classList.remove('ok');
      safeAreaStatus.classList.add('warning');
    }
  }

  function syncGuide() {
    if (!safeAreaGuide || !displayCanvas) return;

    const zoom = Math.max(1, numericValue(zoomSelect, 1));
    const margin = SAFE_MARGIN * zoom;

    safeAreaGuide.style.left = `${margin}px`;
    safeAreaGuide.style.top = `${margin}px`;
    safeAreaGuide.style.width = `${(WIDTH - SAFE_MARGIN * 2) * zoom}px`;
    safeAreaGuide.style.height = `${(HEIGHT - SAFE_MARGIN * 2) * zoom}px`;
    safeAreaGuide.hidden = !safeAreaToggle.checked;

    updateSafeAreaStatus();
  }

  function applyRecommendedTextOrigin() {
    textXInput.value = String(SAFE_MARGIN);
    textYInput.value = String(SAFE_MARGIN);

    textXInput.dispatchEvent(new Event('input', { bubbles: true }));
    textYInput.dispatchEvent(new Event('input', { bubbles: true }));

    updateSafeAreaStatus();
  }

  zoomSelect.addEventListener('change', syncGuide);
  safeAreaToggle.addEventListener('change', syncGuide);

  [textInput, textXInput, textYInput, textSizeSelect]
    .forEach((control) => control.addEventListener('input', updateSafeAreaStatus));

  // app.js registra primero resetProject(). Este listener corre después y sólo
  // aplica el default visual del Designer; no introduce offset en el renderer.
  newProjectButton.addEventListener('click', applyRecommendedTextOrigin);

  applyRecommendedTextOrigin();
  syncGuide();
})();
