(() => {
  'use strict';

  const WIDTH = 320;
  const HEIGHT = 170;
  const FIELD_PADDING = 3;
  const FIELD_GAP = 4;

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
  const textObjectItem = document.getElementById('textObjectItem');
  const objectName = document.getElementById('objectName');
  const objectId = document.getElementById('objectId');
  const inspectorContract = document.getElementById('inspectorContract');
  const copyContractButton = document.getElementById('copyContractButton');

  const fieldName = document.getElementById('fieldName');
  const fieldId = document.getElementById('fieldId');
  const fieldVariable = document.getElementById('fieldVariable');
  const fieldCapacity = document.getElementById('fieldCapacity');
  const fieldX = document.getElementById('fieldX');
  const fieldY = document.getElementById('fieldY');
  const fieldPreview = document.getElementById('fieldPreview');
  const fieldLabel = document.getElementById('fieldLabel');
  const fieldUnit = document.getElementById('fieldUnit');
  const fieldValueSize = document.getElementById('fieldValueSize');
  const fieldLabelSize = document.getElementById('fieldLabelSize');
  const fieldLayout = document.getElementById('fieldLayout');

  const selectedObjectStatus = document.getElementById('selectedObjectStatus');
  const selectedGeometryStatus = document.getElementById('selectedGeometryStatus');
  const zoomStatus = document.getElementById('zoomStatus');

  const trackedInputs = [
    fieldName, fieldId, fieldVariable, fieldCapacity, fieldX, fieldY,
    fieldPreview, fieldLabel, fieldUnit, fieldValueSize, fieldLabelSize,
    fieldLayout
  ].filter(Boolean);

  let selectedKind = 'textField';
  let bottomCollapsed = false;

  function numberValue(element, fallback = 0) {
    const parsed = Number(element && element.value);
    return Number.isFinite(parsed) ? parsed : fallback;
  }

  function nominalTextBounds(text, size) {
    if (!text) return { width: 0, height: 0 };
    const scale = Math.max(1, Math.trunc(size || 1));
    return {
      width: text.length * 6 * scale - scale,
      height: 7 * scale
    };
  }

  function readGeometry() {
    const x = Math.max(0, Math.min(WIDTH - 1, numberValue(fieldX)));
    const y = Math.max(0, Math.min(HEIGHT - 1, numberValue(fieldY)));
    const capacity = Math.max(1, Math.trunc(numberValue(fieldCapacity, 1)));
    const labelSize = Math.max(1, Math.trunc(numberValue(fieldLabelSize, 1)));
    const valueSize = Math.max(1, Math.trunc(numberValue(fieldValueSize, 1)));
    const pad = Math.max(FIELD_PADDING, labelSize, valueSize);
    const labelBounds = nominalTextBounds(fieldLabel.value, labelSize);
    const unitBounds = nominalTextBounds(fieldUnit.value, labelSize);
    const valueBounds = nominalTextBounds('W'.repeat(capacity), valueSize);
    const layout = fieldLayout.value;

    let fieldW;
    let fieldH;
    let valueX;
    let valueY;

    if (layout === 'STACKED') {
      const valueAndUnitW = valueBounds.width + (unitBounds.width > 0 ? FIELD_GAP + unitBounds.width : 0);
      fieldW = 2 * pad + Math.max(labelBounds.width, valueAndUnitW);
      fieldH = 2 * pad + labelBounds.height + (labelBounds.height > 0 ? FIELD_GAP : 0) + Math.max(valueBounds.height, unitBounds.height);
      valueX = x + pad;
      valueY = y + pad + labelBounds.height + (labelBounds.height > 0 ? FIELD_GAP : 0);
    } else {
      fieldW = 2 * pad + labelBounds.width + (labelBounds.width > 0 ? FIELD_GAP : 0) + valueBounds.width + (unitBounds.width > 0 ? FIELD_GAP + unitBounds.width : 0);
      fieldH = 2 * pad + Math.max(labelBounds.height, valueBounds.height, unitBounds.height);
      valueX = x + pad + labelBounds.width + (labelBounds.width > 0 ? FIELD_GAP : 0);
      valueY = y + pad;
    }

    return {
      x, y, fieldW, fieldH, pad, layout,
      labelSize, valueSize,
      labelBounds,
      valueBounds,
      unitBounds,
      labelX: x + pad,
      labelY: y + pad,
      valueX,
      valueY,
      unitX: valueX + valueBounds.width + (unitBounds.width > 0 ? FIELD_GAP : 0),
      unitY: valueY
    };
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

    if (!geometryToggle.checked || selectedKind !== 'textField') return;

    const zoom = Math.max(1, numberValue(zoomSelect, 3));
    const g = readGeometry();
    const left = g.x * zoom;
    const top = g.y * zoom;
    const width = g.fieldW * zoom;
    const height = g.fieldH * zoom;

    overlayCtx.save();
    overlayCtx.strokeStyle = '#ff8a2a';
    overlayCtx.lineWidth = 2;
    overlayCtx.setLineDash([]);
    overlayCtx.strokeRect(left + 0.5, top + 0.5, width, height);
    overlayCtx.restore();

    regionRect(overlayCtx, g.labelX, g.labelY, g.labelBounds.width, g.labelBounds.height, zoom, 'rgba(225,238,247,.60)');
    regionRect(overlayCtx, g.valueX, g.valueY, g.valueBounds.width, g.valueBounds.height, zoom, 'rgba(40,220,232,.72)');
    regionRect(overlayCtx, g.unitX, g.unitY, g.unitBounds.width, g.unitBounds.height, zoom, 'rgba(121,211,161,.62)');
    drawHandles(overlayCtx, left, top, width, height);

    badge(overlayCtx, `X: ${g.x}   Y: ${g.y}`, left + 7, top - 27);
    badge(overlayCtx, `${g.fieldW} × ${g.fieldH} px`, left + width - 84, top - 27);
  }

  function updateObjectAndStatus() {
    const name = (fieldName.value || 'TEXT').trim();
    const id = (fieldId.value || 'FIELD_STATUS').trim();
    const variable = (fieldVariable.value || 'estadoTexto').trim();
    const capacity = Math.max(1, Math.trunc(numberValue(fieldCapacity, 1)));
    const g = readGeometry();

    objectName.textContent = name;
    objectId.textContent = id;
    inspectorContract.textContent = `char ${variable}[${capacity + 1}];`;
    selectedObjectStatus.textContent = `TEXT ${name} · ${id}`;
    selectedGeometryStatus.textContent = `X:${g.x} Y:${g.y} · ${g.fieldW}×${g.fieldH} px`;
    zoomStatus.textContent = `Zoom: ${zoomSelect.value}×`;
  }

  function refreshUX() {
    updateObjectAndStatus();
    requestAnimationFrame(drawGeometryOverlay);
  }

  function activateTextField() {
    const textTool = document.querySelector('.tool[data-tool="textField"]');
    if (textTool) textTool.click();
    selectedKind = 'textField';
    textObjectItem.classList.add('active');
    refreshUX();
  }

  textObjectItem.addEventListener('click', activateTextField);

  document.querySelectorAll('.tool[data-tool]').forEach((button) => {
    button.addEventListener('click', () => {
      selectedKind = button.dataset.tool;
      textObjectItem.classList.toggle('active', selectedKind === 'textField');
      refreshUX();
    });
  });

  trackedInputs.forEach((input) => {
    input.addEventListener('input', refreshUX);
    input.addEventListener('change', refreshUX);
  });

  geometryToggle.addEventListener('change', refreshUX);
  zoomSelect.addEventListener('change', refreshUX);

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

  displayCanvas.addEventListener('pointerdown', () => setTimeout(refreshUX, 0));
  displayCanvas.addEventListener('pointermove', () => {
    if (selectedKind === 'textField') requestAnimationFrame(refreshUX);
  });
  window.addEventListener('resize', refreshUX);

  const canvasObserver = new MutationObserver(refreshUX);
  canvasObserver.observe(displayCanvas, { attributes: true, attributeFilter: ['width', 'height', 'style'] });

  // El PoC actual conserva TEXT como único objeto editable. La lista de objetos,
  // inspector y overlay ya quedan estructurados para escalar a VALUE/BOOL/BAR.
  refreshUX();
})();
