(() => {
  'use strict';

  const WIDTH = 320;
  const HEIGHT = 170;

  const displayCanvas = document.getElementById('displayCanvas');
  const overlayCanvas = document.getElementById('geometryCanvas');
  const overlayCtx = overlayCanvas.getContext('2d');
  const canvasViewport = document.getElementById('canvasViewport');

  const geometryToggle = document.getElementById('geometryToggle');
  const zoomSelect = document.getElementById('zoomSelect');
  const fitButton = document.getElementById('fitButton');
  const generateButton = document.getElementById('generateButton');
  const contractTab = document.getElementById('contractTab');
  const collapseBottomButton = document.getElementById('collapseBottomButton');
  const copyContractButton = document.getElementById('copyContractButton');
  const inspectorContract = document.getElementById('inspectorContract');

  const selectedObjectStatus = document.getElementById('selectedObjectStatus');
  const selectedGeometryStatus = document.getElementById('selectedGeometryStatus');
  const zoomStatus = document.getElementById('zoomStatus');

  const fieldInputs = [
    'fieldName', 'fieldId', 'fieldVariable', 'fieldCapacity', 'fieldX', 'fieldY',
    'fieldPreview', 'fieldLabel', 'fieldUnit', 'fieldValueSize', 'fieldLabelSize',
    'fieldLayout', 'fieldAlign', 'fieldFrame', 'fieldLabelColor', 'fieldValueColor',
    'fieldBackgroundColor', 'fieldFrameColor', 'fieldIntegerDigits',
    'fieldDecimalDigits', 'fieldSigned', 'fieldLeadingZeros'
  ].map((id) => document.getElementById(id)).filter(Boolean);

  let bottomCollapsed = false;

  function editor() {
    return window.JWPLCHMIEditor || null;
  }

  function hasFieldSelection() {
    return Boolean(editor()?.hasFieldSelection?.());
  }

  function selectedField() {
    return editor()?.getSelectedField?.() || null;
  }

  function selectedGeometry() {
    return editor()?.computeSelectedGeometry?.() || null;
  }

  function badge(ctx, text, x, y) {
    ctx.save();
    ctx.font = '11px Segoe UI, sans-serif';
    const metrics = ctx.measureText(text);
    const width = Math.ceil(metrics.width) + 14;
    const height = 23;
    const bx = Math.max(2, Math.min(overlayCanvas.width - width - 2, x));
    const by = Math.max(2, y);
    ctx.fillStyle = 'rgba(10, 20, 28, 0.95)';
    ctx.strokeStyle = '#617787';
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.roundRect(bx, by, width, height, 5);
    ctx.fill();
    ctx.stroke();
    ctx.fillStyle = '#dce9f1';
    ctx.fillText(text, bx + 7, by + 15);
    ctx.restore();
  }

  function regionRect(ctx, x, y, w, h, zoom, color, dash = [4, 3]) {
    if (w <= 0 || h <= 0) return;
    ctx.save();
    ctx.strokeStyle = color;
    ctx.lineWidth = 1;
    ctx.setLineDash(dash);
    ctx.strokeRect(x * zoom + 0.5, y * zoom + 0.5, w * zoom, h * zoom);
    ctx.restore();
  }

  function drawHandles(ctx, left, top, width, height) {
    const size = 7;
    const points = [
      [left, top], [left + width / 2, top], [left + width, top],
      [left, top + height / 2], [left + width, top + height / 2],
      [left, top + height], [left + width / 2, top + height], [left + width, top + height]
    ];
    ctx.save();
    ctx.fillStyle = '#ff8a2a';
    ctx.strokeStyle = '#18242d';
    points.forEach(([px, py]) => {
      ctx.fillRect(Math.round(px - size / 2), Math.round(py - size / 2), size, size);
      ctx.strokeRect(Math.round(px - size / 2) + 0.5, Math.round(py - size / 2) + 0.5, size - 1, size - 1);
    });
    ctx.restore();
  }

  function syncOverlaySize() {
    if (overlayCanvas.width !== displayCanvas.width) overlayCanvas.width = displayCanvas.width;
    if (overlayCanvas.height !== displayCanvas.height) overlayCanvas.height = displayCanvas.height;
    overlayCanvas.style.width = displayCanvas.style.width || `${displayCanvas.width}px`;
    overlayCanvas.style.height = displayCanvas.style.height || `${displayCanvas.height}px`;
  }

  function drawGeometryOverlay() {
    syncOverlaySize();
    overlayCtx.clearRect(0, 0, overlayCanvas.width, overlayCanvas.height);
    if (!geometryToggle.checked || !hasFieldSelection()) return;

    const field = selectedField();
    const g = selectedGeometry();
    if (!field || !g) return;

    const zoom = Math.max(1, Number(zoomSelect.value) || 3);
    const left = field.x * zoom;
    const top = field.y * zoom;
    const width = g.fieldW * zoom;
    const height = g.fieldH * zoom;

    overlayCtx.save();
    overlayCtx.strokeStyle = '#ff8a2a';
    overlayCtx.lineWidth = 2;
    overlayCtx.strokeRect(left + 0.5, top + 0.5, width, height);
    overlayCtx.restore();

    const labelX = field.x + g.pad;
    const labelY = field.y + g.pad;
    regionRect(overlayCtx, labelX, labelY, g.labelBounds.width, g.labelBounds.height, zoom, 'rgba(225,238,247,.60)');
    regionRect(overlayCtx, g.valueX, g.valueY, g.valueW, g.valueH, zoom, 'rgba(40,220,232,.72)');
    drawHandles(overlayCtx, left, top, width, height);
    badge(overlayCtx, `X: ${field.x}   Y: ${field.y}`, left + 7, top - 27);
    badge(overlayCtx, `${g.fieldW} × ${g.fieldH} px`, left + width - 84, top - 27);
  }

  function updateStatus() {
    const field = selectedField();
    const g = selectedGeometry();
    if (hasFieldSelection() && field && g) {
      selectedObjectStatus.textContent = `${field.type} ${field.name || 'Sin nombre'} · ${field.id || 'SIN_ID'}`;
      selectedGeometryStatus.textContent = `X:${field.x} Y:${field.y} · ${g.fieldW}×${g.fieldH} px`;
    } else {
      selectedObjectStatus.textContent = editor()?.getSelectedTool?.() === 'rawText'
        ? 'Texto GFX RAW'
        : 'Sin objeto seleccionado';
      selectedGeometryStatus.textContent = 'X:— Y:—';
    }
    zoomStatus.textContent = `Zoom: ${zoomSelect.value}×`;
  }

  function refreshUX() {
    updateStatus();
    requestAnimationFrame(drawGeometryOverlay);
  }

  geometryToggle.addEventListener('change', refreshUX);
  zoomSelect.addEventListener('change', refreshUX);
  fieldInputs.forEach((input) => {
    input.addEventListener('input', refreshUX);
    input.addEventListener('change', refreshUX);
  });

  fitButton.addEventListener('click', () => {
    const availableW = Math.max(320, canvasViewport.clientWidth - 36);
    const availableH = Math.max(170, canvasViewport.clientHeight - 30);
    const raw = Math.min(availableW / WIDTH, availableH / HEIGHT);
    const choices = [2, 3, 4, 6, 8];
    let best = 2;
    choices.forEach((candidate) => {
      if (candidate <= raw) best = candidate;
    });
    zoomSelect.value = String(best);
    zoomSelect.dispatchEvent(new Event('change', { bubbles: true }));
  });

  generateButton.addEventListener('click', () => {
    document.body.classList.remove('bottom-collapsed');
    bottomCollapsed = false;
    collapseBottomButton.textContent = '⌄';
    contractTab.click();
  });

  collapseBottomButton.addEventListener('click', () => {
    bottomCollapsed = !bottomCollapsed;
    document.body.classList.toggle('bottom-collapsed', bottomCollapsed);
    collapseBottomButton.textContent = bottomCollapsed ? '⌃' : '⌄';
    setTimeout(refreshUX, 0);
  });

  copyContractButton.addEventListener('click', async () => {
    try {
      await navigator.clipboard.writeText(inspectorContract.textContent);
      copyContractButton.textContent = '✓';
      setTimeout(() => { copyContractButton.textContent = '⧉'; }, 900);
    } catch (_) {
      copyContractButton.textContent = '!';
      setTimeout(() => { copyContractButton.textContent = '⧉'; }, 900);
    }
  });

  window.addEventListener('resize', refreshUX);
  window.addEventListener('jwplc:editor-refresh', refreshUX);
  displayCanvas.addEventListener('pointermove', refreshUX);
  document.querySelector('.left-panel')?.addEventListener('click', () => setTimeout(refreshUX, 0));

  const canvasObserver = new MutationObserver(refreshUX);
  canvasObserver.observe(displayCanvas, { attributes: true, attributeFilter: ['width', 'height', 'style'] });

  refreshUX();
})();
