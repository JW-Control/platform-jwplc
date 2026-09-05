(() => {
  'use strict';

  const WIDTH = 320;
  const HEIGHT = 170;

  const COLORS = [
    { name: 'BLACK', value: 0x0000 },
    { name: 'WHITE', value: 0xFFFF },
    { name: 'RED', value: 0xF800 },
    { name: 'GREEN', value: 0x07E0 },
    { name: 'BLUE', value: 0x001F },
    { name: 'CYAN', value: 0x07FF },
    { name: 'YELLOW', value: 0xFFE0 },
    { name: 'ORANGE', value: 0xFD20 }
  ];

  const pixelLayer = new Uint16Array(WIDTH * HEIGHT);
  const framebuffer = new Uint16Array(WIDTH * HEIGHT);

  const displayCanvas = document.getElementById('displayCanvas');
  const displayCtx = displayCanvas.getContext('2d', { alpha: false });
  const previewCanvas = document.getElementById('previewCanvas');
  const previewCtx = previewCanvas.getContext('2d', { alpha: false });

  const logicalCanvas = document.createElement('canvas');
  logicalCanvas.width = WIDTH;
  logicalCanvas.height = HEIGHT;
  const logicalCtx = logicalCanvas.getContext('2d', { alpha: false });

  const zoomSelect = document.getElementById('zoomSelect');
  const gridToggle = document.getElementById('gridToggle');
  const clearButton = document.getElementById('clearButton');
  const newProjectButton = document.getElementById('newProjectButton');
  const demoButton = document.getElementById('demoButton');
  const cursorStatus = document.getElementById('cursorStatus');
  const pixelStatus = document.getElementById('pixelStatus');
  const palette = document.getElementById('palette');
  const activeColorSwatch = document.getElementById('activeColorSwatch');
  const activeColorName = document.getElementById('activeColorName');
  const activeColorValue = document.getElementById('activeColorValue');

  const textInput = document.getElementById('textInput');
  const textXInput = document.getElementById('textX');
  const textYInput = document.getElementById('textY');
  const textSizeSelect = document.getElementById('textSize');
  const textBackgroundSelect = document.getElementById('textBackground');
  const textScaleStatus = document.getElementById('textScaleStatus');
  const textBoundsStatus = document.getElementById('textBoundsStatus');

  let zoom = Number(zoomSelect.value);
  let selectedColor = COLORS.find((color) => color.name === 'ORANGE');
  let selectedTool = 'text';
  let drawing = false;
  let draggingText = false;
  let lastPoint = null;

  const textState = {
    x: Number(textXInput.value),
    y: Number(textYInput.value),
    size: Number(textSizeSelect.value),
    value: textInput.value,
    foreground: selectedColor.value,
    background: 0x0000
  };

  function hex565(value) {
    return `0x${value.toString(16).toUpperCase().padStart(4, '0')}`;
  }

  function rgb565ToRgb888(value) {
    const r5 = (value >> 11) & 0x1F;
    const g6 = (value >> 5) & 0x3F;
    const b5 = value & 0x1F;

    return {
      r: Math.round((r5 * 255) / 31),
      g: Math.round((g6 * 255) / 63),
      b: Math.round((b5 * 255) / 31)
    };
  }

  function rgb565ToCss(value) {
    const { r, g, b } = rgb565ToRgb888(value);
    return `rgb(${r}, ${g}, ${b})`;
  }

  function indexFor(x, y) {
    return y * WIDTH + x;
  }

  function inside(x, y) {
    return x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT;
  }

  function setLayerPixel(x, y, value) {
    if (!inside(x, y)) return false;
    const index = indexFor(x, y);
    if (pixelLayer[index] === value) return false;
    pixelLayer[index] = value;
    return true;
  }

  function setBufferPixel(buffer, x, y, value) {
    if (!inside(x, y)) return;
    buffer[indexFor(x, y)] = value;
  }

  function getPixel(x, y) {
    if (!inside(x, y)) return null;
    return framebuffer[indexFor(x, y)];
  }

  function clearAll() {
    pixelLayer.fill(0x0000);
    textInput.value = '';
    textState.value = '';
    render();
  }

  function resetProject() {
    pixelLayer.fill(0x0000);
    selectedTool = 'text';
    textState.x = 20;
    textState.y = 20;
    textState.size = 2;
    textState.value = 'HOLA JWPLC';
    textState.foreground = selectedColor.value;
    textState.background = 0x0000;

    textInput.value = textState.value;
    textXInput.value = String(textState.x);
    textYInput.value = String(textState.y);
    textSizeSelect.value = String(textState.size);
    textBackgroundSelect.value = 'BLACK';
    syncToolButtons();
    render();
  }

  function rasterLine(x0, y0, x1, y1, value) {
    let dx = Math.abs(x1 - x0);
    let sx = x0 < x1 ? 1 : -1;
    let dy = -Math.abs(y1 - y0);
    let sy = y0 < y1 ? 1 : -1;
    let error = dx + dy;
    let changed = false;

    while (true) {
      changed = setLayerPixel(x0, y0, value) || changed;
      if (x0 === x1 && y0 === y1) break;
      const e2 = 2 * error;
      if (e2 >= dy) {
        error += dy;
        x0 += sx;
      }
      if (e2 <= dx) {
        error += dx;
        y0 += sy;
      }
    }

    return changed;
  }

  function fillLayerRect(x, y, width, height, value) {
    const x0 = Math.max(0, x);
    const y0 = Math.max(0, y);
    const x1 = Math.min(WIDTH, x + width);
    const y1 = Math.min(HEIGHT, y + height);

    for (let py = y0; py < y1; py += 1) {
      for (let px = x0; px < x1; px += 1) {
        setLayerPixel(px, py, value);
      }
    }
  }

  function fillBufferRect(buffer, x, y, width, height, value) {
    const x0 = Math.max(0, x);
    const y0 = Math.max(0, y);
    const x1 = Math.min(WIDTH, x + width);
    const y1 = Math.min(HEIGHT, y + height);

    for (let py = y0; py < y1; py += 1) {
      for (let px = x0; px < x1; px += 1) {
        setBufferPixel(buffer, px, py, value);
      }
    }
  }

  function drawLayerRect(x, y, width, height, value) {
    if (width <= 0 || height <= 0) return;
    rasterLine(x, y, x + width - 1, y, value);
    rasterLine(x, y + height - 1, x + width - 1, y + height - 1, value);
    rasterLine(x, y, x, y + height - 1, value);
    rasterLine(x + width - 1, y, x + width - 1, y + height - 1, value);
  }

  function drawDemo() {
    pixelLayer.fill(0x0000);

    const orange = 0xFD20;
    const white = 0xFFFF;
    const green = 0x07E0;
    const red = 0xF800;

    drawLayerRect(6, 6, 308, 158, orange);
    fillLayerRect(14, 22, 92, 5, orange);
    fillLayerRect(14, 34, 140, 2, orange);

    drawLayerRect(18, 52, 130, 42, white);
    fillLayerRect(25, 62, 48, 22, orange);
    fillLayerRect(86, 62, 50, 22, green);

    drawLayerRect(18, 108, 220, 20, white);
    fillLayerRect(21, 111, 136, 14, green);

    drawLayerRect(248, 52, 50, 50, white);
    fillLayerRect(262, 66, 22, 22, red);

    drawLayerRect(236, 136, 62, 18, orange);
    fillLayerRect(244, 142, 45, 6, orange);

    textState.value = 'HOLA JWPLC';
    textState.x = 170;
    textState.y = 24;
    textState.size = 2;
    textState.foreground = white;
    textState.background = 0x0000;
    textInput.value = textState.value;
    textXInput.value = String(textState.x);
    textYInput.value = String(textState.y);
    textSizeSelect.value = String(textState.size);
    textBackgroundSelect.value = 'BLACK';

    render();
  }

  function drawClassicChar(buffer, x, y, charCode, foreground, background, size) {
    const font = window.JWPLCGfxClassicFont;
    const glyph = font.glyphFor(charCode);
    const scale = Math.max(1, Math.trunc(size));

    for (let column = 0; column < font.cellWidth; column += 1) {
      const bits = column < font.bytesPerGlyph ? glyph[column] : 0;

      for (let row = 0; row < font.cellHeight; row += 1) {
        const on = column < font.bytesPerGlyph && ((bits >> row) & 0x01) !== 0;
        const color = on ? foreground : background;
        fillBufferRect(
          buffer,
          x + column * scale,
          y + row * scale,
          scale,
          scale,
          color
        );
      }
    }
  }

  function drawClassicText(buffer) {
    if (!textState.value) return;

    const font = window.JWPLCGfxClassicFont;
    const scale = Math.max(1, Math.trunc(textState.size));
    let cursorX = textState.x;
    let cursorY = textState.y;

    for (const character of textState.value) {
      if (character === '\n') {
        cursorX = textState.x;
        cursorY += font.cellHeight * scale;
        continue;
      }

      if (character === '\r') continue;

      drawClassicChar(
        buffer,
        cursorX,
        cursorY,
        character.codePointAt(0),
        textState.foreground,
        textState.background,
        scale
      );

      cursorX += font.cellWidth * scale;
    }
  }

  function composeFramebuffer() {
    framebuffer.set(pixelLayer);
    drawClassicText(framebuffer);
  }

  function rebuildLogicalImage() {
    composeFramebuffer();

    const image = logicalCtx.createImageData(WIDTH, HEIGHT);
    const bytes = image.data;

    for (let i = 0; i < framebuffer.length; i += 1) {
      const { r, g, b } = rgb565ToRgb888(framebuffer[i]);
      const offset = i * 4;
      bytes[offset] = r;
      bytes[offset + 1] = g;
      bytes[offset + 2] = b;
      bytes[offset + 3] = 255;
    }

    logicalCtx.putImageData(image, 0, 0);
  }

  function drawGrid() {
    if (!gridToggle.checked || zoom < 3) return;

    displayCtx.save();
    displayCtx.strokeStyle = 'rgba(118, 151, 176, 0.18)';
    displayCtx.lineWidth = 1;
    displayCtx.beginPath();

    for (let x = 0; x <= WIDTH; x += 1) {
      const px = x * zoom + 0.5;
      displayCtx.moveTo(px, 0);
      displayCtx.lineTo(px, HEIGHT * zoom);
    }

    for (let y = 0; y <= HEIGHT; y += 1) {
      const py = y * zoom + 0.5;
      displayCtx.moveTo(0, py);
      displayCtx.lineTo(WIDTH * zoom, py);
    }

    displayCtx.stroke();
    displayCtx.restore();
  }

  function updateTextMetrics() {
    const font = window.JWPLCGfxClassicFont;
    const scale = Math.max(1, Math.trunc(textState.size));
    const width = textState.value.length * font.cellWidth * scale;
    const height = font.cellHeight * scale;
    textScaleStatus.textContent = `${scale}×`;
    textBoundsStatus.textContent = `${width} × ${height} px`;
  }

  function render() {
    rebuildLogicalImage();

    displayCanvas.width = WIDTH * zoom;
    displayCanvas.height = HEIGHT * zoom;
    displayCanvas.style.width = `${WIDTH * zoom}px`;
    displayCanvas.style.height = `${HEIGHT * zoom}px`;

    displayCtx.imageSmoothingEnabled = false;
    displayCtx.clearRect(0, 0, displayCanvas.width, displayCanvas.height);
    displayCtx.drawImage(
      logicalCanvas,
      0,
      0,
      WIDTH,
      HEIGHT,
      0,
      0,
      WIDTH * zoom,
      HEIGHT * zoom
    );

    drawGrid();

    previewCtx.imageSmoothingEnabled = false;
    previewCtx.clearRect(0, 0, WIDTH, HEIGHT);
    previewCtx.drawImage(logicalCanvas, 0, 0);

    updateTextMetrics();
  }

  function pointFromPointer(event) {
    const rect = displayCanvas.getBoundingClientRect();
    const scaleX = displayCanvas.width / rect.width;
    const scaleY = displayCanvas.height / rect.height;
    const x = Math.floor(((event.clientX - rect.left) * scaleX) / zoom);
    const y = Math.floor(((event.clientY - rect.top) * scaleY) / zoom);
    return { x, y };
  }

  function activeDrawValue() {
    return selectedTool === 'erase' ? 0x0000 : selectedColor.value;
  }

  function drawAt(point) {
    if (!inside(point.x, point.y)) return;

    const value = activeDrawValue();
    let changed;

    if (lastPoint === null) {
      changed = setLayerPixel(point.x, point.y, value);
    } else {
      changed = rasterLine(lastPoint.x, lastPoint.y, point.x, point.y, value);
    }

    lastPoint = point;
    if (changed) render();
  }

  function placeTextAt(point) {
    if (!inside(point.x, point.y)) return;
    textState.x = point.x;
    textState.y = point.y;
    textXInput.value = String(textState.x);
    textYInput.value = String(textState.y);
    render();
  }

  function updateCursor(point) {
    if (!inside(point.x, point.y)) {
      cursorStatus.textContent = 'X: — · Y: —';
      pixelStatus.textContent = 'Pixel: —';
      return;
    }

    const value = getPixel(point.x, point.y);
    cursorStatus.textContent = `X: ${point.x} · Y: ${point.y}`;
    pixelStatus.textContent = `Pixel: ${hex565(value)}`;
  }

  function updateActiveColorUI() {
    activeColorSwatch.style.background = rgb565ToCss(selectedColor.value);
    activeColorName.textContent = selectedColor.name;
    activeColorValue.textContent = hex565(selectedColor.value);
  }

  function syncToolButtons() {
    document.querySelectorAll('.tool[data-tool]').forEach((item) => {
      item.classList.toggle('active', item.dataset.tool === selectedTool);
    });
  }

  function buildPalette() {
    COLORS.forEach((color) => {
      const button = document.createElement('button');
      button.className = 'palette-button';
      button.title = `${color.name} ${hex565(color.value)}`;
      button.style.background = rgb565ToCss(color.value);
      button.dataset.colorName = color.name;

      if (color.name === selectedColor.name) {
        button.classList.add('active');
      }

      button.addEventListener('click', () => {
        selectedColor = color;
        if (selectedTool === 'text') {
          textState.foreground = color.value;
        }

        document.querySelectorAll('.palette-button').forEach((item) => {
          item.classList.toggle('active', item === button);
        });

        updateActiveColorUI();
        render();
      });

      palette.appendChild(button);
    });
  }

  function buildBackgroundSelect() {
    COLORS.forEach((color) => {
      const option = document.createElement('option');
      option.value = color.name;
      option.textContent = `${color.name} · ${hex565(color.value)}`;
      if (color.name === 'BLACK') option.selected = true;
      textBackgroundSelect.appendChild(option);
    });
  }

  function syncTextStateFromControls() {
    textState.value = textInput.value;
    textState.x = Math.max(0, Math.min(WIDTH - 1, Number(textXInput.value) || 0));
    textState.y = Math.max(0, Math.min(HEIGHT - 1, Number(textYInput.value) || 0));
    textState.size = Math.max(1, Number(textSizeSelect.value) || 1);

    const bg = COLORS.find((color) => color.name === textBackgroundSelect.value);
    textState.background = bg ? bg.value : 0x0000;

    textXInput.value = String(textState.x);
    textYInput.value = String(textState.y);
    render();
  }

  displayCanvas.addEventListener('pointerdown', (event) => {
    const point = pointFromPointer(event);
    updateCursor(point);
    displayCanvas.setPointerCapture(event.pointerId);

    if (selectedTool === 'text') {
      draggingText = true;
      placeTextAt(point);
      return;
    }

    drawing = true;
    lastPoint = null;
    drawAt(point);
  });

  displayCanvas.addEventListener('pointermove', (event) => {
    const point = pointFromPointer(event);
    updateCursor(point);

    if (selectedTool === 'text' && draggingText) {
      placeTextAt(point);
    } else if (drawing) {
      drawAt(point);
    }
  });

  displayCanvas.addEventListener('pointerup', (event) => {
    drawing = false;
    draggingText = false;
    lastPoint = null;
    if (displayCanvas.hasPointerCapture(event.pointerId)) {
      displayCanvas.releasePointerCapture(event.pointerId);
    }
  });

  displayCanvas.addEventListener('pointercancel', () => {
    drawing = false;
    draggingText = false;
    lastPoint = null;
  });

  displayCanvas.addEventListener('pointerleave', () => {
    if (!drawing && !draggingText) {
      cursorStatus.textContent = 'X: — · Y: —';
      pixelStatus.textContent = 'Pixel: —';
    }
  });

  zoomSelect.addEventListener('change', () => {
    zoom = Number(zoomSelect.value);
    render();
  });

  gridToggle.addEventListener('change', render);
  clearButton.addEventListener('click', clearAll);
  newProjectButton.addEventListener('click', resetProject);
  demoButton.addEventListener('click', drawDemo);

  [textInput, textXInput, textYInput, textSizeSelect, textBackgroundSelect]
    .forEach((control) => control.addEventListener('input', syncTextStateFromControls));

  document.querySelectorAll('.tool[data-tool]').forEach((button) => {
    button.addEventListener('click', () => {
      selectedTool = button.dataset.tool;
      if (selectedTool === 'text') {
        textState.foreground = selectedColor.value;
      }
      syncToolButtons();
      render();
    });
  });

  buildPalette();
  buildBackgroundSelect();
  updateActiveColorUI();
  syncToolButtons();
  resetProject();
})();
