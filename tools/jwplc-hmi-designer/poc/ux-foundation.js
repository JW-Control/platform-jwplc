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
  let fitResizeTimer = null;

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

  function badge(ctx, text, x, y, maxCssW) {
    ctx.save();
    ctx.font = '600 11px "Segoe UI", system-ui, -apple-system, sans-serif';
    const metrics = ctx.measureText(text);
    const width = Math.ceil(metrics.width) + 16;
    const height = 22;
    const bx = Math.round(Math.max(2, Math.min(maxCssW - width - 2, x)));
    const by = Math.round(Math.max(2, y));
    ctx.fillStyle = 'rgba(10, 20, 28, 0.94)';
    ctx.strokeStyle = '#617787';
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.roundRect(bx + 0.5, by + 0.5, width, height, 5);
    ctx.fill();
    ctx.stroke();
    ctx.fillStyle = '#e6f1f8';
    ctx.textBaseline = 'middle';
    ctx.fillText(text, bx + 8, by + height / 2 + 0.5);
    ctx.restore();
  }

  function regionRect(ctx, x, y, w, h, zoom, color, dash = [4, 3]) {
    if (w <= 0 || h <= 0) return;
    ctx.save();
    ctx.strokeStyle = color;
    ctx.lineWidth = 1;
    ctx.setLineDash(dash);
    ctx.strokeRect(Math.round(x * zoom) + 0.5, Math.round(y * zoom) + 0.5, Math.round(w * zoom), Math.round(h * zoom));
    ctx.restore();
  }

  function syncOverlaySize() {
    const dpr = Math.max(1, window.devicePixelRatio || 1);
    const cssW = displayCanvas.width;
    const cssH = displayCanvas.height;
    const targetW = Math.round(cssW * dpr);
    const targetH = Math.round(cssH * dpr);
    if (overlayCanvas.width !== targetW) overlayCanvas.width = targetW;
    if (overlayCanvas.height !== targetH) overlayCanvas.height = targetH;
    overlayCanvas.style.width = displayCanvas.style.width || `${cssW}px`;
    overlayCanvas.style.height = displayCanvas.style.height || `${cssH}px`;
    overlayCtx.setTransform(dpr, 0, 0, dpr, 0, 0);
    return { cssW, cssH };
  }

  function drawGeometryOverlay() {
    const { cssW, cssH } = syncOverlaySize();
    overlayCtx.clearRect(0, 0, cssW, cssH);
    if (!geometryToggle.checked || !hasFieldSelection()) return;

    const field = selectedField();
    const g = selectedGeometry();
    if (!field || !g) return;

    const zoom = Math.max(0.5, Number(zoomSelect.value) || 3);
    const left = g.fieldX * zoom;
    const top = g.fieldY * zoom;
    const height = g.fieldH * zoom;

    const labelX = g.fieldX + g.pad;
    const labelY = g.fieldY + g.pad;
    if (g.labelBounds && g.labelBounds.width > 0 && g.labelBounds.height > 0) {
      regionRect(overlayCtx, labelX, labelY, g.labelBounds.width, g.labelBounds.height, zoom, 'rgba(225,238,247,.65)');
    }
    if (g.valueW > 0 && g.valueH > 0) {
      regionRect(overlayCtx, g.valueX, g.valueY, g.valueW, g.valueH, zoom, 'rgba(40,220,232,.80)');
    }

    const badgeY = top >= 28 ? (top - 26) : Math.min(cssH - 25, top + height + 5);
    badge(overlayCtx, `X: ${g.fieldX}  Y: ${g.fieldY}  ·  ${g.fieldW} × ${g.fieldH} px`, left, badgeY, cssW);
  }

  function updateStatus() {
    const field = selectedField();
    const g = selectedGeometry();
    const activePg = Number(editor()?.getActivePage?.() ?? 0);
    const selPms = (window.JWPLCHMIPixelMaps?.getSelectedMaps?.() || []).filter(
      (pm) => Number(pm.page || 0) === activePg && pm.editorVisible !== false && pm.pixels?.length
    );
    const selFields = editor()?.getSelectedFields?.() || (field ? [field] : []);
    const totalCount = selFields.length + selPms.length;

    if (totalCount > 1) {
      selectedObjectStatus.textContent = `${totalCount} objetos seleccionados`;
      const b = editor()?.getSelectionBounds?.(selFields, selPms);
      if (b) {
        selectedGeometryStatus.textContent = `X:${b.minX} Y:${b.minY} · ${b.width}×${b.height} px`;
      } else {
        selectedGeometryStatus.textContent = 'X:— Y:—';
      }
    } else if (hasFieldSelection() && field && g) {
      selectedObjectStatus.textContent = `${field.type} ${field.name || 'Sin nombre'} · ${field.id || 'SIN_ID'}`;
      selectedGeometryStatus.textContent = `X:${field.x} Y:${field.y} · ${g.fieldW}×${g.fieldH} px`;
    } else if (selPms.length === 1) {
      const pm = selPms[0];
      const b = window.JWPLCHMIPixelMaps?.getBounds?.(pm);
      selectedObjectStatus.textContent = `PIXEL ${pm.name || 'Pixel'} · ${pm.key}`;
      selectedGeometryStatus.textContent = b ? `X:${b.x} Y:${b.y} · ${b.width}×${b.height} px` : 'X:— Y:—';
    } else {
      selectedObjectStatus.textContent = editor()?.getSelectedTool?.() === 'rawText'
        ? 'Texto GFX RAW'
        : 'Sin objeto seleccionado';
      selectedGeometryStatus.textContent = 'X:— Y:—';
    }
    const currentZoom = Number(zoomSelect.value) || 1;
    const fitActive = zoomSelect.selectedOptions?.[0]?.dataset.fitOption === '1';
    zoomStatus.textContent = fitActive
      ? `Zoom: Fit ${currentZoom.toFixed(2)}×`
      : `Zoom: ${zoomSelect.value}×`;
  }

  function refreshUX() {
    updateStatus();
    requestAnimationFrame(drawGeometryOverlay);
  }

  function exactFitZoom() {
    const availableW = Math.max(160, canvasViewport.clientWidth - 20);
    const availableH = Math.max(85, canvasViewport.clientHeight - 20);
    const raw = Math.min(availableW / WIDTH, availableH / HEIGHT, 8);
    return Math.max(0.5, Math.floor(raw * 100) / 100);
  }

  function applyExactFit() {
    const editor = window.JWPLCHMIEditor || window.jwplc;
    if (editor?.fitCanvas) {
      editor.fitCanvas();
      refreshUX();
      return;
    }
    const value = exactFitZoom();
    let option = zoomSelect.querySelector('option[data-fit-option="1"]');
    if (!option) {
      option = document.createElement('option');
      option.dataset.fitOption = '1';
      zoomSelect.insertBefore(option, zoomSelect.firstChild);
    }
    option.value = value.toFixed(2);
    option.textContent = `Fit · ${value.toFixed(2)}×`;
    zoomSelect.value = option.value;
    zoomSelect.dispatchEvent(new Event('change', { bubbles: true }));
    refreshUX();
  }

  geometryToggle.addEventListener('change', refreshUX);
  zoomSelect.addEventListener('change', refreshUX);
  fieldInputs.forEach((input) => {
    input.addEventListener('input', refreshUX);
    input.addEventListener('change', refreshUX);
  });

  fitButton?.addEventListener('click', (event) => {
    event.preventDefault();
    const editor = window.JWPLCHMIEditor || window.jwplc;
    editor?.fitCanvas?.();
  });

  const fitSelectionButton = document.getElementById('fitSelectionButton');
  fitSelectionButton?.addEventListener('click', (event) => {
    event.preventDefault();
    const editor = window.JWPLCHMIEditor || window.jwplc;
    editor?.fitSelection?.();
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

  window.addEventListener('resize', () => {
    refreshUX();
    if (zoomSelect.selectedOptions?.[0]?.dataset.fitOption !== '1') return;
    clearTimeout(fitResizeTimer);
    fitResizeTimer = setTimeout(applyExactFit, 90);
  });
  window.addEventListener('jwplc:editor-refresh', refreshUX);
  displayCanvas.addEventListener('pointermove', refreshUX);
  document.querySelector('.left-panel')?.addEventListener('click', () => setTimeout(refreshUX, 0));

  const canvasObserver = new MutationObserver(refreshUX);
  canvasObserver.observe(displayCanvas, { attributes: true, attributeFilter: ['width', 'height', 'style'] });

  refreshUX();
})();

// Alpha11: los gates del Designer se cargan fuera del core para no acoplar
// funcionalidad experimental al runtime/codegen base ya validado.
(() => {
  const V = '1791859500';
  function loadScript(src, onDone) {
    const script = document.createElement('script');
    script.src = `${src}?v=${V}`;
    script.async = false;
    script.onload = () => onDone?.();
    script.onerror = () => onDone?.();
    document.body.appendChild(script);
  }

  loadScript('./designer-color-editor.js?v=1793801300', () => {
  loadScript('./designer-bool.js?v=1793802300', () => {
    loadScript('./designer-bar.js?v=1793804800', () => {
      loadScript('./designer-zone-indicator.js?v=1793801300', () => {
        loadScript('./designer-field-inspector.js?v=1793801300', () => {
          loadScript('./designer-menu.js?v=1793801700');
          loadScript('./designer-event-log.js?v=1793802700');
          loadScript('./designer-icons.js?v=1793806000');
          loadScript('./designer-idle.js?v=1793801300');
          loadScript('./designer-field-visibility.js?v=1793802300', () => {
            loadScript('./designer-pages.js?v=1793802300', () => {
              loadScript('./designer-flasher.js?v=1793801500', () => {
                loadScript('./designer-port-picker.js?v=1793801500');
                loadScript('./designer-live.js?v=1793801600');
              });
            });
          });
        });
      });
    });
  });
  });
})();