// Núcleo puro del codegen (A15-5): funciones sin DOM ni estado global que
// convierten el modelo del Designer en C++ contra JWPLC_UI. Se carga en el
// navegador (window.JWPLCHMICodegenCore) y en node (require) para los tests
// (tests/codegen-core.test.cjs). Las capas de app.js, designer-bool.js y
// designer-bar.js delegan aquí la definición de cada campo.
(function (root, factory) {
  const api = factory();
  if (typeof module === 'object' && module.exports) module.exports = api;
  else root.JWPLCHMICodegenCore = api;
})(typeof self !== 'undefined' ? self : this, () => {
  'use strict';

  const SCREEN_W = 320;
  const SCREEN_H = 170;
  const DEFAULT_BAR_THICKNESS = 12; // DEFAULT_BAR_HEIGHT de JWPLC_UI.cpp

  function sanitizeSymbol(value, fallback) {
    const cleaned = String(value || '').trim().replace(/[^A-Za-z0-9_]/g, '_');
    if (!cleaned) return fallback;
    return /^[A-Za-z_]/.test(cleaned) ? cleaned : `_${cleaned}`;
  }

  // Alpha15 I3: tildes, ñ y ° con la fuente GLCD (cp437); designer-cp437.js.
  const cp437 = (typeof module === 'object' && module.exports)
    ? require('./designer-cp437.js')
    : self.JWPLCCp437;

  const fonts = (typeof module === 'object' && module.exports)
    ? require('./designer-fonts.js')
    : (typeof self !== 'undefined' ? self.JWPLCDesignerFonts : (typeof globalThis !== 'undefined' ? globalThis.JWPLCDesignerFonts : null));

  function fontCppSymbol(id) {
    if (!id || id === 'GLCD') return null;
    if (fonts?.cppSymbol) return fonts.cppSymbol(id);
    if (String(id).startsWith('JWPLC_FONT_')) return id;
    if (id === 'FONT2' || id === 'FONT_16') return 'JWPLC_FONT_16';
    if (id === 'FONT4' || id === 'FONT_26') return 'JWPLC_FONT_26';
    if (id === '7SEG_48' || id === 'FONT_7SEG_48') return 'JWPLC_FONT_7SEG_48';
    return `JWPLC_FONT_${id}`;
  }

  function styleFontsArgs(field) {
    const labelSym = fontCppSymbol(field?.labelFont);
    const valueSym = fontCppSymbol(field?.valueFont);
    if (!labelSym && !valueSym) return '';
    const labelArg = labelSym ? `&${labelSym}` : 'nullptr';
    const valueArg = valueSym ? `&${valueSym}` : 'nullptr';
    return `,
            ${labelArg},
            ${valueArg}`;
  }

  // Literal C++ (sin las comillas) con los bytes que dibuja la TFT.
  function cppString(value) {
    return cp437.cppEscape(value || '');
  }

  function cppBool(value) {
    return value ? 'true' : 'false';
  }

  function cppFloat(value) {
    const number = Number(value);
    if (!Number.isFinite(number)) return '0.0f';
    return `${Number.isInteger(number) ? number.toFixed(1) : String(number)}f`;
  }

  // 'TRANSPARENT' u otro valor no numérico -> 0x0000: los campos del runtime
  // no tienen fondo transparente (el refresco parcial borra con el fondo).
  function hex565(value) {
    const num = Number(value);
    return `0x${(Number.isFinite(num) ? num & 0xFFFF : 0).toString(16).toUpperCase().padStart(4, '0')}`;
  }

  function textOrNull(value) {
    return value ? `"${cppString(value)}"` : 'nullptr';
  }

  function colorsBlock(label, value, background, frame) {
    return `JWPLC_UIColors(\n            ${hex565(label)},\n            ${hex565(value)},\n            ${hex565(background)},\n            ${hex565(frame)})`;
  }

  function fieldColors(field) {
    return colorsBlock(field.labelColor, field.valueColor, field.backgroundColor, field.frameColor);
  }

  const DURATION_PATTERN_CPP = { SS: 'SS', MM_SS: 'MM_SS', HH_MM_SS: 'HH_MM_SS', COMPACT: 'COMPACT' };

  // Alpha15 I1: número (con separador de miles opcional) o duración.
  // Sin separador sale el constructor de 4 argumentos de siempre.
  // VALUE de fecha/hora (I2): sin variable ni setter, lo llena el firmware desde el RTC.
  function isClockField(field) {
    return field?.type === 'VALUE' && field.valueKind === 'DATETIME';
  }

  const DATETIME_PATTERNS = ['HH_MM', 'HH_MM_SS', 'DD_MM', 'DD_MM_YYYY', 'DD_MM_YYYY_HH_MM'];

  function valueFormatBlock(field) {
    if (isClockField(field)) {
      const pattern = DATETIME_PATTERNS.includes(field.dateTimePattern) ? field.dateTimePattern : 'HH_MM';
      return `JWPLC_UIDateTimeFormat(\n            JWPLC_UI_DATETIME_${pattern})`;
    }
    if (field.valueKind === 'DURATION') {
      const pattern = DURATION_PATTERN_CPP[field.durationPattern] || 'MM_SS';
      const unit = field.durationUnit === 'MS' ? 'MILLIS' : 'SECONDS';
      return `JWPLC_UIDurationFormat(\n            JWPLC_UI_DURATION_${pattern},\n            JWPLC_UI_DURATION_${unit},\n            ${Math.max(1, field.integerDigits)},\n            ${cppBool(field.signedValue)})`;
    }
    const sep = [',', '.', ' '].includes(field.thousandsSep) ? `,\n            '${field.thousandsSep}'` : '';
    return `JWPLC_UIValueFormat(\n            ${Math.max(1, field.integerDigits)},\n            ${Math.max(0, field.decimalDigits)},\n            ${cppBool(field.signedValue)},\n            ${cppBool(field.leadingZeros)}${sep})`;
  }

  // Tipo C++ de la variable de un VALUE. float por defecto (como siempre);
  // duraciones y contadores con miles sin decimales usan enteros de 32 bits,
  // que float no representa exactos por encima de 2^24 (~4,6 h en ms).
  function valueCppType(field) {
    if (isClockField(field)) return 'none';
    const integer = field.valueKind === 'DURATION' ||
      ([',', '.', ' '].includes(field.thousandsSep) && !(Number(field.decimalDigits) > 0));
    if (!integer) return 'float';
    return field.signedValue ? 'int32_t' : 'uint32_t';
  }

  function valueDeclaration(field, variable) {
    if (isClockField(field)) return '';
    const type = valueCppType(field);
    return type === 'float' ? `float ${variable} = 0.0f;` : `${type} ${variable} = 0;`;
  }

  function valueFieldBlock(field, id) {
    const fontArgs = styleFontsArgs(field);
    return `    JWPLC_UIValueField(\n        ${id},\n        JWPLC_UIRect(${field.x}, ${field.y}),\n        JWPLC_UIText(${textOrNull(field.label)}, ${textOrNull(field.unit)}),\n        ${valueFormatBlock(field)},\n        JWPLC_UIValueStyle(\n            ${field.valueSize},\n            ${field.labelSize},\n            ${cppBool(field.frame)},\n            JWPLC_UI_LAYOUT_${field.layout},\n            JWPLC_UI_ALIGN_${field.align}${fontArgs}),\n        ${field.page},\n        ${fieldColors(field)})`;
  }

  function textFieldBlock(field, id) {
    const fontArgs = styleFontsArgs(field);
    return `    JWPLC_UITextField(\n        ${id},\n        JWPLC_UIRect(${field.x}, ${field.y}),\n        JWPLC_UIText(${textOrNull(field.label)}, ${textOrNull(field.unit)}, ${field.capacity}),\n        JWPLC_UITextFieldStyle(\n            ${field.valueSize},\n            ${field.labelSize},\n            ${cppBool(field.frame)},\n            JWPLC_UI_LAYOUT_${field.layout},\n            JWPLC_UI_ALIGN_${field.align}${fontArgs}),\n        ${field.page},\n        ${fieldColors(field)})`;
  }

  function boolStateColorsOn(field) {
    return Boolean(field.stateColors);
  }

  // Colores por estado (texto del valor y fondo de todo el campo): requieren
  // JWPLC_Display con JWPLC_UI_HAS_BOOL_STATE_BACKGROUNDS; con un package
  // anterior el header compila con un solo color (colors.value).
  function boolTextBlock(field) {
    const texts = `"${cppString(field.falseText)}",\n            "${cppString(field.trueText)}"`;
    if (field.boolStyle === 'PILOT') {
      // Piloto (Alpha15 I1). Fondo por estado sólo si difiere del fondo del campo.
      const bgOf = (v) => (Number.isFinite(Number(v)) ? Number(v) : Number(field.backgroundColor)) & 0xFFFF;
      const base = Number(field.backgroundColor) & 0xFFFF;
      const withBg = bgOf(field.falseBackground) !== base || bgOf(field.trueBackground) !== base;
      const args = `${hex565(field.falseColor)},\n            ${hex565(field.trueColor)}` +
        (withBg ? `,\n            ${hex565(bgOf(field.falseBackground))},\n            ${hex565(bgOf(field.trueBackground))}` : '');
      return `#ifdef JWPLC_UI_HAS_BOOL_PILOT\n` +
        `        JWPLC_UIBoolPilot(\n            ${args}),\n` +
        `#else\n` +
        `        JWPLC_UIBoolText(\n            ${texts}),\n` +
        `#endif\n`;
    }
    if (!boolStateColorsOn(field)) return `        JWPLC_UIBoolText(\n            ${texts}),\n`;
    const bg = (v) => hex565(Number.isFinite(Number(v)) ? v : field.backgroundColor);
    return `#ifdef JWPLC_UI_HAS_BOOL_STATE_BACKGROUNDS\n` +
      `        JWPLC_UIBoolText(\n            ${texts},\n            ${hex565(field.falseColor)},\n            ${hex565(field.trueColor)},\n            ${bg(field.falseBackground)},\n            ${bg(field.trueBackground)}),\n` +
      `#else\n` +
      `        JWPLC_UIBoolText(\n            ${texts}),\n` +
      `#endif\n`;
  }

  // field: con falseText/trueText ya resueltos (designer-bool ensureBoolState).
  function boolFieldBlock(field, id) {
    const fontArgs = styleFontsArgs(field);
    return `    JWPLC_UIBoolField(\n        ${id},\n        JWPLC_UIRect(${field.x}, ${field.y}),\n        JWPLC_UIText(${textOrNull(field.label)}, ${textOrNull(field.unit)}),\n${boolTextBlock(field)}        JWPLC_UIBoolStyle(\n            ${field.valueSize},\n            ${field.labelSize},\n            ${cppBool(field.frame)},\n            JWPLC_UI_LAYOUT_${field.layout},\n            JWPLC_UI_ALIGN_${field.align}${fontArgs}),\n        ${field.page || 0},\n        ${fieldColors(field)})`;
  }

  // field: normalizado por designer-bar (ensureBarState -> applyFirmwareCaps).
  function barFieldBlock(field, id) {
    const label = (field.showLabel && field.barLabel) ? `"${cppString(field.barLabel)}"` : 'nullptr';
    const unit = textOrNull(field.barUnit);
    const isCircular = field.barPreset === 'CIRCULAR';

    if (isCircular) {
      const radius = Math.max(10, Math.trunc(Number(field.circleRadius) || 26));
      const diameter = radius * 2;
      const sweep = Number(field.circleSweep) || 360;
      const thick = Math.max(2, Math.min(radius - 2, Math.trunc(Number(field.circleThickness) || 5)));
      const startQuad = field.circleStartAngle === 'RIGHT' ? 1 : (field.circleStartAngle === 'BOTTOM' ? 2 : (field.circleStartAngle === 'LEFT' ? 3 : 0));
      const cw = (field.circleDirection !== 'CCW');
      const seg = (field.circleStyle === 'SEGMENTED');
      const centerMode = field.circleCenterMode === 'PERCENT' ? 1 : (field.circleCenterMode === 'VALUE' ? 2 : 0);
      const centerTextSize = Number(field.circleTextSize) || 1;
      const centerTextColor = hex565(field.circleTextColor || 0xFFFF);
      const trackColor = hex565(Number.isFinite(Number(field.circleTrackColor)) ? Number(field.circleTrackColor) : (Number.isFinite(Number(field.trackColor)) ? Number(field.trackColor) : 0x0000));
      const styleHeader = `    // Medidor Circular (${sweep}°) - Estilo: ${field.circleStyle || 'CONTINUOUS'} - Inicio: ${field.circleStartAngle || 'TOP'} (${cw ? 'CW' : 'CCW'})\n`;
      const rect = `JWPLC_UIRect(${field.x}, ${field.y}, ${diameter}, ${diameter})`;

      return `${styleHeader}` +
        `    JWPLC_UICircularField(\n` +
        `        ${id},\n` +
        `        ${rect},\n` +
        `        JWPLC_UIText(${label}, nullptr),\n` +
        `        JWPLC_UIRange(${cppFloat(field.barMin)}, ${cppFloat(field.barMax)}),\n` +
        `        JWPLC_UICircularStyle(\n` +
        `            ${sweep},\n` +
        `            ${thick},\n` +
        `            ${centerMode},\n` +
        `            ${field.labelSize || 1},\n` +
        `            ${cppBool(field.frame)},\n` +
        `            ${trackColor},\n` +
        `            ${startQuad},\n` +
        `            ${cppBool(cw)},\n` +
        `            ${cppBool(seg)},\n` +
        `            ${centerTextSize},\n` +
        `            ${centerTextColor}),\n` +
        `        ${field.page || 0},\n` +
        `        ${colorsBlock(field.labelColor, field.valueColor, field.backgroundColor, field.frameColor)})`;
    }

    const isGrad = (field.barFillMode === 'GRADIENT' || field.barPreset === 'GRADIENT');
    const isVert = field.barOrientation === 'VERTICAL';
    const isBlocks = field.barStyle === 'BLOCKS';
    const borderColor = Number.isFinite(Number(field.barBorderColor)) ? Number(field.barBorderColor) : 0xFFFF;
    const borderComment = field.barBorderEnabled ? `, Borde: ${hex565(borderColor)} (${field.barBorderWidth || 1}px)` : '';
    const fillLabel = isGrad ? 'con degradado' : (isBlocks ? 'por bloques' : 'sólida');
    const styleHeader = `    // Barra ${fillLabel} (${field.barOrientation || 'HORIZONTAL'}) - Radio: ${field.barCornerRadius || 0}${borderComment}\n`;
    const width = isVert
      ? Math.max(6, Math.min(SCREEN_W, Math.trunc(Number(field.barWidth) || 14)))
      : Math.max(20, Math.min(SCREEN_W, Math.trunc(Number(field.barWidth) || 110)));
    const height = isVert
      ? Math.max(10, Math.min(SCREEN_H, Math.trunc(Number(field.barHeight) || 60)))
      : 'JWPLC_UI_AUTO';
    const horizontalThickness = Math.max(4, Math.min(SCREEN_H, Math.trunc(Number(field.barHeight) || DEFAULT_BAR_THICKNESS)));
    const thickness = isVert ? Math.min(255, width) : (horizontalThickness === DEFAULT_BAR_THICKNESS ? 0 : horizontalThickness);
    const length = isVert ? height : 0;
    // Vertical: el tamaño de la barra va en la config y el firmware calcula la
    // caja del campo. Horizontal: ancho total del campo y grosor en la config.
    const rect = (field.barAutoWidth || isVert)
      ? `JWPLC_UIRect(${field.x}, ${field.y})`
      : `JWPLC_UIRect(${field.x}, ${field.y}, ${width}, ${height})`;

    const hasRadius = (field.barCornerRadius === 'FULL' || (Number(field.barCornerRadius) || 0) > 0);
    const hasBorder = Boolean(field.barBorderEnabled);
    const hasSize = thickness > 0 || length > 0;
    const hasConfig = isVert || isBlocks || isGrad || hasRadius || hasBorder || hasSize;
    // En JWPLC_UIBarConfig colorEnd 0 significa sólido: el negro va como 0x0001.
    const endRaw = field.valueColorEnd;
    const endColor = (endRaw === undefined || endRaw === null || !Number.isFinite(Number(endRaw))) ? 0x07E0 : (Number(endRaw) & 0xFFFF);
    const colorEnd = isGrad ? hex565(endColor === 0 ? 0x0001 : endColor) : '0';
    const rVal = field.barCornerRadius === 'FULL' ? 255 : Math.max(0, Math.trunc(Number(field.barCornerRadius) || 0));
    const bColor = hasBorder ? hex565(borderColor) : '0xFFFF';
    const bWidth = hasBorder ? Math.max(1, Math.min(10, Math.trunc(Number(field.barBorderWidth) || 1))) : 1;

    let configBlock = '';
    if (hasConfig) {
      const args = [
        isVert ? 'JWPLC_UI_BAR_VERTICAL' : 'JWPLC_UI_BAR_HORIZONTAL',
        isBlocks ? 'JWPLC_UI_BAR_STYLE_BLOCKS' : 'JWPLC_UI_BAR_STYLE_SOLID',
        colorEnd
      ];
      if (hasRadius || hasBorder || hasSize) args.push(rVal, cppBool(hasBorder), bColor, bWidth);
      if (hasSize) args.push(thickness, length);
      configBlock = `,\n        JWPLC_UIBarConfig(\n            ${args.join(',\n            ')})`;
    }

    return `${styleHeader}` +
      `    JWPLC_UIBarField(\n        ${id},\n        ${rect},\n        JWPLC_UIText(${label}, ${unit}),\n        JWPLC_UIRange(${cppFloat(field.barMin)}, ${cppFloat(field.barMax)}),\n        JWPLC_UIBarStyle(\n            ${field.labelSize},\n            ${cppBool(field.frame)},\n            JWPLC_UI_LAYOUT_${field.layout}),\n        ${field.page || 0},\n        ${colorsBlock(field.labelColor, field.valueColor, field.trackColor || field.backgroundColor, field.frameColor)}${configBlock})`;
  }

  function trendChannelVariables(field, primaryVar) {
    const vars = [sanitizeSymbol(primaryVar || field.variable, 'temperatura')];
    if (field.ch2Enabled) {
      vars.push(sanitizeSymbol(field.ch2Variable, 'consigna'));
    }
    if (field.ch3Enabled) {
      vars.push(sanitizeSymbol(field.ch3Variable, 'salida'));
    }
    return vars;
  }

  function trendFieldBlock(field, id) {
    const width = Math.max(40, Math.trunc(Number(field.width || field.w) || 200));
    const height = Math.max(30, Math.trunc(Number(field.height || field.h) || 80));
    const rect = `JWPLC_UIRect(${field.x}, ${field.y}, ${width}, ${height})`;
    const title = textOrNull(field.title || field.label);
    const unit = textOrNull(field.unit);
    const minVal = Number(field.minValue) || 0;
    const maxVal = Number(field.maxValue) || 100;
    const sampleMs = Math.max(50, Math.trunc(Number(field.sampleIntervalMs) || 1000));
    const visiblePts = Math.max(20, Math.trunc(Number(field.visibleSamples) || 150));
    const scaleMode = field.scaleMode === 'MANUAL' ? 1 : 0;
    const decimals = Math.max(0, Math.min(3, Math.trunc(Number(field.decimals) ?? 1)));
    const lineW = Number(field.lineWidth) === 2 ? 2 : 1;
    const showAxes = cppBool(field.showAxes !== false);
    const showGuides = cppBool(field.showGuides !== false);
    const showTime = cppBool(field.showTimeAxis !== false);
    const enableSD = cppBool(Boolean(field.enableSD));
    const sdIntervalMs = Math.max(100, Math.trunc(Number(field.sdIntervalMs) || 1000));
    const curveColor = hex565(field.curveColor ?? 0xFD20);
    const axesColor = hex565(field.axesColor ?? 0xFFFF);
    const gridColor = hex565(field.gridColor ?? 0x4208);
    const textColor = hex565(field.textColor ?? 0xFFFF);
    const bgColor = hex565(field.backgroundColor ?? 0x0000);

    const ch2On = Boolean(field.ch2Enabled);
    const ch3On = Boolean(field.ch3Enabled);
    const channelCount = 1 + (ch2On ? 1 : 0) + (ch3On ? 1 : 0);
    const c2Color = hex565(ch2On ? (field.ch2Color ?? 0x07FF) : (ch3On ? (field.ch3Color ?? 0x07E0) : 0x07FF));
    const c3Color = hex565(field.ch3Color ?? 0x07E0);

    const activeThresholds = [];
    if (field.thr1Enabled) activeThresholds.push({ val: Number(field.thr1Value) || 0, color: hex565(field.thr1Color ?? 0xF800) });
    if (field.thr2Enabled) activeThresholds.push({ val: Number(field.thr2Value) || 0, color: hex565(field.thr2Color ?? 0xFFE0) });
    const thresholdCount = activeThresholds.length;
    const t1Val = activeThresholds[0]?.val ?? 85;
    const t1Color = activeThresholds[0]?.color ?? hex565(0xF800);
    const t2Val = activeThresholds[1]?.val ?? 20;
    const t2Color = activeThresholds[1]?.color ?? hex565(0xFFE0);

    const hasExtras = channelCount > 1 || thresholdCount > 0;
    const extrasArgs = hasExtras
      ? `,\n            ${showGuides},\n            ${channelCount},\n            ${c2Color},\n            ${c3Color},\n            ${thresholdCount},\n            ${cppFloat(t1Val)},\n            ${t1Color},\n            ${cppFloat(t2Val)},\n            ${t2Color}`
      : (field.showGuides === false ? `,\n            ${showGuides}` : '');

    const metaParts = [`Muestreo: ${sampleMs}ms`, `Escala: ${field.scaleMode || 'AUTO'}`];
    if (channelCount > 1) metaParts.push(`Canales: ${channelCount}`);
    if (thresholdCount > 0) metaParts.push(`Umbrales: ${thresholdCount}`);
    const styleHeader = `    // Gráfica de Tendencia (${width}x${height}) - ${metaParts.join(' - ')}\n`;

    return `${styleHeader}` +
      `    JWPLC_UITrendField(\n` +
      `        ${id},\n` +
      `        ${rect},\n` +
      `        JWPLC_UIText(${title}, ${unit}),\n` +
      `        JWPLC_UIRange(${cppFloat(minVal)}, ${cppFloat(maxVal)}),\n` +
      `        JWPLC_UITrendConfig(\n` +
      `            ${sampleMs},\n` +
      `            ${visiblePts},\n` +
      `            ${scaleMode},\n` +
      `            ${decimals},\n` +
      `            ${lineW},\n` +
      `            ${showAxes},\n` +
      `            ${showTime},\n` +
      `            ${enableSD},\n` +
      `            ${sdIntervalMs},\n` +
      `            ${curveColor},\n` +
      `            ${axesColor},\n` +
      `            ${gridColor},\n` +
      `            ${textColor},\n` +
      `            ${bgColor}${extrasArgs}),\n` +
      `        ${field.page || 0},\n` +
      `        ${colorsBlock(field.labelColor ?? 0xFFFF, field.curveColor ?? 0xFD20, field.backgroundColor ?? 0x0000, field.axesColor ?? 0xFFFF)})`;
  }

  function fieldBlock(field, id) {
    if (field.type === 'VALUE') return valueFieldBlock(field, id);
    if (field.type === 'BOOL') return boolFieldBlock(field, id);
    if (field.type === 'BAR') return barFieldBlock(field, id);
    if (field.type === 'TREND') return trendFieldBlock(field, id);
    return textFieldBlock(field, id);
  }

  // Alpha15 I3: el sketch escribe el TEXT en UTF-8 y cada tilde ocupa 2 bytes;
  // el buffer reserva 2 bytes por carácter para que «Presión» no se corte.
  // setText() lo convierte y recorta a la capacidad en caracteres de la TFT.
  function textBufferSize(field) {
    return 2 * Math.max(1, Number(field.capacity) || 1) + 1;
  }

  function textDeclaration(field, variable) {
    const capacity = Math.max(1, Number(field.capacity) || 1);
    return `char ${variable}[${textBufferSize(field)}] = {}; // ${capacity} caracteres (UTF-8: cada tilde ocupa 2 bytes)`;
  }

  function trendVariableDeclarations(field, variable) {
    return trendChannelVariables(field, variable).map((v) => `float ${v} = 0.0f;`).join('\n');
  }

  function variableDeclaration(field, variable) {
    if (field.type === 'VALUE') return valueDeclaration(field, variable);
    if (field.type === 'TREND') return trendVariableDeclarations(field, variable);
    if (field.type === 'BAR') return `float ${variable} = 0.0f;`;
    if (field.type === 'BOOL') return `bool ${variable} = false;`;
    return textDeclaration(field, variable);
  }

  function setterCall(field, id, variable) {
    if (isClockField(field)) return '';
    if (field.type === 'VALUE') return `JWPLC_Display.setValue(${id}, ${variable});`;
    if (field.type === 'BOOL') return `JWPLC_Display.setBool(${id}, ${variable});`;
    if (field.type === 'BAR') return `JWPLC_Display.setBar(${id}, ${variable});`;
    if (field.type === 'TREND') {
      const vars = trendChannelVariables(field, variable);
      return `JWPLC_Display.setTrend(${id}, ${vars.join(', ')});`;
    }
    return `JWPLC_Display.setText(${id}, ${variable});`;
  }

  return Object.freeze({
    sanitizeSymbol,
    cppString,
    cppBool,
    cppFloat,
    hex565,
    valueFieldBlock,
    valueFormatBlock,
    valueCppType,
    valueDeclaration,
    textDeclaration,
    textBufferSize,
    isClockField,
    textFieldBlock,
    boolFieldBlock,
    boolStateColorsOn,
    barFieldBlock,
    trendChannelVariables,
    trendVariableDeclarations,
    trendFieldBlock,
    fontCppSymbol,
    styleFontsArgs,
    fieldBlock,
    variableDeclaration,
    setterCall
  });
});
