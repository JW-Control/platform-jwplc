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


  // --- Alpha11 · Inspector BAR Rediseño (Propuesta 2: Visual e Intuitivo) ---
  const barInspectorWrap = document.createElement('div');
  barInspectorWrap.id = 'barInspectorWrap';
  barInspectorWrap.className = 'bar-inspector-wrap';
  barInspectorWrap.style.display = 'none';
  barInspectorWrap.innerHTML = `
    <!-- Cabecera de navegación: Pestañas principales y Subpestañas de Diseño -->
    <div class="bar-nav-header">
      <div class="bar-main-tabs" role="tablist">
        <button type="button" class="bar-main-tab active" data-main-tab="design" role="tab" aria-selected="true">
          <span>Diseño</span>
        </button>
        <button type="button" class="bar-main-tab" data-main-tab="plc" role="tab" aria-selected="false">
          <span>Datos PLC</span>
        </button>
        <button type="button" class="bar-main-tab" data-main-tab="tech" role="tab" aria-selected="false">
          <span>Técnico</span>
        </button>
      </div>

      <div class="bar-sub-tabs" id="barDesignSubTabs" role="tablist">
        <button type="button" class="bar-sub-tab active" data-sub-tab="shape" role="tab" aria-selected="true">Forma</button>
        <button type="button" class="bar-sub-tab" data-sub-tab="content" role="tab" aria-selected="false">Contenido</button>
        <button type="button" class="bar-sub-tab" data-sub-tab="appearance" role="tab" aria-selected="false">Apariencia</button>
        <button type="button" class="bar-sub-tab" data-sub-tab="placement" role="tab" aria-selected="false">Ubicación</button>
      </div>
    </div>

    <!-- Contenido Scrollable -->
    <div class="bar-scroll-body">
      <!-- PANEL 1: DISEÑO -->
      <div class="bar-tab-panel" id="barPanelDesign" data-panel="design">

        <!-- SUBPANEL 1.1: FORMA -->
        <div class="bar-subpanel" id="barSubpanelShape" data-subpanel="shape">
          <!-- Grupo A: Tipo de indicador -->
          <div class="bar-card">
            <div class="bar-card-section-title">▾ Tipo de indicador</div>
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
            </div>
          </div>

          <!-- Simulación / Valor de prueba (debajo del tipo de indicador) -->
          <div class="bar-top-slider-wrap">
            <div class="bar-top-slider-header">
              <span>Simulación / Valor de prueba</span>
              <strong id="barTopSimReadout">50.0 %</strong>
            </div>
            <input id="barTopSimSlider" class="bar-top-slider-input" type="range" min="0" max="100" step="0.5" value="50" />
          </div>

          <!-- Grupo B: Parámetros para Barra (solo cuando Barra está activa) -->
          <div id="barLinearShapeGroup" class="bar-shape-group">
            <div class="bar-card">
              <!-- Orientación (visual buttons) -->
              <div class="field-label full">
                <span class="bar-visual-group-title">Orientación</span>
                <div class="bar-visual-grid-2" id="barOrientButtonsWrap">
                  <button type="button" class="bar-visual-btn bar-orient-btn active" data-orient="HORIZONTAL" title="Orientación Horizontal">
                    <svg width="20" height="14" viewBox="0 0 20 14"><rect x="1" y="4" width="18" height="6" rx="1" stroke-width="1.5"/><rect x="3" y="5.5" width="8" height="3" fill="currentColor"/></svg>
                    <span>Horizontal</span>
                  </button>
                  <button type="button" class="bar-visual-btn bar-orient-btn" data-orient="VERTICAL" title="Orientación Vertical">
                    <svg width="14" height="20" viewBox="0 0 14 20"><rect x="4" y="1" width="6" height="18" rx="1" stroke-width="1.5"/><rect x="5.5" y="9" width="3" height="8" fill="currentColor"/></svg>
                    <span>Vertical</span>
                  </button>
                </div>
              </div>

              <!-- Modo Ancho y Tipo de Relleno -->
              <div class="bar-two-cols" style="margin-top:6px;">
                <label class="field-label">Modo de ancho
                  <select id="barFormaWidthMode" class="field-input">
                    <option value="AUTO">AUTO</option>
                    <option value="FIXED" selected>FIJO</option>
                  </select>
                </label>
                <label class="field-label">Tipo de Relleno
                  <select id="barFormaFillMode" class="field-input">
                    <option value="SOLID" selected>Color Sólido</option>
                    <option value="GRADIENT">Degradado</option>
                  </select>
                </label>
              </div>

              <!-- Longitud y Grosor -->
              <div class="bar-two-cols" style="margin-top:6px;">
                <label class="field-label" id="barFormaWidthLabel"><span id="barFormaWidthName">Longitud de barra (px)</span>
                  <input id="barFormaWidth" class="field-input" type="number" min="6" max="320" value="110" />
                </label>
                <label class="field-label" id="barFormaHeightLabel"><span id="barFormaHeightName">Grosor de barra (px)</span>
                  <input id="barFormaHeight" class="field-input" type="number" min="4" max="170" value="12" />
                </label>
              </div>

              <!-- Estilo de Barra -->
              <div class="field-label full" style="margin-top:6px;">
                <span class="bar-visual-group-title">Estilo de Barra</span>
                <div class="bar-visual-grid-4" id="barLinearStyleWrap">
                  <button type="button" class="bar-visual-btn bar-linear-style-btn active" data-style="SOLID" title="Relleno continuo liso">
                    <svg width="22" height="14" viewBox="0 0 22 14">
                      <rect x="1" y="2" width="20" height="10" rx="1" stroke-width="1.5"/>
                      <rect x="3" y="4" width="12" height="6" fill="currentColor"/>
                    </svg>
                    <span>Sólido</span>
                  </button>
                  <button type="button" class="bar-visual-btn bar-linear-style-btn" data-style="STRIPES" title="Líneas diagonales a 45° tipo industrial">
                    <svg width="22" height="14" viewBox="0 0 22 14">
                      <rect x="1" y="2" width="20" height="10" rx="1" stroke-width="1.5"/>
                      <path d="M 4 10 L 8 4 M 8 10 L 12 4 M 12 10 L 16 4" stroke-width="1.5" stroke-linecap="round"/>
                    </svg>
                    <span>Diagonales</span>
                  </button>
                  <button type="button" class="bar-visual-btn bar-linear-style-btn" data-style="BLOCKS" title="Segmentos rectangulares clásico Win XP">
                    <svg width="22" height="14" viewBox="0 0 22 14">
                      <rect x="1" y="2" width="20" height="10" rx="1" stroke-width="1.5"/>
                      <rect x="3" y="4" width="3" height="6" fill="currentColor"/>
                      <rect x="8" y="4" width="3" height="6" fill="currentColor"/>
                      <rect x="13" y="4" width="3" height="6" fill="currentColor"/>
                    </svg>
                    <span>Bloques</span>
                  </button>
                  <button type="button" class="bar-visual-btn bar-linear-style-btn" data-style="DOTS" title="Matriz de puntos LED lineales">
                    <svg width="22" height="14" viewBox="0 0 22 14">
                      <rect x="1" y="2" width="20" height="10" rx="1" stroke-width="1.5"/>
                      <circle cx="5" cy="7" r="1.8" fill="currentColor"/>
                      <circle cx="10" cy="7" r="1.8" fill="currentColor"/>
                      <circle cx="15" cy="7" r="1.8" fill="currentColor"/>
                    </svg>
                    <span>Puntos</span>
                  </button>
                </div>
              </div>

              <!-- Redondeo de Esquinas -->
              <div class="field-label full" style="margin-top:6px;">
                <span class="bar-visual-group-title">Redondeo de Esquinas</span>
                <div class="bar-visual-grid-3" id="barLinearRadiusWrap">
                  <button type="button" class="bar-visual-btn bar-radius-btn active" data-radius="0" title="Esquinas rectas (0px)">
                    <svg width="20" height="14" viewBox="0 0 20 14"><rect x="2" y="3" width="16" height="8" rx="0" stroke-width="1.5"/></svg>
                    <span>Recto</span>
                  </button>
                  <button type="button" class="bar-visual-btn bar-radius-btn" data-radius="3" title="Esquinas suavizadas (3px)">
                    <svg width="20" height="14" viewBox="0 0 20 14"><rect x="2" y="3" width="16" height="8" rx="2.5" stroke-width="1.5"/></svg>
                    <span>Leve</span>
                  </button>
                  <button type="button" class="bar-visual-btn bar-radius-btn" data-radius="FULL" title="Extremos en semicírculo completo">
                    <svg width="20" height="14" viewBox="0 0 20 14"><rect x="2" y="3" width="16" height="8" rx="4" stroke-width="1.5"/></svg>
                    <span>Píldora</span>
                  </button>
                </div>
              </div>

              <!-- Borde de Barra -->
              <div class="field-label full" style="margin-top:6px;">
                <div style="display:flex;align-items:center;justify-content:space-between;margin-bottom:3px;">
                  <label class="field-label-checkbox" style="margin:0;cursor:pointer;">
                    <input id="barFormaBorderEnabled" type="checkbox" />
                    <span>Borde de la barra</span>
                  </label>
                  <div id="barFormaBorderColorWrap" class="bar-color-input-wrap compact" style="display:none;">
                    <input id="barFormaBorderColorNative" type="color" value="#ffffff" />
                    <span id="barFormaBorderColorHex" class="hex-label compact">0xFFFF</span>
                  </div>
                </div>
                <div id="barFormaBorderWidthWrap" style="display:none;align-items:center;justify-content:space-between;margin-top:4px;padding:3px 6px;background:#0d1820;border:1px solid #1e313f;border-radius:4px;">
                  <span style="font-size:10px;color:#8ea3b3;font-weight:600;">Grosor de borde (px)</span>
                  <input id="barFormaBorderWidth" class="field-input" type="number" min="1" max="10" value="1" style="width:60px;padding:2px 6px;text-align:right;" />
                </div>
              </div>
            </div>
          </div>

          <!-- Grupo C: Parámetros para Circular (solo cuando Circular está activa) -->
          <div id="barCircularShapeGroup" class="bar-shape-group" style="display:none;">
            <div class="bar-card">
              <!-- Apertura del Arco -->
              <div class="field-label full">
                <span class="bar-visual-group-title">Apertura del Arco</span>
                <div class="bar-visual-grid-4" id="barCircularArcWrap">
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

              <!-- Dirección de llenado -->
              <div class="field-label full" style="margin-top:6px;">
                <span class="bar-visual-group-title">Dirección de llenado</span>
                <div class="bar-visual-grid-2" id="barCircularDirWrap">
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

              <!-- Inicio del 0 -->
              <div class="field-label full" style="margin-top:6px;">
                <span class="bar-visual-group-title">Inicio del 0</span>
                <div class="bar-visual-grid-4" id="barCircularQuadrantWrap">
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

              <!-- Estilo de Anillo -->
              <div class="field-label full" style="margin-top:6px;">
                <span class="bar-visual-group-title">Estilo de Anillo</span>
                <div class="bar-visual-grid-4" id="barCircularStyleWrap">
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

              <!-- Dimensiones Radio y Grosor -->
              <div class="bar-two-cols" style="margin-top:6px;">
                <label class="field-label">Radio exterior (px)
                  <input id="barFormaCircleRadius" class="field-input" type="number" min="10" max="80" value="26" />
                </label>
                <label class="field-label">Grosor anillo (px)
                  <input id="barFormaCircleThickness" class="field-input" type="number" min="2" max="25" value="5" />
                </label>
              </div>

              <!-- Tipo de Relleno Arco -->
              <label class="field-label full" style="margin-top:6px;">Tipo de Relleno Arco
                <select id="barFormaCircleColorMode" class="field-input">
                  <option value="SOLID" selected>Color Sólido</option>
                  <option value="GRADIENT">Degradado</option>
                </select>
              </label>
            </div>
          </div>
        </div>

        <!-- SUBPANEL 1.2: CONTENIDO -->
        <div class="bar-subpanel" id="barSubpanelContent" data-subpanel="content" style="display:none;">
          <div class="bar-card">
            <div class="bar-two-cols">
              <label class="field-label">Etiqueta visible
                <input id="barContentLabel" class="field-input" type="text" value="Nivel" maxlength="24" />
              </label>
              <label class="field-label">Unidad
                <input id="barContentUnit" class="field-input" type="text" value="%" maxlength="12" />
              </label>
            </div>

            <!-- Matriz 3x3 de alineación y opciones -->
            <div class="bar-align-matrix-row" id="barContentAlignRow" style="margin-top:8px;">
              <div class="bar-matrix-col">
                <span class="bar-matrix-label">Alineación (3×3)</span>
                <div id="barContentMatrixWrap" class="align-matrix-3x3">
                  <button type="button" class="matrix-cell" data-align-v="TOP" data-align-h="LEFT" title="Arriba Izquierda">↖</button>
                  <button type="button" class="matrix-cell active" data-align-v="TOP" data-align-h="CENTER" title="Arriba Centro">↑</button>
                  <button type="button" class="matrix-cell" data-align-v="TOP" data-align-h="RIGHT" title="Arriba Derecha">↗</button>
                  <button type="button" class="matrix-cell" data-align-v="MID" data-align-h="LEFT" title="Medio Izquierda">←</button>
                  <button type="button" class="matrix-cell" data-align-v="MID" data-align-h="CENTER" title="Centro">·</button>
                  <button type="button" class="matrix-cell" data-align-v="MID" data-align-h="RIGHT" title="Medio Derecha">→</button>
                  <button type="button" class="matrix-cell" data-align-v="BOTTOM" data-align-h="LEFT" title="Abajo Izquierda">↙</button>
                  <button type="button" class="matrix-cell" data-align-v="BOTTOM" data-align-h="CENTER" title="Abajo Centro">↓</button>
                  <button type="button" class="matrix-cell" data-align-v="BOTTOM" data-align-h="RIGHT" title="Abajo Derecha">↘</button>
                </div>
              </div>
              <div class="bar-options-col">
                <label class="field-label-checkbox">
                  <input id="barContentShowLabel" type="checkbox" checked />
                  <span>Mostrar etiqueta</span>
                </label>
                <label class="field-label" id="barContentLabelSizeWrap">Tamaño etiqueta
                  <select id="barContentLabelSize" class="field-input">
                    <option value="1" selected>1×</option>
                    <option value="2">2×</option>
                    <option value="3">3×</option>
                    <option value="4">4×</option>
                  </select>
                </label>
              </div>
            </div>

            <!-- Opciones de texto para Barra -->
            <div id="barContentLinearWrap" style="margin-top:8px;">
              <label class="field-label full">Mostrar % de Carga
                <select id="barContentLinearPctPos" class="field-input">
                  <option value="NONE" selected>No (Por defecto)</option>
                  <option value="RIGHT">Lado derecho (Exterior)</option>
                  <option value="INSIDE">Al interior de la barra</option>
                </select>
              </label>
            </div>

            <!-- Opciones de texto para Circular -->
            <div id="barContentCircularWrap" style="display:none;margin-top:8px;">
              <label class="field-label full">Texto al Centro
                <select id="barContentCircleCenterMode" class="field-input">
                  <option value="PERCENT" selected>Porcentaje (Ej: 75%)</option>
                  <option value="FRACTION">Fracción (Ej: 8/8, 7/16)</option>
                  <option value="VALUE">Valor actual (Ej: 50)</option>
                  <option value="NONE">Oculto (Sin texto)</option>
                </select>
              </label>
              <label class="field-label full" id="barContentCircleTextSizeWrap" style="margin-top:6px;">
                <span style="display:flex;align-items:center;justify-content:space-between;">
                  <span>Tamaño texto central</span>
                  <span id="barContentCircleSizeHint" class="bar-text-limit-hint">Máx: 2×</span>
                </span>
                <select id="barContentCircleTextSize" class="field-input">
                  <option value="1" selected>1×</option>
                  <option value="2">2×</option>
                  <option value="3">3×</option>
                  <option value="4">4×</option>
                </select>
              </label>
            </div>
          </div>
        </div>

        <!-- SUBPANEL 1.3: APARIENCIA -->
        <div class="bar-subpanel" id="barSubpanelAppearance" data-subpanel="appearance" style="display:none;">
          <div class="bar-card">
            <!-- Barra Lineal Colores -->
            <div id="barAppLinearFillWrap">
              <div id="barAppLinearSolidWrap" class="field-label full">
                <span class="bar-field-subtitle">Color de Barra</span>
                <div class="bar-color-input-wrap">
                  <input id="barAppLinearColorNative" type="color" value="#00ffff" />
                  <span id="barAppLinearColorHex" class="hex-label compact">0x07FF</span>
                </div>
              </div>
              <div id="barAppLinearGradWrap" style="display:none;">
                <div class="bar-grad-row full">
                  <label class="field-label" style="flex:1;min-width:0;">Color Inicial
                    <div class="bar-color-input-wrap compact">
                      <input id="barAppLinearGradStartNative" type="color" value="#ff0000" />
                      <span id="barAppLinearGradStartHex" class="hex-label compact">0xF800</span>
                    </div>
                  </label>
                  <button type="button" id="barAppLinearSwapGradBtn" class="bar-swap-icon-btn" title="Intercalar color inicial y final">⇄</button>
                  <label class="field-label" style="flex:1;min-width:0;">Color Final
                    <div class="bar-color-input-wrap compact">
                      <input id="barAppLinearGradEndNative" type="color" value="#00ff00" />
                      <span id="barAppLinearGradEndHex" class="hex-label compact">0x07E0</span>
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
              <label class="field-label full" style="margin-top:6px;">Color de Pista vacía
                <div class="bar-color-input-wrap">
                  <input id="barAppLinearTrackNative" type="color" value="#18232c" />
                  <span id="barAppLinearTrackHex" class="hex-label compact">0x18C3</span>
                </div>
              </label>
              <div id="barAppLinearPctColWrap" class="field-label full" style="display:none;margin-top:6px;">
                <span class="bar-field-subtitle">Color de Porcentaje</span>
                <div class="bar-color-input-wrap">
                  <input id="barAppLinearPctColNative" type="color" value="#ffffff" />
                  <span id="barAppLinearPctColHex" class="hex-label compact">0xFFFF</span>
                </div>
              </div>
            </div>

            <!-- Circular Colores -->
            <div id="barAppCircularFillWrap" style="display:none;">
              <div id="barAppCircularSolidWrap" class="field-label full">
                <span class="bar-field-subtitle">Color Arco Carga</span>
                <div class="bar-color-input-wrap">
                  <input id="barAppCircularColorNative" type="color" value="#00ffff" />
                  <span id="barAppCircularColorHex" class="hex-label compact">0x07FF</span>
                </div>
              </div>
              <div id="barAppCircularGradWrap" style="display:none;">
                <div class="bar-grad-row full">
                  <label class="field-label" style="flex:1;min-width:0;">Color Inicial
                    <div class="bar-color-input-wrap compact">
                      <input id="barAppCircularGradStartNative" type="color" value="#ff0000" />
                      <span id="barAppCircularGradStartHex" class="hex-label compact">0xF800</span>
                    </div>
                  </label>
                  <button type="button" id="barAppCircularSwapGradBtn" class="bar-swap-icon-btn" title="Intercalar color inicial y final">⇄</button>
                  <label class="field-label" style="flex:1;min-width:0;">Color Final
                    <div class="bar-color-input-wrap compact">
                      <input id="barAppCircularGradEndNative" type="color" value="#00ff00" />
                      <span id="barAppCircularGradEndHex" class="hex-label compact">0x07E0</span>
                    </div>
                  </label>
                </div>
              </div>
              <label class="field-label full" style="margin-top:6px;">Pista del Anillo
                <div class="bar-color-input-wrap">
                  <input id="barAppCircularTrackNative" type="color" value="#212930" />
                  <span id="barAppCircularTrackHex" class="hex-label compact">0x2104</span>
                </div>
              </label>
              <div id="barAppCircularTextColWrap" class="field-label full" style="margin-top:6px;">
                <span class="bar-field-subtitle">Color texto central</span>
                <div class="bar-color-input-wrap">
                  <input id="barAppCircularTextColNative" type="color" value="#ffffff" />
                  <span id="barAppCircularTextColHex" class="hex-label compact">0xFFFF</span>
                </div>
              </div>
            </div>

            <!-- Colores comunes -->
            <div class="bar-two-cols" style="margin-top:8px;">
              <label class="field-label" id="barAppLabelColWrap">Color etiqueta
                <div class="bar-color-input-wrap">
                  <input id="barAppLabelColNative" type="color" value="#ffffff" />
                  <span id="barAppLabelColHex" class="hex-label compact">0xFFFF</span>
                </div>
              </label>
              <label class="field-label">Color fondo
                <div class="bar-color-input-wrap">
                  <input id="barAppBgColNative" type="color" value="#000000" />
                  <span id="barAppBgColHex" class="hex-label compact">0x0000</span>
                </div>
              </label>
            </div>
          </div>
        </div>

        <!-- SUBPANEL 1.4: UBICACIÓN -->
        <div class="bar-subpanel" id="barSubpanelPlacement" data-subpanel="placement" style="display:none;">
          <div class="bar-card">
            <label class="field-label full">Página
              <input id="barPlacementPage" class="field-input" type="text" value="01 - Principal" disabled />
            </label>
            <div class="bar-two-cols" style="margin-top:8px;">
              <label class="field-label">Coordenada X1 (px)
                <input id="barPlacementX" class="field-input" type="number" min="0" max="319" value="20" />
              </label>
              <label class="field-label">Coordenada Y1 (px)
                <input id="barPlacementY" class="field-input" type="number" min="0" max="169" value="20" />
              </label>
            </div>
            <div class="readout" style="margin-top:8px;">
              <span>Ancho × alto calculado</span>
              <strong id="barPlacementBounds">—</strong>
            </div>
            <div class="readout" style="margin-top:4px;">
              <span>Región del valor</span>
              <strong id="barPlacementValueBounds">—</strong>
            </div>
          </div>
        </div>

      </div>

      <!-- PANEL 2: DATOS PLC -->
      <div class="bar-tab-panel" id="barPanelPlc" data-panel="plc" style="display:none;">
        <!-- Tarjeta 1: Vinculación de datos -->
        <div class="bar-card">
          <div class="bar-card-header">
            <span class="bar-card-title bar-cyan-title">
              <svg width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2"><path d="M10 13a5 5 0 0 0 7.54.54l3-3a5 5 0 0 0-7.07-7.07l-1.72 1.71"/><path d="M14 11a5 5 0 0 0-7.54-.54l-3 3a5 5 0 0 0 7.07 7.07l1.71-1.71"/></svg>
              Vinculación de datos
            </span>
          </div>
          <div class="field-label full" style="margin-top:6px;">
            <span>Variable vinculada (C++)</span>
            <div style="display:flex;gap:4px;align-items:center;">
              <input id="barPlcVariable" class="field-input code-input" type="text" value="nivel4" maxlength="32" style="flex:1;" />
              <button type="button" id="barCopyVarBtn" class="bar-copy-icon-btn" title="Copiar nombre de variable">⧉</button>
            </div>
          </div>
          <label class="field-label full" style="margin-top:6px;">Tipo C++
            <input id="barPlcCppType" class="field-input" value="float" disabled />
          </label>
          <div class="bar-info-note" style="margin-top:8px;">
            <span class="bar-info-icon">ℹ</span>
            <span>La variable debe existir en el proyecto y ser de tipo numérico (int, float, etc.).</span>
          </div>
        </div>

        <!-- Tarjeta 2: Rango / Escalado -->
        <div class="bar-card" style="margin-top:10px;">
          <div class="bar-card-header">
            <span class="bar-card-title bar-cyan-title">
              <svg width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2"><path d="M18 20V10M12 20V4M6 20v-6"/></svg>
              Rango / Escalado
            </span>
          </div>
          <div class="bar-two-cols" style="margin-top:6px;">
            <label class="field-label">Valor mínimo (PLC)
              <input id="barPlcMin" class="field-input" type="number" value="0" />
            </label>
            <label class="field-label">Valor máximo (PLC)
              <input id="barPlcMax" class="field-input" type="number" value="100" />
            </label>
          </div>
          <label class="field-label full" style="margin-top:6px;">Unidad
            <input id="barPlcUnit" class="field-input" type="text" value="%" maxlength="12" />
          </label>
          <div class="bar-info-note" style="margin-top:8px;">
            <span class="bar-info-icon">ℹ</span>
            <span>El valor de la variable se escala al rango del indicador (0% - 100%).</span>
          </div>
        </div>

        <!-- Tarjeta 3: Simulación local (ÚNICAMENTE AQUÍ) -->
        <div class="bar-card" style="margin-top:10px;">
          <div class="bar-card-header" style="justify-content:space-between;">
            <span class="bar-card-title bar-cyan-title">
              <svg width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2"><polygon points="5 3 19 12 5 21 5 3"/></svg>
              Simulación local
            </span>
            <label class="bar-toggle-switch" title="Habilitar / Deshabilitar simulación">
              <input id="barPlcSimToggle" type="checkbox" checked />
              <span class="bar-toggle-slider"></span>
            </label>
          </div>
          <div class="bar-sim-header" style="margin-top:8px;display:flex;justify-content:space-between;align-items:center;">
            <span style="font-size:11px;color:#8ea3b3;font-weight:600;">Valor de prueba</span>
            <strong id="barPlcSimReadout" class="bar-cyan-readout" style="font-size:13px;font-family:'Cascadia Code',Consolas,monospace;color:#00d7ef;">50.0 %</strong>
          </div>
          <div style="margin-top:6px;">
            <input id="barPlcSimSlider" class="bar-top-slider-input" type="range" min="0" max="100" step="0.5" value="50" />
          </div>
          <div style="display:flex;justify-content:space-between;margin-top:2px;font-size:9.5px;color:#6b8699;font-family:'Cascadia Code',Consolas,monospace;">
            <span id="barPlcSimMinLabel">0</span>
            <span id="barPlcSimMaxLabel">100</span>
          </div>
          <div class="bar-info-note" style="margin-top:8px;">
            <span class="bar-info-icon">ℹ</span>
            <span>La simulación no modifica el firmware. Solo se visualiza en el editor.</span>
          </div>
        </div>
      </div>

      <!-- PANEL 3: TÉCNICO -->
      <div class="bar-tab-panel" id="barPanelTech" data-panel="tech" style="display:none;">
        <!-- Tarjeta 1: Identidad -->
        <div class="bar-card">
          <div class="bar-card-header">
            <span class="bar-card-title bar-cyan-title">
              <svg width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2"><path d="M21 16V8a2 2 0 0 0-1-1.73l-7-4a2 2 0 0 0-2 0l-7 4A2 2 0 0 0 3 8v8a2 2 0 0 0 1 1.73l7 4a2 2 0 0 0 2 0l7-4A2 2 0 0 0 21 16z"/><polyline points="3.27 6.96 12 12.01 20.73 6.96"/><line x1="12" y1="22.08" x2="12" y2="12"/></svg>
              Identidad
            </span>
          </div>
          <label class="field-label full" style="margin-top:6px;">Nombre del objeto
            <input id="barTechName" class="field-input" type="text" value="BAR 4" maxlength="24" />
          </label>
          <div class="field-label full" style="margin-top:6px;">
            <span>ID C++ del campo</span>
            <div style="display:flex;gap:4px;align-items:center;">
              <input id="barTechId" class="field-input code-input" type="text" value="FIELD_BAR_4" maxlength="32" style="flex:1;" />
              <button type="button" id="barCopyIdBtn" class="bar-copy-icon-btn" title="Copiar ID C++">⧉</button>
            </div>
          </div>
          <label class="field-label full" style="margin-top:6px;">Página
            <input id="barTechPage" class="field-input" type="text" value="01 - Principal" disabled />
          </label>
        </div>

        <!-- Tarjeta 2: Contrato C++ -->
        <div class="bar-card" style="margin-top:10px;">
          <div class="bar-card-header">
            <span class="bar-card-title bar-cyan-title">
              <svg width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2"><polyline points="16 18 22 12 16 6"/><polyline points="8 6 2 12 8 18"/></svg>
              Contrato C++
            </span>
          </div>
          <span style="font-size:10px;color:#8ea3b3;margin-top:4px;display:block;">Declaración (en tiempo de compilación)</span>
          <div class="bar-code-box" style="margin-top:4px;">
            <code id="barTechContractCode">float nivel4 = 0.0f;</code>
            <button type="button" id="barTechCopyContractBtn" class="bar-copy-icon-btn" title="Copiar código">⧉</button>
          </div>
          <div class="bar-info-note" style="margin-top:8px;">
            <span class="bar-info-icon">ℹ</span>
            <span>Esta variable se genera en el código del proyecto y puede ser utilizada en la lógica de control.</span>
          </div>
        </div>

        <!-- Tarjeta 3: Diagnóstico -->
        <div class="bar-card" style="margin-top:10px;">
          <div class="bar-card-header">
            <span class="bar-card-title bar-cyan-title">
              <svg width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2"><polyline points="22 12 18 12 15 21 9 3 6 12 2 12"/></svg>
              Diagnóstico
            </span>
          </div>
          <div class="bar-two-cols" style="margin-top:6px;">
            <div class="readout"><span>Valor actual (simulado)</span><strong id="barDiagSimVal" class="bar-cyan-readout">50.0 %</strong></div>
            <div class="readout"><span>Valor en unidades</span><strong id="barDiagUnitVal">50.0</strong></div>
          </div>
          <div class="readout" style="margin-top:4px;">
            <span>Rango configurado</span>
            <strong id="barDiagRange">0 — 100 %</strong>
          </div>
          <span style="font-size:10px;color:#8ea3b3;margin-top:8px;display:block;font-weight:600;">Geometría (px)</span>
          <div class="bar-two-cols" style="margin-top:4px;">
            <div class="readout"><span>X</span><strong id="barDiagX">20</strong></div>
            <div class="readout"><span>Y</span><strong id="barDiagY">20</strong></div>
          </div>
          <div class="bar-two-cols" style="margin-top:4px;">
            <div class="readout"><span>Ancho</span><strong id="barDiagW">110 px</strong></div>
            <div class="readout"><span>Alto</span><strong id="barDiagH">29 px</strong></div>
          </div>
          <div class="bar-two-cols" style="margin-top:4px;">
            <div class="readout"><span>Padding</span><strong id="barDiagPad">3 px</strong></div>
            <div class="readout"><span>Gap</span><strong>4 px</strong></div>
          </div>
          <div class="readout" style="margin-top:4px;">
            <span>Región de valor</span>
            <strong id="barDiagValRegion">—</strong>
          </div>
        </div>

        <!-- Tarjeta 4: Acciones avanzadas -->
        <div class="bar-card" style="margin-top:10px;">
          <div class="bar-card-header">
            <span class="bar-card-title" style="color:#dce9f1;font-size:11px;">Acciones avanzadas</span>
          </div>
          <div style="display:flex;gap:6px;margin-top:8px;">
            <button type="button" class="bar-action-btn" id="barTechDuplicateBtn" style="flex:1;">Duplicar objeto</button>
            <button type="button" class="bar-action-btn bar-action-danger" id="barTechDeleteBtn" style="flex:1;">Eliminar objeto</button>
          </div>
        </div>
      </div>
    </div>`;

  // Inserción en el DOM justo después del título del inspector
  const contentDetails = document.getElementById('fieldContentDetails');
  if (contentDetails) {
    contentDetails.insertAdjacentElement('beforebegin', barInspectorWrap);
  } else {
    fieldSection.querySelector('.inspector-title')?.insertAdjacentElement('afterend', barInspectorWrap);
  }

  // --- Elementos interactivos del nuevo inspector BAR ---
  const barDesignSubTabs = document.getElementById('barDesignSubTabs');
  const barPanelDesign = document.getElementById('barPanelDesign');
  const barPanelPlc = document.getElementById('barPanelPlc');
  const barPanelTech = document.getElementById('barPanelTech');
  const barSubpanelShape = document.getElementById('barSubpanelShape');
  const barSubpanelContent = document.getElementById('barSubpanelContent');
  const barSubpanelAppearance = document.getElementById('barSubpanelAppearance');
  const barSubpanelPlacement = document.getElementById('barSubpanelPlacement');
  const barLinearShapeGroup = document.getElementById('barLinearShapeGroup');
  const barCircularShapeGroup = document.getElementById('barCircularShapeGroup');
  const barTopSimSlider = document.getElementById('barTopSimSlider');
  const barTopSimReadout = document.getElementById('barTopSimReadout');

  // Forma (Barra)
  const barOrientButtonsWrap = document.getElementById('barOrientButtonsWrap');
  const barFormaWidthMode = document.getElementById('barFormaWidthMode');
  const barFormaFillMode = document.getElementById('barFormaFillMode');
  const barFormaWidth = document.getElementById('barFormaWidth');
  const barFormaHeight = document.getElementById('barFormaHeight');
  const barFormaWidthName = document.getElementById('barFormaWidthName');
  const barFormaHeightName = document.getElementById('barFormaHeightName');
  const barLinearStyleWrap = document.getElementById('barLinearStyleWrap');
  const barLinearRadiusWrap = document.getElementById('barLinearRadiusWrap');
  const barFormaBorderEnabled = document.getElementById('barFormaBorderEnabled');
  const barFormaBorderColorWrap = document.getElementById('barFormaBorderColorWrap');
  const barFormaBorderColorNative = document.getElementById('barFormaBorderColorNative');
  const barFormaBorderColorHex = document.getElementById('barFormaBorderColorHex');
  const barFormaBorderWidthWrap = document.getElementById('barFormaBorderWidthWrap');
  const barFormaBorderWidth = document.getElementById('barFormaBorderWidth');

  // Forma (Circular)
  const barCircularArcWrap = document.getElementById('barCircularArcWrap');
  const barCircularDirWrap = document.getElementById('barCircularDirWrap');
  const barCircularQuadrantWrap = document.getElementById('barCircularQuadrantWrap');
  const barCircularStyleWrap = document.getElementById('barCircularStyleWrap');
  const barFormaCircleRadius = document.getElementById('barFormaCircleRadius');
  const barFormaCircleThickness = document.getElementById('barFormaCircleThickness');
  const barFormaCircleColorMode = document.getElementById('barFormaCircleColorMode');

  // Contenido
  const barContentLabel = document.getElementById('barContentLabel');
  const barContentUnit = document.getElementById('barContentUnit');
  const barContentShowLabel = document.getElementById('barContentShowLabel');
  const barContentAlignRow = document.getElementById('barContentAlignRow');
  const barContentMatrixWrap = document.getElementById('barContentMatrixWrap');
  const barContentLabelSize = document.getElementById('barContentLabelSize');
  const barContentLabelSizeWrap = document.getElementById('barContentLabelSizeWrap');
  const barContentLinearWrap = document.getElementById('barContentLinearWrap');
  const barContentLinearPctPos = document.getElementById('barContentLinearPctPos');
  const barContentCircularWrap = document.getElementById('barContentCircularWrap');
  const barContentCircleCenterMode = document.getElementById('barContentCircleCenterMode');
  const barContentCircleTextSizeWrap = document.getElementById('barContentCircleTextSizeWrap');
  const barContentCircleTextSize = document.getElementById('barContentCircleTextSize');
  const barContentCircleSizeHint = document.getElementById('barContentCircleSizeHint');

  // Apariencia (Lineal)
  const barAppLinearFillWrap = document.getElementById('barAppLinearFillWrap');
  const barAppLinearSolidWrap = document.getElementById('barAppLinearSolidWrap');
  const barAppLinearColorNative = document.getElementById('barAppLinearColorNative');
  const barAppLinearColorHex = document.getElementById('barAppLinearColorHex');
  const barAppLinearGradWrap = document.getElementById('barAppLinearGradWrap');
  const barAppLinearGradStartNative = document.getElementById('barAppLinearGradStartNative');
  const barAppLinearGradStartHex = document.getElementById('barAppLinearGradStartHex');
  const barAppLinearGradEndNative = document.getElementById('barAppLinearGradEndNative');
  const barAppLinearGradEndHex = document.getElementById('barAppLinearGradEndHex');
  const barAppLinearSwapGradBtn = document.getElementById('barAppLinearSwapGradBtn');
  const barAppLinearTrackNative = document.getElementById('barAppLinearTrackNative');
  const barAppLinearTrackHex = document.getElementById('barAppLinearTrackHex');
  const barAppLinearPctColWrap = document.getElementById('barAppLinearPctColWrap');
  const barAppLinearPctColNative = document.getElementById('barAppLinearPctColNative');
  const barAppLinearPctColHex = document.getElementById('barAppLinearPctColHex');

  // Apariencia (Circular)
  const barAppCircularFillWrap = document.getElementById('barAppCircularFillWrap');
  const barAppCircularSolidWrap = document.getElementById('barAppCircularSolidWrap');
  const barAppCircularColorNative = document.getElementById('barAppCircularColorNative');
  const barAppCircularColorHex = document.getElementById('barAppCircularColorHex');
  const barAppCircularGradWrap = document.getElementById('barAppCircularGradWrap');
  const barAppCircularGradStartNative = document.getElementById('barAppCircularGradStartNative');
  const barAppCircularGradStartHex = document.getElementById('barAppCircularGradStartHex');
  const barAppCircularGradEndNative = document.getElementById('barAppCircularGradEndNative');
  const barAppCircularGradEndHex = document.getElementById('barAppCircularGradEndHex');
  const barAppCircularSwapGradBtn = document.getElementById('barAppCircularSwapGradBtn');
  const barAppCircularTrackNative = document.getElementById('barAppCircularTrackNative');
  const barAppCircularTrackHex = document.getElementById('barAppCircularTrackHex');
  const barAppCircularTextColWrap = document.getElementById('barAppCircularTextColWrap');
  const barAppCircularTextColNative = document.getElementById('barAppCircularTextColNative');
  const barAppCircularTextColHex = document.getElementById('barAppCircularTextColHex');

  // Apariencia (Común)
  const barAppLabelColWrap = document.getElementById('barAppLabelColWrap');
  const barAppLabelColNative = document.getElementById('barAppLabelColNative');
  const barAppLabelColHex = document.getElementById('barAppLabelColHex');
  const barAppBgColNative = document.getElementById('barAppBgColNative');
  const barAppBgColHex = document.getElementById('barAppBgColHex');

  // Ubicación
  const barPlacementPage = document.getElementById('barPlacementPage');
  const barPlacementX = document.getElementById('barPlacementX');
  const barPlacementY = document.getElementById('barPlacementY');
  const barPlacementBounds = document.getElementById('barPlacementBounds');
  const barPlacementValueBounds = document.getElementById('barPlacementValueBounds');

  // Datos PLC
  const barPlcVariable = document.getElementById('barPlcVariable');
  const barCopyVarBtn = document.getElementById('barCopyVarBtn');
  const barPlcCppType = document.getElementById('barPlcCppType');
  const barPlcMin = document.getElementById('barPlcMin');
  const barPlcMax = document.getElementById('barPlcMax');
  const barPlcUnit = document.getElementById('barPlcUnit');
  const barPlcSimToggle = document.getElementById('barPlcSimToggle');
  const barPlcSimReadout = document.getElementById('barPlcSimReadout');
  const barPlcSimSlider = document.getElementById('barPlcSimSlider');
  const barPlcSimMinLabel = document.getElementById('barPlcSimMinLabel');
  const barPlcSimMaxLabel = document.getElementById('barPlcSimMaxLabel');

  // Técnico
  const barTechName = document.getElementById('barTechName');
  const barTechId = document.getElementById('barTechId');
  const barCopyIdBtn = document.getElementById('barCopyIdBtn');
  const barTechPage = document.getElementById('barTechPage');
  const barTechContractCode = document.getElementById('barTechContractCode');
  const barTechCopyContractBtn = document.getElementById('barTechCopyContractBtn');
  const barDiagSimVal = document.getElementById('barDiagSimVal');
  const barDiagUnitVal = document.getElementById('barDiagUnitVal');
  const barDiagRange = document.getElementById('barDiagRange');
  const barDiagX = document.getElementById('barDiagX');
  const barDiagY = document.getElementById('barDiagY');
  const barDiagW = document.getElementById('barDiagW');
  const barDiagH = document.getElementById('barDiagH');
  const barDiagPad = document.getElementById('barDiagPad');
  const barDiagValRegion = document.getElementById('barDiagValRegion');
  const barTechDuplicateBtn = document.getElementById('barTechDuplicateBtn');
  const barTechDeleteBtn = document.getElementById('barTechDeleteBtn');

  const alignMatrixCells = barInspectorWrap.querySelectorAll('.align-matrix-3x3 .matrix-cell');

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
    if (typeof field.barStyle !== 'string') field.barStyle = 'SOLID';
    if (field.barCornerRadius === undefined || field.barCornerRadius === null) field.barCornerRadius = 0;
    if (typeof field.barBorderEnabled !== 'boolean') field.barBorderEnabled = false;
    if (!Number.isFinite(Number(field.barBorderColor))) field.barBorderColor = 0xFFFF;
    if (!Number.isFinite(Number(field.barBorderWidth)) || field.barBorderWidth < 1) field.barBorderWidth = 1;

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

  function traceRoundedRect(ctx, x, y, w, h, r) {
    if (w <= 0 || h <= 0) return;
    const radius = Math.max(0, Math.min(r, Math.floor(Math.min(w, h) / 2)));
    if (radius <= 0) {
      ctx.rect(x, y, w, h);
    } else if (typeof ctx.roundRect === 'function') {
      ctx.roundRect(x, y, w, h, radius);
    } else {
      ctx.moveTo(x + radius, y);
      ctx.lineTo(x + w - radius, y);
      ctx.quadraticCurveTo(x + w, y, x + w, y + radius);
      ctx.lineTo(x + w, y + h - radius);
      ctx.quadraticCurveTo(x + w, y + h, x + w - radius, y + h);
      ctx.lineTo(x + radius, y + h);
      ctx.quadraticCurveTo(x, y + h, x, y + h - radius);
      ctx.lineTo(x, y + radius);
      ctx.quadraticCurveTo(x, y, x + radius, y);
      ctx.closePath();
    }
  }

  function drawSolidBorder(ctx, x, y, w, h, r, bThick, colorCss, scale) {
    const t = Math.max(1, Math.trunc(Number(bThick) || 1)) * scale;
    const X = Math.round(x * scale);
    const Y = Math.round(y * scale);
    const W = Math.round(w * scale);
    const H = Math.round(h * scale);
    const R = Math.round(r * scale);

    ctx.fillStyle = colorCss;

    if (R <= 0) {
      // 100% Píxeles sólidos puros. Cero difuminado
      ctx.fillRect(X, Y, W, t);
      ctx.fillRect(X, Y + H - t, W, t);
      ctx.fillRect(X, Y, t, H);
      ctx.fillRect(X + W - t, Y, t, H);
    } else {
      // Contorno perimetral redondeado 100% sólido con regla evenodd
      ctx.save();
      ctx.beginPath();
      traceRoundedRect(ctx, X, Y, W, H, R);
      const innerW = W - 2 * t;
      const innerH = H - 2 * t;
      if (innerW > 0 && innerH > 0) {
        const innerR = Math.max(0, R - t);
        traceRoundedRect(ctx, X + t, Y + t, innerW, innerH, innerR);
        ctx.fill('evenodd');
      } else {
        ctx.fill();
      }
      ctx.restore();
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
      // BARRAS LINEALES (Sólida, Diagonales, Bloques Win XP, Puntos LED)
      const isGrad = (field.barFillMode === 'GRADIENT' || field.barPreset === 'GRADIENT');
      const pctPos = field.linearPctPos || 'RIGHT';
      const barStyle = field.barStyle || 'SOLID';

      // Cálculo de radio de esquinas
      const minDim = Math.min(g.valueW, g.valueH);
      let radPx = 0;
      if (field.barCornerRadius === 'FULL') {
        radPx = Math.floor(minDim / 2);
      } else {
        radPx = Math.min(Number(field.barCornerRadius) || 0, Math.floor(minDim / 2));
      }

      // 1. Pista vacía de fondo de la barra
      ctx.fillStyle = rgb565ToCss(field.trackColor || 0x18C3);
      if (radPx <= 0) {
        ctx.fillRect(g.valueX * scale, g.valueY * scale, g.valueW * scale, g.valueH * scale);
      } else {
        ctx.beginPath();
        traceRoundedRect(ctx, g.valueX * scale, g.valueY * scale, g.valueW * scale, g.valueH * scale, radPx * scale);
        ctx.fill();
      }

      // 2. Relleno activo según estilo
      if (field.barOrientation === 'VERTICAL') {
        const fillH = Math.round(norm * g.valueH);
        if (fillH > 0) {
          const activeCol565 = field.valueColor || 0x07FF;
          const endCol565 = field.valueColorEnd || 0x07E0;
          const activeCss = rgb565ToCss(activeCol565);

          if (barStyle === 'SOLID') {
            if (isGrad) {
              const grad = ctx.createLinearGradient(0, (g.valueY + g.valueH) * scale, 0, g.valueY * scale);
              grad.addColorStop(0, rgb565ToCss(activeCol565));
              grad.addColorStop(1, rgb565ToCss(endCol565));
              ctx.fillStyle = grad;
            } else {
              ctx.fillStyle = activeCss;
            }
            if (radPx <= 0) {
              ctx.fillRect(g.valueX * scale, (g.valueY + g.valueH - fillH) * scale, g.valueW * scale, fillH * scale);
            } else {
              ctx.save();
              ctx.beginPath();
              traceRoundedRect(ctx, g.valueX * scale, g.valueY * scale, g.valueW * scale, g.valueH * scale, radPx * scale);
              ctx.clip();
              ctx.fillRect(g.valueX * scale, (g.valueY + g.valueH - fillH) * scale, g.valueW * scale, fillH * scale);
              ctx.restore();
            }
          } else if (barStyle === 'STRIPES') {
            ctx.save();
            ctx.beginPath();
            traceRoundedRect(ctx, g.valueX * scale, g.valueY * scale, g.valueW * scale, g.valueH * scale, radPx * scale);
            ctx.clip();
            ctx.beginPath();
            ctx.rect(g.valueX * scale, (g.valueY + g.valueH - fillH) * scale, g.valueW * scale, fillH * scale);
            ctx.clip();
            const step = Math.max(4, Math.round(7 * scale));
            ctx.lineWidth = Math.max(1, Math.round(2 * scale));
            const startY = (g.valueY + g.valueH) * scale + g.valueW * scale;
            const endY = (g.valueY + g.valueH - fillH) * scale - g.valueW * scale;
            for (let sy = startY; sy >= endY; sy -= step) {
              const prop = Math.max(0, Math.min(1, ((g.valueY + g.valueH) * scale - sy) / (g.valueH * scale)));
              const col = isGrad ? interpolateColor565(activeCol565, endCol565, prop) : activeCol565;
              ctx.strokeStyle = rgb565ToCss(col);
              ctx.beginPath();
              ctx.moveTo(g.valueX * scale, sy);
              ctx.lineTo((g.valueX + g.valueW) * scale, sy - g.valueW * scale);
              ctx.stroke();
            }
            ctx.restore();
          } else if (barStyle === 'BLOCKS') {
            ctx.save();
            ctx.beginPath();
            traceRoundedRect(ctx, g.valueX * scale, g.valueY * scale, g.valueW * scale, g.valueH * scale, radPx * scale);
            ctx.clip();
            const blockH = Math.max(3, Math.round(6 * scale));
            const gap = Math.max(1, Math.round(2 * scale));
            const totalBlocks = Math.max(1, Math.floor((g.valueH * scale + gap) / (blockH + gap)));
            const activeBlocks = Math.round(norm * totalBlocks);
            for (let i = 0; i < activeBlocks; i++) {
              const by = (g.valueY + g.valueH) * scale - (i + 1) * (blockH + gap) + gap;
              const prop = i / Math.max(1, totalBlocks - 1);
              const col = isGrad ? interpolateColor565(activeCol565, endCol565, prop) : activeCol565;
              ctx.fillStyle = rgb565ToCss(col);
              ctx.fillRect(g.valueX * scale, by, g.valueW * scale, blockH);
            }
            ctx.restore();
          } else if (barStyle === 'DOTS') {
            ctx.save();
            ctx.beginPath();
            traceRoundedRect(ctx, g.valueX * scale, g.valueY * scale, g.valueW * scale, g.valueH * scale, radPx * scale);
            ctx.clip();
            const dotR = Math.max(1.5, Math.min(g.valueW * scale / 2 - 1.5 * scale, 3.5 * scale));
            const step = dotR * 2 + Math.max(2, Math.round(3 * scale));
            const totalDots = Math.max(1, Math.floor((g.valueH * scale - 2 * scale) / step));
            const activeDots = Math.round(norm * totalDots);
            const cx = (g.valueX + g.valueW / 2) * scale;
            for (let i = 0; i < totalDots; i++) {
              const cy = (g.valueY + g.valueH) * scale - 1.5 * scale - dotR - i * step;
              const isActive = (i < activeDots);
              if (isActive) {
                const prop = i / Math.max(1, totalDots - 1);
                const col = isGrad ? interpolateColor565(activeCol565, endCol565, prop) : activeCol565;
                ctx.fillStyle = rgb565ToCss(col);
              } else {
                ctx.fillStyle = rgb565ToCss(field.trackColor || 0x18C3);
              }
              ctx.beginPath();
              ctx.arc(cx, cy, dotR, 0, 2 * Math.PI);
              ctx.fill();
            }
            ctx.restore();
          }
        }

        // Borde exclusivo perimetral de la barra (Sólido 100%, cero difuminado)
        if (field.barBorderEnabled) {
          const bCol = Number.isFinite(Number(field.barBorderColor)) ? Number(field.barBorderColor) : 0xFFFF;
          const bColorCss = rgb565ToCss(bCol);
          const bWidth = Math.max(1, Math.trunc(Number(field.barBorderWidth) || 1));
          drawSolidBorder(ctx, g.valueX, g.valueY, g.valueW, g.valueH, radPx, bWidth, bColorCss, scale);
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
          const activeCol565 = field.valueColor || 0x07FF;
          const endCol565 = field.valueColorEnd || 0x07E0;
          const activeCss = rgb565ToCss(activeCol565);

          if (barStyle === 'SOLID') {
            if (isGrad) {
              const grad = ctx.createLinearGradient(g.valueX * scale, 0, (g.valueX + g.valueW) * scale, 0);
              grad.addColorStop(0, rgb565ToCss(activeCol565));
              grad.addColorStop(1, rgb565ToCss(endCol565));
              ctx.fillStyle = grad;
            } else {
              ctx.fillStyle = activeCss;
            }
            if (radPx <= 0) {
              ctx.fillRect(g.valueX * scale, g.valueY * scale, fillW * scale, g.valueH * scale);
            } else {
              ctx.save();
              ctx.beginPath();
              traceRoundedRect(ctx, g.valueX * scale, g.valueY * scale, g.valueW * scale, g.valueH * scale, radPx * scale);
              ctx.clip();
              ctx.fillRect(g.valueX * scale, g.valueY * scale, fillW * scale, g.valueH * scale);
              ctx.restore();
            }
          } else if (barStyle === 'STRIPES') {
            ctx.save();
            traceRoundedRect(ctx, g.valueX * scale, g.valueY * scale, g.valueW * scale, g.valueH * scale, radPx * scale);
            ctx.clip();
            ctx.beginPath();
            ctx.rect(g.valueX * scale, g.valueY * scale, fillW * scale, g.valueH * scale);
            ctx.clip();
            const step = Math.max(4, Math.round(7 * scale));
            ctx.lineWidth = Math.max(1, Math.round(2 * scale));
            const startX = g.valueX * scale - g.valueH * scale;
            const endX = (g.valueX + fillW) * scale + g.valueH * scale;
            for (let sx = startX; sx < endX; sx += step) {
              const prop = Math.max(0, Math.min(1, (sx - g.valueX * scale) / (g.valueW * scale)));
              const col = isGrad ? interpolateColor565(activeCol565, endCol565, prop) : activeCol565;
              ctx.strokeStyle = rgb565ToCss(col);
              ctx.beginPath();
              ctx.moveTo(sx, (g.valueY + g.valueH) * scale);
              ctx.lineTo(sx + g.valueH * scale, g.valueY * scale);
              ctx.stroke();
            }
            ctx.restore();
          } else if (barStyle === 'BLOCKS') {
            ctx.save();
            traceRoundedRect(ctx, g.valueX * scale, g.valueY * scale, g.valueW * scale, g.valueH * scale, radPx * scale);
            ctx.clip();
            const blockW = Math.max(3, Math.round(6 * scale));
            const gap = Math.max(1, Math.round(2 * scale));
            const totalBlocks = Math.max(1, Math.floor((g.valueW * scale + gap) / (blockW + gap)));
            const activeBlocks = Math.round(norm * totalBlocks);
            for (let i = 0; i < activeBlocks; i++) {
              const bx = g.valueX * scale + i * (blockW + gap);
              if (bx + blockW > (g.valueX + g.valueW) * scale) break;
              const prop = i / Math.max(1, totalBlocks - 1);
              const col = isGrad ? interpolateColor565(activeCol565, endCol565, prop) : activeCol565;
              ctx.fillStyle = rgb565ToCss(col);
              ctx.fillRect(bx, g.valueY * scale, blockW, g.valueH * scale);
            }
            ctx.restore();
          } else if (barStyle === 'DOTS') {
            ctx.save();
            traceRoundedRect(ctx, g.valueX * scale, g.valueY * scale, g.valueW * scale, g.valueH * scale, radPx * scale);
            ctx.clip();
            const dotR = Math.max(1.5, Math.min(g.valueH * scale / 2 - 1.5 * scale, 3.5 * scale));
            const step = dotR * 2 + Math.max(2, Math.round(3 * scale));
            const totalDots = Math.max(1, Math.floor((g.valueW * scale - 2 * scale) / step));
            const activeDots = Math.round(norm * totalDots);
            const cy = (g.valueY + g.valueH / 2) * scale;
            for (let i = 0; i < totalDots; i++) {
              const cx = g.valueX * scale + 1.5 * scale + dotR + i * step;
              if (cx + dotR > (g.valueX + g.valueW) * scale) break;
              const isActive = (i < activeDots);
              if (isActive) {
                const prop = i / Math.max(1, totalDots - 1);
                const col = isGrad ? interpolateColor565(activeCol565, endCol565, prop) : activeCol565;
                ctx.fillStyle = rgb565ToCss(col);
              } else {
                ctx.fillStyle = rgb565ToCss(field.trackColor || 0x18C3);
              }
              ctx.beginPath();
              ctx.arc(cx, cy, dotR, 0, 2 * Math.PI);
              ctx.fill();
            }
            ctx.restore();
          }
        }

        // Borde exclusivo perimetral de la barra (Sólido 100%, cero difuminado)
        if (field.barBorderEnabled) {
          const bCol = Number.isFinite(Number(field.barBorderColor)) ? Number(field.barBorderColor) : 0xFFFF;
          const bColorCss = rgb565ToCss(bCol);
          const bWidth = Math.max(1, Math.trunc(Number(field.barBorderWidth) || 1));
          drawSolidBorder(ctx, g.valueX, g.valueY, g.valueW, g.valueH, radPx, bWidth, bColorCss, scale);
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

  // --- Pestañas y Navegación ---
  let currentMainTab = 'design';
  let currentSubTab = 'shape';

  function switchMainTab(tab) {
    currentMainTab = tab;
    document.querySelectorAll('.bar-main-tab').forEach((b) => {
      const isAct = b.dataset.mainTab === tab;
      b.classList.toggle('active', isAct);
      b.setAttribute('aria-selected', isAct ? 'true' : 'false');
    });

    const isDesign = (tab === 'design');
    if (barDesignSubTabs) barDesignSubTabs.style.display = isDesign ? 'flex' : 'none';
    if (barPanelDesign) barPanelDesign.style.display = isDesign ? 'block' : 'none';
    if (barPanelPlc) barPanelPlc.style.display = (tab === 'plc') ? 'block' : 'none';
    if (barPanelTech) barPanelTech.style.display = (tab === 'tech') ? 'block' : 'none';
  }

  function switchSubTab(subTab) {
    currentSubTab = subTab;
    document.querySelectorAll('.bar-sub-tab').forEach((b) => {
      const isAct = b.dataset.subTab === subTab;
      b.classList.toggle('active', isAct);
      b.setAttribute('aria-selected', isAct ? 'true' : 'false');
    });

    if (barSubpanelShape) barSubpanelShape.style.display = (subTab === 'shape') ? 'block' : 'none';
    if (barSubpanelContent) barSubpanelContent.style.display = (subTab === 'content') ? 'block' : 'none';
    if (barSubpanelAppearance) barSubpanelAppearance.style.display = (subTab === 'appearance') ? 'block' : 'none';
    if (barSubpanelPlacement) barSubpanelPlacement.style.display = (subTab === 'placement') ? 'block' : 'none';
  }

  document.querySelectorAll('.bar-main-tab').forEach((btn) => {
    btn.addEventListener('click', (e) => {
      e.preventDefault();
      switchMainTab(btn.dataset.mainTab);
    });
  });

  document.querySelectorAll('.bar-sub-tab').forEach((btn) => {
    btn.addEventListener('click', (e) => {
      e.preventDefault();
      switchSubTab(btn.dataset.subTab);
    });
  });

  // Función utilitaria para copiar al portapapeles con confirmación visual (✓)
  function copyToClipboard(text, button) {
    if (!text) return;
    const onSuccess = () => {
      if (button) {
        const orig = button.textContent;
        button.textContent = '✓';
        setTimeout(() => { button.textContent = orig; }, 1200);
      }
    };
    if (navigator.clipboard && navigator.clipboard.writeText) {
      navigator.clipboard.writeText(text).then(onSuccess).catch(() => {
        fallbackCopy(text, onSuccess);
      });
    } else {
      fallbackCopy(text, onSuccess);
    }
  }

  function fallbackCopy(text, cb) {
    try {
      const ta = document.createElement('textarea');
      ta.value = text;
      ta.style.position = 'fixed';
      ta.style.opacity = '0';
      document.body.appendChild(ta);
      ta.select();
      document.execCommand('copy');
      document.body.removeChild(ta);
      if (cb) cb();
    } catch {}
  }

  barCopyVarBtn?.addEventListener('click', () => copyToClipboard(barPlcVariable?.value || '', barCopyVarBtn));
  barCopyIdBtn?.addEventListener('click', () => copyToClipboard(barTechId?.value || '', barCopyIdBtn));
  barTechCopyContractBtn?.addEventListener('click', () => {
    const code = barTechContractCode?.textContent?.split('//')[0]?.trim() || barTechContractCode?.textContent || '';
    copyToClipboard(code, barTechCopyContractBtn);
  });

  // Acciones avanzadas (Técnico)
  barTechDuplicateBtn?.addEventListener('click', () => {
    const ed = editor();
    if (ed?.duplicateSelectedField) ed.duplicateSelectedField();
    else document.getElementById('duplicateObjectButton')?.click();
  });

  barTechDeleteBtn?.addEventListener('click', () => {
    const ed = editor();
    if (ed?.deleteSelectedField) ed.deleteSelectedField();
    else document.getElementById('deleteObjectButton')?.click();
  });

  function updateBarFromControls(commit = false) {
    const field = selectedField();
    if (!isBar(field)) return;

    if (barPlcMin) field.barMin = Number(barPlcMin.value) || 0;
    if (barPlcMax) field.barMax = Number(barPlcMax.value) || 100;
    if (barTopSimSlider) {
      barTopSimSlider.min = String(field.barMin);
      barTopSimSlider.max = String(field.barMax);
    }
    if (barPlcSimSlider) {
      barPlcSimSlider.min = String(field.barMin);
      barPlcSimSlider.max = String(field.barMax);
    }
    if (barPlcSimMinLabel) barPlcSimMinLabel.textContent = String(field.barMin);
    if (barPlcSimMaxLabel) barPlcSimMaxLabel.textContent = String(field.barMax);

    if (barContentShowLabel) field.showLabel = barContentShowLabel.checked;

    if (field.barPreset === 'CIRCULAR') {
      if (barFormaCircleRadius) field.circleRadius = Math.max(10, Math.min(80, Number(barFormaCircleRadius.value) || 26));
      if (barFormaCircleThickness) field.circleThickness = Math.max(2, Math.min(25, Number(barFormaCircleThickness.value) || 5));
      if (barFormaCircleColorMode) field.circleColorMode = barFormaCircleColorMode.value || 'SOLID';
      if (field.circleColorMode === 'GRADIENT') {
        if (barAppCircularGradStartNative) {
          field.valueColor = hexCssTo565(barAppCircularGradStartNative.value);
          if (barAppCircularGradStartHex) barAppCircularGradStartHex.textContent = hex565(field.valueColor);
        }
        if (barAppCircularGradEndNative) {
          field.valueColorEnd = hexCssTo565(barAppCircularGradEndNative.value);
          if (barAppCircularGradEndHex) barAppCircularGradEndHex.textContent = hex565(field.valueColorEnd);
        }
      } else {
        if (barAppCircularColorNative) {
          field.valueColor = hexCssTo565(barAppCircularColorNative.value);
          if (barAppCircularColorHex) barAppCircularColorHex.textContent = hex565(field.valueColor);
        }
      }
      if (barAppCircularTrackNative) {
        field.circleTrackColor = hexCssTo565(barAppCircularTrackNative.value);
        if (barAppCircularTrackHex) barAppCircularTrackHex.textContent = hex565(field.circleTrackColor);
      }
      if (barContentCircleCenterMode) {
        field.circleCenterMode = barContentCircleCenterMode.value || 'PERCENT';
        field.circleShowPercent = (field.circleCenterMode !== 'NONE');
      }
      if (barContentCircleTextSize) {
        field.circleTextSize = Number(barContentCircleTextSize.value) || 1;
      }
      if (barAppCircularTextColNative) {
        field.circleTextColor = hexCssTo565(barAppCircularTextColNative.value);
        if (barAppCircularTextColHex) barAppCircularTextColHex.textContent = hex565(field.circleTextColor);
      }
    } else {
      // Linear
      if (barFormaWidthMode) field.barAutoWidth = (barFormaWidthMode.value === 'AUTO');
      const valLength = Math.max(10, Math.min(WIDTH, Number(barFormaWidth?.value) || 110));
      const valThick = Math.max(4, Math.min(HEIGHT, Number(barFormaHeight?.value) || 12));
      if (field.barOrientation === 'VERTICAL') {
        field.barHeight = valLength;
        field.barWidth = valThick;
      } else {
        field.barWidth = valLength;
        field.barHeight = valThick;
      }

      if (barFormaFillMode) field.barFillMode = barFormaFillMode.value || 'SOLID';
      if (field.barFillMode === 'GRADIENT') {
        if (barAppLinearGradStartNative) {
          field.valueColor = hexCssTo565(barAppLinearGradStartNative.value);
          if (barAppLinearGradStartHex) barAppLinearGradStartHex.textContent = hex565(field.valueColor);
        }
        if (barAppLinearGradEndNative) {
          field.valueColorEnd = hexCssTo565(barAppLinearGradEndNative.value);
          if (barAppLinearGradEndHex) barAppLinearGradEndHex.textContent = hex565(field.valueColorEnd);
        }
      } else {
        if (barAppLinearColorNative) {
          field.valueColor = hexCssTo565(barAppLinearColorNative.value);
          if (barAppLinearColorHex) barAppLinearColorHex.textContent = hex565(field.valueColor);
        }
      }

      if (barAppLinearTrackNative) {
        field.trackColor = hexCssTo565(barAppLinearTrackNative.value);
        if (barAppLinearTrackHex) barAppLinearTrackHex.textContent = hex565(field.trackColor);
      }

      if (barContentLinearPctPos) {
        field.linearPctPos = barContentLinearPctPos.value || 'NONE';
      }
      if (barAppLinearPctColNative) {
        field.linearPctColor = hexCssTo565(barAppLinearPctColNative.value);
        if (barAppLinearPctColHex) barAppLinearPctColHex.textContent = hex565(field.linearPctColor);
      }
    }

    if (barAppLabelColNative) {
      field.labelColor = hexCssTo565(barAppLabelColNative.value);
      if (barAppLabelColHex) barAppLabelColHex.textContent = hex565(field.labelColor);
    }
    if (barAppBgColNative) {
      field.backgroundColor = hexCssTo565(barAppBgColNative.value);
      if (barAppBgColHex) barAppBgColHex.textContent = hex565(field.backgroundColor);
    }

    const norm = normalizedFor(field);
    const pctText = `${(norm * 100).toFixed(1)} %`;
    if (barTopSimReadout) barTopSimReadout.textContent = pctText;
    if (barPlcSimReadout) barPlcSimReadout.textContent = pctText;
    if (barDiagSimVal) barDiagSimVal.textContent = pctText;
    if (barDiagUnitVal) barDiagUnitVal.textContent = `${Number(field.barValue || 0).toFixed(1)} ${field.barUnit || ''}`.trim();
    if (barDiagRange) barDiagRange.textContent = `${field.barMin} — ${field.barMax} ${field.barUnit || ''}`.trim();

    const g = barGeometry(field);
    if (barPlacementBounds) barPlacementBounds.textContent = `${g.fieldW} × ${g.fieldH} px`;
    if (barPlacementValueBounds) barPlacementValueBounds.textContent = `${g.valueW} × ${g.valueH} px`;
    if (barDiagW) barDiagW.textContent = `${g.fieldW} px`;
    if (barDiagH) barDiagH.textContent = `${g.fieldH} px`;
    if (barDiagValRegion) barDiagValRegion.textContent = `${g.valueX}, ${g.valueY} (${g.valueW} × ${g.valueH} px)`;

    triggerCoreRender(commit);
  }

  // --- Simulación local (Debajo de Tipo de indicador y en Datos PLC) ---
  function syncSimulationControls(val, commit = false) {
    const field = selectedField();
    if (!isBar(field)) return;
    field.barValue = val;
    if (barTopSimSlider) barTopSimSlider.value = String(val);
    if (barPlcSimSlider) barPlcSimSlider.value = String(val);

    const norm = normalizedFor(field);
    const pctText = `${(norm * 100).toFixed(1)} %`;
    if (barTopSimReadout) barTopSimReadout.textContent = pctText;
    if (barPlcSimReadout) barPlcSimReadout.textContent = pctText;
    if (barDiagSimVal) barDiagSimVal.textContent = pctText;
    if (barDiagUnitVal) barDiagUnitVal.textContent = `${val.toFixed(1)} ${field.barUnit || ''}`.trim();

    triggerCoreRender(commit);
  }

  function setupSliderInteraction(slider) {
    if (!slider) return;
    let isDragging = false;

    function updateFromPointer(e) {
      if (slider.disabled) return;
      const rect = slider.getBoundingClientRect();
      if (rect.width <= 0) return;
      const clampedX = Math.max(0, Math.min(rect.width, e.clientX - rect.left));
      const ratio = clampedX / rect.width;
      const min = Number(slider.min) || 0;
      const max = Number(slider.max) || 100;
      const step = Number(slider.step) || 0.5;
      let val = min + ratio * (max - min);
      if (step > 0) val = Math.round(val / step) * step;
      val = Math.max(min, Math.min(max, val));
      syncSimulationControls(val, false);
    }

    slider.addEventListener('pointerdown', (e) => {
      if (e.button !== 0 || slider.disabled) return;
      isDragging = true;
      try { slider.setPointerCapture(e.pointerId); } catch {}
      updateFromPointer(e);
    });

    slider.addEventListener('pointermove', (e) => {
      if (!isDragging) return;
      updateFromPointer(e);
    });

    const stopDrag = (e) => {
      if (!isDragging) return;
      isDragging = false;
      try {
        if (slider.hasPointerCapture(e.pointerId)) {
          slider.releasePointerCapture(e.pointerId);
        }
      } catch {}
      syncSimulationControls(Number(slider.value), true);
    };

    slider.addEventListener('pointerup', stopDrag);
    slider.addEventListener('pointercancel', stopDrag);

    slider.addEventListener('input', () => {
      if (isDragging) return;
      syncSimulationControls(Number(slider.value), false);
    });

    slider.addEventListener('change', () => {
      if (isDragging) return;
      syncSimulationControls(Number(slider.value), true);
    });
  }

  setupSliderInteraction(barTopSimSlider);
  setupSliderInteraction(barPlcSimSlider);

  barPlcSimToggle?.addEventListener('change', () => {
    const enabled = barPlcSimToggle.checked;
    if (barPlcSimSlider) {
      barPlcSimSlider.disabled = !enabled;
      barPlcSimSlider.style.opacity = enabled ? '1' : '0.4';
    }
    if (barTopSimSlider) {
      barTopSimSlider.disabled = !enabled;
      barTopSimSlider.style.opacity = enabled ? '1' : '0.4';
    }
  });

  // Forma inputs
  barFormaWidth?.addEventListener('input', () => {
    const field = selectedField();
    if (isBar(field)) {
      field.barAutoWidth = false;
      if (barFormaWidthMode) barFormaWidthMode.value = 'FIXED';
    }
    updateBarFromControls(false);
  });
  barFormaWidth?.addEventListener('change', () => {
    const field = selectedField();
    if (isBar(field)) {
      field.barAutoWidth = false;
      if (barFormaWidthMode) barFormaWidthMode.value = 'FIXED';
    }
    updateBarFromControls(true);
  });

  barFormaHeight?.addEventListener('input', () => updateBarFromControls(false));
  barFormaHeight?.addEventListener('change', () => updateBarFromControls(true));
  barFormaWidthMode?.addEventListener('change', () => updateBarFromControls(true));

  barFormaCircleRadius?.addEventListener('input', () => updateBarFromControls(false));
  barFormaCircleRadius?.addEventListener('change', () => updateBarFromControls(true));
  barFormaCircleThickness?.addEventListener('input', () => updateBarFromControls(false));
  barFormaCircleThickness?.addEventListener('change', () => updateBarFromControls(true));

  // Botones de orientación visual (Horizontal / Vertical)
  document.querySelectorAll('.bar-orient-btn').forEach((btn) => {
    btn.addEventListener('click', () => {
      const field = selectedField();
      if (!isBar(field)) return;
      const oldOrientation = field.barOrientation || 'HORIZONTAL';
      const newOrientation = btn.dataset.orient || 'HORIZONTAL';
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
  });

  // Modo de relleno lineal
  barFormaFillMode?.addEventListener('change', () => {
    const field = selectedField();
    if (!isBar(field)) return;
    field.barFillMode = barFormaFillMode.value;
    const isGrad = (field.barFillMode === 'GRADIENT');
    if (barAppLinearSolidWrap) barAppLinearSolidWrap.style.display = isGrad ? 'none' : 'block';
    if (barAppLinearGradWrap) barAppLinearGradWrap.style.display = isGrad ? 'grid' : 'none';
    if (isGrad && !field.valueColorEnd) field.valueColorEnd = 0x07E0;
    updateBarFromControls(true);
  });

  // Modo de color circular
  barFormaCircleColorMode?.addEventListener('change', () => {
    const field = selectedField();
    if (!isBar(field)) return;
    field.circleColorMode = barFormaCircleColorMode.value;
    const isGrad = (field.circleColorMode === 'GRADIENT');
    if (barAppCircularSolidWrap) barAppCircularSolidWrap.style.display = isGrad ? 'none' : 'block';
    if (barAppCircularGradWrap) barAppCircularGradWrap.style.display = isGrad ? 'grid' : 'none';
    if (isGrad && !field.valueColorEnd) field.valueColorEnd = 0x07E0;
    updateBarFromControls(true);
  });

  // Botones de Estilo de Barra Lineal
  document.querySelectorAll('.bar-linear-style-btn').forEach((btn) => {
    btn.addEventListener('click', () => {
      const field = selectedField();
      if (!isBar(field)) return;
      field.barStyle = btn.dataset.style || 'SOLID';
      document.querySelectorAll('.bar-linear-style-btn').forEach((b) => b.classList.toggle('active', b === btn));
      triggerCoreRender(true);
    });
  });

  // Botones de Redondeo de Esquinas
  document.querySelectorAll('.bar-radius-btn').forEach((btn) => {
    btn.addEventListener('click', () => {
      const field = selectedField();
      if (!isBar(field)) return;
      field.barCornerRadius = btn.dataset.radius === 'FULL' ? 'FULL' : (Number(btn.dataset.radius) || 0);
      document.querySelectorAll('.bar-radius-btn').forEach((b) => b.classList.toggle('active', b === btn));
      triggerCoreRender(true);
    });
  });

  // Borde de Barra
  barFormaBorderEnabled?.addEventListener('change', () => {
    const field = selectedField();
    if (!isBar(field)) return;
    field.barBorderEnabled = barFormaBorderEnabled.checked;
    if (barFormaBorderColorWrap) barFormaBorderColorWrap.style.display = field.barBorderEnabled ? 'flex' : 'none';
    if (barFormaBorderWidthWrap) barFormaBorderWidthWrap.style.display = field.barBorderEnabled ? 'flex' : 'none';
    triggerCoreRender(true);
  });

  barFormaBorderWidth?.addEventListener('input', () => {
    const field = selectedField();
    if (!isBar(field)) return;
    field.barBorderWidth = Math.max(1, Math.min(10, Math.trunc(Number(barFormaBorderWidth.value) || 1)));
    triggerCoreRender(false);
  });
  barFormaBorderWidth?.addEventListener('change', () => {
    const field = selectedField();
    if (!isBar(field)) return;
    field.barBorderWidth = Math.max(1, Math.min(10, Math.trunc(Number(barFormaBorderWidth.value) || 1)));
    triggerCoreRender(true);
  });

  barFormaBorderColorNative?.addEventListener('input', () => {
    const field = selectedField();
    if (!isBar(field)) return;
    field.barBorderColor = hexCssTo565(barFormaBorderColorNative.value);
    if (barFormaBorderColorHex) barFormaBorderColorHex.textContent = hex565(field.barBorderColor);
    triggerCoreRender(false);
  });
  barFormaBorderColorNative?.addEventListener('change', () => {
    const field = selectedField();
    if (!isBar(field)) return;
    field.barBorderColor = hexCssTo565(barFormaBorderColorNative.value);
    if (barFormaBorderColorHex) barFormaBorderColorHex.textContent = hex565(field.barBorderColor);
    triggerCoreRender(true);
  });

  // Botones circulares (Arco, Dirección, Cuadrante, Estilo de anillo)
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

  // Contenido
  barContentLabel?.addEventListener('input', () => {
    const field = selectedField();
    if (!isBar(field)) return;
    field.barLabel = barContentLabel.value;
    if (fieldLabel) fieldLabel.value = field.barLabel;
    triggerCoreRender(false);
  });
  barContentLabel?.addEventListener('change', () => {
    const field = selectedField();
    if (!isBar(field)) return;
    field.barLabel = barContentLabel.value;
    if (fieldLabel) fieldLabel.value = field.barLabel;
    triggerCoreRender(true);
  });

  barContentUnit?.addEventListener('input', () => {
    const field = selectedField();
    if (!isBar(field)) return;
    field.barUnit = barContentUnit.value;
    if (barPlcUnit) barPlcUnit.value = field.barUnit;
    if (fieldUnit) fieldUnit.value = field.barUnit;
    triggerCoreRender(false);
  });
  barContentUnit?.addEventListener('change', () => {
    const field = selectedField();
    if (!isBar(field)) return;
    field.barUnit = barContentUnit.value;
    if (barPlcUnit) barPlcUnit.value = field.barUnit;
    if (fieldUnit) fieldUnit.value = field.barUnit;
    triggerCoreRender(true);
  });

  barContentShowLabel?.addEventListener('change', () => {
    const field = selectedField();
    if (!isBar(field)) return;
    field.showLabel = barContentShowLabel.checked;
    patchInspector(field);
    triggerCoreRender(true);
  });

  barContentLabelSize?.addEventListener('change', () => {
    const field = selectedField();
    if (!isBar(field)) return;
    field.labelSize = Math.max(1, Math.trunc(Number(barContentLabelSize.value) || 1));
    if (fieldLabelSize) fieldLabelSize.value = String(field.labelSize);
    triggerCoreRender(true);
  });

  barContentLinearPctPos?.addEventListener('change', () => {
    const field = selectedField();
    if (!isBar(field)) return;
    field.linearPctPos = barContentLinearPctPos.value || 'NONE';
    patchInspector(field);
    triggerCoreRender(true);
  });

  barContentCircleCenterMode?.addEventListener('change', () => {
    const field = selectedField();
    if (!isBar(field)) return;
    field.circleCenterMode = barContentCircleCenterMode.value || 'PERCENT';
    field.circleShowPercent = (field.circleCenterMode !== 'NONE');
    patchInspector(field);
    triggerCoreRender(true);
  });

  barContentCircleTextSize?.addEventListener('change', () => {
    const field = selectedField();
    if (!isBar(field)) return;
    field.circleTextSize = Number(barContentCircleTextSize.value) || 1;
    triggerCoreRender(true);
  });

  // Matriz 3x3
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

  // Intercambio de colores de degradado
  barAppLinearSwapGradBtn?.addEventListener('click', () => {
    const field = selectedField();
    if (!isBar(field)) return;
    const temp = field.valueColor;
    field.valueColor = field.valueColorEnd || 0x07E0;
    field.valueColorEnd = temp || 0xF800;
    barAppLinearGradStartNative.value = rgb565ToHexCss(field.valueColor);
    barAppLinearGradEndNative.value = rgb565ToHexCss(field.valueColorEnd);
    barAppLinearGradStartHex.textContent = hex565(field.valueColor);
    barAppLinearGradEndHex.textContent = hex565(field.valueColorEnd);
    triggerCoreRender(true);
  });

  barAppCircularSwapGradBtn?.addEventListener('click', () => {
    const field = selectedField();
    if (!isBar(field)) return;
    const temp = field.valueColor;
    field.valueColor = field.valueColorEnd || 0x07E0;
    field.valueColorEnd = temp || 0xF800;
    barAppCircularGradStartNative.value = rgb565ToHexCss(field.valueColor);
    barAppCircularGradEndNative.value = rgb565ToHexCss(field.valueColorEnd);
    barAppCircularGradStartHex.textContent = hex565(field.valueColor);
    barAppCircularGradEndHex.textContent = hex565(field.valueColorEnd);
    triggerCoreRender(true);
  });

  // Color Pickers Nativo
  [
    [barAppLinearColorNative, 'valueColor', barAppLinearColorHex],
    [barAppLinearGradStartNative, 'valueColor', barAppLinearGradStartHex],
    [barAppLinearGradEndNative, 'valueColorEnd', barAppLinearGradEndHex],
    [barAppLinearTrackNative, 'trackColor', barAppLinearTrackHex],
    [barAppLinearPctColNative, 'linearPctColor', barAppLinearPctColHex],
    [barAppCircularColorNative, 'valueColor', barAppCircularColorHex],
    [barAppCircularGradStartNative, 'valueColor', barAppCircularGradStartHex],
    [barAppCircularGradEndNative, 'valueColorEnd', barAppCircularGradEndHex],
    [barAppCircularTrackNative, 'circleTrackColor', barAppCircularTrackHex],
    [barAppCircularTextColNative, 'circleTextColor', barAppCircularTextColHex],
    [barAppLabelColNative, 'labelColor', barAppLabelColHex],
    [barAppBgColNative, 'backgroundColor', barAppBgColHex]
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

  // Ubicación: X, Y
  barPlacementX?.addEventListener('input', () => {
    const field = selectedField();
    if (!isBar(field)) return;
    field.x = Math.max(0, Math.min(WIDTH - 1, Number(barPlacementX.value) || 0));
    if (fieldX) fieldX.value = String(field.x);
    triggerCoreRender(false);
  });
  barPlacementX?.addEventListener('change', () => {
    const field = selectedField();
    if (!isBar(field)) return;
    field.x = Math.max(0, Math.min(WIDTH - 1, Number(barPlacementX.value) || 0));
    if (fieldX) fieldX.value = String(field.x);
    triggerCoreRender(true);
  });
  barPlacementY?.addEventListener('input', () => {
    const field = selectedField();
    if (!isBar(field)) return;
    field.y = Math.max(0, Math.min(HEIGHT - 1, Number(barPlacementY.value) || 0));
    if (fieldY) fieldY.value = String(field.y);
    triggerCoreRender(false);
  });
  barPlacementY?.addEventListener('change', () => {
    const field = selectedField();
    if (!isBar(field)) return;
    field.y = Math.max(0, Math.min(HEIGHT - 1, Number(barPlacementY.value) || 0));
    if (fieldY) fieldY.value = String(field.y);
    triggerCoreRender(true);
  });

  // Datos PLC: Min, Max, Variable, Unit
  barPlcMin?.addEventListener('input', () => updateBarFromControls(false));
  barPlcMin?.addEventListener('change', () => updateBarFromControls(true));
  barPlcMax?.addEventListener('input', () => updateBarFromControls(false));
  barPlcMax?.addEventListener('change', () => updateBarFromControls(true));

  barPlcUnit?.addEventListener('input', () => {
    const field = selectedField();
    if (!isBar(field)) return;
    field.barUnit = barPlcUnit.value;
    if (barContentUnit) barContentUnit.value = field.barUnit;
    if (fieldUnit) fieldUnit.value = field.barUnit;
    triggerCoreRender(false);
  });
  barPlcUnit?.addEventListener('change', () => {
    const field = selectedField();
    if (!isBar(field)) return;
    field.barUnit = barPlcUnit.value;
    if (barContentUnit) barContentUnit.value = field.barUnit;
    if (fieldUnit) fieldUnit.value = field.barUnit;
    triggerCoreRender(true);
  });

  barPlcVariable?.addEventListener('input', () => {
    const field = selectedField();
    if (!isBar(field)) return;
    field.variable = barPlcVariable.value;
    if (fieldVariable) fieldVariable.value = field.variable;
    if (barTechContractCode) {
      const varName = sanitizeSymbol(field.variable, 'nivel');
      barTechContractCode.textContent = `float ${varName} = 0.0f; // Vía JWPLC_Display.setBar()`;
    }
  });
  barPlcVariable?.addEventListener('change', () => {
    const field = selectedField();
    if (!isBar(field)) return;
    field.variable = barPlcVariable.value;
    if (fieldVariable) {
      fieldVariable.value = field.variable;
      fieldVariable.dispatchEvent(new Event('change', { bubbles: true }));
    }
    patchGeneratedCode();
  });

  // Técnico: Name, ID
  barTechName?.addEventListener('input', () => {
    const field = selectedField();
    if (!isBar(field)) return;
    field.name = barTechName.value;
    if (fieldName) fieldName.value = field.name;
  });
  barTechName?.addEventListener('change', () => {
    const field = selectedField();
    if (!isBar(field)) return;
    field.name = barTechName.value;
    if (fieldName) {
      fieldName.value = field.name;
      fieldName.dispatchEvent(new Event('change', { bubbles: true }));
    }
    patchObjectList();
  });

  barTechId?.addEventListener('input', () => {
    const field = selectedField();
    if (!isBar(field)) return;
    field.id = barTechId.value;
    if (fieldId) fieldId.value = field.id;
  });
  barTechId?.addEventListener('change', () => {
    const field = selectedField();
    if (!isBar(field)) return;
    field.id = barTechId.value;
    if (fieldId) {
      fieldId.value = field.id;
      fieldId.dispatchEvent(new Event('change', { bubbles: true }));
    }
    patchGeneratedCode();
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
      if (!field.barStyle) field.barStyle = 'SOLID';
      if (field.barCornerRadius === undefined) field.barCornerRadius = 0;
      if (typeof field.barBorderEnabled !== 'boolean') field.barBorderEnabled = false;
      if (!Number.isFinite(Number(field.barBorderColor))) field.barBorderColor = 0xFFFF;
      if (!Number.isFinite(Number(field.barBorderWidth)) || field.barBorderWidth < 1) field.barBorderWidth = 1;
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
    field.barStyle = 'SOLID';
    field.barCornerRadius = 0;
    field.barBorderEnabled = false;
    field.barBorderColor = 0xFFFF;
    field.barBorderWidth = 1;
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

    if (!activeBar) {
      if (barInspectorWrap) barInspectorWrap.style.display = 'none';
      if (fieldSection) {
        fieldSection.querySelectorAll(':scope > details').forEach((d) => {
          d.style.display = '';
        });
      }
      if (fieldValueSizeWrap) fieldValueSizeWrap.hidden = false;
      if (fieldAlign) fieldAlign.disabled = false;
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

    // Campo BAR activo
    ensureBarState(field);
    if (barInspectorWrap) barInspectorWrap.style.display = 'block';
    if (fieldSection) {
      fieldSection.querySelectorAll(':scope > details').forEach((d) => {
        d.style.display = 'none';
      });
    }

    if (numericFormatDetails) numericFormatDetails.hidden = true;
    if (boolDetails) boolDetails.hidden = true;
    if (fieldCapacityWrap) fieldCapacityWrap.hidden = true;
    if (fieldPreviewWrap) fieldPreviewWrap.hidden = true;
    if (fieldValueSizeWrap) fieldValueSizeWrap.hidden = true;
    if (fieldAlign) fieldAlign.disabled = true;
    if (fieldCppType) fieldCppType.value = 'float';
    if (fieldLabel) fieldLabel.value = field.barLabel || '';
    if (fieldUnit) fieldUnit.value = field.barUnit || '';

    // Título del inspector
    if (fieldInspectorTitle) {
      fieldInspectorTitle.textContent = `Inspector — BAR ${serialFor(field)}`;
    }

    const isCirc = (field.barPreset === 'CIRCULAR');

    // Botones de Selector de Tipo (Barra vs Circular)
    document.querySelectorAll('.bar-preset-btn').forEach((btn) => {
      btn.classList.toggle('active', btn.dataset.barPreset === (isCirc ? 'CIRCULAR' : 'LINEAR'));
    });

    // 1. FORMA
    if (barLinearShapeGroup) barLinearShapeGroup.style.display = isCirc ? 'none' : 'block';
    if (barCircularShapeGroup) barCircularShapeGroup.style.display = isCirc ? 'block' : 'none';

    if (!isCirc) {
      // Linear Forma
      const curOrient = field.barOrientation || 'HORIZONTAL';
      document.querySelectorAll('.bar-orient-btn').forEach((btn) => {
        btn.classList.toggle('active', btn.dataset.orient === curOrient);
      });

      const isVert = (curOrient === 'VERTICAL');
      const currentLength = isVert ? field.barHeight : field.barWidth;
      const currentThick = isVert ? field.barWidth : field.barHeight;
      if (barFormaWidth) barFormaWidth.value = String(currentLength || 110);
      if (barFormaHeight) barFormaHeight.value = String(currentThick || 12);
      if (barFormaWidthName) barFormaWidthName.textContent = isVert ? 'Longitud vertical (px)' : 'Longitud horizontal (px)';
      if (barFormaHeightName) barFormaHeightName.textContent = isVert ? 'Grosor horizontal (px)' : 'Grosor vertical (px)';
      if (barFormaWidthMode) barFormaWidthMode.value = field.barAutoWidth ? 'AUTO' : 'FIXED';

      const isGrad = (field.barFillMode === 'GRADIENT');
      if (barFormaFillMode) barFormaFillMode.value = isGrad ? 'GRADIENT' : 'SOLID';

      const curLinearStyle = field.barStyle || 'SOLID';
      document.querySelectorAll('.bar-linear-style-btn').forEach((btn) => {
        btn.classList.toggle('active', btn.dataset.style === curLinearStyle);
      });

      const curRadius = String(field.barCornerRadius !== undefined ? field.barCornerRadius : 0);
      document.querySelectorAll('.bar-radius-btn').forEach((btn) => {
        btn.classList.toggle('active', btn.dataset.radius === curRadius);
      });

      const borderActive = Boolean(field.barBorderEnabled);
      if (barFormaBorderEnabled) barFormaBorderEnabled.checked = borderActive;
      if (barFormaBorderColorWrap) barFormaBorderColorWrap.style.display = borderActive ? 'flex' : 'none';
      if (barFormaBorderWidthWrap) barFormaBorderWidthWrap.style.display = borderActive ? 'flex' : 'none';
      if (barFormaBorderWidth) barFormaBorderWidth.value = String(field.barBorderWidth || 1);
      if (barFormaBorderColorNative) {
        const bCol = Number.isFinite(Number(field.barBorderColor)) ? Number(field.barBorderColor) : 0xFFFF;
        barFormaBorderColorNative.value = rgb565ToHexCss(bCol);
        if (barFormaBorderColorHex) barFormaBorderColorHex.textContent = hex565(bCol);
      }
    } else {
      // Circular Forma
      if (barFormaCircleRadius) barFormaCircleRadius.value = String(field.circleRadius || 26);
      if (barFormaCircleThickness) barFormaCircleThickness.value = String(field.circleThickness || 5);

      const curSweep = Number(field.circleSweep) || 360;
      document.querySelectorAll('.bar-arc-btn').forEach((btn) => {
        btn.classList.toggle('active', Number(btn.dataset.sweep) === curSweep);
      });

      const curDir = field.circleDirection || 'CW';
      document.querySelectorAll('.bar-dir-btn').forEach((btn) => {
        btn.classList.toggle('active', btn.dataset.dir === curDir);
      });

      const curQuad = field.circleStartAngle || 'TOP';
      document.querySelectorAll('.bar-quadrant-btn').forEach((btn) => {
        btn.classList.toggle('active', btn.dataset.quadrant === curQuad);
      });

      const curStyle = field.circleStyle || 'CONTINUOUS';
      document.querySelectorAll('.bar-style-btn').forEach((btn) => {
        btn.classList.toggle('active', btn.dataset.style === curStyle);
      });

      const circleGrad = (field.circleColorMode === 'GRADIENT');
      if (barFormaCircleColorMode) barFormaCircleColorMode.value = circleGrad ? 'GRADIENT' : 'SOLID';
    }

    // 2. CONTENIDO
    if (barContentLinearWrap) barContentLinearWrap.style.display = isCirc ? 'none' : 'block';
    if (barContentCircularWrap) barContentCircularWrap.style.display = isCirc ? 'block' : 'none';

    if (barContentLabel) barContentLabel.value = field.barLabel || '';
    if (barContentUnit) barContentUnit.value = field.barUnit || '';

    const isShowingLabel = Boolean(field.showLabel);
    if (barContentShowLabel) barContentShowLabel.checked = isShowingLabel;

    const curAlignV = field.alignV || 'TOP';
    const curAlignH = field.alignH || 'LEFT';
    alignMatrixCells.forEach((c) => {
      c.classList.toggle('active', c.dataset.alignV === curAlignV && c.dataset.alignH === curAlignH);
    });

    if (barContentMatrixWrap) {
      barContentMatrixWrap.style.opacity = isShowingLabel ? '1' : '0.4';
      barContentMatrixWrap.style.pointerEvents = isShowingLabel ? 'auto' : 'none';
    }
    if (barContentLabelSizeWrap) {
      barContentLabelSizeWrap.style.opacity = isShowingLabel ? '1' : '0.4';
      barContentLabelSizeWrap.style.pointerEvents = isShowingLabel ? 'auto' : 'none';
    }
    if (barContentLabelSize) barContentLabelSize.value = String(field.labelSize || 1);

    const norm = normalizedFor(field);

    if (!isCirc) {
      if (barContentLinearPctPos) barContentLinearPctPos.value = field.linearPctPos || 'NONE';
    } else {
      if (barContentCircleCenterMode) {
        barContentCircleCenterMode.value = field.circleCenterMode || (field.circleShowPercent ? 'PERCENT' : 'NONE');
      }
      const centerText = getCircleCenterText(field, norm);
      const safeLimit = computeSafeCenterTextSize(field, centerText);
      if (barContentCircleSizeHint) barContentCircleSizeHint.textContent = `Máx: ${safeLimit}×`;
      if (barContentCircleTextSize) {
        Array.from(barContentCircleTextSize.options).forEach((opt) => {
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
        barContentCircleTextSize.value = String(clampedSize);
      }
    }

    // 3. APARIENCIA
    if (barAppLinearFillWrap) barAppLinearFillWrap.style.display = isCirc ? 'none' : 'block';
    if (barAppCircularFillWrap) barAppCircularFillWrap.style.display = isCirc ? 'block' : 'none';

    if (!isCirc) {
      const isGrad = (field.barFillMode === 'GRADIENT');
      if (barAppLinearSolidWrap) barAppLinearSolidWrap.style.display = isGrad ? 'none' : 'block';
      if (barAppLinearGradWrap) barAppLinearGradWrap.style.display = isGrad ? 'grid' : 'none';

      if (barAppLinearColorNative) {
        barAppLinearColorNative.value = rgb565ToHexCss(field.valueColor || 0x07FF);
        if (barAppLinearColorHex) barAppLinearColorHex.textContent = hex565(field.valueColor || 0x07FF);
      }
      if (barAppLinearGradStartNative) {
        barAppLinearGradStartNative.value = rgb565ToHexCss(field.valueColor || 0xF800);
        if (barAppLinearGradStartHex) barAppLinearGradStartHex.textContent = hex565(field.valueColor || 0xF800);
      }
      if (barAppLinearGradEndNative) {
        barAppLinearGradEndNative.value = rgb565ToHexCss(field.valueColorEnd || 0x07E0);
        if (barAppLinearGradEndHex) barAppLinearGradEndHex.textContent = hex565(field.valueColorEnd || 0x07E0);
      }
      if (barAppLinearTrackNative) {
        barAppLinearTrackNative.value = rgb565ToHexCss(field.trackColor || 0x18C3);
        if (barAppLinearTrackHex) barAppLinearTrackHex.textContent = hex565(field.trackColor || 0x18C3);
      }
      const showPctColor = (field.linearPctPos === 'RIGHT' || field.linearPctPos === 'INSIDE');
      if (barAppLinearPctColWrap) barAppLinearPctColWrap.style.display = showPctColor ? 'block' : 'none';
      if (barAppLinearPctColNative) {
        barAppLinearPctColNative.value = rgb565ToHexCss(field.linearPctColor || 0xFFFF);
        if (barAppLinearPctColHex) barAppLinearPctColHex.textContent = hex565(field.linearPctColor || 0xFFFF);
      }
    } else {
      const circleGrad = (field.circleColorMode === 'GRADIENT');
      if (barAppCircularSolidWrap) barAppCircularSolidWrap.style.display = circleGrad ? 'none' : 'block';
      if (barAppCircularGradWrap) barAppCircularGradWrap.style.display = circleGrad ? 'grid' : 'none';

      if (barAppCircularColorNative) {
        barAppCircularColorNative.value = rgb565ToHexCss(field.valueColor || 0x07FF);
        if (barAppCircularColorHex) barAppCircularColorHex.textContent = hex565(field.valueColor || 0x07FF);
      }
      if (barAppCircularGradStartNative) {
        barAppCircularGradStartNative.value = rgb565ToHexCss(field.valueColor || 0xF800);
        if (barAppCircularGradStartHex) barAppCircularGradStartHex.textContent = hex565(field.valueColor || 0xF800);
      }
      if (barAppCircularGradEndNative) {
        barAppCircularGradEndNative.value = rgb565ToHexCss(field.valueColorEnd || 0x07E0);
        if (barAppCircularGradEndHex) barAppCircularGradEndHex.textContent = hex565(field.valueColorEnd || 0x07E0);
      }
      if (barAppCircularTrackNative) {
        barAppCircularTrackNative.value = rgb565ToHexCss(field.circleTrackColor || 0x2104);
        if (barAppCircularTrackHex) barAppCircularTrackHex.textContent = hex565(field.circleTrackColor || 0x2104);
      }
      if (barAppCircularTextColNative) {
        barAppCircularTextColNative.value = rgb565ToHexCss(field.circleTextColor || 0xFFFF);
        if (barAppCircularTextColHex) barAppCircularTextColHex.textContent = hex565(field.circleTextColor || 0xFFFF);
      }
    }

    // Colores Comunes
    if (barAppLabelColNative) {
      barAppLabelColNative.value = rgb565ToHexCss(field.labelColor || 0xFFFF);
      if (barAppLabelColHex) barAppLabelColHex.textContent = hex565(field.labelColor || 0xFFFF);
    }
    if (barAppBgColNative) {
      const bgVal = (field.backgroundColor === 'TRANSPARENT' || !field.backgroundColor) ? 0x0000 : field.backgroundColor;
      barAppBgColNative.value = rgb565ToHexCss(bgVal);
      if (barAppBgColHex) barAppBgColHex.textContent = hex565(bgVal);
    }

    // 4. UBICACIÓN
    const pageIdx = editor()?.getActivePage?.() ?? (field.page || 0);
    const pages = editor()?.getPages?.() || [];
    const pageName = pages[pageIdx]?.name || `0${pageIdx + 1} - Principal`;
    if (barPlacementPage) barPlacementPage.value = pageName;
    if (barPlacementX) barPlacementX.value = String(field.x);
    if (barPlacementY) barPlacementY.value = String(field.y);

    const g = barGeometry(field);
    if (barPlacementBounds) barPlacementBounds.textContent = `${g.fieldW} × ${g.fieldH} px`;
    if (barPlacementValueBounds) barPlacementValueBounds.textContent = `${g.valueW} × ${g.valueH} px`;

    // 5. DATOS PLC
    if (barPlcVariable) barPlcVariable.value = field.variable || '';
    if (barPlcCppType) barPlcCppType.value = 'float';
    if (barPlcMin) barPlcMin.value = String(field.barMin);
    if (barPlcMax) barPlcMax.value = String(field.barMax);
    if (barPlcUnit) barPlcUnit.value = field.barUnit || '';

    if (barTopSimSlider) {
      barTopSimSlider.min = String(field.barMin);
      barTopSimSlider.max = String(field.barMax);
      barTopSimSlider.value = String(field.barValue);
    }
    if (barTopSimReadout) barTopSimReadout.textContent = `${(norm * 100).toFixed(1)} %`;

    if (barPlcSimSlider) {
      barPlcSimSlider.min = String(field.barMin);
      barPlcSimSlider.max = String(field.barMax);
      barPlcSimSlider.value = String(field.barValue);
    }
    if (barPlcSimMinLabel) barPlcSimMinLabel.textContent = String(field.barMin);
    if (barPlcSimMaxLabel) barPlcSimMaxLabel.textContent = String(field.barMax);
    if (barPlcSimReadout) barPlcSimReadout.textContent = `${(norm * 100).toFixed(1)} %`;

    // 6. TÉCNICO
    if (barTechName) barTechName.value = field.name || '';
    if (barTechId) barTechId.value = field.id || '';
    if (barTechPage) barTechPage.value = pageName;

    const varName = sanitizeSymbol(field.variable, 'nivel');
    if (barTechContractCode) {
      barTechContractCode.textContent = `float ${varName} = 0.0f; // Vía JWPLC_Display.setBar()`;
    }

    if (barDiagSimVal) barDiagSimVal.textContent = `${(norm * 100).toFixed(1)} %`;
    if (barDiagUnitVal) barDiagUnitVal.textContent = `${Number(field.barValue || 0).toFixed(1)} ${field.barUnit || ''}`.trim();
    if (barDiagRange) barDiagRange.textContent = `${field.barMin} — ${field.barMax} ${field.barUnit || ''}`.trim();
    if (barDiagX) barDiagX.textContent = String(field.x);
    if (barDiagY) barDiagY.textContent = String(field.y);
    if (barDiagW) barDiagW.textContent = `${g.fieldW} px`;
    if (barDiagH) barDiagH.textContent = `${g.fieldH} px`;
    if (barDiagPad) barDiagPad.textContent = `${g.pad} px`;
    if (barDiagValRegion) barDiagValRegion.textContent = `${g.valueX}, ${g.valueY} (${g.valueW} × ${g.valueH} px)`;

    // Herramienta activa
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
    const bCol = Number.isFinite(Number(field.barBorderColor)) ? Number(field.barBorderColor) : 0xFFFF;
    const borderComment = field.barBorderEnabled ? `, Borde: ${hex565(bCol)} (${field.barBorderWidth || 1}px)` : '';
    const styleHeader = `    // Barra ${isGrad ? 'con Degradado' : 'Sólida'} (${field.barOrientation || 'HORIZONTAL'}) - Estilo: ${field.barStyle || 'SOLID'}, Radio: ${field.barCornerRadius || 0}${borderComment}\n`;
    if (isGrad) {
      const rect = field.barAutoWidth
        ? `JWPLC_UIRect(${field.x}, ${field.y})`
        : `JWPLC_UIRect(${field.x}, ${field.y}, ${Math.max(20, Math.min(WIDTH, Math.trunc(Number(field.barWidth) || 110)))}, ${field.barOrientation === 'VERTICAL' ? Math.max(10, Math.trunc(Number(field.barHeight) || 60)) : 'JWPLC_UI_AUTO'})`;
      return `${styleHeader}` +
             `    JWPLC_UIGradientBarField(\n        ${id},\n        ${rect},\n        JWPLC_UIText(${label}, ${unit}),\n        JWPLC_UIRange(${cppFloat(field.barMin)}, ${cppFloat(field.barMax)}),\n        JWPLC_UIBarStyle(\n            ${field.labelSize},\n            ${cppBool(field.frame)},\n            JWPLC_UI_LAYOUT_${field.layout}),\n        ${hex565(field.valueColor)}, /* Inicio */\n        ${hex565(field.valueColorEnd || 0x07E0)}, /* Fin */\n        ${field.page || 0},\n        JWPLC_UIColors(\n            ${hex565(field.labelColor)},\n            ${hex565(field.valueColor)},\n            ${hex565(field.trackColor || 0x18C3)},\n            ${hex565(field.frameColor)}))`;
    }

    const rect = field.barAutoWidth
      ? `JWPLC_UIRect(${field.x}, ${field.y})`
      : `JWPLC_UIRect(${field.x}, ${field.y}, ${Math.max(20, Math.min(WIDTH, Math.trunc(Number(field.barWidth) || 110)))}, ${field.barOrientation === 'VERTICAL' ? Math.max(10, Math.trunc(Number(field.barHeight) || 60)) : 'JWPLC_UI_AUTO'})`;

    return `${styleHeader}` +
           `    JWPLC_UIBarField(\n        ${id},\n        ${rect},\n        JWPLC_UIText(${label}, ${unit}),\n        JWPLC_UIRange(${cppFloat(field.barMin)}, ${cppFloat(field.barMax)}),\n        JWPLC_UIBarStyle(\n            ${field.labelSize},\n            ${cppBool(field.frame)},\n            JWPLC_UI_LAYOUT_${field.layout}),\n        ${field.page || 0},\n        JWPLC_UIColors(\n            ${hex565(field.labelColor)},\n            ${hex565(field.valueColor)},\n            ${hex565(field.trackColor || field.backgroundColor)},\n            ${hex565(field.frameColor)}))`;
  }

  function replaceHelperCall(text, id, replacement) {
    const target = `${id},`;
    let searchPos = 0;
    while (searchPos < text.length) {
      const idIdx = text.indexOf(target, searchPos);
      if (idIdx < 0) return text;
      const prefix = text.slice(Math.max(0, idIdx - 120), idIdx);
      const helperMatch = prefix.match(/(?:JWPLC_UITextField|JWPLC_UIBarField|JWPLC_UIGradientBarField|JWPLC_UICircularField)\s*\(\s*$/);
      if (helperMatch) {
        let start = idIdx - prefix.length + helperMatch.index;
        while (start > 0 && (text[start - 1] === ' ' || text[start - 1] === '\t')) {
          start -= 1;
        }
        const beforePrefix = text.slice(0, start);
        const commentMatch = beforePrefix.match(/(?:^|\n)([ \t]*\/\/[^\r\n]*\r?\n)[ \t]*$/);
        if (commentMatch) {
          start -= commentMatch[1].length;
        }

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
            if (depth === 0) {
              return text.slice(0, start) + replacement + text.slice(index + 1);
            }
          }
        }
        return text;
      }
      searchPos = idIdx + target.length;
    }
    return text;
  }

  function activeBarFieldsInCode(text) {
    return activeBars().filter((field) => text.includes(String(field.id || '')));
  }

  function patchGeneratedCode() {
    if (!codeOutput?.textContent.includes('generado por JWPLC HMI Designer')) return;
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
    drawBars,
    patchGeneratedCode,
    replaceHelperCall
  };
})();
