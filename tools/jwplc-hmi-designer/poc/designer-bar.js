(() => {
  'use strict';

  const WIDTH = 320;
  const HEIGHT = 170;
  const FIELD_PADDING = 3;
  const FIELD_GAP = 4;
  const DEFAULT_BAR_WIDTH = 80;
  const DEFAULT_BAR_HEIGHT = 12;

  const barButton = document.querySelector('[data-tool="barField"]') ||
    [...document.querySelectorAll('.component-tool, .canvas-tool')]
      .find((button) => button.querySelector('strong')?.textContent.trim() === 'BAR');
  const gate = document.querySelector('.page-tabs .gate');
  const bottomSummary = document.querySelector('.bottom-summary');
  const fieldSection = document.getElementById('textFieldControlsSection');
  const fieldInspectorTitle = document.getElementById('fieldInspectorTitle');
  const numericFormatDetails = document.getElementById('numericFormatDetails');
  const boolDetails = document.getElementById('boolTextDetails');
  const fieldCapacityWrap = document.getElementById('fieldCapacityWrap');
  const fieldCppType = document.getElementById('fieldCppType');
  const fieldPreview = document.getElementById('fieldPreview');
  const fieldPreviewWrap = fieldPreview?.closest('label');
  const fieldName = document.getElementById('fieldName');
  const fieldId = document.getElementById('fieldId');
  const fieldVariable = document.getElementById('fieldVariable');
  const fieldLabel = document.getElementById('fieldLabel');
  const fieldUnit = document.getElementById('fieldUnit');
  const fieldX = document.getElementById('fieldX');
  const fieldY = document.getElementById('fieldY');
  const fieldValueSize = document.getElementById('fieldValueSize');
  const fieldValueSizeWrap = fieldValueSize?.closest('label');
  const fieldLabelSize = document.getElementById('fieldLabelSize');
  const fieldAlign = document.getElementById('fieldAlign');
  const fieldLayout = document.getElementById('fieldLayout');
  const fieldFrame = document.getElementById('fieldFrame');
  const fieldLabelColor = document.getElementById('fieldLabelColor');
  const fieldValueColor = document.getElementById('fieldValueColor');
  const fieldBackgroundColor = document.getElementById('fieldBackgroundColor');
  const fieldFrameColor = document.getElementById('fieldFrameColor');
  const fieldPadStatus = document.getElementById('fieldPadStatus');
  const fieldBoundsStatus = document.getElementById('fieldBoundsStatus');
  const fieldValueBoundsStatus = document.getElementById('fieldValueBoundsStatus');
  const fieldValueXYStatus = document.getElementById('fieldValueXYStatus');
  const fieldLayoutStatus = document.getElementById('fieldLayoutStatus');
  const inspectorContract = document.getElementById('inspectorContract');
  const codeOutput = document.getElementById('codeOutput');
  const contractTab = document.getElementById('contractTab');
  const statusTab = document.getElementById('statusTab');
  const generateButton = document.getElementById('generateButton');
  const displayCanvas = document.getElementById('displayCanvas');
  const previewCanvas = document.getElementById('previewCanvas');
  const zoomSelect = document.getElementById('zoomSelect');
  const gridToggle = document.getElementById('gridToggle');

  if (!barButton || !fieldSection || !displayCanvas || !previewCanvas) return;


  // --- Creación de Secciones del Inspector en la barra lateral derecha ---
  const barPresetsDetails = document.createElement('details');
  barPresetsDetails.id = 'barPresetsDetails';
  barPresetsDetails.open = true;
  barPresetsDetails.hidden = true;
  barPresetsDetails.innerHTML = `
    <summary>Tipo de Medidor y Prueba</summary>
    <div class="bar-top-slider-wrap">
      <div class="bar-top-slider-header">
        <span>Simulación / Valor de prueba</span>
        <strong id="barPercentStatus">50.0 %</strong>
      </div>
      <input id="fieldBarValueSlider" class="bar-top-slider-input" type="range" min="0" max="100" step="0.5" value="50" />
      <input id="fieldBarValue" type="hidden" value="50" />
      <input id="fieldBarMin" type="hidden" value="0" />
      <input id="fieldBarMax" type="hidden" value="100" />
    </div>
    <div class="bar-presets-grid two-presets">
      <button type="button" class="bar-preset-btn active" data-bar-preset="LINEAR" title="Barra de nivel (Horizontal o Vertical)">
        <span class="bar-preset-icon">
          <svg width="24" height="14" viewBox="0 0 24 14" fill="none" style="display:block;margin:0 auto 4px;">
            <rect x="1" y="2" width="22" height="10" rx="2" stroke="#52c9ff" stroke-width="1.5" />
            <rect x="3" y="4" width="12" height="6" rx="1" fill="#52c9ff" />
          </svg>
        </span>
        <span class="bar-preset-name">Barra</span>
        <span class="bar-preset-desc">Nivel clásico</span>
      </button>
      <button type="button" class="bar-preset-btn" data-bar-preset="CIRCULAR" title="Medidor circular o anillo de carga">
        <span class="bar-preset-icon">
          <svg width="22" height="22" viewBox="0 0 22 22" fill="none" style="display:block;margin:0 auto 4px;">
            <circle cx="11" cy="11" r="8" stroke="#253a48" stroke-width="2.5" />
            <path d="M 11 3 A 8 8 0 1 1 3 11" stroke="#52c9ff" stroke-width="2.5" stroke-linecap="round" />
          </svg>
        </span>
        <span class="bar-preset-name">Circular</span>
        <span class="bar-preset-desc">Arco / % Carga</span>
      </button>
    </div>`;

  const barConfigDetails = document.createElement('details');
  barConfigDetails.id = 'barConfigDetails';
  barConfigDetails.open = true;
  barConfigDetails.hidden = true;
  barConfigDetails.innerHTML = `
    <summary id="barConfigSummary">Configuración del Medidor</summary>

    <!-- Controles Lineales (Barra Sólida y Degradada) -->
    <div id="barLinearControls" class="inspector-body two-cols">
      <label class="field-label">Orientación
        <select id="fieldBarOrientation" class="field-input">
          <option value="HORIZONTAL" selected>Horizontal</option>
          <option value="VERTICAL">Vertical</option>
        </select>
      </label>
      <label class="field-label">Modo Ancho
        <select id="fieldBarWidthMode" class="field-input">
          <option value="AUTO">AUTO</option>
          <option value="FIXED" selected>FIJO</option>
        </select>
      </label>
      <label class="field-label" id="fieldBarWidthLabel"><span id="fieldBarWidthName">Longitud / Ancho (px)</span>
        <input id="fieldBarWidth" class="field-input" type="number" min="6" max="320" value="110" />
      </label>
      <label class="field-label" id="fieldBarHeightLabel"><span id="fieldBarHeightName">Grosor / Alto (px)</span>
        <input id="fieldBarHeight" class="field-input" type="number" min="4" max="170" value="12" />
      </label>
      <label class="field-label full">Tipo de Relleno
        <select id="fieldBarFillMode" class="field-input">
          <option value="SOLID" selected>Color Sólido</option>
          <option value="GRADIENT">Degradado</option>
        </select>
      </label>
      <div id="fieldBarSolidColorWrap" class="field-label full">
        <span style="font-size:10px;color:#8ea3b3;margin-bottom:4px;display:block;">Color de Barra</span>
        <div class="bar-color-input-wrap">
          <input id="fieldBarSolidColorNative" type="color" value="#00ffff" />
          <span id="fieldBarSolidColorHex" class="hex-label compact">0x07FF</span>
        </div>
      </div>
      <label class="field-label full">Color de Pista vacía
        <div class="bar-color-input-wrap">
          <input id="fieldBarTrackColorNative" type="color" value="#18232c" />
          <span id="fieldBarTrackColorHex" class="hex-label compact">0x18C3</span>
        </div>
      </label>
      <label class="field-label full">Mostrar % de Carga
        <select id="fieldBarLinearPctPos" class="field-input">
          <option value="NONE" selected>No (Por defecto)</option>
          <option value="RIGHT">Lado derecho (Exterior)</option>
          <option value="INSIDE">Al interior de la barra</option>
        </select>
      </label>
      <div id="fieldBarLinearPctColWrap" class="field-label full" style="display:none;">
        <span style="font-size:10px;color:#8ea3b3;margin-bottom:4px;display:block;">Color de Porcentaje</span>
        <div class="bar-color-input-wrap">
          <input id="fieldBarLinearPctColNative" type="color" value="#ffffff" />
          <span id="fieldBarLinearPctColHex" class="hex-label compact">0xFFFF</span>
        </div>
      </div>
    </div>

    <!-- Controles Exclusivos de Degradado Lineal -->
    <div id="barGradientControls" class="inspector-body full" style="display:none;padding:0;">
      <div class="bar-grad-row full">
        <label class="field-label" style="flex:1;min-width:0;">Color Inicial
          <div class="bar-color-input-wrap compact">
            <input id="fieldBarGradStartNative" type="color" value="#ff0000" />
            <span id="fieldBarGradStartHex" class="hex-label compact">0xF800</span>
          </div>
        </label>
        <button type="button" id="barLinearSwapGradBtn" class="bar-swap-icon-btn" title="Intercalar color inicial y final">⇄</button>
        <label class="field-label" style="flex:1;min-width:0;">Color Final
          <div class="bar-color-input-wrap compact">
            <input id="fieldBarGradEndNative" type="color" value="#00ff00" />
            <span id="fieldBarGradEndHex" class="hex-label compact">0x07E0</span>
          </div>
        </label>
      </div>
      <div class="field-label full" style="margin-top:4px;">
        <span style="font-size:10px;color:#8ea3b3;margin-bottom:4px;display:block;">Paletas rápidas de degradado:</span>
        <div class="bar-palette-row">
          <button type="button" class="bar-palette-pill" data-grad-start="0xF800" data-grad-end="0x07E0">
            <span class="bar-palette-preview" style="background:linear-gradient(90deg,#ff0000,#00ff00);"></span>Rojo-Verde
          </button>
          <button type="button" class="bar-palette-pill" data-grad-start="0x07E0" data-grad-end="0xF800">
            <span class="bar-palette-preview" style="background:linear-gradient(90deg,#00ff00,#ff0000);"></span>Verde-Rojo
          </button>
          <button type="button" class="bar-palette-pill" data-grad-start="0x07FF" data-grad-end="0x001F">
            <span class="bar-palette-preview" style="background:linear-gradient(90deg,#00ffff,#0000ff);"></span>Cian-Azul
          </button>
          <button type="button" class="bar-palette-pill" data-grad-start="0xFD20" data-grad-end="0xF800">
            <span class="bar-palette-preview" style="background:linear-gradient(90deg,#ff9900,#ff0000);"></span>Fuego
          </button>
          <button type="button" class="bar-palette-pill" data-grad-start="0x780F" data-grad-end="0xF81F">
            <span class="bar-palette-preview" style="background:linear-gradient(90deg,#7700ff,#ff00ff);"></span>Neón
          </button>
        </div>
      </div>
    </div>

    <!-- Controles Exclusivos de Medidor Circular / Carga -->
    <div id="barCircularControls" class="inspector-body two-cols" style="display:none;">
      <!-- 1. Apertura del Arco (Botones Visuales) -->
      <div class="field-label full">
        <span class="bar-visual-group-title">Apertura del Arco</span>
        <div class="bar-visual-grid-4" id="barArcButtonsWrap">
          <button type="button" class="bar-visual-btn bar-arc-btn active" data-sweep="360" title="360° Anillo completo">
            <svg width="18" height="18" viewBox="0 0 24 24"><circle cx="12" cy="12" r="8" stroke-width="3"/></svg>
            <span>360°</span>
          </button>
          <button type="button" class="bar-visual-btn bar-arc-btn" data-sweep="270" title="270° Tacómetro 3/4">
            <svg width="18" height="18" viewBox="0 0 24 24"><path d="M 6.3 17.7 A 8 8 0 1 1 17.7 17.7" stroke-width="3" stroke-linecap="round"/></svg>
            <span>270°</span>
          </button>
          <button type="button" class="bar-visual-btn bar-arc-btn" data-sweep="240" title="240° Manómetro">
            <svg width="18" height="18" viewBox="0 0 24 24"><path d="M 5.1 16 A 8 8 0 1 1 18.9 16" stroke-width="3" stroke-linecap="round"/></svg>
            <span>240°</span>
          </button>
          <button type="button" class="bar-visual-btn bar-arc-btn" data-sweep="180" title="180° Semicírculo">
            <svg width="18" height="18" viewBox="0 0 24 24"><path d="M 4 13 A 8 8 0 0 1 20 13" stroke-width="3" stroke-linecap="round"/></svg>
            <span>180°</span>
          </button>
        </div>
      </div>

      <!-- 2. Dirección de llenado (Botones) -->
      <div class="field-label full">
        <span class="bar-visual-group-title">Dirección de llenado</span>
        <div class="bar-visual-grid-2" id="barDirButtonsWrap">
          <button type="button" class="bar-visual-btn bar-dir-btn active" data-dir="CW" title="Sentido de agujas del reloj">
            <span style="font-size:13px;line-height:1;">↻</span>
            <span>Horario</span>
          </button>
          <button type="button" class="bar-visual-btn bar-dir-btn" data-dir="CCW" title="Sentido antihorario">
            <span style="font-size:13px;line-height:1;">↺</span>
            <span>Antihorario</span>
          </button>
        </div>
      </div>

      <!-- 3. Punto de inicio del 0 (Botones) -->
      <div class="field-label full">
        <span class="bar-visual-group-title">Inicio del 0</span>
        <div class="bar-visual-grid-4" id="barQuadrantButtonsWrap">
          <button type="button" class="bar-visual-btn bar-quadrant-btn active" data-quadrant="TOP" title="Arriba (12h)">
            <span style="font-size:13px;line-height:1;">↑</span>
            <span>Arriba</span>
          </button>
          <button type="button" class="bar-visual-btn bar-quadrant-btn" data-quadrant="RIGHT" title="Derecha (3h)">
            <span style="font-size:13px;line-height:1;">→</span>
            <span>Derecha</span>
          </button>
          <button type="button" class="bar-visual-btn bar-quadrant-btn" data-quadrant="BOTTOM" title="Abajo (6h)">
            <span style="font-size:13px;line-height:1;">↓</span>
            <span>Abajo</span>
          </button>
          <button type="button" class="bar-visual-btn bar-quadrant-btn" data-quadrant="LEFT" title="Izquierda (9h)">
            <span style="font-size:13px;line-height:1;">←</span>
            <span>Izquierda</span>
          </button>
        </div>
      </div>

      <!-- 4. Estilos de Anillo (Botones inspirados en la imagen) -->
      <div class="field-label full">
        <span class="bar-visual-group-title">Estilo de Anillo</span>
        <div class="bar-visual-grid-4" id="barStyleButtonsWrap">
          <button type="button" class="bar-visual-btn bar-style-btn active" data-style="CONTINUOUS" title="Anillo continuo sólido">
            <svg width="18" height="18" viewBox="0 0 24 24"><circle cx="12" cy="12" r="7.5" stroke-width="3.5"/></svg>
            <span>Sólido</span>
          </button>
          <button type="button" class="bar-visual-btn bar-style-btn" data-style="SEGMENTED" title="Segmentos o bloques discretos">
            <svg width="18" height="18" viewBox="0 0 24 24"><circle cx="12" cy="12" r="7.5" stroke-width="3.5" stroke-dasharray="3.2 2"/></svg>
            <span>Bloques</span>
          </button>
          <button type="button" class="bar-visual-btn bar-style-btn" data-style="DOTS" title="Puntos o esferas independientes">
            <svg width="18" height="18" viewBox="0 0 24 24"><circle cx="12" cy="12" r="7.5" stroke-width="3" stroke-dasharray="0.1 3.8" stroke-linecap="round"/></svg>
            <span>Puntos</span>
          </button>
          <button type="button" class="bar-visual-btn bar-style-btn" data-style="TICKS" title="Marcas o rayas radiales">
            <svg width="18" height="18" viewBox="0 0 24 24"><circle cx="12" cy="12" r="7.5" stroke-width="3.5" stroke-dasharray="1 2.2"/></svg>
            <span>Rayas</span>
          </button>
        </div>
      </div>

      <!-- 5. Dimensiones Radio y Grosor -->
      <label class="field-label">Radio exterior (px)
        <input id="fieldBarCircleRadius" class="field-input" type="number" min="10" max="80" value="26" />
      </label>
      <label class="field-label">Grosor anillo (px)
        <input id="fieldBarCircleThickness" class="field-input" type="number" min="2" max="25" value="5" />
      </label>

      <!-- 6. Relleno y Colores -->
      <label class="field-label full">Tipo de Relleno Arco
        <select id="fieldBarCircleColorMode" class="field-input">
          <option value="SOLID" selected>Color Sólido</option>
          <option value="GRADIENT">Degradado</option>
        </select>
      </label>
      <div id="fieldBarCircleSolidWrap" class="field-label full">
        <span style="font-size:10px;color:#8ea3b3;margin-bottom:4px;display:block;">Color Arco Carga</span>
        <div class="bar-color-input-wrap">
          <input id="fieldBarCircleColorNative" type="color" value="#00ffff" />
          <span id="fieldBarCircleColorHex" class="hex-label compact">0x07FF</span>
        </div>
      </div>
      <div id="fieldBarCircleGradWrap" class="inspector-body full" style="display:none;padding:0;">
        <div class="bar-grad-row full">
          <label class="field-label" style="flex:1;min-width:0;">Color Inicial
            <div class="bar-color-input-wrap compact">
              <input id="fieldBarCircleGradStartNative" type="color" value="#ff0000" />
              <span id="fieldBarCircleGradStartHex" class="hex-label compact">0xF800</span>
            </div>
          </label>
          <button type="button" id="barCircleSwapGradBtn" class="bar-swap-icon-btn" title="Intercalar color inicial y final">⇄</button>
          <label class="field-label" style="flex:1;min-width:0;">Color Final
            <div class="bar-color-input-wrap compact">
              <input id="fieldBarCircleGradEndNative" type="color" value="#00ff00" />
              <span id="fieldBarCircleGradEndHex" class="hex-label compact">0x07E0</span>
            </div>
          </label>
        </div>
      </div>
      <label class="field-label full">Pista del Anillo
        <div class="bar-color-input-wrap">
          <input id="fieldBarCircleTrackNative" type="color" value="#212930" />
          <span id="fieldBarCircleTrackHex" class="hex-label compact">0x2104</span>
        </div>
      </label>

      <!-- 7. Texto Central y Opciones -->
      <label class="field-label full">Texto al Centro
        <select id="fieldBarCircleCenterMode" class="field-input">
          <option value="PERCENT" selected>Porcentaje (Ej: 75%)</option>
          <option value="FRACTION">Fracción (Ej: 8/8, 7/16)</option>
          <option value="VALUE">Valor actual (Ej: 50)</option>
          <option value="NONE">Oculto (Sin texto)</option>
        </select>
      </label>
      <label class="field-label" id="fieldBarCircleTextSizeWrap">
        <span style="display:flex;align-items:center;justify-content:space-between;">
          <span>Tamaño texto</span>
          <span id="fieldBarCircleSizeLimitHint" class="bar-text-limit-hint" title="Límite para no tocar el borde interior del círculo">Máx: 2×</span>
        </span>
        <select id="fieldBarCircleTextSize" class="field-input">
          <option value="1" selected>1×</option>
          <option value="2">2×</option>
          <option value="3">3×</option>
          <option value="4">4×</option>
        </select>
      </label>
      <label class="field-label" id="fieldBarCircleTextColWrap">Color texto
        <div class="bar-color-input-wrap">
          <input id="fieldBarCircleTextColNative" type="color" value="#ffffff" />
          <span id="fieldBarCircleTextColHex" class="hex-label compact">0xFFFF</span>
        </div>
      </label>
    </div>`;

  // Inserción ordenada del inspector para BAR:
  // 1. Tipo de Medidor y Slider de Prueba (primera opción al inicio de la configuración)
  const contentDetails = document.getElementById('fieldContentDetails');
  if (contentDetails) {
    contentDetails.insertAdjacentElement('beforebegin', barPresetsDetails);
    // 2. Configuración del Medidor (inmediatamente debajo de Tipo de Medidor)
    barPresetsDetails.insertAdjacentElement('afterend', barConfigDetails);
  } else {
    const anchorNode = boolDetails || numericFormatDetails;
    anchorNode?.insertAdjacentElement('afterend', barPresetsDetails);
    barPresetsDetails.insertAdjacentElement('afterend', barConfigDetails);
  }

  // Referencias a elementos interactivos del inspector BAR
  const fieldBarMin = document.getElementById('fieldBarMin');
  const fieldBarMax = document.getElementById('fieldBarMax');
  const fieldBarValue = document.getElementById('fieldBarValue');
  const fieldBarValueSlider = document.getElementById('fieldBarValueSlider');
  const barPercentStatus = document.getElementById('barPercentStatus');
  const barConfigSummary = document.getElementById('barConfigSummary');
  const barLinearControls = document.getElementById('barLinearControls');
  const barGradientControls = document.getElementById('barGradientControls');
  const barCircularControls = document.getElementById('barCircularControls');
  const fieldBarShowLabel = document.getElementById('fieldBarShowLabel');
  const fieldBarOrientation = document.getElementById('fieldBarOrientation');
  const fieldBarWidthMode = document.getElementById('fieldBarWidthMode');
  const fieldBarWidth = document.getElementById('fieldBarWidth');
  const fieldBarHeight = document.getElementById('fieldBarHeight');
  const fieldBarWidthName = document.getElementById('fieldBarWidthName');
  const fieldBarHeightName = document.getElementById('fieldBarHeightName');
  const fieldBarFillMode = document.getElementById('fieldBarFillMode');
  const fieldBarSolidColorWrap = document.getElementById('fieldBarSolidColorWrap');
  const fieldBarSolidColorNative = document.getElementById('fieldBarSolidColorNative');
  const fieldBarSolidColorHex = document.getElementById('fieldBarSolidColorHex');
  const fieldBarTrackColorNative = document.getElementById('fieldBarTrackColorNative');
  const fieldBarTrackColorHex = document.getElementById('fieldBarTrackColorHex');
  const fieldBarGradStartNative = document.getElementById('fieldBarGradStartNative');
  const fieldBarGradStartHex = document.getElementById('fieldBarGradStartHex');
  const fieldBarGradEndNative = document.getElementById('fieldBarGradEndNative');
  const fieldBarGradEndHex = document.getElementById('fieldBarGradEndHex');
  const barLinearSwapGradBtn = document.getElementById('barLinearSwapGradBtn');
  const fieldBarLinearPctPos = document.getElementById('fieldBarLinearPctPos');
  const fieldBarLinearPctColWrap = document.getElementById('fieldBarLinearPctColWrap');
  const fieldBarLinearPctColNative = document.getElementById('fieldBarLinearPctColNative');
  const fieldBarLinearPctColHex = document.getElementById('fieldBarLinearPctColHex');
  const fieldBarCircleRadius = document.getElementById('fieldBarCircleRadius');
  const fieldBarCircleThickness = document.getElementById('fieldBarCircleThickness');
  const fieldBarCircleColorMode = document.getElementById('fieldBarCircleColorMode');
  const fieldBarCircleSolidWrap = document.getElementById('fieldBarCircleSolidWrap');
  const fieldBarCircleGradWrap = document.getElementById('fieldBarCircleGradWrap');
  const fieldBarCircleColorNative = document.getElementById('fieldBarCircleColorNative');
  const fieldBarCircleColorHex = document.getElementById('fieldBarCircleColorHex');
  const fieldBarCircleGradStartNative = document.getElementById('fieldBarCircleGradStartNative');
  const fieldBarCircleGradStartHex = document.getElementById('fieldBarCircleGradStartHex');
  const fieldBarCircleGradEndNative = document.getElementById('fieldBarCircleGradEndNative');
  const fieldBarCircleGradEndHex = document.getElementById('fieldBarCircleGradEndHex');
  const barCircleSwapGradBtn = document.getElementById('barCircleSwapGradBtn');
  const fieldBarCircleTrackNative = document.getElementById('fieldBarCircleTrackNative');
  const fieldBarCircleTrackHex = document.getElementById('fieldBarCircleTrackHex');
  const fieldBarCircleCenterMode = document.getElementById('fieldBarCircleCenterMode');
  const fieldBarCircleTextSize = document.getElementById('fieldBarCircleTextSize');
  const fieldBarCircleSizeLimitHint = document.getElementById('fieldBarCircleSizeLimitHint');
  const fieldBarCircleTextColNative = document.getElementById('fieldBarCircleTextColNative');
  const fieldBarCircleTextColHex = document.getElementById('fieldBarCircleTextColHex');
  const alignMatrixCells = document.querySelectorAll('.align-matrix-3x3 .matrix-cell');
  const barAlignMatrixRow = document.getElementById('barAlignMatrixRow');
  const fieldBarLabelSize = document.getElementById('fieldBarLabelSize');
  const fieldBarLabelSizeWrap = document.getElementById('fieldBarLabelSizeWrap');

  function editor() {
    return window.JWPLCHMIEditor || null;
  }

  function selectedField() {
    return editor()?.getSelectedField?.() || null;
  }

  function isBar(field = selectedField()) {
    return field?.type === 'BAR';
  }

  function activeBars() {
    return (editor()?.getAllFields?.() || []).filter((field) => field?.type === 'BAR');
  }

  function visibleBars() {
    const page = Number(editor()?.getActivePage?.() ?? 0);
    return activeBars().filter((field) => Number(field.page || 0) === page);
  }

  function serialFor(field) {
    const match = String(field?.key || field?.id || '').match(/-(\d+)$/);
    if (match) return Number(match[1]);
    const num = String(field?.name || '').match(/(\d+)$/);
    return num ? Number(num[1]) : 1;
  }

  function sanitizeSymbol(value, fallback) {
    const cleaned = String(value || '').trim().replace(/[^A-Za-z0-9_]/g, '_');
    if (!cleaned) return fallback;
    return /^[A-Za-z_]/.test(cleaned) ? cleaned : `_${cleaned}`;
  }

  function cppString(value) {
    return String(value || '').replace(/\\/g, '\\\\').replace(/"/g, '\\"');
  }

  function cppBool(value) {
    return value ? 'true' : 'false';
  }

  function cppFloat(value) {
    const number = Number(value);
    if (!Number.isFinite(number)) return '0.0f';
    return `${Number.isInteger(number) ? number.toFixed(1) : String(number)}f`;
  }

  function hex565(value) {
    if (value === 'TRANSPARENT' || value === -1) return 'TRANSPARENT';
    return `0x${(Number(value || 0) & 0xFFFF).toString(16).toUpperCase().padStart(4, '0')}`;
  }

  function rgbTo565(r, g, b) {
    return (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)) & 0xFFFF;
  }

  function hexCssTo565(hex) {
    const cleaned = String(hex || '').replace('#', '').trim();
    if (cleaned.length === 3) {
      const r = parseInt(cleaned[0] + cleaned[0], 16);
      const g = parseInt(cleaned[1] + cleaned[1], 16);
      const b = parseInt(cleaned[2] + cleaned[2], 16);
      return rgbTo565(r, g, b);
    }
    const num = parseInt(cleaned, 16);
    if (isNaN(num)) return 0xFFFF;
    const r = (num >> 16) & 0xFF;
    const g = (num >> 8) & 0xFF;
    const b = num & 0xFF;
    return rgbTo565(r, g, b);
  }

  function rgb565ToCss(value) {
    if (value === 'TRANSPARENT' || value === -1) return 'transparent';
    const color = Number(value || 0);
    const r = Math.round((((color >> 11) & 0x1F) * 255) / 31);
    const g = Math.round((((color >> 5) & 0x3F) * 255) / 63);
    const b = Math.round(((color & 0x1F) * 255) / 31);
    return `rgb(${r}, ${g}, ${b})`;
  }

  function rgb565ToHexCss(value) {
    if (value === 'TRANSPARENT' || value === -1) return '#000000';
    const color = Number(value || 0);
    const r = Math.round((((color >> 11) & 0x1F) * 255) / 31);
    const g = Math.round((((color >> 5) & 0x3F) * 255) / 63);
    const b = Math.round(((color & 0x1F) * 255) / 31);
    return `#${r.toString(16).padStart(2, '0')}${g.toString(16).padStart(2, '0')}${b.toString(16).padStart(2, '0')}`;
  }

  function nominalTextBounds(text, size) {
    const value = String(text || '');
    if (!value) return { width: 0, height: 0 };
    const scale = Math.max(1, Math.trunc(Number(size) || 1));
    return { width: value.length * 6 * scale - scale, height: 7 * scale };
  }

  function interpolateColor565(c1, c2, t) {
    const clamped = Math.max(0, Math.min(1, Number(t) || 0));
    const r1 = (c1 >> 11) & 0x1F, g1 = (c1 >> 5) & 0x3F, b1 = c1 & 0x1F;
    const r2 = (c2 >> 11) & 0x1F, g2 = (c2 >> 5) & 0x3F, b2 = c2 & 0x1F;
    const r = Math.round(r1 + (r2 - r1) * clamped);
    const g = Math.round(g1 + (g2 - g1) * clamped);
    const b = Math.round(b1 + (b2 - b1) * clamped);
    return (r << 11) | (g << 5) | b;
  }

  function computeSafeCenterTextSize(field, text) {
    const radius = Math.max(10, Math.trunc(Number(field?.circleRadius) || 26));
    const thickness = Math.max(2, Math.min(radius - 2, Math.trunc(Number(field?.circleThickness) || 5)));
    const innerRadius = radius - thickness;
    const len = Math.max(1, String(text || '').length);
    const halfDiagNominal = Math.hypot(len * 3, 4);
    const safeRadius = innerRadius * 0.88;
    const maxSafeSize = Math.max(1, Math.floor(safeRadius / halfDiagNominal));
    return maxSafeSize;
  }

  function getCircleCenterText(field, norm) {
    const mode = field?.circleCenterMode || (field?.circleShowPercent ? 'PERCENT' : 'NONE');
    if (mode === 'NONE') return '';
    if (mode === 'FRACTION') {
      const curVal = Number(field?.barValue) || 0;
      const maxVal = Number(field?.barMax) || 100;
      const isInt = Number.isInteger(curVal) && Number.isInteger(maxVal);
      const vStr = isInt ? String(Math.round(curVal)) : curVal.toFixed(1);
      const mStr = isInt ? String(Math.round(maxVal)) : maxVal.toFixed(1);
      return `${vStr}/${mStr}`;
    }
    if (mode === 'VALUE') {
      const curVal = Number(field?.barValue) || 0;
      return Number.isInteger(curVal) ? String(Math.round(curVal)) : curVal.toFixed(1);
    }
    return `${Math.round(norm * 100)}%`;
  }

  function ensureBarState(field) {
    if (!field || field.type !== 'BAR') return;
    if (typeof field.barPreset !== 'string') field.barPreset = 'LINEAR';
    if (field.barPreset === 'SOLID' || field.barPreset === 'GRADIENT') {
      if (field.barPreset === 'GRADIENT') field.barFillMode = 'GRADIENT';
      field.barPreset = 'LINEAR';
    }
    if (typeof field.barFillMode !== 'string') field.barFillMode = 'SOLID';
    if (typeof field.barOrientation !== 'string') field.barOrientation = 'HORIZONTAL';
    if (typeof field.showLabel !== 'boolean') field.showLabel = true;
    if (typeof field.alignV !== 'string') field.alignV = (field.layout === 'INLINE' ? 'MID' : 'TOP');
    if (typeof field.alignH !== 'string') field.alignH = field.align || 'LEFT';
    if (typeof field.barLabel !== 'string') field.barLabel = field.label || 'Nivel';
    if (typeof field.barUnit !== 'string') field.barUnit = field.unit || '%';
    if (!Number.isFinite(Number(field.barMin))) field.barMin = 0;
    if (!Number.isFinite(Number(field.barMax))) field.barMax = 100;
    if (!Number.isFinite(Number(field.barValue))) field.barValue = 50;
    if (typeof field.barAutoWidth !== 'boolean') field.barAutoWidth = false; // default fixed for direct resize
    if (!Number.isFinite(Number(field.barWidth))) field.barWidth = 110;
    if (!Number.isFinite(Number(field.barHeight))) field.barHeight = 12;

    // Colores de degradado y pista vacía (Invertido por defecto: Rojo -> Verde)
    if (!Number.isFinite(Number(field.valueColorEnd))) field.valueColorEnd = 0x07E0;
    if (!Number.isFinite(Number(field.trackColor))) field.trackColor = 0x18C3;

    // Propiedades de Medidor Circular / Carga
    if (!Number.isFinite(Number(field.circleRadius))) field.circleRadius = 26;
    if (!Number.isFinite(Number(field.circleThickness))) field.circleThickness = 5;
    if (!Number.isFinite(Number(field.circleSweep))) field.circleSweep = 360;
    if (typeof field.circleDirection !== 'string') field.circleDirection = 'CW';
    if (typeof field.circleStartAngle !== 'string') field.circleStartAngle = 'TOP';
    if (typeof field.circleStyle !== 'string') field.circleStyle = 'CONTINUOUS';
    if (typeof field.circleCenterMode !== 'string') field.circleCenterMode = (field.circleShowPercent === false ? 'NONE' : 'PERCENT');
    if (typeof field.circleColorMode !== 'string') field.circleColorMode = 'SOLID';
    if (typeof field.circleShowPercent !== 'boolean') field.circleShowPercent = (field.circleCenterMode !== 'NONE');
    if (!Number.isFinite(Number(field.circleTextSize))) field.circleTextSize = 1;
    if (!Number.isFinite(Number(field.circleTextColor))) field.circleTextColor = 0xFFFF;
    if (!Number.isFinite(Number(field.circleTrackColor))) field.circleTrackColor = 0x2104;

    // Propiedades de Barra Lineal
    if (typeof field.linearPctPos !== 'string') field.linearPctPos = 'NONE';
    if (!Number.isFinite(Number(field.linearPctColor))) field.linearPctColor = 0xFFFF;

    // Asegurar compatibilidad
    field.label = '';
    field.unit = '';
    field.preview = '';
    field.capacity = 1;
    field.valueSize = 1;
  }

  function normalizedFor(field) {
    const min = Number(field.barMin);
    let max = Number(field.barMax);
    const value = Number(field.barValue);
    if (max <= min) max = min + 1;
    const raw = (value - min) / (max - min);
    return Math.max(0, Math.min(1, Number.isFinite(raw) ? raw : 0));
  }

  function barGeometry(field) {
    ensureBarState(field);
    const pad = Math.max(FIELD_PADDING, Math.max(1, Math.trunc(Number(field.labelSize) || 1)));
    const hasLabel = Boolean(field.showLabel && field.barLabel && field.barLabel.trim().length > 0);
    const labelBounds = hasLabel ? nominalTextBounds(field.barLabel, field.labelSize) : { width: 0, height: 0 };
    const hasUnit = Boolean(field.barUnit && field.barUnit.trim().length > 0);
    const norm = normalizedFor(field);
    const isShowingPctRight = (field.linearPctPos === 'RIGHT');
    const pctString = `${Math.round(norm * 100)}%`;
    const rightText = isShowingPctRight ? pctString : (hasUnit ? field.barUnit : '');
    const hasRightText = Boolean(rightText && rightText.trim().length > 0);
    const unitBounds = hasRightText ? nominalTextBounds(rightText, field.labelSize) : { width: 0, height: 0 };
    const alignV = field.alignV || (field.layout === 'INLINE' ? 'MID' : 'TOP');
    const alignH = field.alignH || field.align || 'LEFT';

    // Caso 1: Medidor Circular de Carga
    if (field.barPreset === 'CIRCULAR') {
      const radius = Math.max(10, Math.trunc(Number(field.circleRadius) || 26));
      const thickness = Math.max(2, Math.min(radius - 2, Math.trunc(Number(field.circleThickness) || 5)));
      const diameter = radius * 2;
      const labelH = hasLabel ? labelBounds.height + FIELD_GAP : 0;
      let fieldW, fieldH, cx, cy, labelX = 0, labelY = 0;

      if (!hasLabel) {
        fieldW = 2 * pad + diameter;
        fieldH = 2 * pad + diameter;
        cx = field.x + pad + radius;
        cy = field.y + pad + radius;
      } else if (alignV === 'MID') {
        if (alignH === 'RIGHT') {
          fieldW = 2 * pad + diameter + FIELD_GAP + labelBounds.width;
          fieldH = 2 * pad + Math.max(diameter, labelBounds.height);
          cx = field.x + pad + radius;
          cy = field.y + pad + Math.max(radius, Math.trunc(labelBounds.height / 2));
          labelX = cx + radius + FIELD_GAP;
          labelY = cy - Math.trunc(labelBounds.height / 2);
        } else {
          fieldW = 2 * pad + labelBounds.width + FIELD_GAP + diameter;
          fieldH = 2 * pad + Math.max(diameter, labelBounds.height);
          labelX = field.x + pad;
          labelY = field.y + pad + Math.max(0, Math.trunc((diameter - labelBounds.height) / 2));
          cx = labelX + labelBounds.width + FIELD_GAP + radius;
          cy = field.y + pad + Math.max(radius, Math.trunc(labelBounds.height / 2));
        }
      } else if (alignV === 'BOTTOM') {
        fieldW = 2 * pad + Math.max(labelBounds.width, diameter);
        fieldH = 2 * pad + diameter + labelH;
        cx = field.x + pad + Math.max(radius, Math.trunc(labelBounds.width / 2));
        cy = field.y + pad + radius;
        labelY = cy + radius + FIELD_GAP;
        if (alignH === 'LEFT') labelX = field.x + pad;
        else if (alignH === 'RIGHT') labelX = field.x + fieldW - pad - labelBounds.width;
        else labelX = Math.round(cx - labelBounds.width / 2);
      } else {
        // TOP
        fieldW = 2 * pad + Math.max(labelBounds.width, diameter);
        fieldH = 2 * pad + labelH + diameter;
        cx = field.x + pad + Math.max(radius, Math.trunc(labelBounds.width / 2));
        cy = field.y + pad + labelH + radius;
        labelY = field.y + pad;
        if (alignH === 'LEFT') labelX = field.x + pad;
        else if (alignH === 'RIGHT') labelX = field.x + fieldW - pad - labelBounds.width;
        else labelX = Math.round(cx - labelBounds.width / 2);
      }

      return {
        pad,
        fieldX: field.x,
        fieldY: field.y,
        fieldW,
        fieldH,
        valueX: cx - radius,
        valueY: cy - radius,
        valueW: diameter,
        valueH: diameter,
        cx,
        cy,
        radius,
        thickness,
        hasLabel,
        labelX,
        labelY,
        labelBounds,
        unitBounds: { width: 0, height: 0 }
      };
    }

    // Caso 2: Barra Vertical
    if (field.barOrientation === 'VERTICAL') {
      const valueW = Math.max(6, Math.trunc(Number(field.barWidth) || 14));
      const valueH = Math.max(10, Math.trunc(Number(field.barHeight) || 60));
      const labelH = hasLabel ? labelBounds.height + FIELD_GAP : 0;
      const showUnitBottom = (hasRightText && unitBounds.height > 0);
      const unitH = showUnitBottom ? unitBounds.height + FIELD_GAP : 0;
      const contentW = Math.max(labelBounds.width, valueW, showUnitBottom ? unitBounds.width : 0);
      let fieldW, fieldH, valueX, valueY, labelX = 0, labelY = 0;

      if (!hasLabel) {
        fieldW = 2 * pad + Math.max(valueW, showUnitBottom ? unitBounds.width : 0);
        fieldH = 2 * pad + valueH + unitH;
        valueX = field.x + pad;
        valueY = field.y + pad;
      } else if (alignV === 'MID') {
        const maxH = Math.max(labelBounds.height, valueH, unitH);
        fieldH = 2 * pad + maxH;
        const valY = field.y + pad + Math.round((maxH - valueH) / 2);
        const lblY = field.y + pad + Math.round((maxH - labelBounds.height) / 2);
        if (alignH === 'RIGHT') {
          fieldW = 2 * pad + valueW + FIELD_GAP + labelBounds.width;
          valueX = field.x + pad;
          valueY = valY;
          labelX = valueX + valueW + FIELD_GAP;
          labelY = lblY;
        } else {
          fieldW = 2 * pad + labelBounds.width + FIELD_GAP + valueW;
          labelX = field.x + pad;
          labelY = lblY;
          valueX = labelX + labelBounds.width + FIELD_GAP;
          valueY = valY;
        }
      } else if (alignV === 'BOTTOM') {
        fieldW = 2 * pad + contentW;
        fieldH = 2 * pad + valueH + FIELD_GAP + labelBounds.height + unitH;
        valueX = field.x + pad + Math.trunc((contentW - valueW) / 2);
        valueY = field.y + pad;
        labelY = valueY + valueH + FIELD_GAP;
        if (alignH === 'LEFT') labelX = field.x + pad;
        else if (alignH === 'RIGHT') labelX = field.x + fieldW - pad - labelBounds.width;
        else labelX = field.x + pad + Math.trunc((contentW - labelBounds.width) / 2);
      } else {
        // TOP
        fieldW = 2 * pad + contentW;
        fieldH = 2 * pad + labelH + valueH + unitH;
        valueX = field.x + pad + Math.trunc((contentW - valueW) / 2);
        valueY = field.y + pad + labelH;
        labelY = field.y + pad;
        if (alignH === 'LEFT') labelX = field.x + pad;
        else if (alignH === 'RIGHT') labelX = field.x + fieldW - pad - labelBounds.width;
        else labelX = field.x + pad + Math.trunc((contentW - labelBounds.width) / 2);
      }

      return {
        pad,
        fieldX: field.x,
        fieldY: field.y,
        fieldW,
        fieldH,
        valueX,
        valueY,
        valueW,
        valueH,
        hasLabel,
        labelX,
        labelY,
        labelBounds,
        unitBounds
      };
    }

    // Caso 3: Barra Horizontal Clásica / Degradada
    const valueH = Math.max(4, Math.trunc(Number(field.barHeight) || DEFAULT_BAR_HEIGHT));
    let valueW = DEFAULT_BAR_WIDTH;
    let fieldW, fieldH, valueX, valueY, labelX = 0, labelY = 0;
    const labelH = hasLabel ? labelBounds.height + FIELD_GAP : 0;
    const showUnitRight = (hasRightText && unitBounds.width > 0);
    const unitW = showUnitRight ? unitBounds.width + FIELD_GAP : 0;

    if (!hasLabel) {
      fieldW = Math.max(20, Math.min(WIDTH, Math.trunc(Number(field.barWidth) || 110)));
      valueW = Math.max(4, fieldW - 2 * pad - unitW);
      fieldH = 2 * pad + Math.max(valueH, unitBounds.height);
      valueX = field.x + pad;
      valueY = field.y + pad;
    } else if (alignV === 'MID') {
      const maxH = Math.max(labelBounds.height, valueH, unitBounds.height);
      const valY = field.y + pad + Math.round((maxH - valueH) / 2);
      const lblY = field.y + pad + Math.round((maxH - labelBounds.height) / 2);
      if (alignH === 'RIGHT') {
        fieldW = Math.max(20, Math.min(WIDTH, Math.trunc(Number(field.barWidth) || 110)));
        valueW = Math.max(4, fieldW - 2 * pad - labelBounds.width - FIELD_GAP - unitW);
        fieldH = 2 * pad + maxH;
        valueX = field.x + pad;
        valueY = valY;
        labelX = valueX + valueW + FIELD_GAP;
        labelY = lblY;
      } else {
        fieldW = Math.max(20, Math.min(WIDTH, Math.trunc(Number(field.barWidth) || 110)));
        valueW = Math.max(4, fieldW - 2 * pad - labelBounds.width - FIELD_GAP - unitW);
        fieldH = 2 * pad + maxH;
        labelX = field.x + pad;
        labelY = lblY;
        valueX = labelX + labelBounds.width + FIELD_GAP;
        valueY = valY;
      }
    } else if (alignV === 'BOTTOM') {
      fieldW = Math.max(20, Math.min(WIDTH, Math.trunc(Number(field.barWidth) || 110)));
      valueW = Math.max(4, fieldW - 2 * pad - unitW);
      fieldH = 2 * pad + Math.max(valueH, unitBounds.height) + FIELD_GAP + labelBounds.height;
      valueX = field.x + pad;
      valueY = field.y + pad;
      labelY = valueY + Math.max(valueH, unitBounds.height) + FIELD_GAP;
      if (alignH === 'LEFT') labelX = field.x + pad;
      else if (alignH === 'RIGHT') labelX = field.x + fieldW - pad - labelBounds.width;
      else labelX = field.x + pad + Math.max(0, Math.trunc((valueW - labelBounds.width) / 2));
    } else {
      // TOP (Stacked)
      fieldW = Math.max(20, Math.min(WIDTH, Math.trunc(Number(field.barWidth) || 110)));
      valueW = Math.max(4, fieldW - 2 * pad - unitW);
      fieldH = 2 * pad + labelH + Math.max(valueH, unitBounds.height);
      labelY = field.y + pad;
      valueX = field.x + pad;
      valueY = field.y + pad + labelH;
      if (alignH === 'LEFT') labelX = field.x + pad;
      else if (alignH === 'RIGHT') labelX = field.x + fieldW - pad - labelBounds.width;
      else labelX = field.x + pad + Math.max(0, Math.trunc((valueW - labelBounds.width) / 2));
    }

    // Cálculo de posición de unidad (%) perfectamente centrada con la barra
    let unitX = valueX + valueW + (unitW > 0 ? FIELD_GAP : 0);
    if (alignV === 'MID' && alignH === 'RIGHT') {
      unitX = valueX + valueW + (unitW > 0 ? FIELD_GAP : 0);
      labelX = unitX + (unitW > 0 ? unitBounds.width + FIELD_GAP : 0);
    }
    const unitY = valueY + Math.round((valueH - (unitBounds.height || 7)) / 2);

    return {
      pad,
      fieldX: field.x,
      fieldY: field.y,
      fieldW,
      fieldH,
      valueX,
      valueY,
      valueW,
      valueH,
      hasLabel,
      labelX,
      labelY,
      unitX,
      unitY,
      labelBounds,
      unitBounds
    };
  }

  function drawClassicText(ctx, text, x, y, size, color, scale) {
    const font = window.JWPLCGfxClassicFont;
    if (!font || !text) return;
    const textScale = Math.max(1, Math.trunc(Number(size) || 1));
    ctx.fillStyle = rgb565ToCss(color);
    let cursorX = x;
    for (const character of String(text)) {
      const glyph = font.glyphFor(character.codePointAt(0));
      for (let column = 0; column < font.cellWidth; column += 1) {
        const bits = column < font.bytesPerGlyph ? glyph[column] : 0;
        for (let row = 0; row < font.cellHeight; row += 1) {
          if (column < font.bytesPerGlyph && ((bits >> row) & 0x01) !== 0) {
            ctx.fillRect(
              (cursorX + column * textScale) * scale,
              (y + row * textScale) * scale,
              textScale * scale,
              textScale * scale);
          }
        }
      }
      cursorX += font.cellWidth * textScale;
    }
  }

  function drawBarField(ctx, field, scale) {
    const g = barGeometry(field);
    const norm = normalizedFor(field);

    // Fondo del componente (sólo si no es transparente)
    const isTrans = field.transparentBackground || field.backgroundColor === 'TRANSPARENT' || field.backgroundColor === -1;
    if (!isTrans) {
      ctx.fillStyle = rgb565ToCss(field.backgroundColor);
      ctx.fillRect(g.fieldX * scale, g.fieldY * scale, g.fieldW * scale, g.fieldH * scale);
    }

    // Marco exterior si está activado (se ajusta al contorno exacto)
    if (field.frame && g.fieldW > 1 && g.fieldH > 1) {
      ctx.fillStyle = rgb565ToCss(field.frameColor);
      ctx.fillRect(g.fieldX * scale, g.fieldY * scale, g.fieldW * scale, scale);
      ctx.fillRect(g.fieldX * scale, (g.fieldY + g.fieldH - 1) * scale, g.fieldW * scale, scale);
      ctx.fillRect(g.fieldX * scale, g.fieldY * scale, scale, g.fieldH * scale);
      ctx.fillRect((g.fieldX + g.fieldW - 1) * scale, g.fieldY * scale, scale, g.fieldH * scale);
    }

    // Dibujo de Etiqueta visible (sólo si está habilitada)
    if (g.hasLabel && field.barLabel && g.labelBounds.width > 0) {
      drawClassicText(ctx, field.barLabel, g.labelX, g.labelY, field.labelSize, field.labelColor, scale);
    }

    // --- RENDERIZADO SEGÚN PRESET ---
    if (field.barPreset === 'CIRCULAR') {
      const sweepDeg = Number(field.circleSweep) || 360;
      const isCCW = (field.circleDirection === 'CCW');
      const startQuadrant = field.circleStartAngle || 'TOP';
      const circleStyle = field.circleStyle || 'CONTINUOUS';

      // Ángulo según cuadrante inicial
      let quadrantAngle = -Math.PI / 2; // TOP
      if (startQuadrant === 'RIGHT') quadrantAngle = 0;
      else if (startQuadrant === 'BOTTOM') quadrantAngle = Math.PI / 2;
      else if (startQuadrant === 'LEFT') quadrantAngle = Math.PI;

      const totalSweepRad = (sweepDeg * Math.PI) / 180;
      let startRad = quadrantAngle;
      if (sweepDeg < 360) {
        startRad = quadrantAngle - totalSweepRad / 2;
      }

      const rMid = g.radius - g.thickness / 2;
      const trackCss = rgb565ToCss(field.circleTrackColor || 0x2104);
      const activeColor565 = field.valueColor || 0x07FF;
      const endColor565 = field.valueColorEnd || 0x07E0;
      const isGrad = (field.circleColorMode === 'GRADIENT');

      ctx.save();

      if (circleStyle === 'CONTINUOUS') {
        // --- 1. CONTINUO / SÓLIDO ---
        ctx.beginPath();
        ctx.arc(g.cx * scale, g.cy * scale, rMid * scale, startRad, startRad + totalSweepRad, false);
        ctx.strokeStyle = trackCss;
        ctx.lineWidth = g.thickness * scale;
        ctx.lineCap = sweepDeg === 360 ? 'butt' : 'round';
        ctx.stroke();

        const activeSweepRad = norm * totalSweepRad;
        if (activeSweepRad > 0.002) {
          ctx.beginPath();
          if (!isCCW) {
            ctx.arc(g.cx * scale, g.cy * scale, rMid * scale, startRad, startRad + activeSweepRad, false);
          } else {
            if (sweepDeg === 360) {
              ctx.arc(g.cx * scale, g.cy * scale, rMid * scale, startRad, startRad - activeSweepRad, true);
            } else {
              const endRad = startRad + totalSweepRad;
              ctx.arc(g.cx * scale, g.cy * scale, rMid * scale, endRad, endRad - activeSweepRad, true);
            }
          }

          if (isGrad) {
            if (ctx.createConicGradient) {
              const conicStart = !isCCW ? startRad : (sweepDeg === 360 ? startRad - totalSweepRad : startRad);
              const conic = ctx.createConicGradient(conicStart, g.cx * scale, g.cy * scale);
              conic.addColorStop(0, rgb565ToCss(activeColor565));
              conic.addColorStop(Math.min(1, Math.max(0.01, norm)), rgb565ToCss(endColor565));
              ctx.strokeStyle = conic;
            } else {
              const grad = ctx.createLinearGradient((g.cx - g.radius) * scale, (g.cy - g.radius) * scale, (g.cx + g.radius) * scale, (g.cy + g.radius) * scale);
              grad.addColorStop(0, rgb565ToCss(activeColor565));
              grad.addColorStop(1, rgb565ToCss(endColor565));
              ctx.strokeStyle = grad;
            }
          } else {
            ctx.strokeStyle = rgb565ToCss(activeColor565);
          }
          ctx.lineWidth = g.thickness * scale;
          ctx.lineCap = sweepDeg === 360 ? 'butt' : 'round';
          ctx.stroke();
        }
      } else if (circleStyle === 'SEGMENTED') {
        // --- 2. SEGMENTOS / BLOQUES ---
        const segCount = sweepDeg === 360 ? 16 : (sweepDeg === 270 ? 12 : (sweepDeg === 240 ? 10 : 8));
        const segStep = totalSweepRad / segCount;
        const gap = segStep * 0.18;
        const blockSweep = segStep - gap;

        for (let i = 0; i < segCount; i += 1) {
          const segProp = (i + 1) / segCount;
          const isActive = (i / segCount < norm);

          let bStart;
          if (!isCCW) {
            bStart = startRad + i * segStep + gap / 2;
          } else {
            if (sweepDeg === 360) {
              bStart = startRad - (i + 1) * segStep + gap / 2;
            } else {
              const endRad = startRad + totalSweepRad;
              bStart = endRad - (i + 1) * segStep + gap / 2;
            }
          }

          ctx.beginPath();
          ctx.arc(g.cx * scale, g.cy * scale, rMid * scale, bStart, bStart + blockSweep, false);
          if (isActive) {
            const segColor = isGrad ? interpolateColor565(activeColor565, endColor565, segProp) : activeColor565;
            ctx.strokeStyle = rgb565ToCss(segColor);
          } else {
            ctx.strokeStyle = trackCss;
          }
          ctx.lineWidth = g.thickness * scale;
          ctx.lineCap = 'butt';
          ctx.stroke();
        }
      } else if (circleStyle === 'DOTS') {
        // --- 3. PUNTOS / ESFERAS ---
        const dotCount = sweepDeg === 360 ? 16 : 13;
        const dotR = Math.max(1.5, Math.min(g.thickness / 2, 4.5));
        const divisor = sweepDeg === 360 ? dotCount : (dotCount - 1);

        for (let i = 0; i < dotCount; i += 1) {
          const dotProp = i / divisor;
          const isActive = (dotProp <= norm + 0.001);

          let angle;
          if (!isCCW) {
            angle = startRad + i * (totalSweepRad / divisor);
          } else {
            if (sweepDeg === 360) {
              angle = startRad - i * (totalSweepRad / divisor);
            } else {
              const endRad = startRad + totalSweepRad;
              angle = endRad - i * (totalSweepRad / divisor);
            }
          }

          const dx = g.cx + rMid * Math.cos(angle);
          const dy = g.cy + rMid * Math.sin(angle);

          ctx.beginPath();
          ctx.arc(dx * scale, dy * scale, dotR * scale, 0, 2 * Math.PI);
          if (isActive) {
            const dotColor = isGrad ? interpolateColor565(activeColor565, endColor565, dotProp) : activeColor565;
            ctx.fillStyle = rgb565ToCss(dotColor);
          } else {
            ctx.fillStyle = trackCss;
          }
          ctx.fill();
        }
      } else if (circleStyle === 'TICKS') {
        // --- 4. RAYAS / MARCAS RADIALES ---
        const tickCount = sweepDeg === 360 ? 24 : 19;
        const tickLen = Math.max(3, g.thickness);
        const rInner = rMid - tickLen / 2;
        const rOuter = rMid + tickLen / 2;
        const divisor = sweepDeg === 360 ? tickCount : (tickCount - 1);

        for (let i = 0; i < tickCount; i += 1) {
          const tickProp = i / divisor;
          const isActive = (tickProp <= norm + 0.001);

          let angle;
          if (!isCCW) {
            angle = startRad + i * (totalSweepRad / divisor);
          } else {
            if (sweepDeg === 360) {
              angle = startRad - i * (totalSweepRad / divisor);
            } else {
              const endRad = startRad + totalSweepRad;
              angle = endRad - i * (totalSweepRad / divisor);
            }
          }

          const cosA = Math.cos(angle);
          const sinA = Math.sin(angle);
          const x1 = g.cx + rInner * cosA;
          const y1 = g.cy + rInner * sinA;
          const x2 = g.cx + rOuter * cosA;
          const y2 = g.cy + rOuter * sinA;

          ctx.beginPath();
          ctx.moveTo(x1 * scale, y1 * scale);
          ctx.lineTo(x2 * scale, y2 * scale);
          if (isActive) {
            const tickColor = isGrad ? interpolateColor565(activeColor565, endColor565, tickProp) : activeColor565;
            ctx.strokeStyle = rgb565ToCss(tickColor);
          } else {
            ctx.strokeStyle = trackCss;
          }
          ctx.lineWidth = Math.max(1, Math.round(1.5 * scale));
          ctx.stroke();
        }
      }

      ctx.restore();

      // 5. Texto al centro del círculo con límite seguro automático
      const centerText = getCircleCenterText(field, norm);
      if (centerText && field.circleCenterMode !== 'NONE') {
        const safeLimit = computeSafeCenterTextSize(field, centerText);
        const reqSize = Math.max(1, Math.trunc(Number(field.circleTextSize) || 1));
        const effectiveSize = Math.min(reqSize, safeLimit);
        const tb = nominalTextBounds(centerText, effectiveSize);
        const tx = Math.round(g.cx - tb.width / 2);
        const ty = Math.round(g.cy - tb.height / 2);
        drawClassicText(ctx, centerText, tx, ty, effectiveSize, field.circleTextColor || 0xFFFF, scale);
      }
    } else {
      // BARRAS LINEALES (Sólida o Degradada)

      // Pista vacía de fondo de la barra
      ctx.fillStyle = rgb565ToCss(field.trackColor || 0x18C3);
      ctx.fillRect(g.valueX * scale, g.valueY * scale, g.valueW * scale, g.valueH * scale);

      // Relleno activo proporcional
      const isGrad = (field.barFillMode === 'GRADIENT' || field.barPreset === 'GRADIENT');
      const pctPos = field.linearPctPos || 'RIGHT';

      if (field.barOrientation === 'VERTICAL') {
        const fillH = Math.round(norm * g.valueH);
        if (fillH > 0) {
          if (isGrad) {
            const grad = ctx.createLinearGradient(0, (g.valueY + g.valueH) * scale, 0, g.valueY * scale);
            grad.addColorStop(0, rgb565ToCss(field.valueColor || 0xF800));
            grad.addColorStop(1, rgb565ToCss(field.valueColorEnd || 0x07E0));
            ctx.fillStyle = grad;
          } else {
            ctx.fillStyle = rgb565ToCss(field.valueColor || 0x07FF);
          }
          ctx.fillRect(g.valueX * scale, (g.valueY + g.valueH - fillH) * scale, g.valueW * scale, fillH * scale);
        }

        // Color de porcentaje configurable (por defecto 0xFFFF / blanco)
        const pctColor = Number.isFinite(Number(field.linearPctColor)) ? field.linearPctColor : 0xFFFF;
        const pctStr = `${Math.round(norm * 100)}%`;

        // Porcentaje al interior o unidad al pie
        if (pctPos === 'INSIDE') {
          const tb = nominalTextBounds(pctStr, 1);
          if (tb.height <= g.valueH && tb.width <= g.valueW + 2) {
            const tx = g.valueX + Math.round((g.valueW - tb.width) / 2);
            const ty = g.valueY + Math.round((g.valueH - tb.height) / 2);
            drawClassicText(ctx, pctStr, tx, ty, 1, pctColor, scale);
          }
          if (field.barUnit) {
            const ux = field.x + g.pad + Math.trunc((g.fieldW - 2 * g.pad - g.unitBounds.width) / 2);
            const uy = g.valueY + g.valueH + FIELD_GAP;
            drawClassicText(ctx, field.barUnit, ux, uy, field.labelSize, field.labelColor, scale);
          }
        } else if (pctPos === 'RIGHT') {
          // Mostrar porcentaje con su color propio
          const ux = field.x + g.pad + Math.trunc((g.fieldW - 2 * g.pad - g.unitBounds.width) / 2);
          const uy = g.valueY + g.valueH + FIELD_GAP;
          drawClassicText(ctx, pctStr, ux, uy, field.labelSize, pctColor, scale);
        } else if (field.barUnit) {
          // Si pctPos === 'NONE', muestra lo que hay en Contenido -> Unidad
          const ux = field.x + g.pad + Math.trunc((g.fieldW - 2 * g.pad - g.unitBounds.width) / 2);
          const uy = g.valueY + g.valueH + FIELD_GAP;
          drawClassicText(ctx, field.barUnit, ux, uy, field.labelSize, field.labelColor, scale);
        }
      } else {
        // Horizontal
        const fillW = Math.round(norm * g.valueW);
        if (fillW > 0) {
          if (isGrad) {
            const grad = ctx.createLinearGradient(g.valueX * scale, 0, (g.valueX + g.valueW) * scale, 0);
            grad.addColorStop(0, rgb565ToCss(field.valueColor || 0xF800));
            grad.addColorStop(1, rgb565ToCss(field.valueColorEnd || 0x07E0));
            ctx.fillStyle = grad;
          } else {
            ctx.fillStyle = rgb565ToCss(field.valueColor || 0x07FF);
          }
          ctx.fillRect(g.valueX * scale, g.valueY * scale, fillW * scale, g.valueH * scale);
        }

        // Color de porcentaje configurable (por defecto 0xFFFF / blanco)
        const pctColor = Number.isFinite(Number(field.linearPctColor)) ? field.linearPctColor : 0xFFFF;
        const pctStr = `${Math.round(norm * 100)}%`;

        // Porcentaje al interior o al lado derecho
        if (pctPos === 'INSIDE') {
          const tb = nominalTextBounds(pctStr, 1);
          if (tb.width <= g.valueW && tb.height <= g.valueH + 2) {
            const tx = g.valueX + Math.round((g.valueW - tb.width) / 2);
            const ty = g.valueY + Math.round((g.valueH - tb.height) / 2);
            drawClassicText(ctx, pctStr, tx, ty, 1, pctColor, scale);
          }
          if (field.barUnit) {
            const ux = (g.unitX !== undefined) ? g.unitX : (g.valueX + g.valueW + FIELD_GAP);
            const uy = (g.unitY !== undefined) ? g.unitY : (g.valueY + Math.round((g.valueH - (g.unitBounds?.height || 7)) / 2));
            drawClassicText(ctx, field.barUnit, ux, uy, field.labelSize, field.labelColor, scale);
          }
        } else if (pctPos === 'RIGHT') {
          // Mostrar porcentaje con su color propio a la derecha
          const ux = (g.unitX !== undefined) ? g.unitX : (g.valueX + g.valueW + FIELD_GAP);
          const uy = (g.unitY !== undefined) ? g.unitY : (g.valueY + Math.round((g.valueH - (g.unitBounds?.height || 7)) / 2));
          drawClassicText(ctx, pctStr, ux, uy, field.labelSize, pctColor, scale);
        } else if (field.barUnit) {
          // Si pctPos === 'NONE', muestra lo que hay en Contenido -> Unidad
          const ux = (g.unitX !== undefined) ? g.unitX : (g.valueX + g.valueW + FIELD_GAP);
          const uy = (g.unitY !== undefined) ? g.unitY : (g.valueY + Math.round((g.valueH - (g.unitBounds?.height || 7)) / 2));
          drawClassicText(ctx, field.barUnit, ux, uy, field.labelSize, field.labelColor, scale);
        }
      }
    }

    return g;
  }

  function drawBars(displayCtx, zoom, previewCtx) {
    if (editor()?.integratedBarLayers) return;
    const bars = visibleBars();
    if (!bars.length) return;
    const dCtx = displayCtx || displayCanvas.getContext('2d', { alpha: false });
    const pCtx = previewCtx || previewCanvas.getContext('2d', { alpha: false });
    const z = zoom || Math.max(1, Number(zoomSelect?.value) || 3);
    bars.forEach((field) => {
      if (pCtx) drawBarField(pCtx, field, 1);
      if (dCtx) drawBarField(dCtx, field, z);
    });
  }

  function patchCanvases() {
    if (editor()?.integratedBarLayers) return;
    const bars = visibleBars();
    if (!bars.length) return;
    const displayCtx = displayCanvas.getContext('2d', { alpha: false });
    const previewCtx = previewCanvas.getContext('2d', { alpha: false });
    const zoom = Math.max(1, Number(zoomSelect?.value) || 3);
    bars.forEach((field) => {
      drawBarField(previewCtx, field, 1);
      drawBarField(displayCtx, field, zoom);
    });
    // Si hay un objeto seleccionado, re-dibujar la selección y los 8 controladores
    // para que jamás queden cubiertos por fondos opacos o negros de la barra
    if (editor()?.hasFieldSelection?.()) {
      editor()?.drawSelectionAndGuides?.();
    }
  }

  function triggerCoreRender(commit = false) {
    const field = selectedField();
    if (!isBar(field)) return;
    fieldX.value = String(field.x);
    fieldY.value = String(field.y);
    fieldX.dispatchEvent(new Event(commit ? 'change' : 'input', { bubbles: true }));
  }

  function updateBarFromControls(commit = false) {
    const field = selectedField();
    if (!isBar(field)) return;

    field.barMin = Number(fieldBarMin.value);
    field.barMax = Number(fieldBarMax.value);
    field.barValue = Number(fieldBarValue.value);
    fieldBarValueSlider.value = String(field.barValue);
    field.showLabel = fieldBarShowLabel.checked;
    field.barOrientation = fieldBarOrientation.value;
    field.barAutoWidth = fieldBarWidthMode.value === 'AUTO';
    const valLength = Math.max(10, Math.min(WIDTH, Number(fieldBarWidth.value) || 110));
    const valThick = Math.max(4, Math.min(HEIGHT, Number(fieldBarHeight.value) || 12));
    if (field.barOrientation === 'VERTICAL') {
      field.barHeight = valLength;
      field.barWidth = valThick;
    } else {
      field.barWidth = valLength;
      field.barHeight = valThick;
    }

    // Colores
    field.trackColor = hexCssTo565(fieldBarTrackColorNative.value);
    fieldBarTrackColorHex.textContent = hex565(field.trackColor);

    if (fieldBarLinearPctPos) {
      field.linearPctPos = fieldBarLinearPctPos.value || 'NONE';
    }

    if (field.barPreset === 'CIRCULAR') {
      field.circleRadius = Math.max(10, Math.min(80, Number(fieldBarCircleRadius.value) || 26));
      field.circleThickness = Math.max(2, Math.min(25, Number(fieldBarCircleThickness.value) || 5));
      field.circleColorMode = fieldBarCircleColorMode.value || 'SOLID';
      if (field.circleColorMode === 'GRADIENT') {
        field.valueColor = hexCssTo565(fieldBarCircleGradStartNative.value);
        field.valueColorEnd = hexCssTo565(fieldBarCircleGradEndNative.value);
        fieldBarCircleGradStartHex.textContent = hex565(field.valueColor);
        fieldBarCircleGradEndHex.textContent = hex565(field.valueColorEnd);
      } else {
        field.valueColor = hexCssTo565(fieldBarCircleColorNative.value);
        fieldBarCircleColorHex.textContent = hex565(field.valueColor);
      }
      field.circleTrackColor = hexCssTo565(fieldBarCircleTrackNative.value);
      field.circleCenterMode = fieldBarCircleCenterMode ? fieldBarCircleCenterMode.value : 'PERCENT';
      field.circleShowPercent = (field.circleCenterMode !== 'NONE');
      field.circleTextSize = Number(fieldBarCircleTextSize ? fieldBarCircleTextSize.value : 1) || 1;
      field.circleTextColor = hexCssTo565(fieldBarCircleTextColNative.value);

      fieldBarCircleTrackHex.textContent = hex565(field.circleTrackColor);
      fieldBarCircleTextColHex.textContent = hex565(field.circleTextColor);
    } else {
      field.barFillMode = fieldBarFillMode?.value || 'SOLID';
      if (field.barFillMode === 'GRADIENT') {
        field.valueColor = hexCssTo565(fieldBarGradStartNative.value);
        field.valueColorEnd = hexCssTo565(fieldBarGradEndNative.value);
        fieldBarGradStartHex.textContent = hex565(field.valueColor);
        fieldBarGradEndHex.textContent = hex565(field.valueColorEnd);
      } else {
        if (fieldBarSolidColorNative) {
          field.valueColor = hexCssTo565(fieldBarSolidColorNative.value);
          if (fieldBarSolidColorHex) fieldBarSolidColorHex.textContent = hex565(field.valueColor);
        }
      }

      if (fieldBarLinearPctColNative) {
        field.linearPctColor = hexCssTo565(fieldBarLinearPctColNative.value);
        if (fieldBarLinearPctColHex) fieldBarLinearPctColHex.textContent = hex565(field.linearPctColor);
      }
    }

    // Actualizar indicador de porcentaje/carga en el header del slider
    const norm = normalizedFor(field);
    if (barPercentStatus) barPercentStatus.textContent = `${(norm * 100).toFixed(1)} %`;

    triggerCoreRender(commit);
  }

  // Sincronización slider de valor de prueba al tope del inspector
  fieldBarValueSlider.addEventListener('input', () => {
    fieldBarValue.value = fieldBarValueSlider.value;
    updateBarFromControls(false);
  });
  fieldBarValueSlider.addEventListener('change', () => {
    fieldBarValue.value = fieldBarValueSlider.value;
    updateBarFromControls(true);
  });

  // Al editar ancho directamente, desactiva AUTO y pone FIJO para que no se bloquee
  fieldBarWidth.addEventListener('input', () => {
    const field = selectedField();
    if (isBar(field)) {
      field.barAutoWidth = false;
      fieldBarWidthMode.value = 'FIXED';
    }
    updateBarFromControls(false);
  });
  fieldBarWidth.addEventListener('change', () => {
    const field = selectedField();
    if (isBar(field)) {
      field.barAutoWidth = false;
      fieldBarWidthMode.value = 'FIXED';
    }
    updateBarFromControls(true);
  });

  [
    fieldBarMin, fieldBarMax, fieldBarValue, fieldBarHeight,
    fieldBarCircleRadius, fieldBarCircleThickness
  ].forEach((input) => {
    input.addEventListener('input', () => updateBarFromControls(false));
    input.addEventListener('change', () => updateBarFromControls(true));
  });

  [
    fieldBarShowLabel, fieldBarWidthMode,
    fieldBarCircleCenterMode, fieldBarCircleTextSize
  ].filter(Boolean).forEach((input) => {
    input.addEventListener('change', () => updateBarFromControls(true));
  });

  fieldBarLinearPctPos?.addEventListener('change', () => {
    const field = selectedField();
    if (!isBar(field)) return;
    field.linearPctPos = fieldBarLinearPctPos.value || 'NONE';
    patchInspector(field);
    triggerCoreRender(true);
  });

  // Botones de Apertura del Arco
  document.querySelectorAll('.bar-arc-btn').forEach((btn) => {
    btn.addEventListener('click', () => {
      const field = selectedField();
      if (!isBar(field)) return;
      field.circleSweep = Number(btn.dataset.sweep) || 360;
      document.querySelectorAll('.bar-arc-btn').forEach((b) => b.classList.toggle('active', b === btn));
      patchInspector(field);
      triggerCoreRender(true);
    });
  });

  // Botones de Dirección de Llenado
  document.querySelectorAll('.bar-dir-btn').forEach((btn) => {
    btn.addEventListener('click', () => {
      const field = selectedField();
      if (!isBar(field)) return;
      field.circleDirection = btn.dataset.dir || 'CW';
      document.querySelectorAll('.bar-dir-btn').forEach((b) => b.classList.toggle('active', b === btn));
      patchInspector(field);
      triggerCoreRender(true);
    });
  });

  // Botones de Punto de Inicio del 0
  document.querySelectorAll('.bar-quadrant-btn').forEach((btn) => {
    btn.addEventListener('click', () => {
      const field = selectedField();
      if (!isBar(field)) return;
      field.circleStartAngle = btn.dataset.quadrant || 'TOP';
      document.querySelectorAll('.bar-quadrant-btn').forEach((b) => b.classList.toggle('active', b === btn));
      patchInspector(field);
      triggerCoreRender(true);
    });
  });

  // Botones de Estilo de Anillo
  document.querySelectorAll('.bar-style-btn').forEach((btn) => {
    btn.addEventListener('click', () => {
      const field = selectedField();
      if (!isBar(field)) return;
      field.circleStyle = btn.dataset.style || 'CONTINUOUS';
      document.querySelectorAll('.bar-style-btn').forEach((b) => b.classList.toggle('active', b === btn));
      patchInspector(field);
      triggerCoreRender(true);
    });
  });

  if (fieldBarLabelSize) {
    fieldBarLabelSize.addEventListener('change', () => {
      const field = selectedField();
      if (!isBar(field)) return;
      field.labelSize = Math.max(1, Math.trunc(Number(fieldBarLabelSize.value) || 1));
      if (fieldLabelSize) fieldLabelSize.value = String(field.labelSize);
      triggerCoreRender(true);
    });
  }

  // Cambio de orientación con intercambio inteligente de longitud y grosor
  fieldBarOrientation.addEventListener('change', () => {
    const field = selectedField();
    if (!isBar(field)) return;
    const oldOrientation = field.barOrientation || 'HORIZONTAL';
    const newOrientation = fieldBarOrientation.value;
    if (oldOrientation !== newOrientation) {
      field.barOrientation = newOrientation;
      const oldW = field.barWidth;
      const oldH = field.barHeight;
      field.barWidth = Math.max(6, Math.min(WIDTH, oldH));
      field.barHeight = Math.max(4, Math.min(HEIGHT, oldW));
    }
    patchInspector(field);
    triggerCoreRender(true);
  });

  // Botones para intercalar colores de degradado
  barLinearSwapGradBtn?.addEventListener('click', () => {
    const field = selectedField();
    if (!isBar(field)) return;
    const temp = field.valueColor;
    field.valueColor = field.valueColorEnd || 0x07E0;
    field.valueColorEnd = temp || 0xF800;
    fieldBarGradStartNative.value = rgb565ToHexCss(field.valueColor);
    fieldBarGradEndNative.value = rgb565ToHexCss(field.valueColorEnd);
    fieldBarGradStartHex.textContent = hex565(field.valueColor);
    fieldBarGradEndHex.textContent = hex565(field.valueColorEnd);
    triggerCoreRender(true);
  });

  barCircleSwapGradBtn?.addEventListener('click', () => {
    const field = selectedField();
    if (!isBar(field)) return;
    const temp = field.valueColor;
    field.valueColor = field.valueColorEnd || 0x07E0;
    field.valueColorEnd = temp || 0xF800;
    fieldBarCircleGradStartNative.value = rgb565ToHexCss(field.valueColor);
    fieldBarCircleGradEndNative.value = rgb565ToHexCss(field.valueColorEnd);
    fieldBarCircleGradStartHex.textContent = hex565(field.valueColor);
    fieldBarCircleGradEndHex.textContent = hex565(field.valueColorEnd);
    triggerCoreRender(true);
  });

  fieldBarFillMode?.addEventListener('change', () => {
    const field = selectedField();
    if (!isBar(field)) return;
    field.barFillMode = fieldBarFillMode.value;
    const isGrad = field.barFillMode === 'GRADIENT';
    if (fieldBarSolidColorWrap) fieldBarSolidColorWrap.style.display = isGrad ? 'none' : 'block';
    if (barGradientControls) barGradientControls.style.display = isGrad ? 'grid' : 'none';
    if (isGrad && !field.valueColorEnd) {
      field.valueColorEnd = 0x07E0;
    }
    updateBarFromControls(true);
  });

  fieldBarCircleColorMode?.addEventListener('change', () => {
    const field = selectedField();
    if (!isBar(field)) return;
    field.circleColorMode = fieldBarCircleColorMode.value;
    const isGrad = field.circleColorMode === 'GRADIENT';
    if (fieldBarCircleSolidWrap) fieldBarCircleSolidWrap.style.display = isGrad ? 'none' : 'block';
    if (fieldBarCircleGradWrap) fieldBarCircleGradWrap.style.display = isGrad ? 'grid' : 'none';
    if (isGrad && !field.valueColorEnd) {
      field.valueColorEnd = 0x07E0;
    }
    updateBarFromControls(true);
  });

  // Alineación de texto con matriz 3x3
  alignMatrixCells.forEach((cell) => {
    cell.addEventListener('click', (e) => {
      e.preventDefault();
      const field = selectedField();
      if (!isBar(field)) return;
      field.alignV = cell.dataset.alignV || 'TOP';
      field.alignH = cell.dataset.alignH || 'LEFT';
      alignMatrixCells.forEach((c) => {
        c.classList.toggle('active', c.dataset.alignV === field.alignV && c.dataset.alignH === field.alignH);
      });
      triggerCoreRender(true);
    });
  });

  // Color pickers nativos
  [
    [fieldBarSolidColorNative, 'valueColor', fieldBarSolidColorHex],
    [fieldBarTrackColorNative, 'trackColor', fieldBarTrackColorHex],
    [fieldBarGradStartNative, 'valueColor', fieldBarGradStartHex],
    [fieldBarGradEndNative, 'valueColorEnd', fieldBarGradEndHex],
    [fieldBarCircleColorNative, 'valueColor', fieldBarCircleColorHex],
    [fieldBarCircleGradStartNative, 'valueColor', fieldBarCircleGradStartHex],
    [fieldBarCircleGradEndNative, 'valueColorEnd', fieldBarCircleGradEndHex],
    [fieldBarCircleTrackNative, 'circleTrackColor', fieldBarCircleTrackHex],
    [fieldBarCircleTextColNative, 'circleTextColor', fieldBarCircleTextColHex],
    [fieldBarLinearPctColNative, 'linearPctColor', fieldBarLinearPctColHex]
  ].forEach(([input, prop, hexLabel]) => {
    if (!input) return;
    input.addEventListener('input', () => {
      const field = selectedField();
      if (!isBar(field)) return;
      const c = hexCssTo565(input.value);
      field[prop] = c;
      if (hexLabel) hexLabel.textContent = hex565(c);
      triggerCoreRender(false);
    });
    input.addEventListener('change', () => {
      const field = selectedField();
      if (!isBar(field)) return;
      const c = hexCssTo565(input.value);
      field[prop] = c;
      if (hexLabel) hexLabel.textContent = hex565(c);
      triggerCoreRender(true);
    });
  });

  // Paletas de degradado rápido
  document.querySelectorAll('.bar-palette-pill').forEach((btn) => {
    btn.addEventListener('click', () => {
      const field = selectedField();
      if (!isBar(field)) return;
      field.valueColor = Number(btn.dataset.gradStart);
      field.valueColorEnd = Number(btn.dataset.gradEnd);
      fieldBarGradStartNative.value = rgb565ToHexCss(field.valueColor);
      fieldBarGradEndNative.value = rgb565ToHexCss(field.valueColorEnd);
      fieldBarGradStartHex.textContent = hex565(field.valueColor);
      fieldBarGradEndHex.textContent = hex565(field.valueColorEnd);
      triggerCoreRender(true);
    });
  });

  // Selector de Presets de un clic
  function setPreset(preset, commit = true) {
    const field = selectedField();
    if (!isBar(field)) return;
    field.barPreset = (preset === 'CIRCULAR') ? 'CIRCULAR' : 'LINEAR';

    if (field.barPreset === 'CIRCULAR') {
      field.barLabel = field.barLabel || 'Carga';
      field.barUnit = '%';
      if (!field.circleRadius) field.circleRadius = 26;
      if (!field.circleThickness) field.circleThickness = 5;
      if (!field.circleSweep) field.circleSweep = 360;
      if (!field.circleDirection) field.circleDirection = 'CW';
      if (!field.circleStartAngle) field.circleStartAngle = 'TOP';
      if (!field.circleStyle) field.circleStyle = 'CONTINUOUS';
      if (!field.circleCenterMode) field.circleCenterMode = 'PERCENT';
      if (!field.circleColorMode) field.circleColorMode = 'SOLID';
      if (!field.valueColorEnd) field.valueColorEnd = 0x07E0;
    } else {
      if (!field.linearPctPos) field.linearPctPos = 'NONE';
      if (!field.barFillMode) field.barFillMode = 'SOLID';
      if (!field.valueColor) field.valueColor = 0x07FF;
      if (!field.valueColorEnd) field.valueColorEnd = 0x07E0;
    }

    patchInspector(field);
    triggerCoreRender(commit);
  }

  document.querySelectorAll('.bar-preset-btn').forEach((btn) => {
    btn.addEventListener('click', (e) => {
      e.preventDefault();
      const preset = btn.dataset.barPreset;
      if (preset) setPreset(preset, true);
    });
  });

  function bindBarTextControl(input, property) {
    if (!input) return;
    ['input', 'change'].forEach((eventName) => {
      input.addEventListener(eventName, (event) => {
        const field = selectedField();
        if (!isBar(field)) return;
        event.stopImmediatePropagation();
        field[property] = input.value;
        triggerCoreRender(eventName === 'change');
      }, true);
    });
  }

  bindBarTextControl(fieldLabel, 'barLabel');
  bindBarTextControl(fieldUnit, 'barUnit');

  function createBarField() {
    const api = editor();
    api?.addTextField?.();
    const field = selectedField();
    if (!field) return;

    const serial = serialFor(field);
    field.type = 'BAR';
    field.name = `BAR ${serial}`;
    field.id = `FIELD_BAR_${serial}`;
    field.variable = `nivel${serial}`;
    field.barPreset = 'LINEAR';
    field.barFillMode = 'SOLID';
    field.barOrientation = 'HORIZONTAL';
    field.linearPctPos = 'NONE';
    field.showLabel = true;
    field.barLabel = 'Nivel';
    field.barUnit = '%';
    field.barMin = 0;
    field.barMax = 100;
    field.barValue = 50;
    field.barAutoWidth = false;
    field.barWidth = 110;
    field.barHeight = 12;
    field.labelSize = 1;
    field.frame = false;
    field.layout = 'STACKED';
    field.alignV = 'TOP';
    field.alignH = 'LEFT';
    field.labelColor = 0xFFFF;
    field.valueColor = 0x07FF;
    field.valueColorEnd = 0x07E0;
    field.trackColor = 0x18C3;
    field.backgroundColor = 'TRANSPARENT';
    field.frameColor = 0xFFFF;
    field.circleRadius = 26;
    field.circleThickness = 5;
    field.circleSweep = 360;
    field.circleDirection = 'CW';
    field.circleStartAngle = 'TOP';
    field.circleStyle = 'CONTINUOUS';
    field.circleCenterMode = 'PERCENT';
    field.circleColorMode = 'SOLID';
    field.circleShowPercent = true;
    field.circleTextSize = 1;
    field.circleTextColor = 0xFFFF;
    field.circleTrackColor = 0x2104;
    ensureBarState(field);

    fieldName.value = field.name;
    fieldName.dispatchEvent(new Event('change', { bubbles: true }));
  }

  barButton.disabled = false;
  barButton.classList.add('tool');
  barButton.dataset.tool = 'barField';
  barButton.title = 'Agregar / seleccionar barra de nivel y medidor';
  const barDescription = barButton.querySelector('span:last-child');
  if (barDescription) barDescription.textContent = 'Barra de nivel';
  barButton.addEventListener('click', (event) => {
    event.preventDefault();
    event.stopImmediatePropagation();
    createBarField();
    setTimeout(patchUI, 0);
  });

  function patchObjectList() {
    document.querySelectorAll('.object-item').forEach((item) => {
      if (item.querySelector('.object-type')?.textContent.trim() !== 'BAR') return;
      const icon = item.querySelector('.object-icon');
      if (icon) {
        icon.textContent = '▰';
        icon.style.fontSize = '13px';
        icon.style.fontWeight = '700';
        icon.style.color = '#52c9ff';
      }
    });
  }

  function patchInspector(field) {
    const activeBar = isBar(field);

    barPresetsDetails.hidden = !activeBar;
    barConfigDetails.hidden = !activeBar;

    if (fieldValueSizeWrap) fieldValueSizeWrap.hidden = activeBar;
    if (fieldAlign) fieldAlign.disabled = activeBar;

    const valColorWrap = fieldValueColor?.closest('label');
    if (valColorWrap) valColorWrap.style.display = activeBar ? 'none' : '';

    const frameColorWrap = fieldFrameColor?.closest('label');
    if (frameColorWrap) frameColorWrap.style.display = field?.frame ? '' : 'none';

    const barAlignMatrixRow = document.getElementById('barAlignMatrixRow');
    const labelSizeWrap = document.getElementById('fieldLabelSizeWrap');
    const alignWrap = document.getElementById('fieldAlignWrap');
    const layoutWrap = document.getElementById('fieldLayoutWrap');
    if (barAlignMatrixRow) barAlignMatrixRow.style.display = activeBar ? 'flex' : 'none';
    if (labelSizeWrap) labelSizeWrap.style.display = activeBar ? 'none' : '';
    if (alignWrap) alignWrap.style.display = activeBar ? 'none' : '';
    if (layoutWrap) layoutWrap.style.display = activeBar ? 'none' : '';

    if (!activeBar) {
      if (fieldUnit) {
        fieldUnit.disabled = false;
        const unitLabelWrap = fieldUnit.closest('label');
        if (unitLabelWrap) {
          unitLabelWrap.style.opacity = '1';
          unitLabelWrap.title = '';
        }
      }
      return;
    }

    ensureBarState(field);
    if (numericFormatDetails) numericFormatDetails.hidden = true;
    if (boolDetails) boolDetails.hidden = true;
    if (fieldCapacityWrap) fieldCapacityWrap.hidden = true;
    if (fieldPreviewWrap) fieldPreviewWrap.hidden = true;
    if (fieldCppType) fieldCppType.value = 'float';
    if (fieldLabel) fieldLabel.value = field.barLabel;
    if (fieldUnit) fieldUnit.value = field.barUnit;
    if (fieldAlign) fieldAlign.value = 'LEFT';
    if (fieldBarLabelSize) fieldBarLabelSize.value = String(field.labelSize || 1);

    // Título dinámico del inspector
    if (fieldInspectorTitle) {
      const isGrad = (field.barFillMode === 'GRADIENT' || field.barPreset === 'GRADIENT');
      const presetLabel = field.barPreset === 'CIRCULAR'
        ? 'Medidor Circular / Carga'
        : (isGrad ? 'Barra con Degradado' : 'Barra de Nivel Sólida');
      fieldInspectorTitle.textContent = `Inspector — ${presetLabel} (BAR)`;
    }

    // Actualizar botones de Presets (2 botones: Barra y Circular)
    document.querySelectorAll('.bar-preset-btn').forEach((btn) => {
      const isCirc = field.barPreset === 'CIRCULAR';
      const targetPreset = isCirc ? 'CIRCULAR' : 'LINEAR';
      btn.classList.toggle('active', btn.dataset.barPreset === targetPreset);
    });

    // Checkbox de etiqueta visible y matriz 3x3 de alineación
    const isShowingLabel = Boolean(field.showLabel);
    fieldBarShowLabel.checked = isShowingLabel;
    const curAlignV = field.alignV || 'TOP';
    const curAlignH = field.alignH || 'LEFT';
    alignMatrixCells.forEach((c) => {
      c.classList.toggle('active', c.dataset.alignV === curAlignV && c.dataset.alignH === curAlignH);
    });

    const matrixCol = barAlignMatrixRow?.querySelector('.bar-matrix-col');
    if (matrixCol) {
      matrixCol.style.opacity = isShowingLabel ? '1' : '0.4';
      matrixCol.style.pointerEvents = isShowingLabel ? 'auto' : 'none';
    }
    if (fieldBarLabelSizeWrap) {
      fieldBarLabelSizeWrap.style.opacity = isShowingLabel ? '1' : '0.4';
      fieldBarLabelSizeWrap.style.pointerEvents = isShowingLabel ? 'auto' : 'none';
    }

    // Rango y Slider
    fieldBarMin.value = String(field.barMin);
    fieldBarMax.value = String(field.barMax);
    fieldBarValue.value = String(field.barValue);
    fieldBarValueSlider.min = String(field.barMin);
    fieldBarValueSlider.max = String(field.barMax);
    fieldBarValueSlider.value = String(field.barValue);

    const norm = normalizedFor(field);
    if (barPercentStatus) barPercentStatus.textContent = `${(norm * 100).toFixed(1)} %`;

    // Visibilidad de subsecciones según el preset
    if (field.barPreset === 'CIRCULAR') {
      if (fieldUnit) {
        fieldUnit.disabled = false;
        const unitLabelWrap = fieldUnit.closest('label');
        if (unitLabelWrap) {
          unitLabelWrap.style.opacity = '1';
          unitLabelWrap.title = '';
        }
      }
      barConfigSummary.textContent = 'Configuración de Medidor Circular';
      barLinearControls.style.display = 'none';
      barGradientControls.style.display = 'none';
      barCircularControls.style.display = 'grid';

      fieldBarCircleRadius.value = String(field.circleRadius || 26);
      fieldBarCircleThickness.value = String(field.circleThickness || 5);

      // Sincronizar botones de apertura de arco
      const curSweep = Number(field.circleSweep) || 360;
      document.querySelectorAll('.bar-arc-btn').forEach((btn) => {
        btn.classList.toggle('active', Number(btn.dataset.sweep) === curSweep);
      });

      // Sincronizar botones de dirección
      const curDir = field.circleDirection || 'CW';
      document.querySelectorAll('.bar-dir-btn').forEach((btn) => {
        btn.classList.toggle('active', btn.dataset.dir === curDir);
      });

      // Sincronizar botones de cuadrante / inicio 0
      const curQuad = field.circleStartAngle || 'TOP';
      document.querySelectorAll('.bar-quadrant-btn').forEach((btn) => {
        btn.classList.toggle('active', btn.dataset.quadrant === curQuad);
      });

      // Sincronizar botones de estilo de anillo
      const curStyle = field.circleStyle || 'CONTINUOUS';
      document.querySelectorAll('.bar-style-btn').forEach((btn) => {
        btn.classList.toggle('active', btn.dataset.style === curStyle);
      });

      const circleGrad = field.circleColorMode === 'GRADIENT';
      if (fieldBarCircleColorMode) fieldBarCircleColorMode.value = circleGrad ? 'GRADIENT' : 'SOLID';
      if (fieldBarCircleSolidWrap) fieldBarCircleSolidWrap.style.display = circleGrad ? 'none' : 'block';
      if (fieldBarCircleGradWrap) fieldBarCircleGradWrap.style.display = circleGrad ? 'grid' : 'none';

      fieldBarCircleColorNative.value = rgb565ToHexCss(field.valueColor || 0x07FF);
      fieldBarCircleColorHex.textContent = hex565(field.valueColor || 0x07FF);
      if (fieldBarCircleGradStartNative) fieldBarCircleGradStartNative.value = rgb565ToHexCss(field.valueColor || 0xF800);
      if (fieldBarCircleGradStartHex) fieldBarCircleGradStartHex.textContent = hex565(field.valueColor || 0xF800);
      if (fieldBarCircleGradEndNative) fieldBarCircleGradEndNative.value = rgb565ToHexCss(field.valueColorEnd || 0x07E0);
      if (fieldBarCircleGradEndHex) fieldBarCircleGradEndHex.textContent = hex565(field.valueColorEnd || 0x07E0);

      fieldBarCircleTrackNative.value = rgb565ToHexCss(field.circleTrackColor || 0x2104);
      fieldBarCircleTrackHex.textContent = hex565(field.circleTrackColor || 0x2104);

      if (fieldBarCircleCenterMode) {
        fieldBarCircleCenterMode.value = field.circleCenterMode || (field.circleShowPercent ? 'PERCENT' : 'NONE');
      }

      // Límite seguro automático para que el texto NO toque el interior del círculo
      const centerText = getCircleCenterText(field, norm);
      const safeLimit = computeSafeCenterTextSize(field, centerText);
      if (fieldBarCircleSizeLimitHint) {
        fieldBarCircleSizeLimitHint.textContent = `Máx: ${safeLimit}×`;
      }
      if (fieldBarCircleTextSize) {
        Array.from(fieldBarCircleTextSize.options).forEach((opt) => {
          const val = Number(opt.value);
          if (val > safeLimit) {
            opt.disabled = true;
            opt.textContent = `${val}× (Excede interior)`;
          } else {
            opt.disabled = false;
            opt.textContent = `${val}×`;
          }
        });
        const clampedSize = Math.min(Number(field.circleTextSize) || 1, safeLimit);
        field.circleTextSize = clampedSize;
        fieldBarCircleTextSize.value = String(clampedSize);
      }

      fieldBarCircleTextColNative.value = rgb565ToHexCss(field.circleTextColor || 0xFFFF);
      fieldBarCircleTextColHex.textContent = hex565(field.circleTextColor || 0xFFFF);
    } else {
      // Lineales
      barCircularControls.style.display = 'none';
      barLinearControls.style.display = 'grid';

      if (fieldBarLinearPctPos) {
        fieldBarLinearPctPos.value = field.linearPctPos || 'NONE';
      }

      const showPctColor = (field.linearPctPos === 'RIGHT' || field.linearPctPos === 'INSIDE');
      if (fieldBarLinearPctColWrap) {
        fieldBarLinearPctColWrap.style.display = showPctColor ? 'block' : 'none';
      }
      if (fieldBarLinearPctColNative) {
        fieldBarLinearPctColNative.value = rgb565ToHexCss(field.linearPctColor || 0xFFFF);
        if (fieldBarLinearPctColHex) fieldBarLinearPctColHex.textContent = hex565(field.linearPctColor || 0xFFFF);
      }

      const isRightPct = (field.linearPctPos === 'RIGHT');
      if (fieldUnit) {
        fieldUnit.disabled = isRightPct;
        const unitLabelWrap = fieldUnit.closest('label');
        if (unitLabelWrap) {
          unitLabelWrap.style.opacity = isRightPct ? '0.5' : '1';
          unitLabelWrap.title = isRightPct ? 'La unidad exterior está reemplazada por el porcentaje de carga' : '';
        }
      }

      fieldBarOrientation.value = field.barOrientation || 'HORIZONTAL';
      fieldBarWidthMode.value = field.barAutoWidth ? 'AUTO' : 'FIXED';
      const isVert = field.barOrientation === 'VERTICAL';
      const currentLength = isVert ? field.barHeight : field.barWidth;
      const currentThick = isVert ? field.barWidth : field.barHeight;
      fieldBarWidth.value = String(currentLength || 110);
      fieldBarWidth.disabled = false; // Siempre modificable
      fieldBarHeight.value = String(currentThick || 12);
      fieldBarTrackColorNative.value = rgb565ToHexCss(field.trackColor || 0x18C3);
      fieldBarTrackColorHex.textContent = hex565(field.trackColor || 0x18C3);

      if (fieldBarWidthName) fieldBarWidthName.textContent = 'Longitud de la barra (px)';
      if (fieldBarHeightName) fieldBarHeightName.textContent = 'Grosor de la barra (px)';

      const isGrad = (field.barFillMode === 'GRADIENT' || field.barPreset === 'GRADIENT');
      if (fieldBarFillMode) fieldBarFillMode.value = isGrad ? 'GRADIENT' : 'SOLID';
      if (fieldBarSolidColorWrap) fieldBarSolidColorWrap.style.display = isGrad ? 'none' : 'block';
      if (barGradientControls) barGradientControls.style.display = isGrad ? 'grid' : 'none';

      barConfigSummary.textContent = isGrad ? 'Configuración de Barra Degradada' : 'Configuración de Barra Sólida';

      if (isGrad) {
        fieldBarGradStartNative.value = rgb565ToHexCss(field.valueColor || 0xF800);
        fieldBarGradStartHex.textContent = hex565(field.valueColor || 0xF800);
        fieldBarGradEndNative.value = rgb565ToHexCss(field.valueColorEnd || 0x07E0);
        fieldBarGradEndHex.textContent = hex565(field.valueColorEnd || 0x07E0);
      } else {
        if (fieldBarSolidColorNative) fieldBarSolidColorNative.value = rgb565ToHexCss(field.valueColor || 0x07FF);
        if (fieldBarSolidColorHex) fieldBarSolidColorHex.textContent = hex565(field.valueColor || 0x07FF);
      }
    }

    const g = barGeometry(field);
    if (fieldPadStatus) fieldPadStatus.textContent = `${g.pad} px`;
    if (fieldBoundsStatus) fieldBoundsStatus.textContent = `${g.fieldW} × ${g.fieldH} px`;
    if (fieldValueBoundsStatus) fieldValueBoundsStatus.textContent = `${g.valueW} × ${g.valueH} px`;
    if (fieldValueXYStatus) fieldValueXYStatus.textContent = `${g.valueX}, ${g.valueY}`;
    if (fieldLayoutStatus) fieldLayoutStatus.textContent = field.layout;
    if (inspectorContract) {
      const varName = sanitizeSymbol(field.variable, 'nivel');
      inspectorContract.textContent = `float ${varName} = 0.0f; // Vía JWPLC_Display.setBar()`;
    }

    document.querySelectorAll('.component-tool, .canvas-tool').forEach((button) => button.classList.remove('active'));
    barButton.classList.add('active');
  }

  function barFieldBlock(field) {
    ensureBarState(field);
    const id = sanitizeSymbol(field.id, `FIELD_BAR_${serialFor(field)}`);
    const label = (field.showLabel && field.barLabel) ? `"${cppString(field.barLabel)}"` : 'nullptr';
    const unit = field.barUnit ? `"${cppString(field.barUnit)}"` : 'nullptr';
    const g = barGeometry(field);

    if (field.barPreset === 'CIRCULAR') {
      return `    // Medidor Circular (${field.circleSweep || 360}°) - Estilo: ${field.circleStyle || 'CONTINUOUS'} - Inicio: ${field.circleStartAngle || 'TOP'} (${field.circleDirection || 'CW'})\n` +
             `    JWPLC_UICircularField(\n        ${id},\n        JWPLC_UIRect(${field.x}, ${field.y}, ${g.fieldW}, ${g.fieldH}),\n        JWPLC_UIText(${label}, ${unit}),\n        JWPLC_UIRange(${cppFloat(field.barMin)}, ${cppFloat(field.barMax)}),\n        ${field.circleSweep || 360},\n        ${field.page || 0},\n        JWPLC_UIColors(\n            ${hex565(field.labelColor)},\n            ${hex565(field.valueColor)},\n            ${hex565(field.circleTrackColor || 0x2104)},\n            ${hex565(field.frameColor)}))`;
    }

    const isGrad = (field.barFillMode === 'GRADIENT' || field.barPreset === 'GRADIENT');
    if (isGrad) {
      const rect = field.barAutoWidth
        ? `JWPLC_UIRect(${field.x}, ${field.y})`
        : `JWPLC_UIRect(${field.x}, ${field.y}, ${Math.max(20, Math.min(WIDTH, Math.trunc(Number(field.barWidth) || 110)))}, ${field.barOrientation === 'VERTICAL' ? Math.max(10, Math.trunc(Number(field.barHeight) || 60)) : JWPLC_UI_AUTO})`;
      return `    // Barra con Degradado (${field.barOrientation || 'HORIZONTAL'})\n` +
             `    JWPLC_UIGradientBarField(\n        ${id},\n        ${rect},\n        JWPLC_UIText(${label}, ${unit}),\n        JWPLC_UIRange(${cppFloat(field.barMin)}, ${cppFloat(field.barMax)}),\n        JWPLC_UIBarStyle(\n            ${field.labelSize},\n            ${cppBool(field.frame)},\n            JWPLC_UI_LAYOUT_${field.layout}),\n        ${hex565(field.valueColor)}, /* Inicio */\n        ${hex565(field.valueColorEnd || 0x07E0)}, /* Fin */\n        ${field.page || 0},\n        JWPLC_UIColors(\n            ${hex565(field.labelColor)},\n            ${hex565(field.valueColor)},\n            ${hex565(field.trackColor || 0x18C3)},\n            ${hex565(field.frameColor)}))`;
    }

    const rect = field.barAutoWidth
      ? `JWPLC_UIRect(${field.x}, ${field.y})`
      : `JWPLC_UIRect(${field.x}, ${field.y}, ${Math.max(20, Math.min(WIDTH, Math.trunc(Number(field.barWidth) || 110)))}, ${field.barOrientation === 'VERTICAL' ? Math.max(10, Math.trunc(Number(field.barHeight) || 60)) : JWPLC_UI_AUTO})`;

    return `    JWPLC_UIBarField(\n        ${id},\n        ${rect},\n        JWPLC_UIText(${label}, ${unit}),\n        JWPLC_UIRange(${cppFloat(field.barMin)}, ${cppFloat(field.barMax)}),\n        JWPLC_UIBarStyle(\n            ${field.labelSize},\n            ${cppBool(field.frame)},\n            JWPLC_UI_LAYOUT_${field.layout}),\n        ${field.page || 0},\n        JWPLC_UIColors(\n            ${hex565(field.labelColor)},\n            ${hex565(field.valueColor)},\n            ${hex565(field.trackColor || field.backgroundColor)},\n            ${hex565(field.frameColor)}))`;
  }

  function replaceHelperCall(text, id, replacement) {
    const marker = `    JWPLC_UITextField(\n        ${id},`;
    const start = text.indexOf(marker);
    if (start < 0) return text;
    const open = text.indexOf('(', start);
    if (open < 0) return text;
    let depth = 0;
    let inString = false;
    let escaped = false;
    for (let index = open; index < text.length; index += 1) {
      const ch = text[index];
      if (inString) {
        if (escaped) escaped = false;
        else if (ch === '\\') escaped = true;
        else if (ch === '"') inString = false;
        continue;
      }
      if (ch === '"') { inString = true; continue; }
      if (ch === '(') depth += 1;
      else if (ch === ')') {
        depth -= 1;
        if (depth === 0) return text.slice(0, start) + replacement + text.slice(index + 1);
      }
    }
    return text;
  }

  function activeBarFieldsInCode(text) {
    return activeBars().filter((field) => text.includes(String(field.id || '')));
  }

  function patchGeneratedCode() {
    if (!codeOutput?.textContent.startsWith('// Código generado por JWPLC HMI Designer')) return;
    let text = codeOutput.textContent
      .replace('// API pública JWPLC_UI — Alpha11 A11-3B', '// API pública JWPLC_UI — Alpha11 A11-3D')
      .replace('// API pública JWPLC_UI — Alpha11 A11-3C', '// API pública JWPLC_UI — Alpha11 A11-3D');

    activeBarFieldsInCode(text).forEach((field) => {
      ensureBarState(field);
      const id = sanitizeSymbol(field.id, `FIELD_BAR_${serialFor(field)}`);
      const variable = sanitizeSymbol(field.variable, `nivel${serialFor(field)}`);
      const legacyDeclaration = `char ${variable}[${Math.max(1, field.capacity) + 1}] = {};`;
      text = text.replace(legacyDeclaration, `float ${variable} = 0.0f;`);
      text = replaceHelperCall(text, id, barFieldBlock(field));
      text = text.replace(
        `// JWPLC_Display.setText(${id}, ${variable});`,
        `// JWPLC_Display.setBar(${id}, ${variable});`);
      text = text.replace(
        `// JWPLC_Display.setValue(${id}, ${variable});`,
        `// JWPLC_Display.setBar(${id}, ${variable});`);
    });
    codeOutput.textContent = text;
  }

  function patchStatusText() {
    if (!codeOutput?.textContent.startsWith('A11 UX Foundation:')) return;
    const barCount = visibleBars().length;
    let text = codeOutput.textContent
      .replace('A11-3C BOOL: IN_PROGRESS', 'A11-3C BOOL: PASS')
      .replace('A11-3D BAR permanece pendiente.', 'A11-3D BAR: IN_PROGRESS');

    if (!text.includes('A11-3D BAR: IN_PROGRESS') && !text.includes('A11-3D BAR: PASS')) {
      const marker = text.includes('A11-3C BOOL: PASS') ? 'A11-3C BOOL: PASS' : 'A11-3B VALUE: PASS';
      text = text.replace(marker, `${marker}\nA11-3D BAR: IN_PROGRESS`);
    }
    if (!text.includes('\n- BAR:')) {
      text = text.replace(/(\n- BOOL: \d+)/, `$1\n- BAR: ${barCount}`);
    } else {
      text = text.replace(/\n- BAR: \d+/, `\n- BAR: ${barCount}`);
    }
    text = text.replace(/\n\nA11-3D BAR permanece pendiente\.?/, '');
    codeOutput.textContent = text;
  }



  const api = editor();
  if (api) {
    const originalGeometry = api.computeSelectedGeometry?.bind(api);
    const originalHasFieldSelection = api.hasFieldSelection?.bind(api);
    api.computeSelectedGeometry = () => isBar() ? barGeometry(selectedField()) : originalGeometry?.();
    api.hasFieldSelection = () => isBar() || Boolean(originalHasFieldSelection?.());
    api.hasBarSelection = () => isBar();
    api.addBarField = createBarField;
  }

  function patchUI() {
    const field = selectedField();
    if (field?.type === 'BAR') ensureBarState(field);
    if (gate) gate.textContent = 'Gate: A11-3D BAR — Presets y Medidores';
    if (bottomSummary) bottomSummary.textContent = 'A11-3D — BAR: Sólida, Degradado y Circular';
    patchObjectList();
    patchInspector(field);
    patchCanvases();
    patchGeneratedCode();
    patchStatusText();
  }

  window.addEventListener('jwplc:editor-refresh', patchUI);
  contractTab?.addEventListener('click', () => setTimeout(patchGeneratedCode, 0));
  statusTab?.addEventListener('click', () => setTimeout(patchStatusText, 0));
  generateButton?.addEventListener('click', () => setTimeout(patchGeneratedCode, 0));
  fieldLayout?.addEventListener('change', () => setTimeout(patchUI, 0));
  fieldFrame?.addEventListener('change', () => setTimeout(patchUI, 0));
  fieldLabelSize?.addEventListener('change', () => setTimeout(patchUI, 0));
  [fieldLabelColor, fieldValueColor, fieldBackgroundColor, fieldFrameColor]
    .filter(Boolean)
    .forEach((input) => input.addEventListener('change', () => setTimeout(patchUI, 0)));

  patchUI();

  window.JWPLCHMIBar = {
    addBarField: createBarField,
    setPreset,
    hasBarSelection: () => isBar(),
    trackedCount: () => activeBars().length,
    computeGeometry: (field) => barGeometry(field),
    drawBarField,
    drawBars
  };
})();
