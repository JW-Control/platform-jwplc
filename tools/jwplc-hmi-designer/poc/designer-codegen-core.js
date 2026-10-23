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

  function sanitizeSymbol(value, fallback) {
    const cleaned = String(value || '').trim().replace(/[^A-Za-z0-9_]/g, '_');
    if (!cleaned) return fallback;
    return /^[A-Za-z_]/.test(cleaned) ? cleaned : `_${cleaned}`;
  }

  // Alpha15 I3: tildes, ñ y ° con la fuente GLCD (cp437); designer-cp437.js.
  const cp437 = (typeof module === 'object' && module.exports)
    ? require('./designer-cp437.js')
    : self.JWPLCCp437;

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
    return `    JWPLC_UIValueField(\n        ${id},\n        JWPLC_UIRect(${field.x}, ${field.y}),\n        JWPLC_UIText(${textOrNull(field.label)}, ${textOrNull(field.unit)}),\n        ${valueFormatBlock(field)},\n        JWPLC_UIValueStyle(\n            ${field.valueSize},\n            ${field.labelSize},\n            ${cppBool(field.frame)},\n            JWPLC_UI_LAYOUT_${field.layout},\n            JWPLC_UI_ALIGN_${field.align}),\n        ${field.page},\n        ${fieldColors(field)})`;
  }

  function textFieldBlock(field, id) {
    return `    JWPLC_UITextField(\n        ${id},\n        JWPLC_UIRect(${field.x}, ${field.y}),\n        JWPLC_UIText(${textOrNull(field.label)}, ${textOrNull(field.unit)}, ${field.capacity}),\n        JWPLC_UITextFieldStyle(\n            ${field.valueSize},\n            ${field.labelSize},\n            ${cppBool(field.frame)},\n            JWPLC_UI_LAYOUT_${field.layout},\n            JWPLC_UI_ALIGN_${field.align}),\n        ${field.page},\n        ${fieldColors(field)})`;
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
    return `    JWPLC_UIBoolField(\n        ${id},\n        JWPLC_UIRect(${field.x}, ${field.y}),\n        JWPLC_UIText(${textOrNull(field.label)}, ${textOrNull(field.unit)}),\n${boolTextBlock(field)}        JWPLC_UIBoolStyle(\n            ${field.valueSize},\n            ${field.labelSize},\n            ${cppBool(field.frame)},\n            JWPLC_UI_LAYOUT_${field.layout},\n            JWPLC_UI_ALIGN_${field.align}),\n        ${field.page || 0},\n        ${fieldColors(field)})`;
  }

  // field: normalizado por designer-bar (ensureBarState -> applyFirmwareCaps).
  function barFieldBlock(field, id) {
    const label = (field.showLabel && field.barLabel) ? `"${cppString(field.barLabel)}"` : 'nullptr';
    const unit = textOrNull(field.barUnit);
    const isCircular = field.barPreset === 'CIRCULAR';
    const isGrad = (field.barFillMode === 'GRADIENT' || field.barPreset === 'GRADIENT');
    const borderColor = Number.isFinite(Number(field.barBorderColor)) ? Number(field.barBorderColor) : 0xFFFF;
    const borderComment = field.barBorderEnabled ? `, Borde: ${hex565(borderColor)} (${field.barBorderWidth || 1}px)` : '';
    const unsupportedNote = isCircular
      ? '    // AVISO: medidor circular requiere firmware (Alpha13); se genera barra lineal.\n'
      : (isGrad ? '    // AVISO: degradado requiere firmware (Alpha13); se genera color sólido.\n' : '');
    const styleHeader = `${unsupportedNote}    // Barra Sólida (${field.barOrientation || 'HORIZONTAL'}) - Estilo: ${field.barStyle || 'SOLID'}, Radio: ${field.barCornerRadius || 0}${borderComment}\n`;
    const width = Math.max(20, Math.min(SCREEN_W, Math.trunc(Number(field.barWidth) || 110)));
    const height = field.barOrientation === 'VERTICAL' ? Math.max(10, Math.trunc(Number(field.barHeight) || 60)) : 'JWPLC_UI_AUTO';
    const rect = field.barAutoWidth
      ? `JWPLC_UIRect(${field.x}, ${field.y})`
      : `JWPLC_UIRect(${field.x}, ${field.y}, ${width}, ${height})`;

    return `${styleHeader}` +
      `    JWPLC_UIBarField(\n        ${id},\n        ${rect},\n        JWPLC_UIText(${label}, ${unit}),\n        JWPLC_UIRange(${cppFloat(field.barMin)}, ${cppFloat(field.barMax)}),\n        JWPLC_UIBarStyle(\n            ${field.labelSize},\n            ${cppBool(field.frame)},\n            JWPLC_UI_LAYOUT_${field.layout}),\n        ${field.page || 0},\n        ${colorsBlock(field.labelColor, field.valueColor, field.trackColor || field.backgroundColor, field.frameColor)})`;
  }

  function fieldBlock(field, id) {
    if (field.type === 'VALUE') return valueFieldBlock(field, id);
    if (field.type === 'BOOL') return boolFieldBlock(field, id);
    if (field.type === 'BAR') return barFieldBlock(field, id);
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

  function variableDeclaration(field, variable) {
    if (field.type === 'VALUE') return valueDeclaration(field, variable);
    if (field.type === 'BAR') return `float ${variable} = 0.0f;`;
    if (field.type === 'BOOL') return `bool ${variable} = false;`;
    return textDeclaration(field, variable);
  }

  function setterCall(field, id, variable) {
    if (isClockField(field)) return '';
    if (field.type === 'VALUE') return `JWPLC_Display.setValue(${id}, ${variable});`;
    if (field.type === 'BOOL') return `JWPLC_Display.setBool(${id}, ${variable});`;
    if (field.type === 'BAR') return `JWPLC_Display.setBar(${id}, ${variable});`;
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
    fieldBlock,
    variableDeclaration,
    setterCall
  });
});
